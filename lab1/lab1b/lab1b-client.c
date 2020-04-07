// NAME: Collin Prince
// UID: 505091865
// EMAIL: cprince99@g.ucla.edu

#include "zlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <errno.h>
#include <poll.h>
#include <termios.h>
//for zlib
#include <assert.h>
#include <ulimit.h>


#define CHUNK 1000 //bigger chunk used for decompressing/compressing

extern int errno;

//global flags used for different options
int port_flag;
int log_flag;
int compress_flag;
int log_fd; //global file descriptor if we are writing to log
z_stream send_strm, rec_strm; //streams for compression
struct termios og_set, new_set;

int client_creation(int portno); //create socket func
void opt_handler(int argc, char** argv, int* portno); //handle options
void error_exit(); //handle errors
void close_pipe(int fd); //close pipe and check for -1
int handle_stdin(int pipe_out); //handle stdin/stdout
int handle_shell(int pip); //handle output from server

int compress_buf(char* buf, int buf_size, int fd_out); //compression func
int decompress_buf(char* buf, char* buf_out, int buf_size); //decompression func
int compression_start(); //initialize streams for compression/decompression


void set_term(struct termios *orig); //set terminal to new term settings
void reset_term(struct termios *orig); //reset terminal to old term settings

char usage[] = "./lab1b-client --port=port# [--compress] [--log=LOGFILE]\r\n"
  "Creates a client for communication between a client-server socket\r\n"
  "Options:\r\n"
  "--port=port#       mandatory option to specify port # for socket connection\r\n"
  "--compress         option to specify whether socket comm. should be compressed\r\n"
  "--log=LOGFILE      option to specify whether data info should be written to LOGFILE\r\n";



int main(int argc, char** argv) {
  ulimit(UL_SETFSIZE, 10000); //limit output size
  int portno; //ints that will be used for options
  opt_handler(argc, argv, &portno);
  set_term(&og_set);
  int socket = client_creation(portno);

  struct pollfd fds[2];
  fds[0].fd = 0; //fds[0] refers to 0 (stdin)
  fds[1].fd = socket; //fds[1] refers to socket
  fds[0].events = POLLIN | POLLHUP | POLLERR; //set poll events
  fds[1].events = POLLIN | POLLHUP | POLLERR;
  int err_check1, err_check2;
  while (1) {
    poll(fds, 2, 0); //poll continuosuly
    if (fds[0].revents & POLLIN) { //input from client stdin
      err_check1 = handle_stdin(socket); 
      if (err_check1 == 1)
	break;
    }
    if (fds[1].revents & POLLIN) { //output from server
      err_check2 = handle_shell(socket);
      if (err_check2 == 1)
	break;
    }
    if (err_check1 != 1 && ((fds[0].revents & POLLHUP || fds[0].revents & POLLERR) || (fds[1].revents & POLLHUP || fds[1].revents & POLLERR))) //POLLHUP or POLLERR
      break;
  }
  if (log_flag) //close log file descriptor if we were logging
    close_pipe(log_fd);
  close_pipe(socket);

  reset_term(&og_set); //rest terminal to original 
  return 0;

}

int client_creation(int portno) {
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Error on socket creation\r\n");
    reset_term(&og_set);
    exit(1);
  }

  server = gethostbyname("localhost");
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
	(char *)&serv_addr.sin_addr.s_addr,
	server->h_length);
  serv_addr.sin_port = htons(portno);
  if ( (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
    fprintf(stderr, "Issue with connecting in client prog\r\n");
    reset_term(&og_set);
    exit(1);
  }

  return sockfd;
}

void opt_handler(int argc, char**argv, int* portno) {
  log_flag = 0;
  compress_flag = 0;
  port_flag = 0;
  static struct option longopt[] = {
				    {"port", required_argument, 0, 'p'},
				    {"log", required_argument, 0, 'l'},
				    {"compress", no_argument, 0, 'c'},
				    {0, 0, 0, 0}
  };
  int opt;
  while ( (opt =  getopt_long(argc, argv, "cp:l:", longopt, NULL)) != -1) {
    switch (opt) {
    case 'p':
      *portno = atoi(optarg);
      port_flag = 1;
      break;
    case 'l':
      log_flag = 1;
      log_fd = creat(optarg, 0666);
      if (log_fd == -1) {
	fprintf(stderr, "Error: Could not open/create log file\n");
	exit(1);
      }
      break;
    case 'c':
      compress_flag = 1;
      compression_start();
      break;
    case '?':
      fprintf(stderr, "Error: invalid option\r\n");
      exit(1);
      break;
    }
  }
  if (port_flag == 0) {
    fprintf(stderr, "Error: Required port option not used \r\n");
    fprintf(stderr, usage);
    exit(1);
  }
  if (optind < argc) //print out extra args
  {
    for (; optind < argc; optind++)      
      fprintf(stderr, "invalid argument: %s\n", argv[optind]);
    fprintf(stderr, usage);
    exit(1);
  }
  
}

//need to correctly format data and THEN compress
int handle_stdin(int pipe_out) {
  int x, i, j;
  char buf[256];
  char holder[2];
  char un_buf[CHUNK];

  x = read(STDIN_FILENO, buf, 256); //read into buf
  if (x == -1)
    error_exit();
  if (x && (log_flag==1) && (compress_flag == 0)) {
    dprintf(log_fd, "SENT %d bytes: %s\r\n", x, buf); //send to file
  }
  j=0;
  for (i=0; i < x; i++) {
    if (buf[i] == 0x0D || buf[i] == 0x0A) { //cr or lf
      holder[0] = 0x0D;
      holder[1] = 0x0A;
      if (write(STDOUT_FILENO, holder, 2) == -1)
	error_exit(); //echo newline
      if (compress_flag) {
	un_buf[j] = 0x0A;
	j += 1;
      }
      else {
        if (write(pipe_out, holder+1, 1) == -1)
	  error_exit(); //send linefeed
      }
    } else { //not cr or lf, just standard char
      if (write(STDOUT_FILENO, buf+i, 1) == -1) //write to stdout and pipe if normal
	error_exit();
      if (compress_flag) { //write to compress buf if compress opt
	un_buf[j] = buf[i];
	j += 1;
      }
      else {
	if (write(pipe_out, buf+i, 1) == -1)
	  error_exit();
      }
    }
  }

  if (compress_flag == 1) {
    compression_start();
    compress_buf(un_buf, j, pipe_out);
  }
  return 0;
}

int handle_shell(int pip) {
  char* buf;
  char un_buf[CHUNK];
  char read_buf[CHUNK];
  int i;
  int eof_check = 0;
  int cnt = read(pip, read_buf, CHUNK); //read into buf
  if (cnt == -1)
    error_exit();
  if (cnt == 0) //check for EOF
    return 1;
  if (cnt && (log_flag==1)) {
    dprintf(log_fd, "RECEIVED %d bytes: %s\r\n", cnt, read_buf);
  }
  if (compress_flag == 1) {
    compression_start();
    cnt = decompress_buf(read_buf, un_buf, cnt);
    buf = un_buf;
  }
  else
    buf = read_buf;
  
  for (i=0; i < cnt; i++) {
    if (buf[i] == 0x0A) { //lf
      char cr[2];
      cr[0] = 0x0D;
      cr[1] = 0x0A;
      if (write(1, cr, 2) == -1)
	error_exit();
    }
    else { //standard
      if (write(1, buf+i, 1) == -1)
	error_exit();
    }
  }
  if (eof_check == 1)
    return 1;
  return 0;
}



void error_exit() {
  fprintf(stderr, "Error: %s\r\n", strerror(errno));
  reset_term(&og_set);
  exit(1);
}

void close_pipe(int fd) {
  if (close(fd) == -1)
    error_exit();
}


void set_term(struct termios *orig) {
  int err = tcgetattr(0, orig); //get term settings
  if (err == -1) {
    fprintf(stderr, "Error getting terminal settings\n");
    exit(1);
  }
  new_set = *orig; //create new struct, and set term to these specs
  new_set.c_iflag = ISTRIP;
  new_set.c_oflag = 0;
  new_set.c_lflag = 0;
  err = tcsetattr(0, TCSANOW, &new_set);
  if (err == -1)
    error_exit();
}


void reset_term(struct termios *orig) {
  int err = tcsetattr(0, TCSANOW,  orig); //return to old settings, exit
  if (err == -1) {
    fprintf(stderr, "Failed to reset terminal settings to original\r\n");
    exit(1);
  }
  exit(0);
}


int compress_buf(char* buf, int buf_size, int fd_out) {
  int ret;
  unsigned char out[CHUNK];
  send_strm.avail_in = buf_size;
  send_strm.next_in = (Bytef *)buf;
  send_strm.avail_out = CHUNK;
  send_strm.next_out = out;
  do {
    ret = deflate(&send_strm, Z_SYNC_FLUSH);
    if (ret == Z_STREAM_ERROR) {
      fprintf(stderr, "Error with compression: %s, \r\n", send_strm.msg);
      (void)deflateEnd(&send_strm);
      error_exit();
    }
    int have = CHUNK - send_strm.avail_out;
    write(fd_out, out, have);
    if (log_flag == 1) {
      dprintf(log_fd, "SENT %d, bytes: %s\r\n", have, out);
    }
    
  } while (send_strm.avail_out == 0);
  assert(send_strm.avail_in == 0);
  return 0;
}

int compression_start() {
  int ret;
  send_strm.zalloc = Z_NULL;
  send_strm.zfree = Z_NULL;
  send_strm.opaque = Z_NULL;
  rec_strm.zalloc = Z_NULL;
  rec_strm.zfree = Z_NULL;
  rec_strm.opaque = Z_NULL;
  ret = deflateInit(&send_strm, 6);
  if (ret != Z_OK) {
    fprintf(stderr, "Error: Could not init deflate operation\n");
    exit(1);
  }
  ret = inflateInit(&rec_strm);
  if (ret != Z_OK) {
    fprintf(stderr, "Error: Could not init inflate operation\n");
    exit(1);
  }
  return 0;
}

int decompress_buf(char* buf, char* buf_out, int buf_size) {
  int ret;
  rec_strm.avail_in = buf_size;
  rec_strm.next_in = (Bytef *)buf;
  rec_strm.avail_out = CHUNK;
  rec_strm.next_out = (Bytef *) buf_out;
  do {
    ret = inflate(&rec_strm, Z_SYNC_FLUSH);
    if (ret == Z_STREAM_ERROR) {
      fprintf(stderr, "Error with compression: %s \r\n", rec_strm.msg);
      (void)inflateEnd(&rec_strm);
      error_exit();
    }
  } while (rec_strm.avail_out == 0);
  (void)inflateEnd(&rec_strm);
  int have = CHUNK - rec_strm.avail_out;
  return have;
}

