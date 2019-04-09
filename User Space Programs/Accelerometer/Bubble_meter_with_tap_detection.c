#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

// number of characters to read from /dev/accel
#define accel_BYTES 21

#define RED     31
#define GREEN   32
#define YELLOW  33
#define WHITE   37

#define X_MAX   80
#define Y_MAX   24
#define OPT_EN  2
#define FILTER  1

#define XL345_DOUBLETAP    0x20
#define XL345_SINGLETAP    0x40

#define NO_TAP     0
#define SINGLE_TAP 1
#define DOUBLE_TAP 2

#define WAIT_TIME 		2
#define ANIMATION_SPEED 100000

int read_from_driver_FD(int, char[], int);
void plot_pixel(int, int, char, char);
void draw_axis();
void draw_bubble(int, int, int, int);

volatile sig_atomic_t stop;
void catchSIGINT(int signum){
    stop = 1;
}

int main(int argc, char *argv[])
{
	// Declare other variables
	int accel_FD; // file descriptor
	char accel_buffer[accel_BYTES]; // buffer for accel char data
	int accel_data[5] = {0, 0, 0, 0, 0};
	char* sub_elements = NULL; // sub elements in the driver message (HH, X, Y, Z, mg/lsb) 
	int i = 0;
	float x = 0.0, y = 0.0, z = 1000.0; // Assume the board is on a horizontal surface
	float alpha = 0.0;
    int tap_data = 0;
    time_t current_time = time(NULL), last_tap_time = time(NULL);

	// To make tests easier, the filter alhpa is passed as an argv
	if(argc == OPT_EN)
	{
		alpha = atof(argv[FILTER]);
		if (alpha < 0)
			alpha = 0;
		if (alpha > 1)
			alpha = 1;
	}

	// catch SIGINT from ^C, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);


	// Open the character device driver
	if ((accel_FD = open("/dev/accel", O_RDONLY)) == -1)
	{
		printf("Error opening /dev/accel: %s\n", strerror(errno));
		return -1;
	}

	printf ("\e[2J"); 					// clear the screen
  	printf ("\e[?25l");					// hide the cursor

	// Print values read from the driver
	while(!stop)
	{
		read_from_driver_FD(accel_FD, accel_buffer, accel_BYTES);
		sub_elements = strtok(accel_buffer, " ");
		while(sub_elements && !stop && i<5)
		{
		  if (i == 0)
		    accel_data[i] = strtol(sub_elements, NULL, 16);
		  else
			accel_data[i] = atoi(sub_elements);
			
		  sub_elements = strtok(NULL, " ");
		  i++;
		}
			
		i = 0;

		if (accel_data[0] & XL345_DOUBLETAP){
			tap_data = DOUBLE_TAP;
		    last_tap_time = time(NULL);
		}
		else if (accel_data[0] & XL345_SINGLETAP) {
		    tap_data = SINGLE_TAP;
		    last_tap_time = time(NULL);
		}

		current_time = time(NULL);
		if ((difftime(current_time, last_tap_time) > WAIT_TIME)) {
		  tap_data = NO_TAP;
		}

		x = (x*alpha) + (accel_data[1]*accel_data[4])*(1-alpha);
		y = (y*alpha) + (accel_data[2]*accel_data[4])*(1-alpha);
		z = (z*alpha) + (accel_data[3]*accel_data[4])*(1-alpha);

		printf ("\e[2J"); 					// clear the screen
		draw_axis();
		printf ("\e[%2dm\e[%d;%dHx=%.0f y=%.0f z=%.0f", WHITE, 1, 1, x, y, z);
		draw_bubble((int)x, (int)y, (int) z, tap_data);
		usleep(ANIMATION_SPEED);
	}

	printf ("\e[2J"); 					// clear the screen
	printf ("\e[%2dm", WHITE);			// reset foreground color
	printf ("\e[%d;%dH", 1, 1);		    // move cursor to upper left
	printf ("\e[?25h");					// show the cursor
	fflush (stdout);
	close (accel_FD);
	return 0;
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

void plot_pixel(int x, int y, char color, char c)
{
  	printf ("\e[%2dm\e[%d;%dH%c", color, y, x, c);
  	fflush (stdout);
}

void draw_axis() {
	int i = 0;

	for (i=1; i <= Y_MAX; i++)
		plot_pixel(X_MAX/2, i, WHITE, '|');
	for (i=1; i <= X_MAX; i++)
		plot_pixel(i, Y_MAX/2, WHITE, '-');

	printf ("\e[%2dm\e[%d;%dH%s", WHITE, 1, X_MAX/2+1, "1000");
	printf ("\e[%2dm\e[%d;%dH%s", WHITE, Y_MAX, X_MAX/2+1, "-1000");
	printf ("\e[%2dm\e[%d;%dH%s", WHITE, Y_MAX/2-1, X_MAX-4, "1000");
	printf ("\e[%2dm\e[%d;%dH%s", WHITE, Y_MAX/2-1, 1, "-1000");
	printf ("\e[%2dm\e[%d;%dH%s", WHITE, Y_MAX, X_MAX-12, "Values in mg");

}

void draw_bubble(int x, int y, int z, int tap_data) {
  int o_color = YELLOW;
  if(x > 1000)
		x = 1000;
	if(x < -1000)
		x = -1000;

	if(y > 1000)
		y = 1000;
	if(y < -1000)
		y = -1000;
  if(z < 0)
    o_color = RED;
  else
    o_color = YELLOW;

  switch (tap_data) {
    case 2:
      plot_pixel((X_MAX/2)+((x*80)/2000),
               (Y_MAX/2)-((y*24)/2000), RED, 'X');
    break;
    case 1:
      plot_pixel((X_MAX/2)+((x*80)/2000),
               (Y_MAX/2)-((y*24)/2000), GREEN, '*');
    break;
    case 0:
      plot_pixel((X_MAX/2)+((x*80)/2000),
    				   (Y_MAX/2)-((y*24)/2000), o_color, 'O');
    break;
    default:
      plot_pixel((X_MAX/2)+((x*80)/2000),
               (Y_MAX/2)-((y*24)/2000), o_color, 'O');
    break;
  }
}
