#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define SW_BYTES 4
#define KEY_BYTES 2
#define HEX_BYTES 6
#define STOPWATCH_BYTES 9

#define HEX_CLEAR_MSG "CLR"
#define HEX_CLEAR_MSG_LEN 4
#define LEDR_CLEAR_MSG "0000000000"
#define LEDR_CLEAR_MSG_LEN 11	
#define STOPWATCH_DISP_MSG "disp"
#define STOPWATCH_DISP_MSG_LEN 5	
#define STOPWATCH_NODISP_MSG "nodisp"
#define STOPWATCH_NODISP_MSG_LEN 7
#define STOPWATCH_INIT_TIME_MSG "01:00:00"
#define STOPWATCH_INIT_TIME_MSG_LEN 9
#define STOPWATCH_RUN_MSG "run"		
#define STOPWATCH_RUN_MSG_LEN 4
#define STOPWATCH_STOP_MSG "stop"
#define STOPWATCH_STOP_MSG_LEN 5	

int read_from_driver_FD(int, char*, int);
void game_loop(int);
int randomize_calculation(int);
int read_answer(void);
int check_game_over(int);
int elapsed_time_hundreth(int, int);
int convert_stopwatch_time_to_hundreths (char *);

volatile sig_atomic_t stop;
void catchSIGINT(int signum){
    stop = 1;
}

int main(int argc, char *argv[]){

	int key_FD;								
	int sw_FD;
	int ledr_FD;
	int stopwatch_FD;
	
  	char key_buffer[KEY_BYTES+1];
	char sw_buffer[SW_BYTES+1];
	char stopwatch_buffer[STOPWATCH_BYTES+1] = STOPWATCH_INIT_TIME_MSG;
	
  	// catch SIGINT from ^C, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);
    	
	if ((key_FD = open("/dev/KEY", O_RDONLY)) == -1){
		printf("Error opening /dev/KEY: %s\n", strerror(errno));
		return -1;
	}
	
	if ((sw_FD = open("/dev/SW", O_RDONLY)) == -1){
		printf("Error opening /dev/SW: %s\n", strerror(errno));
		return -1;
	}
	
	if ((ledr_FD = open("/dev/LEDR", O_WRONLY)) == -1){
		printf("Error opening /dev/LEDR: %s\n", strerror(errno));
		return -1;
	}
	
	if ((stopwatch_FD = open("/dev/stopwatch", O_RDWR)) == -1){
		printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
		return -1;
	}
	
	write (ledr_FD, LEDR_CLEAR_MSG, LEDR_CLEAR_MSG_LEN);
	write (stopwatch_FD, STOPWATCH_DISP_MSG, STOPWATCH_DISP_MSG_LEN);
	write (stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
	write (stopwatch_FD, STOPWATCH_STOP_MSG, STOPWATCH_STOP_MSG_LEN);
	
	printf("Set stopwatch if desired. Press KEY0 to start\n"
		   "Divisions are rounded down.\n");
		
	while (!stop)
	{
		int keys = 0;
		int switches = 0;
		char temp_time_number[3] ="\0";

		if(read_from_driver_FD(key_FD, key_buffer, KEY_BYTES) != 0)
			return 0;
			
		keys = atoi(key_buffer);
		
		switch(keys)
		{
			case 0:
				// No key press
			break;
			case 1:
				write (stopwatch_FD, STOPWATCH_RUN_MSG, STOPWATCH_RUN_MSG_LEN);				
				game_loop(stopwatch_FD);
				write (stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
				write (stopwatch_FD, STOPWATCH_STOP_MSG, STOPWATCH_STOP_MSG_LEN);
				printf("Set stopwatch if desired. Press KEY0 to start\n"
				       "Divisions are rounded down.\n");
				break;
			case 2:
				// Pushing hundreth
				if(read_from_driver_FD(sw_FD, sw_buffer, SW_BYTES) != 0 || 
				   read_from_driver_FD(stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES) != 0)
					return 0;

				switches =	(int) strtol (sw_buffer, NULL, 16);
				if (switches > 99)
					switches = 99;

				sprintf(temp_time_number, "%02i", switches);
				stopwatch_buffer[6] = temp_time_number[0];
				stopwatch_buffer[7] = temp_time_number[1];
				write (stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
				write (ledr_FD, sw_buffer, strlen(sw_buffer));
				break;
			case 4:
				// Pushing seconds
				if(read_from_driver_FD(sw_FD, sw_buffer, SW_BYTES) != 0 ||
				   read_from_driver_FD(stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES) != 0)
					return 0;

				switches =	(int) strtol (sw_buffer, NULL, 16);
				if (switches > 59)
					switches = 59;

				sprintf(temp_time_number, "%02i", switches);
				stopwatch_buffer[3] = temp_time_number[0];
				stopwatch_buffer[4] = temp_time_number[1];
				write (stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
				write (ledr_FD, sw_buffer, strlen(sw_buffer));
			break;
			case 8:
				// Pushing minutes
				if(read_from_driver_FD(sw_FD, sw_buffer, SW_BYTES) != 0 ||
				   read_from_driver_FD(stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES) != 0)
					return 0;

				switches =	(int) strtol (sw_buffer, NULL, 16);
				if (switches > 59)
					switches = 59;

				sprintf(temp_time_number, "%02i", switches);
				stopwatch_buffer[0] = temp_time_number[0];
				stopwatch_buffer[1] = temp_time_number[1];
				write (stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
				write (ledr_FD, sw_buffer, strlen(sw_buffer));
			break;
			default:
			printf("Illegal keypress detected. Please press one key at a time.\n"); 
		}

		usleep (10000);
	}
	
	write (ledr_FD, LEDR_CLEAR_MSG, LEDR_CLEAR_MSG_LEN);
	write (stopwatch_FD, STOPWATCH_NODISP_MSG, STOPWATCH_NODISP_MSG_LEN);
	
	close (key_FD);
	close (sw_FD);
	close (ledr_FD);
	close (stopwatch_FD);
	printf ("\nExiting application.\n");
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

void game_loop(int stopwatch_FD)
{
	int game_over = 0;
	int correct_answers = -1;
	int expected_result = 0;
	int answer = 0;
	int accumulated_time = 0.0;
	int user_timer_setting_hundreths = 0;
	char stopwatch_buffer[STOPWATCH_BYTES+1];
	
	read_from_driver_FD(stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
	user_timer_setting_hundreths = convert_stopwatch_time_to_hundreths(stopwatch_buffer);
	

	while (!game_over)
	{
		write (stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
		correct_answers++;
		expected_result = randomize_calculation(correct_answers);
		answer = read_answer();
		
		while (!(game_over = check_game_over(stopwatch_FD)) && expected_result != answer)
		{
			printf("Try again: ");
			answer = read_answer();
		}
		
		if(!game_over)
			accumulated_time += elapsed_time_hundreth(user_timer_setting_hundreths, stopwatch_FD);
	}
	
	printf("Time expired! You answered %i questions, in an average of %f seconds.\n\n", 
			correct_answers, correct_answers > 0 ? ((float)accumulated_time/(float)correct_answers)/100 : (float)accumulated_time/100);
}

int randomize_calculation(int correct_answers)
{
	char operands[] = {'+', '-', '*', '/'};
	char current_op = '+';
	int operand1 = 0;
	int operand2 = 0;
	int expected_result = 0;
	
	srand(time(NULL));
	
	if(correct_answers >= 5 && correct_answers <= 9)
		current_op = operands[rand()%2];
	if(correct_answers >= 10 && correct_answers <= 14)
		current_op = operands[rand()%3];
	if(correct_answers >= 15)
		current_op = operands[rand()%4];
	
	operand1 = (rand() % 501);
	operand2 = (rand() % 101);
	
	switch (current_op)
	{
		case '+':
			expected_result = operand1 + operand2;
		break;
		
		case '-':
			expected_result = operand1 - operand2;
		break;
		
		case '*':
			expected_result = operand1 * operand2;
		break;
		
		case '/':
			expected_result = operand1 / operand2;
		break;
	}
	
	printf("%i %c %i = ", operand1, current_op, operand2);
	
	return expected_result;
}

int read_answer()
{
	char answer[10];
	scanf("%s", answer);
	
	return atoi(answer);
}

int check_game_over(int stopwatch_FD)
{
	char stopwatch_buffer[STOPWATCH_BYTES+1];
	read_from_driver_FD(stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
	if(!strcmp(stopwatch_buffer, "00:00:00\n"))
		return 1;
	
	return 0;
}

int elapsed_time_hundreth(int stopwatch_max, int stopwatch_FD)
{
	char stopwatch_buffer[STOPWATCH_BYTES+1];
	
	read_from_driver_FD(stopwatch_FD, stopwatch_buffer, STOPWATCH_BYTES);
	
	return stopwatch_max - convert_stopwatch_time_to_hundreths(stopwatch_buffer);
}

int convert_stopwatch_time_to_hundreths (char * stopwatch_buffer)
{
	return ((stopwatch_buffer[0] * 10 + stopwatch_buffer[1]) * 60 * 100) +
	       ((stopwatch_buffer[3] * 10 + stopwatch_buffer[4]) * 100) +
	       ((stopwatch_buffer[6] + stopwatch_buffer[7]));
}