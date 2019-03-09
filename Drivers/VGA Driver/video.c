#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "address_map_arm.h"
#include "pixel_ctrl_map.h"
#include "char_ctrl_map.h"
#include "video.h"

#define DOUBLE_BUFFER 1
#define DEVICE_NAME "video"
#define MAX_SIZE 40+1
#define PIXEL_CMD_PREAMBLE_SIZE 6
#define LINE_CMD_PREAMBLE_SIZE 5
#define BOX_CMD_PREAMBLE_SIZE 4
#define TEXT_CMD_PREAMBLE_SIZE 5
#define SUCCESS 0

// Declare global variables needed to use the pixel buffer
void *LW_virtual; // used to access FPGA light-weight bridge
void *SDRAM_virtual; // used to access the SDRAM
volatile int * pixel_ctrl_ptr; // virtual address of pixel buffer controller
volatile int * char_ctrl_ptr; // virtual address of the character buffer controller
int pixel_buffer; // used for virtual address of pixel buffer
int char_buffer; // used for virtual address of character buffer
int pixel_back_buffer; // used for virtual address of pixel back buffer
int write_buffer; // used for virtual address of write buffer
int resolution_x, resolution_y; // VGA screen size
int c_resolution_x, c_resolution_y;

// Declare variables needed for a character device driver

static dev_t dev_no = 0;
static struct cdev *cdev = NULL;
static struct class *class = NULL;
static char msg[MAX_SIZE];

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

/* Code to initialize the video driver */
static int __init start_video(void)
{
	int err = 0;
	int resolution = 0;
	int c_resolution = 0;
	// initialize the dev_t, cdev, and class data structures
	/* Get a device number. Get one minor number (0) */
	if ((err = alloc_chrdev_region (&dev_no, 0, 1, DEVICE_NAME)) < 0) {
		printk (KERN_ERR "video: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}

	// Allocate and initialize the character device
	cdev = cdev_alloc (); 
	cdev->ops = &fops; 
	cdev->owner = THIS_MODULE; 
   
	// Add the character device to the kernel
	if ((err = cdev_add (cdev, dev_no, 1)) < 0) {
		printk (KERN_ERR "video: cdev_add() failed with return value %d\n", err);
		return err;
	}
	
	class = class_create (THIS_MODULE, DEVICE_NAME);
	device_create (class, NULL, dev_no, NULL, DEVICE_NAME );

// generate a virtual address for the FPGA lightweight bridge
        LW_virtual = ioremap_nocache (0xFF200000, 0x00005000);
        if (LW_virtual == 0)
                printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");

// Create virtual memory access to the pixel buffer controller
        pixel_ctrl_ptr = (unsigned int *) (LW_virtual + 0x00003020);
        get_screen_specs (pixel_ctrl_ptr); // determine X, Y screen size

// Create virtual memory access to the character buffer controller
		char_ctrl_ptr = (unsigned int*) (LW_virtual + CHAR_BUF_CTRL_BASE);

// Create virtual memory access to the pixel buffer
        pixel_buffer = (int) ioremap_nocache (0xC8000000, 0x0003FFFF);
        if (pixel_buffer == 0)
                printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");

// Create virtual memory access to the character buffer
        char_buffer = (int) ioremap_nocache (0xC9000000, 0x00002FFF);
        if (char_buffer == 0)
                printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");

#ifndef DOUBLE_BUFFER
		*(pixel_ctrl_ptr + BACK_BUFFER_REGISTER) = *(pixel_ctrl_ptr + BUFFER_REGISTER);
#else
		pixel_back_buffer = (int) ioremap_nocache( SDRAM_BASE, FPGA_ONCHIP_SPAN );
		if (pixel_back_buffer == 0)
		{
                printk (KERN_ERR "SDRAM Error: ioremap_nocache returned NULL\n");
				//*(pixel_ctrl_ptr + BACK_BUFFER_REGISTER) = *(pixel_ctrl_ptr + BUFFER_REGISTER);
		}
	
		*(pixel_ctrl_ptr + BACK_BUFFER_REGISTER) = SDRAM_BASE;
#endif
	
		write_buffer = pixel_buffer;
		
		resolution  = *(pixel_ctrl_ptr + RESOLUTION_REGISTER);
		c_resolution = *(char_ctrl_ptr + RESOLUTION_REGISTER);
		resolution_x = resolution & 0xFFFF;
		resolution_y = (resolution >> 16) & 0xFFFF;
		c_resolution_x = c_resolution & 0xFFFF;
		c_resolution_y = (c_resolution >> 16) & 0xFFFF;

/* Erase the pixel buffer */
        clear_screen ( );
        return 0;
}

void get_screen_specs(volatile int * pixel_ctrl_ptr)
{
	sprintf(msg, "%i %i\n", resolution_x, resolution_y);
}
void clear_screen(void)
{
	int x = 0;
	int y = 0;

	for (x = 0; x < resolution_x; x++)
		for (y = 0; y < resolution_y; y++)
			plot_pixel(x, y, 0);
}
void plot_pixel(int x, int y, short int color)
{
	short int* pixel = (short int*) (write_buffer + ((x & 0x1FF) << 1) + ((y & 0xFF) << 10));
	*pixel = color;
}

void draw_line(int x0, int y0, int x1, int y1, short int color)
{
	int deltax = 0;
	int deltay = 0;
	int error = 0;
	int x = 0;
	int y = 0;
	int y_step = 0;
	int is_steep = (abs(y1-y0) > abs(x1-x0));
	
	if (is_steep)
	{
		swap_int(&x0, &y0);
		swap_int(&x1, &y1);
	}
	if (x0 > x1)
	{
		swap_int(&x0, &x1);
		swap_int(&y0, &y1);
	}
	
	deltax = x1 - x0;
	deltay = abs(y1 - y0);
	error = -(deltax / 2);
	y = y0;
	
	if (y0 < y1)
		y_step = 1;
	else
		y_step = -1;
	
	for (x = x0; x <= x1; x++)
	{
		if (is_steep)
			plot_pixel(y, x, color);
		else
			plot_pixel(x, y, color);
		
		error = error + deltay;
		
		if (error > 0)
		{
			y = y + y_step;
			error = error - deltax;
		}
	}
		
}

void draw_box(int x0, int y0, int x1, int y1, short int color)
{
	int i = 0;

	if (x0 > resolution_x)
		x0 = resolution_x;
	if (x1 > resolution_x)
		x1 = resolution_x;
	if (y0 > resolution_y)
		y0 = resolution_y;
	if (y1 > resolution_y)
		y1 = resolution_y;

	if (y0 > y1)
		swap_int(&y1, &y0);

	for(i=y0; i<=y1; i++){
		draw_line(x0, i, x1, i, color);
	}
}

void sync_loop(void)
{
	*(pixel_ctrl_ptr + BUFFER_REGISTER) = 0x1;
	
	while(*(pixel_ctrl_ptr + STATUS_REGISTER) & 1)
	{
		// Block client
	}

	if(*(pixel_ctrl_ptr + BUFFER_REGISTER) == SDRAM_BASE)
		write_buffer = pixel_buffer;
	else
		write_buffer = pixel_back_buffer;
	
	return;
}

static void __exit stop_video(void)
{
/* unmap the physical-to-virtual mappings */
    iounmap (LW_virtual);
    iounmap ((void *) pixel_buffer);
	iounmap ((void *) pixel_back_buffer);

/* Remove the device from the kernel */
	device_destroy (class, dev_no);
	cdev_del (cdev);
	class_destroy (class);
	unregister_chrdev_region (dev_no, 1);
}
static int device_open(struct inode *inode, struct file *file)
{
     return SUCCESS;
}
static int device_release(struct inode *inode, struct file *file)
{
     return 0;
}
static ssize_t device_read(struct file *filp, char *buffer,
                           size_t length, loff_t *offset)
{
	size_t bytes;
	get_screen_specs(pixel_ctrl_ptr);
	bytes = strlen (msg) - (*offset);	// how many bytes not yet sent?
	bytes = bytes > length ? length : bytes;	// too much to send all at once?
	
	if (bytes)
		if (copy_to_user (buffer, &msg[*offset], bytes) != 0)
			printk (KERN_ERR "Error: copy_to_user unsuccessful");
	*offset = bytes;	// keep track of number of bytes sent to the user
	
	return bytes;
}

static ssize_t device_write(struct file *filp, const char
                            *buffer, size_t length, loff_t *offset)
{
	size_t bytes = length;
	unsigned long ret = 0;
	
	if (bytes > MAX_SIZE - 1)
		bytes = MAX_SIZE - 1;
	
	ret = copy_from_user (msg, buffer, bytes);
	msg[bytes] = '\0';
	if (msg[bytes-1] == '\n')
		msg[bytes-1] = '\0';
	
	if (!strcmp(msg, "clear"))
		clear_screen();
	else if (!strncmp(msg, "pixel ", PIXEL_CMD_PREAMBLE_SIZE))
	{
		pixel_data pixel = parse_pixel_command(msg);
		plot_pixel(pixel.x, pixel.y, pixel.color);
	}
	else if (!strncmp(msg, "line ", LINE_CMD_PREAMBLE_SIZE))
	{
		line_box_data line = parse_line_box_command(msg, 0);
		draw_line(line.x0, line.y0, line.x1, line.y1, line.color);
	}
	else if (!strncmp(msg, "box ", BOX_CMD_PREAMBLE_SIZE))
	{
		line_box_data box = parse_line_box_command(msg, 1);
		draw_box(box.x0, box.y0, box.x1, box.y1, box.color);
	}
	else if (!strcmp(msg, "sync"))
	{
		sync_loop();
	}
	else if (!strcmp(msg, "erase"))
	{
		erase_characters();
	}
	else if (!strncmp(msg, "text ", TEXT_CMD_PREAMBLE_SIZE))
	{
		text_data text = parse_text_command(msg);
		write_text(text.x, text.y, text.string);
	}


	return bytes;
}

pixel_data parse_pixel_command(char* command)
{
	// When this function is called, we already know the command starts with "pixel "

	pixel_data pixel = {0, 0, 0};
	int err = 0;
	char* arguments = command + PIXEL_CMD_PREAMBLE_SIZE;
	char* comma_pos = strchr(arguments, ',');
	char* space_pos = strchr(arguments, ' ');

	// Input is not correctly formatted
	if (comma_pos == NULL || space_pos == NULL)
		return pixel;

	// By moving the \0 to the position of the space and the comma
	// in the argument string we create substrings that are parsed
	// by the kstrtoint function. If the function fails, it means
	// there is a syntatic error in the command, and then a default
	// pixel is returned
	err = kstrtoint(space_pos+1, 16, &pixel.color);
	*space_pos = '\0';
	err |= kstrtoint(comma_pos+1, 10, &pixel.y);
	*comma_pos = '\0';
	err |= kstrtoint(arguments, 10, &pixel.x);

	// If the color input is not a valid hex number, the  pixel 0,0
	// is set to black. This may be  inconvenient for the user, but
	// it is a way of communicating to the client that something is
	// wrong in the application layer.
	if (err)
	{
		pixel.x = 0;
		pixel.y = 0;
		pixel.color = 0;
	}

	return pixel;
}

text_data parse_text_command(char* command)
{
	// When this function is called, we already know the command starts with "text "

	text_data text = {0, 0, " "};
	int err = 0;
	char* arguments = command + TEXT_CMD_PREAMBLE_SIZE;
	char* comma_pos = strchr(arguments, ',');
	char* space_pos = strchr(arguments, ' ');

	// Input is not correctly formatted
	if (comma_pos == NULL || space_pos == NULL)
		return text;

	// By moving the \0 to the position of the space and the comma
	// in the argument string we create substrings that are parsed
	// by the kstrtoint function. If the function fails, it means
	// there is a syntatic error in the command, and then a default
	// text is returned
	text.string = space_pos + 1;
	*space_pos = '\0';
	err |= kstrtoint(comma_pos+1, 10, &text.y);
	*comma_pos = '\0';
	err |= kstrtoint(arguments, 10, &text.x);


	// Forcing string limitation based on the resolution.
	if (text.x >= c_resolution_x)
		text.x = c_resolution_x - 1;
	if (text.y >= c_resolution_y)
		text.y = c_resolution_y - 1;

	// If x and y inputs are not valid numbers, the  position 0,0
	// is set to " ". This may be  inconvenient for the user, but
	// it is a way of communicating to the client that something is
	// wrong in the application layer.
	if (err)
	{
		text.x = 0;
		text.y = 0;
		text.string = " ";
	}

	return text;
}

line_box_data parse_line_box_command(char* command, char isBox)
{
	line_box_data line = {0, 0, 0, 0, 0};
	int err = 0;
	char* arguments = command + (isBox ? BOX_CMD_PREAMBLE_SIZE : LINE_CMD_PREAMBLE_SIZE);
	char* comma = NULL;

	char* point_space = strchr(arguments, ' ');
	*point_space = '\0';
	comma = strchr(arguments, ',');
	*comma = '\0';
	err = kstrtoint(arguments, 10, &line.x0);
	err |= kstrtoint(comma+1, 10, &line.y0);

	arguments = point_space+1;

	point_space = strchr(arguments, ' ');
	*point_space = '\0';
	comma = strchr(arguments, ',');
	*comma = '\0';
	err |= kstrtoint(arguments, 10, &line.x1);
	err |= kstrtoint(comma+1, 10, &line.y1);

	arguments = point_space+1;
	
	err |= kstrtoint(arguments, 16, &line.color);
	
	return line;
}

void put_char(int x, int y, char character_in)
{
	short int* character = (short int*) (char_buffer + (x & 0x7F) + ((y & 0x3F) << 7));

	*character = character_in;
}

void erase_characters(void)
{
	int x = 0;
	int y = 0;

	for (x = 0; x < c_resolution_x; x++)
		for (y = 0; y < c_resolution_y; y++)
			put_char(x, y, ' ');	
}

void write_text(int x, int y, char* string)
{
	int written_chars = 0;

	while(written_chars != strlen(string))
	{
		put_char(x,y,string[written_chars]);
		x++;

		if (x > c_resolution_x - 1)
		{
			x=0;
			y++;
		}

		if(y > c_resolution_y - 1)
			y=0;

		written_chars++;
	}
}

void swap_int(int* a, int* b)
{
	*a = *a + *b;
	*b = *a - *b;
	*a = *a - *b;
}

MODULE_LICENSE("GPL");
module_init (start_video);
module_exit (stop_video);
