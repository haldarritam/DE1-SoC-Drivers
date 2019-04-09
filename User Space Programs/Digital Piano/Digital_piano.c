#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include "address_map_arm.h"
#include "aux_functions.h"

//#define DYNAMIC_VOLUME

// Global variables
volatile unsigned int* Audio_Base = NULL;
volatile void* LW_Bridge = NULL;
int fd = -1; // used to open /dev/mem for access to physical addresses
int chord_vol_mask[FREQS_IN_MIDDLE_C_SCALE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int note_faders[FREQS_IN_MIDDLE_C_SCALE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int video_buffer[VIDEO_X_RES];
char flag_press_release = 0;
int video_FD;                                             // video file descriptor
int key_FD;                                               // key file descriptor
int ledr_FD;                                              // ledr file descriptor
int hex_FD;                                               // hex file descriptor
int stopwatch_FD;                                         // hex file descriptor

char command[COMMAND_STR_SIZE];
int video_i = 0;										  // Iterator for video_buffer
pthread_mutex_t mutex_chord_vol, mutex_video_buffer;

recording_node* first = NULL;
recording_node* next = NULL;
recording_node* current = NULL;

volatile sig_atomic_t stop = 0;
void catchSIGINT(int signum){
	stop = 1;
}

int main (int argc, char** argv)
{
	int err = 0;
	int kbd_fd = -1;
	int recording = 0;
	int playing = 0;
	struct input_event kbd_event;
	pthread_t tid_audio, tid_video;

	memset(video_buffer,0,(VIDEO_X_RES * sizeof(int)));
	
	// Catch SIGINT from ^C
	signal(SIGINT, catchSIGINT);

	// Open the character device driver
  	if ((video_FD = open("/dev/IntelFPGAUP/video", O_RDWR)) == -1) {
		printf("Error opening /dev/IntelFPGAUP/video: %s\n", strerror(errno));
		return -1;
 	 }
	
	// Open the character device driver
  	if ((key_FD = open("/dev/IntelFPGAUP/KEY", O_RDWR)) == -1) {
		printf("Error opening /dev/IntelFPGAUP/KEY: %s\n", strerror(errno));
		return -1;
 	 }

	// Open the character device driver
  	if ((ledr_FD = open("/dev/IntelFPGAUP/LEDR", O_RDWR)) == -1) {
		printf("Error opening /dev/IntelFPGAUP/LEDR: %s\n", strerror(errno));
		return -1;
 	 }

	// Open the character device driver
  	if ((hex_FD = open("/dev/IntelFPGAUP/HEX", O_RDWR)) == -1) {
		printf("Error opening /dev/IntelFPGAUP/HEX: %s\n", strerror(errno));
		return -1;
 	 }

	// Open the character device driver
	if ((stopwatch_FD = open("/dev/stopwatch", O_RDWR)) == -1){
		printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
		return -1;
	}
										  
	if(map_virtual() != 0)
	{
		printf("Fatal error mapping virtual addresses.\n"
		       "Aborting...\n");
		return -1;
	}
	
	err = parse_cmd_line(argc, argv, chord_vol_mask);
	if (err != SUCCESS)
	{
		print_error(err);
		return err;
	}
	
	kbd_fd = init_keyboard(argv[PARAM_KEYBOARD]);
	if(kbd_fd == ERR_INVALID_KBD)
	{
		print_error(ERR_INVALID_KBD);
		return ERR_INVALID_KBD;
	}
	
	// Spawn the audio thread.
	if ((err = pthread_create(&tid_audio, NULL, &audio_thread, NULL)) != 0)
	{
		printf("pthread_create failed:[%s]\n", strerror(err));
		return err;
	}

	// Spawn the video thread.
	if ((err = pthread_create(&tid_video, NULL, &video_thread, NULL)) != 0)
	{
		printf("pthread_create failed:[%s]\n", strerror(err));
		return err;
	}

	set_processor_affinity(0);
	
	while(!stop)
	{
		int key = 0;
		long total_play_time = 0;
		key = read_key();
		set_rec_play(key, &recording, &playing);
		control_ledr_hex(key, recording, playing);

		if(!playing)
			kbd_event = read_keyboard(kbd_fd);
			
		else
		{
			total_play_time = time_in_hundreths();
			if(total_play_time <= current->ev_time)
			{
				kbd_event = current->ev;
				current = current->next;
				if (current == NULL)
				{
					playing = 0;
					current = first;
					write (stopwatch_FD, STOPWATCH_STOP_MSG , STOPWATCH_STOP_MSG_LEN);
					write (stopwatch_FD, STOPWATCH_NODISP_MSG, STOPWATCH_NODISP_MSG_LEN);
					write (ledr_FD, LEDR_CLEAR_MSG , LEDR_MSG_LEN);
				}
			}

		}

		if (kbd_event.code > 0)
		{
			if(recording && kbd_event.code != 0 &&
				(kbd_event.value == KEY_PRESSED || kbd_event.value == KEY_RELEASED))
				record_note(kbd_event);

			update_notes_volume(kbd_event);
			kbd_event.code=-1;
		}
	}


	// Freeing resources
	pthread_cancel(tid_audio);
	pthread_join(tid_audio, NULL);
	pthread_cancel(tid_video);
	pthread_join(tid_video, NULL);

	write (video_FD, "clear", COMMAND_STR_SIZE); 					// clear the screen
	write (video_FD, "sync", COMMAND_STR_SIZE);
	write (video_FD, "clear", COMMAND_STR_SIZE); 					// clear the screen
	write (video_FD, "sync", COMMAND_STR_SIZE);
	
	write (stopwatch_FD, STOPWATCH_STOP_MSG , STOPWATCH_STOP_MSG_LEN);
	write (stopwatch_FD, STOPWATCH_NODISP_MSG, STOPWATCH_NODISP_MSG_LEN);

	delete_recording();

	close (kbd_fd);
	close (video_FD);
	close (key_FD);
	close (hex_FD);
	close (ledr_FD);
	close (stopwatch_FD);
	unmap_virtual();
	close_physical(fd);
	
	return SUCCESS;
}

void delete_recording(void)
{
	current = first;

	while(current != NULL)
	{
		next = current->next;
		free(current);
		current = next;
	}

	first = NULL;
	current = NULL;
	next = NULL;
}

void record_note(struct input_event kbd_event)
{
	next = (recording_node*) malloc(sizeof(recording_node));
	
	if(first == NULL)
	{
		first = next;
		current = first;
		current->next = NULL;
	}
	else
	{
		current->next = next;
		current = current->next;
		current->next = NULL;
	}

	current->ev = kbd_event;
	current->ev_time = time_in_hundreths();
}

long time_in_hundreths()
{
	char stopwatch_buffer[STOPWATCH_BYTES+1];
	const long char_to_num_offset = 48;
	read_from_driver_FD(stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
	
	return (((stopwatch_buffer[0] - char_to_num_offset) * 10 + (stopwatch_buffer[1] - char_to_num_offset)) * 60 * 100) +
	       (((stopwatch_buffer[3] - char_to_num_offset) * 10 + (stopwatch_buffer[4] - char_to_num_offset)) * 100) +
	       (((stopwatch_buffer[6] - char_to_num_offset) * 10 + (stopwatch_buffer[7] - char_to_num_offset)));
}

void set_rec_play(int key, int* recording, int* playing)
{
	if(*recording && time_in_hundreths() == 0)
	{
		*recording = 0;
		write (ledr_FD, LEDR_CLEAR_MSG , LEDR_MSG_LEN);
		write (stopwatch_FD, STOPWATCH_NODISP_MSG, STOPWATCH_NODISP_MSG_LEN);
		write (stopwatch_FD, STOPWATCH_STOP_MSG, STOPWATCH_STOP_MSG_LEN);
	}

	if(!*playing && !*recording)
	{
		write (ledr_FD, LEDR_CLEAR_MSG , LEDR_MSG_LEN);
		write (stopwatch_FD, STOPWATCH_NODISP_MSG, STOPWATCH_NODISP_MSG_LEN);
		write (stopwatch_FD, STOPWATCH_STOP_MSG, STOPWATCH_STOP_MSG_LEN);
	}

	switch(key)
	{
		case REC_KEY:
			if (*playing)
			{
				printf("ERROR: to record, please stop playing.\n");
				return;
			}

			if(!*recording)
				*recording = 1;
			else
				*recording = 0;
		break;
		case PLAY_KEY:
			if(*recording)
			{
				printf("ERROR: to play, please stop recording.\n");
				return;
			}

			if(!*playing && first != NULL)
				*playing = 1;
			else
				*playing = 0;
		break;
		default:
			// do nothing
		break;
	}
}

void control_ledr_hex(int key, int recording, int playing)
{
	
	char stopwatch_buffer[STOPWATCH_BYTES+1] = STOPWATCH_INIT_TIME_MSG;

	switch(key)
	{
		case REC_KEY:
			if(recording)
			{
				write (ledr_FD, LEDR_REC_MSG , LEDR_MSG_LEN);
				write (stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
				write (stopwatch_FD, STOPWATCH_DISP_MSG, STOPWATCH_DISP_MSG_LEN);
				write (stopwatch_FD, STOPWATCH_RUN_MSG, STOPWATCH_RUN_MSG_LEN);
				delete_recording();
			}
			else if (!playing)
			{
				write (ledr_FD, LEDR_CLEAR_MSG , LEDR_MSG_LEN);
				write (stopwatch_FD, STOPWATCH_NODISP_MSG, STOPWATCH_NODISP_MSG_LEN);
				write (stopwatch_FD, STOPWATCH_STOP_MSG, STOPWATCH_STOP_MSG_LEN);
			}
		break;
		case PLAY_KEY:
			if(!recording)
			{
				current = first;
				write (ledr_FD, LEDR_PLAY_MSG , LEDR_MSG_LEN);
				write (stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
				write (stopwatch_FD, STOPWATCH_DISP_MSG, STOPWATCH_DISP_MSG_LEN);
				write (stopwatch_FD, STOPWATCH_RUN_MSG, STOPWATCH_RUN_MSG_LEN);
			}
		break;
		default:
			// do nothing
		break;
	} 
}

int read_key(void)
{
	int bytes = 0, bytes_read = 0;
	char key_buffer[KEY_BYTES+1];
	
	
	while ((bytes = read (key_FD, key_buffer, KEY_BYTES)) > 0)
		bytes_read += bytes;	// read the foo device until EOF
	if (bytes_read != KEY_BYTES) {
		fprintf (stderr, "Error: %d bytes expected from %s, but %d bytes read\n", KEY_BYTES, 
			"/dev/KEY", bytes_read);
		return 0;
	}
	bytes_read = 0;
	
	return atoi(key_buffer);
}

void* video_thread(void* none)
{
	set_processor_affinity(0);
	char video_cmd_str[COMMAND_STR_SIZE] = "";

	while(1)
	{
		pthread_testcancel();
		int i = 0;
		int sample_i = 0;
		int sample_next = 0;
		
		write (video_FD, "sync", COMMAND_STR_SIZE);
		write (video_FD, "clear", COMMAND_STR_SIZE);
		
		for(i = 0; i < VIDEO_X_RES - 1; i++)
		{
			pthread_mutex_lock(&mutex_video_buffer);
			sample_i = (int)((float)video_buffer[i]/((float)MAX_VOL) * (VIDEO_Y_RES - 1)/2.0 + VIDEO_Y_RES/2.0);
			sample_next = (int)((float)video_buffer[i+1]/((float)MAX_VOL) * (VIDEO_Y_RES - 1)/2.0 + VIDEO_Y_RES/2.0);
			pthread_mutex_unlock(&mutex_video_buffer);
			
			sprintf(video_cmd_str,"line %i,%i %i,%i %04x", i, sample_i, i + 1, sample_next, GREEN);
			write(video_FD, video_cmd_str, COMMAND_STR_SIZE);
		}
		
	}
	
}

void* audio_thread(void* none)
{
	set_processor_affinity(1);

	int sample = 0;
	int f = 0;
	int sample_i[FREQS_IN_MIDDLE_C_SCALE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	float frequencies[FREQS_IN_MIDDLE_C_SCALE] = {261.626, 277.183, 293.665, 
												  311.127, 329.628, 349.228, 
												  369.994, 391.995, 415.305, 
												  440.000, 466.164, 493.883, 523.251};

	while(1)
	{
		pthread_testcancel();
		
		for(f = 0; f < FREQS_IN_MIDDLE_C_SCALE; f++)
		{
			pthread_mutex_lock(&mutex_chord_vol);

			note_release_decay(f);
			sample += get_next_sample_for_frequency(frequencies[f], SAMPLE_RATE, sample_i[f], chord_vol_mask[f]);
			note_decay(f);
		
			
			if(chord_vol_mask[f] != 0)
				sample_i[f]++;
			else
				sample_i[f] = 0;			
			pthread_mutex_unlock(&mutex_chord_vol);
		}
	
		output_sample(sample);
		push_to_video_buffer(sample);
		buffer_overflow_preventor();
		sample = 0;
	}
}

void note_decay(int f)
{
	if (chord_vol_mask[f] > 20000)
		chord_vol_mask[f] -= 20000;
	else if (chord_vol_mask[f] > 0 && chord_vol_mask[f] <= 20000)
		chord_vol_mask[f] -= 1;		
	else
		chord_vol_mask[f] = 0;
}

void push_to_video_buffer(int sample)
{
	pthread_mutex_lock(&mutex_chord_vol);
	int fading_now = find_abs_max(note_faders, FREQS_IN_MIDDLE_C_SCALE) == 0;
	pthread_mutex_unlock(&mutex_chord_vol);


	pthread_mutex_lock(&mutex_video_buffer);
	if(flag_press_release && video_i < VIDEO_X_RES && fading_now)
	{
		video_buffer[video_i] = sample;
		video_i++;
	}
	else if (video_i >= VIDEO_X_RES)
	{
		flag_press_release = 0;
		video_i = 0;
	}
	else if (fading_now)
		video_i = 0;
	pthread_mutex_unlock(&mutex_video_buffer);
}

int find_abs_max(int array[], int size)
{
	int max = 0, i = 0;
	for(i = 0; i < size; i++)
	{
		if(abs(array[i]) > max)
			max = abs(array[i]);
	}
	
	return max;
}

// Implements an assympothic fade to
// prevent changes in the DC level.
void note_release_decay(int note)
{
	if(note_faders[note] > 0)
	{
		if (chord_vol_mask[note] > note_faders[note] && chord_vol_mask[note] < 2*note_faders[note])
		{
			note_faders[note] /= 2;
			chord_vol_mask[note]-=note_faders[note];
		}
		else
			chord_vol_mask[note]-=note_faders[note];

		if(chord_vol_mask[note] <= 0)
		{
			note_faders[note] = 0;
			chord_vol_mask[note] = 0;
		}
	}
}

void bit_mask_volume_mask(int key, int state)
{
	if(state == KEY_PRESSED)
	{
		chord_vol_mask[key] = state;
		note_faders[key] = 0;

		pthread_mutex_lock(&mutex_video_buffer);
		flag_press_release = 1;
		video_i = 0;
		pthread_mutex_unlock(&mutex_video_buffer);
	}
	else
	{
		note_faders[key] = INIT_FADING_INTENSITY;

		pthread_mutex_lock(&mutex_video_buffer);
		flag_press_release = 1;
		pthread_mutex_unlock(&mutex_video_buffer);
	}
}

void process_individual_key(int key_code, int state)
{
	switch(key_code)
	{
		case KEY_2:
			bit_mask_volume_mask(CS_DB, state);
		break;
		case KEY_3:
			bit_mask_volume_mask(DS_EB, state);
		break;
		case KEY_5:
			bit_mask_volume_mask(FS_GB, state);
		break;
		case KEY_6:
			bit_mask_volume_mask(GS_AB, state);
		break;
		case KEY_7:
			bit_mask_volume_mask(AS_BB, state);
		break;
		case KEY_Q:
			bit_mask_volume_mask(C, state);
		break;
		case KEY_W:
			bit_mask_volume_mask(D, state);
		break;
		case KEY_E:
			bit_mask_volume_mask(E, state);
		break;
		case KEY_R:
			bit_mask_volume_mask(F, state);
		break;
		case KEY_T:
			bit_mask_volume_mask(G, state);
		break;
		case KEY_Y:
			bit_mask_volume_mask(A, state);
		break;
		case KEY_U:
			bit_mask_volume_mask(B, state);
		break;
		case KEY_I:
			bit_mask_volume_mask(C2, state);
		break;
	}
}

void update_notes_volume(struct input_event kbd_event)
{
	int i = 0;
#if defined(DYNAMIC_VOLUME)
	int notes_in_chord = 0;
#endif
	
	pthread_mutex_lock(&mutex_chord_vol);
	switch(kbd_event.value)
	{
		case KEY_PRESSED:
			process_individual_key((int)kbd_event.code, KEY_PRESSED);
		break;
		case KEY_RELEASED:
			process_individual_key((int)kbd_event.code, KEY_RELEASED);
		break;
		default:
			pthread_mutex_unlock(&mutex_chord_vol);
			return;
	}
	
	if((int)kbd_event.code == NO_KEY)
	{
		pthread_mutex_unlock(&mutex_chord_vol);
		return;
	}

#if defined(DYNAMIC_VOLUME)
	// 1) Create a binary mask for all the notes, allowing the
	// program to count how many keys were pressed together
	for (i = 0; i < FREQS_IN_MIDDLE_C_SCALE; i++)
	{
		// > 0 because bit masks are later replaced by scaling
		// factors. See step 2) below.
		if(chord_vol_mask[i] > 0)
			notes_in_chord++;
	}
#endif
	
	// 2) Based on the bit mask '1s' count, scale the maximum
	// volume and  set the volume  value in the mask array to 
	// later scale samples
	for (i = 0; i < FREQS_IN_MIDDLE_C_SCALE; i++)
	{
		if((chord_vol_mask[i] > 0) && (note_faders[i] == 0))
			// Dividing MAX_VOL by 2 because our eardrums
			// are bleeding here. Sorry...
#if defined(DYNAMIC_VOLUME)
			chord_vol_mask[i] = (MAX_VOL/2) / notes_in_chord;
#else
			chord_vol_mask[i] = (MAX_VOL/2) / 5; // Limit set to 5(10) because cheap keyboards
                                                 // ghost before that many keys are pressed.
#endif
	}
	pthread_mutex_unlock(&mutex_chord_vol);
	
}

struct input_event read_keyboard(int fd)
{
	struct input_event ev;
	ev.code = -1;
	int event_size = sizeof (struct input_event);
	
	// Read keyboard
	if (read (fd, &ev, event_size) < event_size)
		return ev;
	
	return ev;
}

int init_keyboard(char* dev_path)
{
	int fd = -1;
	
	// Open keyboard device
	if ((fd = open (dev_path, O_RDONLY | O_NONBLOCK)) == -1)
	{
		printf ("Could not open %s\n", dev_path);
		return ERR_INVALID_KBD;
	}
	
	return fd;
}

void print_error(int err)
{
	switch(err)
	{
		case ERR_INVALID_N_PARAMS:
			printf("ERR: Invalid number of input parameters.\n");
		break;
		case ERR_INVALID_KBD:
			printf("ERR: Invalid keyboard dev path.\n");
	}  

	printf("Usage: ./part5 path_to_keyboard_dev.\n");
}

int parse_cmd_line(int argc, char** argv, int chord_vol_mask[])
{
	if (argc != N_EXPECTED_PARAMS)
		return ERR_INVALID_N_PARAMS;
		
	return SUCCESS;
}

int current_WSLC()
{
	return *(Audio_Base + FIFOSPACE) & WSLC_MASK;
}

void buffer_overflow_preventor()
{
	static int buffer_shrinking = 0;
	
	while(current_WSLC() < 64 && buffer_shrinking){/*wait*/}
			
	if(current_WSLC() == 64)
	{
		buffer_shrinking = 0;
	}
	
	if(current_WSLC() == 0)
	{
		buffer_shrinking = 1;
	}
}

void output_sample(int sample)
{
	*(Audio_Base + LDATA) = sample;
	*(Audio_Base + RDATA) = sample;
}

// Generaters the nth sample of a sin wave of a specific frequency (freq)
// where the sampling frequency is fs at a give volume
int get_next_sample_for_frequency(float freq, int fs, int sample, unsigned int volume)
{
	return (int) (sin(2*M_PI*(freq/fs)*sample) * volume);
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

int map_virtual(void)
{ 
   // Create virtual memory access to the FPGA light-weight bridge
   if ((fd = open_physical (fd)) == -1)
      return (-1);
   if ((LW_Bridge = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL)
      return (-1);
  
   Audio_Base = (unsigned int*) (LW_Bridge + AUDIO_BASE);

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

int set_processor_affinity(unsigned int core)
{
	cpu_set_t cpuset;
	pthread_t current_thread = pthread_self();
	
	if(core >= sysconf(_SC_NPROCESSORS_ONLN))
	{
		printf("CPU Core %d does not exist!\n", core);
		return -1;
	}

	// Zero out the cpuset mask
	CPU_ZERO(&cpuset);
	// Set the mask bit for specified core
	CPU_SET(core, &cpuset);

	return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

}
