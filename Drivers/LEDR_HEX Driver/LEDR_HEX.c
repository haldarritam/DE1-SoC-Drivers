#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "address_map_arm.h"

#define SUCCESS 0
#define ledr_DEVICE_NAME "LEDR"
#define hex_DEVICE_NAME "HEX"
#define MAX_SIZE 8

/* Kernel character device driver. This driver outputs the ledrs pressed as a HEX value include
 * /dev/ledr and the hexitches state as a HEX value in /dev/hex.
 * This version of the code uses copy_to_user to send data to user-space.
 */

static int ledr_device_open (struct inode *, struct file *);
static int ledr_device_release (struct inode *, struct file *);
static ssize_t ledr_device_write (struct file *, const char *, size_t, loff_t *);

static int hex_device_open (struct inode *, struct file *);
static int hex_device_release (struct inode *, struct file *);
static ssize_t hex_device_write (struct file *, const char *, size_t, loff_t *);

static dev_t ledr_dev_no = 0;
static struct cdev *ledr_cdev = NULL;
static struct class *ledr_class = NULL;
static char ledr_msg[MAX_SIZE];

static dev_t hex_dev_no = 0;
static struct cdev *hex_cdev = NULL;
static struct class *hex_class = NULL;
static char hex_msg[MAX_SIZE];

static struct file_operations ledr_fops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = ledr_device_write,
	.open = ledr_device_open,
	.release = ledr_device_release
};

static struct file_operations hex_fops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = hex_device_write,
	.open = hex_device_open,
	.release = hex_device_release
};

void* LW_virtual;
volatile unsigned int* ledr_ptr;

void write_on_display(int display, int i);
int check_invalid_input (char* input, int len);

static int __init start_driver(void)
{
	int err = 0;
	int d = 0;

	LW_virtual = ioremap_nocache(LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
	ledr_ptr = (unsigned int *) (LW_virtual + LEDR_BASE);
	*ledr_ptr = 0x000;
	
	/* Get a device number. Get one minor number (0) */
	if ((err = alloc_chrdev_region (&ledr_dev_no, 0, 1, ledr_DEVICE_NAME)) < 0) {
		printk (KERN_ERR "ledr: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}
	
	/* Get a device number. Get one minor number (0) */
	if ((err = alloc_chrdev_region (&hex_dev_no, 0, 1, hex_DEVICE_NAME)) < 0) {
		printk (KERN_ERR "hex: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}

	// Allocate and initialize the character device
	ledr_cdev = cdev_alloc (); 
	ledr_cdev->ops = &ledr_fops; 
	ledr_cdev->owner = THIS_MODULE; 
   
	// Add the character device to the kernel
	if ((err = cdev_add (ledr_cdev, ledr_dev_no, 1)) < 0) {
		printk (KERN_ERR "ledr: cdev_add() failed with return value %d\n", err);
		return err;
	}
	
	// Allocate and initialize the character device
	hex_cdev = cdev_alloc (); 
	hex_cdev->ops = &hex_fops; 
	hex_cdev->owner = THIS_MODULE; 
   
	// Add the character device to the kernel
	if ((err = cdev_add (hex_cdev, hex_dev_no, 1)) < 0) {
		printk (KERN_ERR "hex: cdev_add() failed with return value %d\n", err);
		return err;
	}
	
	ledr_class = class_create (THIS_MODULE, ledr_DEVICE_NAME);
	device_create (ledr_class, NULL, ledr_dev_no, NULL, ledr_DEVICE_NAME );
	
	hex_class = class_create (THIS_MODULE, hex_DEVICE_NAME);
	device_create (hex_class, NULL, hex_dev_no, NULL, hex_DEVICE_NAME );
	
	*ledr_ptr = 0x000;
	
	for (d = 0; d < 7; d++)
		write_on_display(d, 10);

	return 0;
}

static void __exit stop_driver(void)
{
	int d = 0;
	*ledr_ptr = 0x000;
	
	for (d = 0; d < 7; d++)
		write_on_display(d, 10);
	
	device_destroy (ledr_class, ledr_dev_no);
	cdev_del (ledr_cdev);
	class_destroy (ledr_class);
	unregister_chrdev_region (ledr_dev_no, 1);
	
	device_destroy (hex_class, hex_dev_no);
	cdev_del (hex_cdev);
	class_destroy (hex_class);
	unregister_chrdev_region (hex_dev_no, 1);
}

/* Called when a process opens ledr */
static int ledr_device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process opens hex */
static int hex_device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process closes ledr */
static int ledr_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Called when a process closes hex */
static int hex_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Called when a process reads from ledr. Provides character data from ledr_msg.
 * Returns, and sets *offset to, the number of bytes read. */
static ssize_t ledr_device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
	size_t bytes = length;
	unsigned long ret = 0;
	unsigned int number = 0;
	char char_overflow = 0;
	static char prev_char_overflow = 0;
	
	if (bytes > MAX_SIZE - 1)
	{
		bytes = MAX_SIZE - 1;
		char_overflow = 1;
	}
	else {char_overflow = 0;}
	
	ret = copy_from_user (ledr_msg, buffer, bytes);
	ledr_msg[bytes] = '\0';
	
	if(!prev_char_overflow && !kstrtouint(ledr_msg, 16, &number))
		*ledr_ptr = number > 0x3FF ? 0x3FF : number;
	
	prev_char_overflow = char_overflow;
	
	return bytes;
}

/* Called when a process reads from hex. Provides character data from hex_msg.
 * Returns, and sets *offset to, the number of bytes read. */
static ssize_t hex_device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
	size_t bytes = length;
	unsigned long ret = 0;
	unsigned int number = 0;
	char char_overflow = 0;
	static char prev_char_overflow = 0;
	unsigned char clear_command = 0;
	
	if (bytes > MAX_SIZE - 1)
	{
		bytes = MAX_SIZE - 1;
		char_overflow = 1;
	}
	else {char_overflow = 0;}
	
	ret = copy_from_user (hex_msg, buffer, bytes);
	hex_msg[bytes] = '\0';
	
	if(!prev_char_overflow)
	{
		int i = 0;
		
		// CLR is used as a code to clear all  the displays
		// We consider the  command with 3 or 4  characters
		// because some implementations like the Linux echo
		// append a \n
		if((strlen(hex_msg) == 3 || strlen(hex_msg) == 4) && 
		    hex_msg[0] == 'C' && hex_msg[1] == 'L' && hex_msg[2] == 'R')
			clear_command = 1;
		
		if(!clear_command && check_invalid_input(hex_msg, strlen(hex_msg)))
			return length;
		
		for (i = 0; i < 7; i++)
			write_on_display(i, 10);
			
		if(clear_command)
			return length;
			
		for (i = 0; i < bytes - 1; i++)
		{
			char individual_number [2];
			individual_number[0] = hex_msg[bytes-2-i];
			individual_number[1] = '\0';
			if(!kstrtouint(individual_number, 10, &number))
				write_on_display(i, number);
		}
	}
	
	prev_char_overflow = char_overflow;
	
	return bytes;
}

int check_invalid_input (char* input, int len)
{
	int i = 0;
	for (i = 0; i < len; i++)
	{
		printk("%c\n", input[i]);
		// (len - 1) && len > 1 : because some implementations
		// like the Linux echo append a \n to the message, but
		// we don't want to accept messages that are only \n
		if (!((input[i] >= '0' && input[i] <= '9') || 
		   (input[i] == '\n' && i == (len - 1) && len > 1)))
			return 1;
	}
	
	return 0;
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