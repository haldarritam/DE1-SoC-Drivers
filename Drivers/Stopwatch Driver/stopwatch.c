#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "address_map_arm.h"
#include "interrupt_ID.h"
#include "timer_interface.h"

#define SUCCESS 0
#define TIMER_DEVICE_NAME "stopwatch"
#define MAX_SIZE 10

//#define TEST_MODE
#ifdef TEST_MODE
#define HUNDRETH_SEC_LOW 0x1000
#define HUNDRETH_SEC_HIGH 0x0000
#else
#define HUNDRETH_SEC_LOW 0x4240
#define HUNDRETH_SEC_HIGH 0x000F
#endif

/* Kernel character device driver. This driver outputs the Timer0 remaining
 * countdown time to /dev/stopwatch.
 * This version of the code uses copy_to_user to send data to user-space.
 */

static int timer_device_open (struct inode *, struct file *);
static int timer_device_release (struct inode *, struct file *);
static ssize_t timer_device_read (struct file *, char *, size_t, loff_t *);
static ssize_t timer_device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);

static dev_t timer_dev_no = 0;
static struct cdev *timer_cdev = NULL;
static struct class *timer_class = NULL;
static char timer_msg[MAX_SIZE];

static struct file_operations timer_fops = {
	.owner = THIS_MODULE,
	.read = timer_device_read,
	.write = timer_device_write,
	.open = timer_device_open,
	.release = timer_device_release
};

static bool display_countdown = false;


void start_timer(void);
void stop_timer(void);
int timer_reached_zero(void);
void set_timer(char* setting, int length);
int time_check(int* check_unit, int limit, int wrap_to, int decrement_unit);
irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs);


typedef struct timer_val
{
   int hundreth, dec, sec_l, sec_h, min_l, min_h;
} timer_val;

timer_val timer = {9, 9, 9, 5, 9, 5};

void write_on_display(int display, int i);
void clear_display(void);
void update_display(timer_val timer);

void* LW_virtual;
volatile unsigned int* timer_ptr;

static int __init start_driver(void)
{
	int err = 0;

	LW_virtual = ioremap_nocache(LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
	timer_ptr = LW_virtual + TIMER0_BASE;

	/* Get a device number. Get one minor number (0) */
	if ((err = alloc_chrdev_region (&timer_dev_no, 0, 1, TIMER_DEVICE_NAME)) < 0) {
		printk (KERN_ERR "timer: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}

	// Allocate and initialize the character device
	timer_cdev = cdev_alloc (); 
	timer_cdev->ops = &timer_fops; 
	timer_cdev->owner = THIS_MODULE; 
   
	// Add the character device to the kernel
	if ((err = cdev_add (timer_cdev, timer_dev_no, 1)) < 0) {
		printk (KERN_ERR "timer: cdev_add() failed with return value %d\n", err);
		return err;
	}
	
	timer_class = class_create (THIS_MODULE, TIMER_DEVICE_NAME);
	device_create (timer_class, NULL, timer_dev_no, NULL, TIMER_DEVICE_NAME );
	
    *(timer_ptr + TMR_SVAL_LOW) = HUNDRETH_SEC_LOW;
    *(timer_ptr + TMR_SVAL_HIGH) = HUNDRETH_SEC_HIGH;
    *(timer_ptr + TMR_CONTROL) = 0x07;
    *(timer_ptr + TMR_STATUS) = 0x00;
	
	// Register the interrupt handler for the timer
    err = request_irq (TIMER0_IRQ, (irq_handler_t) timer_irq_handler, IRQF_TIMER, 
      "timer_irq_handler", (void *) (timer_irq_handler));
	
	return err;
}

static void __exit stop_driver(void)
{
	free_irq (TIMER0_IRQ, (void*) timer_irq_handler);
	stop_timer();
	clear_display();
	device_destroy (timer_class, timer_dev_no);
	cdev_del (timer_cdev);
	class_destroy (timer_class);
	unregister_chrdev_region (timer_dev_no, 1);
}

/* Called when a process opens timer */
static int timer_device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process closes timer */
static int timer_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Called when a process reads from timer. Provides character data from timer_msg.
 * Returns, and sets *offset to, the number of bytes read. */
static ssize_t timer_device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	size_t bytes;
	sprintf(timer_msg, "%i%i:%i%i:%i%i\n", timer.min_h, timer.min_l, 
	        timer.sec_h, timer.sec_l, timer.dec, timer.hundreth);
	bytes = strlen (timer_msg) - (*offset);	// how many bytes not yet sent?
	bytes = bytes > length ? length : bytes;	// too much to send all at once?
	
	if (bytes)
		if (copy_to_user (buffer, &timer_msg[*offset], bytes) != 0)
			printk (KERN_ERR "Error: copy_to_user unsuccessful");
	*offset = bytes;	// keep track of number of bytes sent to the user
	
	return bytes;
}

static ssize_t timer_device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
	size_t bytes = length;
	unsigned long ret = 0;
	
	if (bytes > MAX_SIZE - 1)
		bytes = MAX_SIZE - 1;
	
	ret = copy_from_user (timer_msg, buffer, bytes);
	timer_msg[bytes] = '\0';
	if (timer_msg[bytes-1] == '\n')
		timer_msg[bytes-1] = '\0';
	
	if (!strcmp(timer_msg, "stop"))
		stop_timer();
	else if (!strcmp(timer_msg, "run"))
		start_timer();
	else if (!strcmp(timer_msg, "disp"))
		display_countdown = true;
	else if (!strcmp(timer_msg, "nodisp"))
		display_countdown = false;
	else
	{
		set_timer(timer_msg, strlen(timer_msg));
		if(display_countdown)
			update_display(timer);
	}

	return bytes;
}



// -- IRQ and timer support functions --

void start_timer(void)
{
	*(timer_ptr + TMR_CONTROL) = (*(timer_ptr + TMR_CONTROL) & ~(0x03<<0x02)) | (0x01<<0x02);
}

void stop_timer(void)
{
	*(timer_ptr + TMR_CONTROL) = (*(timer_ptr + TMR_CONTROL) & ~(0x03<<0x02)) | (0x02<<0x02);
}

int timer_reached_zero(void)
{
	return timer.hundreth == 0 && timer.dec == 0 &&
           timer.sec_l == 0 && timer.sec_h == 0 &&
	       timer.min_l == 0 && timer.min_h == 0;
}

void set_timer(char* setting, int length)
{
	unsigned int time [6] = {0, 0, 0, 0, 0 ,0}; //min_h, min_l, sec_h, sec_l, dec, hundreth
	unsigned char setting_iter = 0;
	unsigned int* time_iter = time;
	char number[2] = "\0";
	number[1] = '\0';
	
	if (length != 8 || setting[2] != ':' || setting[5] != ':')
		return;
	
	while (setting_iter < 8)
	{
		if (setting_iter != 2 && setting_iter != 5)
		{
			number[0] = setting[setting_iter];
			if(kstrtouint(number, 10, time_iter++))
			{
				printk("Invalid timer setting input. Please use format MM:SS:DD\n");
				return;
			}
		}
		setting_iter++;
	}

	// Checking if the set values are valid. 
	// If not, set to the maximum possible.
	if ((time[0] * 10 + time[1]) > 59)
	{
		time[0] = 5;
		time[1] = 9;
	}
	
	if ((time[2] * 10 + time[3]) > 59)
	{
		time[2] = 5;
		time[3] = 9;
	}

	timer.min_h = time[0];
	timer.min_l = time[1];
	timer.sec_h = time[2];
	timer.sec_l = time[3];
	timer.dec = time[4];
	timer.hundreth = time[5];
}

int time_check(int* check_unit, int limit, int wrap_to, int decrement_unit)
{
   if (*check_unit <= limit)
   {
      *check_unit = wrap_to;
      decrement_unit--;
   }
   return decrement_unit;
}

irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
   *(timer_ptr + TMR_STATUS) = 0x00;
   
   if (timer_reached_zero())
   {
	   stop_timer();
	   return (irq_handler_t) IRQ_HANDLED;
   }
   
   timer.hundreth --;
   timer.dec   = time_check(&timer.hundreth, -1, 9, timer.dec);  
   timer.sec_l = time_check(&timer.dec, -1, 9, timer.sec_l);   
   timer.sec_h = time_check(&timer.sec_l, -1, 9, timer.sec_h);
   timer.min_l = time_check(&timer.sec_h, -1, 5, timer.min_l);
   timer.min_h = time_check(&timer.min_l, -1, 9, timer.min_h);
   timer.min_h = time_check(&timer.min_h, -1, 5, timer.min_h);

   if (display_countdown)
   {
   	  update_display(timer);
   }
   else
   {
   	  clear_display();
   }
     
   return (irq_handler_t) IRQ_HANDLED;
}

void update_display(timer_val timer)
{
	write_on_display(0, timer.hundreth);
	write_on_display(1, timer.dec);
	write_on_display(2, timer.sec_l);
	write_on_display(3, timer.sec_h);
	write_on_display(4, timer.min_l);
	write_on_display(5, timer.min_h);
}

void clear_display(void)
{
	int i = 0;
	for (i = 0; i < 6; i++)
		write_on_display(i, 10);
}

void write_on_display(int display, int i)
{
   volatile unsigned int *HEX_ptr;
   char numbers [] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                      0x6D, 0x7D, 0x07, 0x7F, 0x67, 0x00};
   
   if (display > 6)
   {
      printk("Assignment to invalid display. "
         "Ignoring request.\n");
      return;
   }

   if (display == 4 || display == 5)
   {
      HEX_ptr = LW_virtual + HEX5_HEX4_BASE;
      display -= 4;
   }
   else 
      HEX_ptr = LW_virtual + HEX3_HEX0_BASE;
  
   if (i > 10)
	   i = 10;

   *HEX_ptr = (*HEX_ptr & ~(0xFF << (display * 8))) | (numbers[i] << (display * 8));
}

MODULE_LICENSE("GPL");
module_init (start_driver);
module_exit (stop_driver);