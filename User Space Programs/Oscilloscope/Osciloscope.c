#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include "address_map_arm.h"

#define X_RES               320
#define Y_RES               240 
#define MAX_ADC             4095
#define ADC_DATA_MASK       0xFFF
#define WHITE               0xFFFF
#define COMMAND_STR_SIZE 	40
#define SW_EDGE_BIT_MASK	0x01
#define TRIGGER_THRESHOLD	1000
#define VIDEO_V_OFFSET		44
#define KEY_BYTES			2


// Global variables
timer_t interval_timer_id;

volatile unsigned int* ADC_Base = NULL;
volatile unsigned int* SW_Base = NULL;
volatile void* LW_Bridge = NULL;
volatile sig_atomic_t stop = 0;
int base_sweep_time = 100;
int fd = -1; // used to open /dev/mem for access to physical addresses
int samples[X_RES];
int sampling_complete = 0;
int video_FD = -1;                                             // video file 
int key_FD = -1; 

typedef enum trigger_type
{
  FALLING,
  RISING
} trigger_type;


void clear_screen(void);
void free_resources(void);
void push_to_video(int samples[], int size);
void wait_trigger(trigger_type trigger_at);
int map_virtual(void);
void unmap_virtual(void);
int open_physical (int fd);
void close_physical (int fd);
void* map_physical(int fd, unsigned int base, unsigned int span);
int unmap_physical(void* virtual_base, unsigned int span);
void update_sweep_time(void);
int read_from_driver_FD(int driver_FD, char buffer[], int buffer_len);


struct itimerspec interval_timer_start = 
{
  .it_interval = {.tv_sec=0,.tv_nsec=312500},
  .it_value = {.tv_sec=0,.tv_nsec=312500}
};

struct itimerspec interval_timer_stop = 
{
  .it_interval = {.tv_sec=0,.tv_nsec=0},
  .it_value = {.tv_sec=0,.tv_nsec=0}
};

void catchSIGINT(int signum){
	stop = 1;
}

// Handler function that is called when a timeout occurs
void timeout_handler(int signo)
{
  static int sample_index = 0;

  *ADC_Base = 0x01;	// Set ADC ready to read.
  samples[sample_index++] = *ADC_Base & ADC_DATA_MASK;
  if (sample_index == X_RES)
  {
    sample_index = 0;
    sampling_complete = 1;
  }
}

int main(void){
 
 
  // Set up the signal handling
  struct sigaction act;
  sigset_t set;
  sigemptyset (&set);
  sigaddset (&set, SIGALRM);
  act.sa_flags = 0;
  act.sa_mask = set;
  act.sa_handler = &timeout_handler;
  sigaction (SIGALRM, &act, NULL);
  trigger_type trigger_at = FALLING;

  // Open the character device driver
  if ((video_FD = open("/dev/IntelFPGAUP/video", O_RDWR)) == -1) 
  {
    printf("Error opening /dev/IntelFPGAUP/video: %s\n", strerror(errno));
    free_resources();
    return -1;
  }

  // Open the character device driver
  if ((key_FD = open("/dev/IntelFPGAUP/KEY", O_RDWR)) == -1) 
  {
    printf("Error opening /dev/IntelFPGAUP/KEY: %s\n", strerror(errno));
    free_resources();
    return -1;
  }

  if(map_virtual() != 0)
	{
		printf("Fatal error mapping virtual addresses.\n"
		       "Aborting...\n");
    	free_resources();
		return -1;
	}
  
  clear_screen();

  // Create a monotonically increasing timer
  timer_create (CLOCK_MONOTONIC, NULL, &interval_timer_id);

  // Catch SIGINT from ^C
  signal(SIGINT, catchSIGINT);

  while(!stop)
  {
	trigger_at = ((*SW_Base & SW_EDGE_BIT_MASK) == 0)?FALLING:RISING;
	update_sweep_time();
    wait_trigger(trigger_at);
    
    // Starting the timer
    timer_settime (interval_timer_id, 0, &interval_timer_start, NULL);
    while(!sampling_complete) {/* wait */}
    sampling_complete = 0;
    // Stopping the timer
    timer_settime (interval_timer_id, 0, &interval_timer_stop, NULL);
    push_to_video(samples, X_RES);
  }

  clear_screen();
  free_resources();
  
  return 0;
}

void update_sweep_time(void)
{
  char key_buffer[KEY_BYTES+1];
  int keys = 0;
  // read the key driver
  if(read_from_driver_FD(key_FD, key_buffer, KEY_BYTES) != 0)
	return;

  keys = atoi(key_buffer);
  
  switch(keys)
  {
	case 1:
		base_sweep_time += 100; 
	break;
	case 2:
		base_sweep_time -= 100; 
	break;
	default:
		return;
	break;
  }

  if (base_sweep_time > 500)
  {
	base_sweep_time = 500;
    return;
  }
  if (base_sweep_time < 100)
  {
	base_sweep_time = 100;
    return;
  }

  timer_settime (interval_timer_id, 0, &interval_timer_stop, NULL);
  interval_timer_start.it_interval.tv_nsec= base_sweep_time * 1000000 / 320;
  interval_timer_start.it_value.tv_nsec= base_sweep_time * 1000000 / 320;
  timer_settime (interval_timer_id, 0, &interval_timer_start, NULL);
}

void clear_screen(void)
{
  write (video_FD, "clear", COMMAND_STR_SIZE);
  write (video_FD, "sync", COMMAND_STR_SIZE);
  write (video_FD, "clear", COMMAND_STR_SIZE);
}
void free_resources(void)
{
  if (LW_Bridge != NULL)
    unmap_virtual();

  if (fd != -1)
	  close_physical(fd);
  
  if (video_FD != -1)
    close (video_FD);

  if (key_FD != -1)
    close (key_FD);
}
void push_to_video(int samples[], int size)
{
  char video_cmd_str[COMMAND_STR_SIZE] = "";
  int i = 0;

  write (video_FD, "clear", COMMAND_STR_SIZE);
  for (i = 0; i < size; i++)
  {  
    sprintf(video_cmd_str,"line %i,%i %i,%i %04x"
		, i
		, (samples[i] * Y_RES / MAX_ADC + VIDEO_V_OFFSET)
		, i + 1
		, (samples[i+1] * Y_RES / MAX_ADC + VIDEO_V_OFFSET)
		, WHITE);
    write(video_FD, video_cmd_str, COMMAND_STR_SIZE);
  }
  write (video_FD, "sync", COMMAND_STR_SIZE);
}
void wait_trigger(trigger_type trigger_at)
{
  int previous_sample = (*ADC_Base & ADC_DATA_MASK);
  int current_sample = 0;

  while (!stop)
  {
	*ADC_Base = 0x01;	//Setting ADC ready to read.
    current_sample = (*ADC_Base & ADC_DATA_MASK);

    if (current_sample - previous_sample > TRIGGER_THRESHOLD && trigger_at == FALLING)
      return;
    if (current_sample - previous_sample < -TRIGGER_THRESHOLD && trigger_at == RISING)
      return;
      
    previous_sample = current_sample;
  }
}

 int map_virtual(void)
{ 
   // Create virtual memory access to the FPGA light-weight bridge
   if ((fd = open_physical (fd)) == -1)
      return (-1);
   if ((LW_Bridge = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL)
      return (-1);
  
   ADC_Base = (unsigned int*) (LW_Bridge + ADC_BASE);
   SW_Base = (unsigned int*) (LW_Bridge + SW_BASE);

return 0;
}

void unmap_virtual(void)
{
	unmap_physical ((void*)LW_Bridge, LW_BRIDGE_SPAN);
	close_physical (fd);   // close /dev/mem
}

// Open /dev/mem, if not already done, to give access to physical addresses
int open_physical (int fd)
{
   if (fd == -1)
      if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1)
      {
         printf ("ERROR: could not open \"/dev/mem\"...\n");
         return (-1);
      }
   return fd;
}

// Close /dev/mem to give access to physical addresses
void close_physical (int fd)
{
   close (fd);
}

/*
 * Establish a virtual address mapping for the physical addresses starting at base, and
 * extending by span bytes.
 */
void* map_physical(int fd, unsigned int base, unsigned int span)
{
   void *virtual_base;
   
   // Get a mapping from physical addresses to virtual addresses
   virtual_base = mmap (NULL, span, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, base);
   if (virtual_base == MAP_FAILED)
   {
      printf ("ERROR: mmap() failed...\n");
      close (fd);
      return (NULL);
   }
   return virtual_base;
}

/*
 * Close the previously-opened virtual address mapping
 */
int unmap_physical(void* virtual_base, unsigned int span)
{
   if (munmap (virtual_base, span) != 0)
   {
      printf ("ERROR: munmap() failed...\n");
      return (-1);
   }
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

