#ifndef _VIDEO_
#define _VIDEO_

typedef struct pixel_data
{
	int x, y, color;
} pixel_data;

typedef struct line_box_data
{
	int x0, y0, x1, y1, color;
} line_box_data;

typedef struct text_data
{
	int x, y;
	char *string;
} text_data;

void get_screen_specs(volatile int*);
void clear_screen(void);
void plot_pixel(int, int, short int);
void draw_line(int, int, int, int, short int);
void draw_box(int, int, int, int, short int);
void sync_loop(void);
void put_char(int, int, char);
void erase_characters(void);
void write_text(int, int, char*);
pixel_data parse_pixel_command(char*);
line_box_data parse_line_box_command(char*, char);
text_data parse_text_command(char*);

void swap_int(int*, int*);

static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);

#endif
