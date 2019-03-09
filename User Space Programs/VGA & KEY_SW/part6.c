#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "color.h"
#define INIT_EL_NUM 8
#define SW_BYTES 4
#define KEY_BYTES 2
#define video_BYTES 8                                       // number of characters to read from /dev/video
#define COMMAND_STR_SIZE 64
#define ELEMENT_SIZE 3

#define ANIM_SPEED_FACT 2

typedef enum direction {INC, DEC} direction;
typedef struct element {
	int x;
	int y;
	direction dirx;
	direction diry;
	int line_color;
} element;

void plot_pixel(int, int, char, char);
void draw_line(int, int, int, int, char, char);
void draw_frame(element*, int, int);
void swap_int(int*, int*);
void init_objects(element*, int);
void init_object(element*);
void move_objects(element*, int, int);
int read_keys(int*, int*, element*, int, int);
int more_elements(element*, int);
int check_display_lines(int);
int read_from_driver_FD(int, char[], int);

int screen_x, screen_y;
int video_FD;                                             // file descriptor
char command[COMMAND_STR_SIZE]; 
volatile sig_atomic_t stop;
void catchSIGINT(int signum){
    stop = 1;
}

int main(void) 
{
	int key_FD;								
	int sw_FD;

	int n_elements = INIT_EL_NUM;

	int stride = 1;
	int slow_down_counter = 0;
	int slow_down_factor  = 0;
	unsigned int frames = 0;
	char frames_str[40] = "";

	// catch SIGINT from ^C, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);

	char video_buffer[video_BYTES];                        // buffer for video char data
	char *argument;

	// Open the character device driver
  	if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
		printf("Error opening /dev/video: %s\n", strerror(errno));
		return -1;
 	 }

	// Read VGA screen size from the video driver
	if(read_from_driver_FD(video_FD, video_buffer, video_BYTES) != 0){
		printf("Error: Cannot read from the driver (/dev/video)\n");
		return 1;
	}

	if ((key_FD = open("/dev/KEY", O_RDONLY)) == -1){
		printf("Error opening /dev/KEY: %s\n", strerror(errno));
		return -1;
	}
	
	if ((sw_FD = open("/dev/SW", O_RDONLY)) == -1){
		printf("Error opening /dev/SW: %s\n", strerror(errno));
		return -1;
	}

	argument = strchr(video_buffer,' ');
	*argument = '\0';
	screen_y = atoi(argument + 1);
	screen_x = atoi(video_buffer);
	
	element* elements = (element*) calloc(n_elements, sizeof(element));
	
	init_objects(elements, n_elements);
	draw_frame(elements, n_elements, check_display_lines(sw_FD));
	
	while(!stop)
	{
		frames++;
		sprintf(frames_str, "text 0,0 Number of frames: %i", frames);
		
		write (video_FD, "sync", COMMAND_STR_SIZE);

		// Strategy to  have a slower animation: repeat  frames
		// for a couple of screen refreshes. This will keep the
		// frame rate at 60, but  give the impression of slower
		// movement.
		if (slow_down_counter >= slow_down_factor)
		{
			slow_down_counter = 0;
			move_objects(elements, n_elements, stride);
		}
		else
			slow_down_counter++;
		write (video_FD, "clear", COMMAND_STR_SIZE); 					// clear the screen
		draw_frame(elements, n_elements, check_display_lines(sw_FD));
		n_elements = read_keys(&stride, &slow_down_factor, elements, n_elements, key_FD);
		write(video_FD, frames_str, COMMAND_STR_SIZE);
	}
	
	free(elements);
	write (video_FD, "clear", COMMAND_STR_SIZE); 					// clear the screen
	write (video_FD, "sync", COMMAND_STR_SIZE);
	write (video_FD, "clear", COMMAND_STR_SIZE); 					// clear the screen
	write (video_FD, "erase", COMMAND_STR_SIZE); 					// clear the screen
	close (key_FD);
	close (sw_FD);
	close (video_FD);

	return 0;
}

int read_keys(int* stride, int* slow_down_factor, element* elements, int n_elements, int key_FD)
{
	int keys = 0;
	char key_buffer[KEY_BYTES+1];

	if(read_from_driver_FD(key_FD, key_buffer, KEY_BYTES) != 0)
		return n_elements;
		
	keys = atoi(key_buffer);
	
	switch(keys)
	{
		case 0:
			// No key press
		break;
		case 1:
			// Faster	
			*slow_down_factor -= 1;
			if (*slow_down_factor < 0)
			{
				*slow_down_factor = 0;
				*stride += ANIM_SPEED_FACT;
			}

			if (*stride > 9)
				*stride = 9;
		break;
		case 2:
			// Slower
			*stride -= ANIM_SPEED_FACT;
			if (*stride <= 0)
			{
				*stride = 1;
				*slow_down_factor += 1;
			}

			if(*slow_down_factor > 5)
				*slow_down_factor = 5;
		break;
		case 4:
			// Increase
			n_elements = more_elements(elements, n_elements);
		break;
		case 8:
			// Decrease
			if (n_elements != 1)
				n_elements--;
		break;
		default:
		printf("Illegal keypress detected. Please press one key at a time.\n"); 
	}

return n_elements;
}

int check_display_lines(int sw_FD)
{
	char sw_buffer[SW_BYTES+1] = "\0";
	int switches = 0;

	if(read_from_driver_FD(sw_FD, sw_buffer, SW_BYTES) == 0)
		switches =	(int) strtol (sw_buffer, NULL, 16);

	if (switches)
		return 0;
	
	return 1;
}

void move_objects(element* elements, int n_elements, int stride)
{
	int i = 0;
	for (i = 0; i < n_elements; i++)
	{
		if (elements[i].dirx == INC && elements[i].x >= (screen_x - ELEMENT_SIZE - 1))
			elements[i].dirx = DEC;
		
		if (elements[i].dirx == DEC && elements[i].x <= 0)
			elements[i].dirx = INC;
		
		if (elements[i].diry == INC && elements[i].y >= (screen_y - ELEMENT_SIZE - 1))
			elements[i].diry = DEC;
		
		if (elements[i].diry == DEC && elements[i].y <= 0)
			elements[i].diry = INC;
		

		elements[i].dirx == INC ? (elements[i].x += stride) :  (elements[i].x -= stride);
		elements[i].diry == INC ? (elements[i].y += stride) :  (elements[i].y -= stride);


		if(elements[i].x >= (screen_x - ELEMENT_SIZE - 1))
			elements[i].x = screen_x - ELEMENT_SIZE - 1;
		
		if(elements[i].x <= 0)
			elements[i].x = 0;

		if(elements[i].y >= (screen_y - ELEMENT_SIZE - 1))
			elements[i].y = screen_y - ELEMENT_SIZE - 1;

		if(elements[i].y <= 0)
			elements[i].y = 0;
	}
}

void draw_frame(element* elements, int n_elements, int draw_lines)
{
	int i = 0;
	for (i = 0; i < n_elements-1 && draw_lines; i++)
	{
		sprintf (command, "line %d,%d %d,%d %X\n", elements[i].x + ELEMENT_SIZE/2,
           		elements[i].y + ELEMENT_SIZE/2, elements[i+1].x + ELEMENT_SIZE/2, 
				elements[i+1].y + ELEMENT_SIZE/2, elements[i].line_color);
  		write (video_FD, command, COMMAND_STR_SIZE);	
	}
	if (draw_lines) {
		sprintf (command, "line %d,%d %d,%d %X\n", elements[n_elements-1].x + ELEMENT_SIZE/2,
           		elements[n_elements-1].y + ELEMENT_SIZE/2, elements[0].x + ELEMENT_SIZE/2, 
				elements[0].y + ELEMENT_SIZE/2, elements[i].line_color);
  		write (video_FD, command, COMMAND_STR_SIZE);
	}		
	
	// Ploting the objects  in a separate loop  because we want them
	// to always be drawn on top of everything else, even when lines
	// and objects cross on top of each other.
	for (i = 0; i < n_elements; i++) {
		sprintf (command, "box %d,%d %d,%d %X\n", elements[i].x,
           		elements[i].y, elements[i].x + ELEMENT_SIZE, elements[i].y + ELEMENT_SIZE, WHITE);
  		write (video_FD, command, COMMAND_STR_SIZE);
	}
}

void init_objects(element* elements, int n_elements)
{
	int i = 0;
	srand(time(NULL));

	
	for (i = 0; i < n_elements; i++)
	{
		init_object(&elements[i]);
	}
}


int more_elements(element* elements, int n_elements)
{
	static int allocated_elements = INIT_EL_NUM;
	
	if (allocated_elements == n_elements)
	{
		allocated_elements = n_elements * 2;
		elements = (element *) realloc(elements, allocated_elements*sizeof(elements[0]));
	}
	
	init_object(&elements[n_elements]);
	return n_elements + 1;
}

void init_object(element* cur_element)
{
	static int spawned_so_far = 0;
	int line_colors[] = {RED, GREEN, BLUE, PINK, YELLOW, LIGHT_BLUE, PURPLE, 
						DISGUSTING_GREEN, BROWN, ORANGE, WEIRDO, GREY};
	
	cur_element->x = rand() % 316 + 2;
	cur_element->y = rand() % 236 + 2;
	cur_element->dirx = (direction) rand() % 2;
	cur_element->diry = (direction) rand() % 2;
	cur_element->line_color = line_colors[spawned_so_far % (sizeof(line_colors)/sizeof(line_colors[0]))];
	spawned_so_far++;
}

int read_from_driver_FD(int driver_FD, char buffer[], int buffer_len)
{
	int bytes_read = 0;
	int bytes = 0;

	while ((bytes = read (driver_FD, buffer, buffer_len)) > 0)
		bytes_read += bytes;	// read the foo device until EOF	
		buffer[bytes_read] = '\0';	

	if (bytes_read != buffer_len) 
	{
		fprintf (stderr, "Error: %d bytes expected from driver "
			     "FD, but %d bytes read\n", buffer_len, bytes_read);
		return 1;
	}

	bytes_read = 0;

	return 0;
}
