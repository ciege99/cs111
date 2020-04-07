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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>

char usage[] = "./lab4c_tls : collect and print the current temperature\n"
  "Options: \n"
  "--scale=[F|C]    print the temperature in fahrenheit or celsius\n"
  "--period=NUM     specify that the temp. should be collected every NUM second  s\n"
  "--host=hostname  specify the host name of server\n"
  "--id=IDNUM       specify the id of the user\n"
  "--log=logfile    specify the logfile for the program\n"
  "PORTNUM          specify the port number for the server\n";

int period = 1; //default period of 1 second
int scale = 0; //flag for fahrenehit (0) or celsius (1)

const int B  = 4275;         // B value for thermistor
const int R0 = 100000;          // R0 is 100k ohms
const int PIN1 = 1; //analog input pin for AIN0 -- temp senson
int run_flag = 1; //flag for button
int log_fd; //log file descriptor
int socketfd = -1; //file descriptor for socket
int socket_flag = 0; //flag for socket

int stop = 0; //flag for start/stop command
int started_collection = 0;

// for 4c
int id_flag = 0;
int host_flag = 0;
int port_flag = 0;
int log_flag = 0; //use this as flag for logging
char id[10]; // use this for recoding id
/* PROBABLY NEED TO MAKE HOST NOT FIXED SIZE */
char host[1024]; // use this to record host address
int portno = 0; //use this to record portno 
SSL *ssl; //ssl pointer
int ssl_flag = 0; //ssl flag for cleanup

/* DUMMY 
typedef int mraa_aio_context;
int mraa_aio_init(int thing) {
  return 1;
}
int mraa_aio_read(int sensor) {
  return 1;
}
void mraa_aio_close(int sensor) {}
int log(int thing) {} 
// END OF DUMMY */

mraa_aio_context sensor; //temperature sensor
// mraa_gpio_context button; //button
// int button_set = 0; //flags for sensor and button
int sensor_set = 0;


void return_time(char* str, time_t* time_raw, struct tm* time_info);
void report(char* str, float temp, time_t* time_raw, struct tm* time_info);
void set_flag();
void command_parse(char * str);
void exit_proc(int status);
void setup_tcp();
void setup_tls();
  
int main(int argc, char** argv) {
  static struct option longopts[] = { //get opts and handle
    {"scale", required_argument, 0, 's'},
    {"period", required_argument, 0, 'p'},
    {"log", required_argument, 0, 'l'},
    {"id", required_argument, 0, 'i'},
    {"host", required_argument, 0, 'h'},
    {0, 0, 0, 0}
  };
  int opt = 0;
  while ( (opt =  getopt_long(argc, argv, "s:p:l:i:h:", longopts, NULL)) != -1) {
    unsigned int i;
    switch(opt) {
    case 's':
      if (strlen(optarg) > 1) { //too long of an arg for the option
	fprintf(stderr, "Incorrect argument passed to --scale option\n");
	fprintf(stderr, "%s", usage);
	exit_proc(1);
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
	exit_proc(1);
      }
      break;
    case 'p':
      for (i = 0; i < strlen(optarg); i++) { //make sure we are given numbers
      	if (! isdigit(optarg[i])) {
      	    fprintf(stderr, "Incorrect argument passed to --period option\n");
      	    fprintf(stderr, "%s", usage);
      	    exit_proc(1);
      	  }
      }
      period = atoi(optarg); //assign optarg to period
      if (period < 0) {
	fprintf(stderr, "Incorrect argument passed to --period option\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      break;
    case 'l': //log
      log_fd = open(optarg, O_CREAT|O_APPEND|O_RDWR, 0666); //create logfile
      if (log_fd < 0) {
        // fprintf(stdout, "stdout");
        // fprintf(stdout, "%d", log_fd);
	      fprintf(stderr, "Incorrect argument passed to --log option\n");
	      fprintf(stderr, "%s", usage);
	      exit_proc(1);
	    }
      log_flag = 1;
      break;
    case 'i': //id
      if (strlen(optarg) != 9) {
        fprintf(stderr, "ID must be 9 digits long\n");
        exit_proc(1);
      }
      strcpy(id, optarg); //copy optarg into id
      id_flag = 1;
      break;
    case 'h':
      if (strlen(optarg) > 1023) {
        fprintf(stderr, "host name too long\n");
        exit_proc(1);
      }
      strcpy(host, optarg);
      host_flag = 1;
      break;
    case '?': //bad arg
      fprintf(stderr, "Invalid option\n");
      fprintf(stderr, "%s", usage);
      exit_proc(1);
      break;
    }
  }
  if (optind < argc) //report extra arguments
  {
    int i;
    for (i = optind; i < argc; i++) {
        if (i != optind)
          fprintf(stderr, "invalid argument: %s\n", argv[i]);
        else {//get port number
          portno = atoi(argv[i]);
          port_flag = 1;
        }
    }

    if ((i-optind) > 1) {
      fprintf(stderr, "%s", usage);
      exit_proc(1);
    }
  }

  if (! (id_flag && host_flag && log_flag && port_flag)) {
    fprintf(stderr, "Must pass parameters for ID, host, log, and port number\n");
    fprintf(stderr, "%s", usage);
    exit_proc(1);
  }

  int sensor_val; //return val for sensor function
  float R, temperature; //these are used toc compute temperature
  mraa_aio_context sensor; //temperature sensor
  // mraa_gpio_context button; //button

  sensor = mraa_aio_init(PIN1); //init sensor and set flags
  if (sensor == NULL) {
    fprintf(stderr, "Failed to initialize sensor\n");
    exit_proc(2);
  }
  sensor_set = 1;

  setup_tls();
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
    exit_proc(2);
  }
  R = 1023.0/sensor_val-1.0;
  R = R0*R;
  temperature = 1.0/(log(R/R0)/B+1/298.15)-273.15; // convert to temperature
  time(&time_start);
  
  if (!scale) { //fahrenheit
    temperature = temperature*1.8+32;
  }
  report(output, temperature, &time_start, time_info); //initial report
  started_collection = 1; //mark start of collection
  char perm_buf[1024]; //use this for copying over input to buf
  int j = 0;
  while (run_flag) { //loop while run_flag is 1
    time(&time_now);
    //only run when difference in time is greater than period and stop is 1
    if (difftime(time_now, time_start) >= period && stop == 0) {
      sensor_val = mraa_aio_read(sensor);
      if (sensor_val == -1) {
	fprintf(stderr, "Problem reading value from sensor\n");
	exit_proc(2);
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
      fprintf(stderr, "Polling Error\n");
      exit_proc(2);
    }
    if (fds[0].revents & POLLIN) { //there is input waiting from stdin
      int length = SSL_read(ssl, buf, 1024); //read in from stdin
      if (length < 0) {
	fprintf(stderr, "Error on reading in stdin\n");
	exit_proc(2);
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
	    int x = dprintf(fd, perm_buf, j+1);
	    if (x < 0) {
	      fprintf(stderr, "Error writing to log file\n");
	      exit_proc(2);
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
	exit_proc(2);
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
  char report_msg[100];
  int length = sprintf(report_msg, "%s %0.1f\n", str, temp);
  SSL_write(ssl, report_msg, length); //print to stdout
  if (log_flag)
    dprintf(log_fd, "%s %0.1f\n", str, temp); //print to logfile
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
      exit_proc(2);
    }
  }
  else if (strncmp(str, "LOG ", 4) == 0) {
    return; //nothing to do here, handled in func
  }
  else { //bad command
    fprintf(stderr, "Incorrect command passed to stdin\n");
    exit_proc(2);
  }
}


void exit_proc(int status) {
  time_t time_now;
  struct tm* time_info = NULL;
  time(&time_now); //get time
  char output[9];
  return_time(output, &time_now, time_info);
  if (started_collection) {
    char output_buf[100];
    int length = sprintf(output_buf, "%s SHUTDOWN\n", output);
    SSL_write(ssl, output_buf, length);
  }
  if (log_flag)
    dprintf(log_fd, "%s SHUTDOWN\n", output);
  if (log_flag) //shut down protocol
    close(log_fd);
  if (sensor_set) //close button and sensor
    mraa_aio_close(sensor);
  if (socket_flag)
    close(socketfd);
  if (ssl_flag) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }
  
  exit(status);
}

// based on source https://wiki.openssl.org/index.php/Simple_TLS_Server
void setup_tls() {
//  SSL_METHOD *method;
  SSL_CTX *ctx; 

  if (SSL_library_init() < 0) {
    fprintf(stderr, "Could not initialize SSL library\n");
    exit_proc(2);
  }
  OpenSSL_add_all_algorithms();
  
  const SSL_METHOD *method = TLSv1_client_method();
  ctx = SSL_CTX_new(method);
  if (!ctx) {
    fprintf(stderr, "Could not create TLS server\n");
    exit_proc(2);
  }
  ssl = SSL_new(ctx);
  setup_tcp();

  SSL_set_fd(ssl, socketfd); //setup ssl 
  if (SSL_connect(ssl) != 1) {
    fprintf(stderr, "Could not create SSL connection\n");
    exit_proc(2);
  }
  ssl_flag = 1;

  char id_msg[14];
  sprintf(id_msg, "ID=%s\n", id); //write to buffer
  SSL_write(ssl, id_msg, 13); //write to server
  return;
  // return ctx;
}



void setup_tcp() { //socket set up from source https://aticleworld.com/socket-programming-in-c-using-tcpip/
  int ret;
  socketfd = socket(AF_INET, SOCK_STREAM, 0); //initalize fd;
  if (socketfd == -1) {
    fprintf(stderr, "Could not create socket\n");
    exit_proc(2);
  }
  socket_flag = 1; //set socket flag
  struct hostent *server = gethostbyname(host); //get host
  char* ip = inet_ntoa(* ((struct in_addr*) server->h_addr_list[0])); //convert to ip
  struct sockaddr_in remote = {0};
  remote.sin_addr.s_addr = inet_addr(ip); //set host
  remote.sin_family = AF_INET; //protocol
  remote.sin_port = htons(portno); //port number

  ret = connect(socketfd, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)); //connect to server
  if (ret == -1) { //error connecting to server
    fprintf(stderr, "%d", ret);
    fprintf(stderr, "Could not connect to server\n");
    exit_proc(2);
  }
  if (close(0) < 0) { //close stdin and replace with socketfd 
    fprintf(stderr, "Could not close stdin\n");
    exit_proc(2);
  }
  dup(socketfd); 
  if (close(1) < 0) { //close stdout and replace with socketfd
    fprintf(stderr, "Could not close stdout\n");
    exit_proc(2);
  }
  dup(socketfd);
}
