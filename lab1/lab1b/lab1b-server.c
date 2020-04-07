// NAME: Collin Prince
// UID: 505091865
// EMAIL: cprince99@g.ucla.edu

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <poll.h>
#include <sys/wait.h>
//for zlib
#include <assert.h>
#include "zlib.h"

#define CHUNK 1000

extern int errno;

int compress_flag;
int port_flag;


z_stream send_strm, rec_strm;

int server_creation(int portno);
void opt_handler(int argc, char** argv, int* portno);
void handle_server_shell(int socket);

int handle_stdin(int pipe_out, int child);
int handle_shell(int pip);

int format_output(char* buf, int size, int pipe_out, int child);
int decompress_buf(char* buf, char* buf_out, int buf_size);
int compress_buf(char* buf, int buf_size, int fd_out);
int compression_start();

void sigpipe_handler();
void error_exit();
void close_pipe(int fd);


char usage[] = "./lab1b-server --port=port# [--compress]\r\n"
  "Create a server for communication between a client-server socket\r\n"
  "Options:\r\n"
  "--port=port#       mandatory option to specify port # for socket connection\r\n"
  "--compress         option to specify whether socket comm. should be compresed\r\n";

int main(int argc, char**argv) {
  int portno;
  opt_handler(argc, argv, &portno);
  int socket = server_creation(portno);
  handle_server_shell(socket);
  return 0;
}



int server_creation(int portno) {
  struct sockaddr_in serv_addr, clie_addr;
  int sockfd, new_sockfd;
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    fprintf(stderr, "error");
    exit(1);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Binding error in server program");
    exit(1);
  }

  listen(sockfd, 5);
  socklen_t clilen = sizeof(clie_addr);
  if ( (new_sockfd = accept(sockfd, (struct sockaddr *) &clie_addr, &clilen)) < 0) {
    fprintf(stderr, "Accepting error on server");
    exit(1);
  }
  return new_sockfd;
}


void handle_server_shell(int socket) {

  int t2s[2], s2t[2]; //t2s = term-to-shell, s2t = shell-to-term
  pid_t c_pid;
  signal(SIGPIPE, sigpipe_handler); //signal handler for SIGPIPE
  if (pipe(t2s) == -1)
    error_exit();
  if (pipe(s2t) == -1)
    error_exit();
  
  if ((c_pid = fork()) == -1) //fork processes
    error_exit();
  
  if (c_pid == 0) {
    //read from st2[0], write to t2s[1]
    close_pipe(t2s[1]);
    close_pipe(s2t[0]);
    close_pipe(0);
    //make stdin come in from the pipe, dup t2s to stdin */
    if (dup(t2s[0]) == -1)
      error_exit(); 
    close_pipe(t2s[0]); //close old fd since dup'd
    close_pipe(1); //close stdout, stderr
    close_pipe(2);
    if (dup(s2t[1]) == -1)
      error_exit(); //outward pipe is now dup'd with stdout and stderr
    if (dup(s2t[1]) == -1)
      error_exit();
    close_pipe(s2t[1]); //close original fd
    if (execl("/bin/bash", "/bin/bash", NULL) == -1)
      error_exit();
  } else {
    //write to t2s[1], read from s2t[0], 
    close_pipe(t2s[0]);
    close_pipe(s2t[1]);
    struct pollfd fds[2];
    close_pipe(0);
    dup(socket); //make stdin come from socket
    close_pipe(1);
    dup(socket); //make stdout and stderr go to client
    close(socket); //close original fd since it has been dup'd
    fds[0].fd = 0; // 0 is standin for socket
    fds[1].fd = s2t[0]; //fds[1] refers to s2t[0]
    fds[0].events = POLLIN | POLLHUP | POLLERR; //set poll events
    fds[1].events = POLLIN | POLLHUP | POLLERR; 
    int err_check1, err_check2;
    while (1)
    {
      poll(fds, 2, 0); //poll continuosuly
      if (fds[0].revents & POLLIN) { //input from term
	err_check1 = handle_stdin(t2s[1], c_pid); 
	if (err_check1 == 1)
	  break;
      }
      if (fds[1].revents & POLLIN) { //output from shell
	err_check2 = handle_shell(s2t[0]);
	if (err_check2 == 1) 
	  break;
      }
      if ((fds[0].revents & POLLHUP || fds[0].revents & POLLERR) || (fds[1].revents & POLLHUP || fds[1].revents & POLLERR)) //POLLHUP or POLLERR 
	break; // I took out the err_check1 != 1 in this conditional
    }
    if (err_check1 != 1) {      //if not already closed
      if (close(t2s[1]) == -1)
	error_exit();
    }
    close_pipe(s2t[0]); //close other pipe
    int status;
    waitpid(c_pid, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n", (status & 0x007f), (status & 0xff00)>>8);

  }
}



void opt_handler(int argc, char**argv, int* portno) {
  compress_flag = 0;
  port_flag = 0;
  static struct option longopt[] = {
				    {"port", required_argument, 0, 'p'},
				    {"compress", no_argument, 0, 'c'},
				    {0, 0, 0, 0}
  };
  int opt;
  while ( (opt =  getopt_long(argc, argv, "cp:", longopt, NULL)) != -1) {
    switch (opt) {
    case 'p':
      *portno = atoi(optarg);
      port_flag = 1;
      break;
    case 'c':
      compress_flag = 1;
      break;
    case '?':
      fprintf(stderr, "Error: invalid option\r\n");
      exit(1);
      break;
    }
  }
  if (port_flag == 0) {
    fprintf(stderr, "Required port option not used\r\n");
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


int handle_stdin(int pipe_out, int child) {
  int x, i;
  char read_buf[CHUNK];
  char un_buf[CHUNK];
  char* buf;
  char holder[2];
  int check = 0;
  x = read(STDIN_FILENO, read_buf, CHUNK); //read into buf
  if (x == -1)
    error_exit();
  if (compress_flag == 1) {
    compression_start();
    x = decompress_buf(read_buf, un_buf, x);
    buf = un_buf;
  }
  else
    buf = read_buf;
  
  for (i=0; i < x; i++) {
    if (buf[i] ==  0x04) { //EOF
      if (close(pipe_out) == -1)
	error_exit();
      check = 1;
      holder[0] = '^';
      holder[1] = 'D';
      if (compress_flag == 1) {
	compression_start();
	compress_buf(holder, 2, 1);
      }
      else {
	if (write(1, holder, 2) == -1) //write message to socket
	  error_exit();
      }
      break;
    }
    else if (buf[i] == 0x03) { //SIGINT
      check = 2;
      holder[0] = '^';
      holder[1] = 'C';
      if (compress_flag == 1) {
	compression_start();
	compress_buf(holder, 2, 2);
      } else {
	if (write(1, holder, 2) == -1) //write message to socket
	  error_exit();
      }
      kill(child, SIGINT); //kill child process on ^C
      break;
    }
    else if (buf[i] == 0x0D || buf[i] == 0x0A) { //cr or lf
      holder[0] = 0x0D;
      holder[1] = 0x0A;
      if (write(pipe_out, holder+1, 1) == -1)
	error_exit(); //send linefeed
    } else {
      if (write(pipe_out, buf+i, 1) == -1)
	error_exit();
    }
  }
  if (check) 
    return check;
  return 0;
}



int handle_shell(int pip) {
  char buf[256];
  char un_buf[CHUNK];
  int i;
  int eof_check = 0;
  int cnt = read(pip, buf, 256); //read into buf
  if (cnt == -1)
    error_exit();
  if (cnt == 0) //check for EOF
    return 1;

  int j = 0;
  for (i=0; i < cnt; i++) {
    if (buf[i] == 0x0A) { //lf
      char cr[2];
      cr[0] = 0x0D;
      cr[1] = 0x0A;
      if (compress_flag == 1) {
	un_buf[j] = 0x0D;
	un_buf[j+1] = 0x0A;
	j += 2;
      } else {
	if (write(1, cr, 2) == -1)
	  error_exit();
      }
    }
    else { //standard
      if (compress_flag == 1) {
	un_buf[j] = buf[i];
	j += 1;
      } else {
	if (write(1, buf+i, 1) == -1)
	  error_exit();
      }
    }
  }
  if (eof_check == 1)
    return 1;
  
  if (compress_flag) {
    compression_start();
    compress_buf(un_buf, j, 1);
  }
  return 0;
}


void sigpipe_handler() { //exit normally for sigpipe
  exit(0);
}


void error_exit() {
  fprintf(stderr, "In Server\r\n");
  fprintf(stderr, "Error: %s", strerror(errno));
  exit(1);
}

void close_pipe(int fd) {
  if (close(fd) == -1)
    error_exit();
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
  if (ret != Z_OK)
    error_exit();
  ret = inflateInit(&rec_strm);
  if (ret != Z_OK)
    error_exit();
  return 0;
}


int decompress_buf(char* buf, char* buf_out, int buf_size) {
  int ret;
  rec_strm.avail_in = buf_size;
  rec_strm.next_in = (Bytef *) buf;
  rec_strm.avail_out = CHUNK;
  rec_strm.next_out = (Bytef *) buf_out;
  do {
    ret = inflate(&rec_strm, Z_SYNC_FLUSH);
    if (ret == Z_STREAM_ERROR) {
      (void)deflateEnd(&send_strm);
      error_exit();
    }
  } while (rec_strm.avail_out == 0);
  (void)inflateEnd(&rec_strm);
  int have = CHUNK - rec_strm.avail_out;
  return have;
}


int compress_buf(char* buf, int buf_size, int fd_out) {
  int ret;
  unsigned char out[CHUNK];
  send_strm.avail_in = buf_size;
  send_strm.next_in = (Bytef *) buf;
  send_strm.avail_out = CHUNK;
  send_strm.next_out = out;
  do {
    ret = deflate(&send_strm, Z_SYNC_FLUSH);
    if (ret == Z_STREAM_ERROR) {
      (void)deflateEnd(&send_strm);
      error_exit();
    }
    int have = CHUNK - send_strm.avail_out;
    write(fd_out, out, have);
  } while (send_strm.avail_out == 0);
  assert(send_strm.avail_in == 0);
  return 0;
}


