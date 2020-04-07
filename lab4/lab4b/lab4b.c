/*
NAME: Collin Prince
UID: 505091865
EMAIL: cprince99@g.ucla.edu
*/

#include <signal.h>
#include <mraa.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

char usage[] = "./lab4b : collect and print the current temperature\n"
  "Options: \n"
  "--scale=[F|C]    print the temperature in fahrenheit or celsius\n"
  "--period=NUM     specify that the temp. should be collected every NUM second  s\n";

int period = 1; //default period of 1 second
int scale = 0; //flag for fahrenehit (0) or celsius (1)

const int B  = 4275;         // B value for thermistor
const int R0 = 100000;          // R0 is 100k ohms
const int PIN1 = 1; //analog input pin for AIN0 -- temp senson
const int PIN2 = 60; //pin for button
int run_flag = 1; //flag for button
int log_fd; //log file descriptor
int log_flag = 0; //use this as flag for logging
int stop = 0; //flag for start/stop command

mraa_aio_context sensor; //temperature sensor
mraa_gpio_context button; //button
int button_set = 0; //flags for sensor and button
int sensor_set = 0;


void return_time(char* str, time_t* time_raw, struct tm* time_info);
void report(char* str, float temp, time_t* time_raw, struct tm* time_info);
void set_flag();
void command_parse(char * str);
void exit_proc(int status);
  
int main(int argc, char** argv) {
  static struct option longopts[] = { //get opts and handle
    {"scale", required_argument, 0, 's'},
    {"period", required_argument, 0, 'p'},
    {"log", required_argument, 0, 'l'},
    {0, 0, 0, 0}
  };
  int opt = 0;
  while ( (opt =  getopt_long(argc, argv, "s:p:l:", longopts, NULL)) != -1) {
    unsigned int i;
    switch(opt) {
    case 's':
      if (strlen(optarg) > 1) { //too long of an arg for the option
	fprintf(stderr, "Incorrect argument passed to --scale option\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      if (optarg[0] == 'C') {
	scale = 1;
      }
      else if (optarg[0] == 'F') {
	scale = 0; 
      }
      else { //incorrect arg for this option
	fprintf(stderr, "Incorrect argument passed to --scale option\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      break;
    case 'p':
      for (i = 0; i < strlen(optarg); i++) { //make sure we are given numbers
      	if (! isdigit(optarg[i])) {
      	    fprintf(stderr, "Incorrect argument passed to --period option\n");
      	    fprintf(stderr, "%s", usage);
      	    exit(1);
      	  }
      }
      period = atoi(optarg); //assign optarg to period
      if (period < 0) {
	fprintf(stderr, "Incorrect argument passed to --period option\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      break;
    case 'l':
      log_fd = open(optarg, O_CREAT|O_APPEND|O_RDWR); //create logfile
      if (log_fd < 0) {
	fprintf(stderr, "Incorrect argument passed to --log option\n");
	fprintf(stderr, "%s", usage);
	exit(1);
	}
      log_flag = 1;
      break;
    case '?': //bad arg
      fprintf(stderr, "Invalid option\n");
      fprintf(stderr, "%s", usage);
      exit(1);
      break;
    }
  }
  if (optind < argc) //report extra arguments
  {
    for (; optind < argc; optind++)
          fprintf(stderr, "invalid argument: %s\n", argv[optind]);
    fprintf(stderr, "%s", usage);
    exit(1);
  }

  int sensor_val; //return val for sensor function
  float R, temperature; //these are used toc compute temperature
  mraa_aio_context sensor; //temperature sensor
  mraa_gpio_context button; //button

  sensor = mraa_aio_init(PIN1); //init sensor and button and set flags
  if (sensor == NULL) {
    fprintf(stderr, "Failed to initialize sensor\n");
    exit_proc(1);
  }
  sensor_set = 1;
  button = mraa_gpio_init(PIN2);
  if (button == NULL) {
    fprintf(stderr, "Failed to initialize button\n");
  }
  button_set = 1;
  mraa_result_t status = mraa_gpio_dir(button, MRAA_GPIO_IN); //set up button as input
  if (status != MRAA_SUCCESS) {
    fprintf(stderr, "Failed to setup direction of button\n");
    exit_proc(1);
  }
  status = mraa_gpio_isr(button, MRAA_GPIO_EDGE_BOTH, set_flag, NULL); //interrupt for button
  if (status != MRAA_SUCCESS) {
    fprintf(stderr, "Failed to setup interrupt for button\n");
    exit_proc(1);
  }

  time_t time_start, time_now; //use these to gather time info
  struct tm* time_info = NULL;
  char output[9]; //string for time output
  char buf[1024] = {0}; //string for reading input, initialize to null
  struct pollfd fds[1];
  fds[0].fd = 0; //read in from stdin
  fds[0].events = POLLIN; //only looking for POLLIN

  //algo from grove temperature sensor
  sensor_val = mraa_aio_read(sensor); //initial read
  if (sensor_val == -1) {
    fprintf(stderr, "Problem reading from sensor\n");
    exit_proc(1);
  }
  R = 1023.0/sensor_val-1.0;
  R = R0*R;
  temperature = 1.0/(log(R/R0)/B+1/298.15)-273.15; // convert to temperature
  time(&time_start);
  
  if (!scale) { //fahrenheit
    temperature = temperature*1.8+32;
  }
  report(output, temperature, &time_start, time_info); //initial report
  
  char perm_buf[1024]; //use this for copying over input to buf
  int j = 0;
  while (run_flag) { //loop while run_flag is 1
    time(&time_now);
    mraa_gpio_read(button);
    //only run when difference in time is greater than period and stop is 1
    if (difftime(time_now, time_start) >= period && stop == 0) {
      sensor_val = mraa_aio_read(sensor);
      if (sensor_val == -1) {
	fprintf(stderr, "Problem reading value from sensor\n");
	exit_proc(1);
      }
      R = 1023.0/sensor_val-1.0;
      R = R0*R;
      temperature = 1.0/(log(R/R0)/B+1/298.15)-273.15; // convert to temperature via datasheet
      if (!scale) { //fahrenheit
    	temperature = temperature*1.8+32;
      }
      report(output, temperature, &time_now, time_info); //report time/temp
      time_start = time_now;
    }
    
    int x = poll(fds, 1, 0); //poll on a loop
    if (x < 0) {
      printf("Polling Error\n");
      exit_proc(1);
    }
    if (fds[0].revents & POLLIN) { //there is input waiting from stdin
      int length = read(0, buf, 1024); //read in from stdin
      if (length < 0) {
	fprintf(stderr, "Error on reading in stdin\n");
	exit_proc(1);
      }
      int i;
      int fd;
      if (log_flag) //write commands to log or stdout
	fd = log_fd;
      else
	fd = 1;
      //start at beginning of buf
      for (i = 0; i < length && j < 1024; i++) {
	if (buf[i] == '\n') {
	  perm_buf[j] = '\n'; 
	  if (log_flag) { //write to logfile if needed
	    int x = write(fd, perm_buf, j+1);
	    if (x < 0) {
	      fprintf(stderr, "Error writing to log file\n");
	      exit_proc(1);
	    }
	  }
	  perm_buf[j] = '\0'; //now make the perm_buf null terminated
	  command_parse(perm_buf);
	  memset(perm_buf, 0, j); //make perm_buf blank
	  j = 0;
	}
	else {
	  perm_buf[j] = buf[i]; //otherwise, just copy over
	  j++; //increment j
	}
      }
      
      if (j == 1024) { //limit on buf reached
	fprintf(stderr, "Command was longer than 1024 bytes and "
		"could not be processed\n");
	exit_proc(1);
      }
    }
  }

  exit_proc(0); //exit successfully
  exit(0); //this is here to stop compiler warnings
}

void return_time(char *str, time_t* time_raw, struct tm* time_info) {
  time_info = localtime(time_raw);
  char* temp = asctime(time_info);
  temp = temp+11; //make the string only be the time
  temp[8] = '\0'; //null terminate at the end of the time
  strcpy(str, temp);
}

void report(char* str, float temp, time_t* time_raw, struct tm* time_info) {
  return_time(str, time_raw, time_info);
  fprintf(stdout, "%s %0.1f\n", str, temp); //print to stdout
  if (log_flag)
    dprintf(log_fd, "%s %0.1f\n", str, temp); //print to logfile
}

void set_flag() { //use this for the button interrupt
  run_flag = 0;
}


void command_parse(char * str) {
  if (strcmp(str, "SCALE=F") == 0) {
    scale = 0; //set scale to F
  }
  else if (strcmp(str, "SCALE=C") == 0) {
    scale = 1; //set scale to C
  }
  else if (strcmp(str, "STOP") == 0) {
    stop = 1; // stop collecting input
  }
  else if (strcmp(str, "START") == 0) {
    stop = 0; //start collecting input
  }
  else if (strcmp(str, "OFF") == 0) {
    exit_proc(0); //exit successfully
  }
  else if (strncmp(str, "PERIOD=", 7) == 0) {
    period = atoi(str+7); //get the string of numbers 
    if (period < 0) {
      fprintf(stderr, "Incorrect arg passed to period command\n");
      exit_proc(1);
    }
  }
  else if (strncmp(str, "LOG ", 4) == 0) {
    return; //nothing to do here, handled in func
  }
  else { //bad command
    fprintf(stderr, "Incorrect command passed to stdin\n");
    exit_proc(1);
  }
}


void exit_proc(int status) {
  time_t time_now;
  struct tm* time_info = NULL;
  time(&time_now); //get time
  char output[9];
  return_time(output, &time_now, time_info);
  fprintf(stdout, "%s SHUTDOWN\n", output); //report shutdown
  if (log_flag)
    dprintf(log_fd, "%s SHUTDOWN\n", output);

  if (log_flag) //shut down protocol
    close(log_fd);
  if (sensor_set) //close button and sensor
    mraa_aio_close(sensor);
  if (button_set)
    mraa_gpio_close(button);
  
  exit(status);
}
