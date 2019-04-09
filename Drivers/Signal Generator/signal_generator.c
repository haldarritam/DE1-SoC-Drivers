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
#define SIG_GEN_DEVICE_NAME "signal_generator"
#define MAX_SIZE 10

#define TEN_HZ_PULSE_CTR 0x004C4B40
#define CTR_MASK_LOW     0x0000FFFF
#define CTR_MASK_HIGH    0xFFFF0000
#define MAX_DIGITS       6
#define DIRECTION_REG	 1

/* Kernel character device driver. This driver outputs the Timer0 remaining
 * countdown time to /dev/stopwatch.
 * This version of the code uses copy_to_user to send data to user-space.
 */

static int sig_gen_device_open (struct inode *, struct file *);
static int sig_gen_device_release (struct inode *, struct file *);

static dev_t sig_gen_dev_no = 0;
static struct cdev *sig_gen_cdev = NULL;
static struct class *sig_gen_class = NULL;

static struct file_operations sig_gen_fops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = NULL,
	.open = sig_gen_device_open,
	.release = sig_gen_device_release
};


void start_timer(void);
void stop_timer(void);
irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs);

unsigned int read_frequency(void);
unsigned int convert_freq_to_pulses(unsigned int freq);
void glow_ledr(unsigned int freq);
void set_hex(unsigned int freq);
void break_into_digits(unsigned int digits[], unsigned int number);
void write_on_display(int display, char content);
void driver_main(void);

void* LW_virtual;
volatile unsigned int* timer_ptr;
volatile unsigned int* sw_ptr;
volatile unsigned int* hex_ptr;
volatile unsigned int* ledr_ptr;
volatile unsigned int* prl_ptr;

volatile int signal_state = 0;

static int __init start_driver(void)
{
	int err = 0;

	LW_virtual = ioremap_nocache(LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
	timer_ptr = LW_virtual + TIMER0_BASE;
	sw_ptr = LW_virtual + SW_BASE;
	hex_ptr = LW_virtual + HEX3_HEX0_BASE;
	ledr_ptr = LW_virtual + LEDR_BASE;
	prl_ptr = LW_virtual + GPIO0_BASE;

	/* Get a device number. Get one minor number (0) */
	if ((err = alloc_chrdev_region (&sig_gen_dev_no, 0, 1, SIG_GEN_DEVICE_NAME)) < 0) {
		printk (KERN_ERR "timer: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}

	// Allocate and initialize the character device
	sig_gen_cdev = cdev_alloc (); 
	sig_gen_cdev->ops = &sig_gen_fops; 
	sig_gen_cdev->owner = THIS_MODULE; 
   
	// Add the character device to the kernel
	if ((err = cdev_add (sig_gen_cdev, sig_gen_dev_no, 1)) < 0) {
		printk (KERN_ERR "timer: cdev_add() failed with return value %d\n", err);
		return err;
	}
	
	sig_gen_class = class_create (THIS_MODULE, SIG_GEN_DEVICE_NAME);
	device_create (sig_gen_class, NULL, sig_gen_dev_no, NULL, SIG_GEN_DEVICE_NAME );

	*(prl_ptr + DIRECTION_REG) = 1;

	driver_main();
	*(timer_ptr + TMR_CONTROL) = 0x07;
    *(timer_ptr + TMR_STATUS) = 0x00;

	start_timer();

	// Register the interrupt handler for the timer
    err = request_irq (TIMER0_IRQ, (irq_handler_t) timer_irq_handler, IRQF_TIMER, 
      	  "timer_irq_handler", (void *) (timer_irq_handler));
	
	return err;
}

static void __exit stop_driver(void)
{
	int i = 0;
	for(i = 0; i < MAX_DIGITS; i++)
		write_on_display(i, 0x00);
	*ledr_ptr = 0x00;
	stop_timer();
	free_irq(TIMER0_IRQ, (void*) timer_irq_handler);
	iounmap (LW_virtual);
	device_destroy (sig_gen_class, sig_gen_dev_no);
	cdev_del (sig_gen_cdev);
	class_destroy (sig_gen_class);
	unregister_chrdev_region (sig_gen_dev_no, 1);
}

/* Called when a process opens timer */
static int sig_gen_device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process closes timer */
static int sig_gen_device_release(struct inode *inode, struct file *file)
{
	return 0;
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

irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	*(timer_ptr + TMR_STATUS) = 0x00;
   driver_main();     
   return (irq_handler_t) IRQ_HANDLED;
}

unsigned int read_frequency(void)
{
	// returns the frequency divided by 10.
	return (((*sw_ptr & 0x3C0) >> 6) + 1);	
}

unsigned int convert_freq_to_pulses(unsigned int freq)
{
	const unsigned int ten_hz_pulse_ctr = 0x004C4B40;
	return (ten_hz_pulse_ctr / freq);
}

void glow_ledr(unsigned int freq) 
{
	const char freq_to_ledr_shift = 6;
	*ledr_ptr = freq << freq_to_ledr_shift;
}

void set_hex(unsigned int freq)
{
	char numbers [] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                   	   0x6D, 0x7D, 0x07, 0x7F, 0x67};

	unsigned int digits[MAX_DIGITS] = {0, 0, 0, 0, 0, 0};
	int i = 0;
	int first_non_zero_found = 0;

	break_into_digits(digits, freq);
	
	for(i = MAX_DIGITS-1; i >= 0; i--)
	{
		if (digits[i] == 0 && first_non_zero_found == 0)
			write_on_display(i, 0x00);
		else
		{
			first_non_zero_found = 1;
			write_on_display(i, numbers[digits[i]]);
		}
	}
}

void break_into_digits(unsigned int digits[], unsigned int number)
{
	int slot = 0;

	while(number > 0)
	{
		digits[slot] = number % 10;
		number /= 10;
		slot++;
	}
}

//Writes a value on the HEX displays
//param input: int display - display number, counted
//             from right to left on the DE1-SoC board
//param input: content - hex number representing which
//             segments should be turned on on the display
void write_on_display(int display, char content)
{
   volatile unsigned int *HEX_ptr;

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

   *HEX_ptr = (*HEX_ptr & ~(0xFF << (display * 8))) | (content << (display * 8));
}

void driver_main(void)
{
	unsigned int pulses = 0, freq = 0;

	freq = read_frequency();
	glow_ledr(freq - 1);
	set_hex(freq * 10);
	pulses = convert_freq_to_pulses(freq);
	*prl_ptr = signal_state;
	signal_state = !signal_state;

	stop_timer();
    *(timer_ptr + TMR_SVAL_LOW) = pulses & CTR_MASK_LOW;
    *(timer_ptr + TMR_SVAL_HIGH) = (pulses & CTR_MASK_HIGH) >> 16;
	*(timer_ptr + TMR_STATUS) = 0x00;
	start_timer();
}

MODULE_LICENSE("GPL");
module_init (start_driver);
module_exit (stop_driver);
