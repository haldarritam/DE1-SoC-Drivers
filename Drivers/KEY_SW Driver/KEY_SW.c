#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "address_map_arm.h"

#define SUCCESS 0
#define KEY_DEVICE_NAME "KEY"
#define SW_DEVICE_NAME "SW"
#define MAX_SIZE 5

/* Kernel character device driver. This driver outputs the keys pressed as a HEX value include
 * /dev/KEY and the switches state as a HEX value in /dev/SW.
 * This version of the code uses copy_to_user to send data to user-space.
 */

static int key_device_open (struct inode *, struct file *);
static int key_device_release (struct inode *, struct file *);
static ssize_t key_device_read (struct file *, char *, size_t, loff_t *);

static int sw_device_open (struct inode *, struct file *);
static int sw_device_release (struct inode *, struct file *);
static ssize_t sw_device_read (struct file *, char *, size_t, loff_t *);

static dev_t key_dev_no = 0;
static struct cdev *key_cdev = NULL;
static struct class *key_class = NULL;
static char key_msg[MAX_SIZE];

static dev_t sw_dev_no = 0;
static struct cdev *sw_cdev = NULL;
static struct class *sw_class = NULL;
static char sw_msg[MAX_SIZE];

static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.read = key_device_read,
	.write = NULL,
	.open = key_device_open,
	.release = key_device_release
};

static struct file_operations sw_fops = {
	.owner = THIS_MODULE,
	.read = sw_device_read,
	.write = NULL,
	.open = sw_device_open,
	.release = sw_device_release
};

void* LW_virtual;
volatile unsigned int* KEY_ptr;
volatile unsigned int* SW_ptr;

static int __init start_driver(void)
{
	int err = 0;

	LW_virtual = ioremap_nocache(LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
	KEY_ptr = (unsigned int *) (LW_virtual + KEY_BASE + 0x0C);
	*KEY_ptr = 0x0F;
	

	SW_ptr = (unsigned int *) (LW_virtual + SW_BASE);
	
	/* Get a device number. Get one minor number (0) */
	if ((err = alloc_chrdev_region (&key_dev_no, 0, 1, KEY_DEVICE_NAME)) < 0) {
		printk (KERN_ERR "key: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}
	
	/* Get a device number. Get one minor number (0) */
	if ((err = alloc_chrdev_region (&sw_dev_no, 0, 1, SW_DEVICE_NAME)) < 0) {
		printk (KERN_ERR "sw: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}

	// Allocate and initialize the character device
	key_cdev = cdev_alloc (); 
	key_cdev->ops = &key_fops; 
	key_cdev->owner = THIS_MODULE; 
   
	// Add the character device to the kernel
	if ((err = cdev_add (key_cdev, key_dev_no, 1)) < 0) {
		printk (KERN_ERR "key: cdev_add() failed with return value %d\n", err);
		return err;
	}
	
	// Allocate and initialize the character device
	sw_cdev = cdev_alloc (); 
	sw_cdev->ops = &sw_fops; 
	sw_cdev->owner = THIS_MODULE; 
   
	// Add the character device to the kernel
	if ((err = cdev_add (sw_cdev, sw_dev_no, 1)) < 0) {
		printk (KERN_ERR "sw: cdev_add() failed with return value %d\n", err);
		return err;
	}
	
	key_class = class_create (THIS_MODULE, KEY_DEVICE_NAME);
	device_create (key_class, NULL, key_dev_no, NULL, KEY_DEVICE_NAME );
	
	sw_class = class_create (THIS_MODULE, SW_DEVICE_NAME);
	device_create (sw_class, NULL, sw_dev_no, NULL, SW_DEVICE_NAME );
	
	sprintf(key_msg, "%01x\n", *KEY_ptr);
	sprintf(sw_msg, "%03x\n", *SW_ptr);

	return 0;
}

static void __exit stop_driver(void)
{
	*KEY_ptr = 0x0F;
	device_destroy (key_class, key_dev_no);
	cdev_del (key_cdev);
	class_destroy (key_class);
	unregister_chrdev_region (key_dev_no, 1);
	
	device_destroy (sw_class, sw_dev_no);
	cdev_del (sw_cdev);
	class_destroy (sw_class);
	unregister_chrdev_region (sw_dev_no, 1);
}

/* Called when a process opens key */
static int key_device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process opens sw */
static int sw_device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process closes key */
static int key_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Called when a process closes sw */
static int sw_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Called when a process reads from key. Provides character data from key_msg.
 * Returns, and sets *offset to, the number of bytes read. */
static ssize_t key_device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	size_t bytes;
	printk("#Reading: %p = %i\n", KEY_ptr, *KEY_ptr);
	sprintf(key_msg, "%01x\n", *KEY_ptr);
	bytes = strlen (key_msg) - (*offset);	// how many bytes not yet sent?
	bytes = bytes > length ? length : bytes;	// too much to send all at once?
	
	if (bytes)
		if (copy_to_user (buffer, &key_msg[*offset], bytes) != 0)
			printk (KERN_ERR "Error: copy_to_user unsuccessful");
	*offset = bytes;	// keep track of number of bytes sent to the user
	*KEY_ptr = 0x0F;
	
	return bytes;
}

/* Called when a process reads from sw. Provides character data from sw_msg.
 * Returns, and sets *offset to, the number of bytes read. */
static ssize_t sw_device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	size_t bytes;
	sprintf(sw_msg, "%03x\n", *SW_ptr);
	bytes = strlen (sw_msg) - (*offset);	// how many bytes not yet sent?
	bytes = bytes > length ? length : bytes;	// too much to send all at once?
	
	if (bytes)
		if (copy_to_user (buffer, &sw_msg[*offset], bytes) != 0)
			printk (KERN_ERR "Error: copy_to_user unsuccessful");
	*offset = bytes;	// keep track of number of bytes sent to the user
	
	return bytes;
}

MODULE_LICENSE("GPL");
module_init (start_driver);
module_exit (stop_driver);