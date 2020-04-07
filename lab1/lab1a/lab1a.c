// NAME: Collin Prince
// EMAIL: cprince99@g.ucla.edu
// ID: 505091865

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

extern int errno;

void io_func(); //handle program flow if no opt
void shell_func(); //handle program flow if used with --shell
int handle_stdin(int pipe_out, int child); //handle input from term
int handle_shell(int pip); //handle input from shell

void error_exit(); //standard exit protocol for error
void exit_proto(struct termios *orig); //standard exit protocol for normal behavior
void set_term(struct termios *orig); //set terminal to new term settings
int opt_handler(); //handle options
void sigpipe_handler(); //handle SIGPIPE
void close_pipe(int fd); //close a pipe and check for error

char usage[] = "Usage: ./lab1a [--shell] \r\n"
  "Run terminal in character-at-a-time, full duplex I/O \r\n"
  "Option: --shell     create a pipe between terminal and shell \r\n";

int main(int argc, char** argv) {
  struct termios og_set;
  int err = tcgetattr(0, &og_set); //get term settings
  if (err == -1) 
    error_exit();
  set_term(&og_set); //call func to change term settings
  int fork_flag = opt_handler(argc, argv); //get opts
  if (fork_flag == 1) //generate shell if passed --shell
    shell_func();
  else               //do normal I/O
    io_func(); 
  exit_proto(&og_set); //reset settings, exit
}


void io_func() {
  int x;
  char buf[256];
  int i = 0;
  int eof_check = 0;
  while (1)
  {
    x = read(0, buf, 256); //continuously read in buffer until error
    if (x == -1)
      error_exit();
    for (i=0; i < x; i++)
    {
      if (buf[i] ==  0x04) { //EOF
	eof_check = 1;
	break;
      }
      else if (buf[i] == 0x0D || buf[i] == 0x0A) { // cr or lf
	char cr[2];
	cr[0] = 0x0D;
	cr[1] = 0x0A;
	if (write(1, cr, 2) == -1)
	  error_exit();
      } else { // write in standard case
	if (write(1, buf+i, 1) == -1)
	  error_exit();
      }
    }
    if (eof_check == 1) //break on EOF
      break;
  }
}


void shell_func() {
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
    fds[0].fd = 0; //fds[0] refers to 0 (stdin)
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
      if (err_check1 != 1 && ((fds[0].revents & POLLHUP || fds[0].revents & POLLERR) || (fds[1].revents & POLLHUP || fds[1].revents & POLLERR))) //POLLHUP or POLLERR
	break;
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


int handle_stdin(int pipe_out,  int child) {
  int x, i;
  char buf[256];
  char holder[2];
  int check = 0;
  x = read(STDIN_FILENO, buf, 256); //read into buf
  if (x == -1)
    error_exit();
  for (i=0; i < x; i++) {
    if (buf[i] ==  0x04) { //EOF
      if (close(pipe_out) == -1)
	error_exit();
      check = 1;
      holder[0] = '^';
      holder[1] = 'D';
      if (write(STDOUT_FILENO, holder, 2) == -1) 
	error_exit();
      break;
    }
    else if (buf[i] == 0x03) { //SIGINT
      check = 2;
      holder[0] = '^';
      holder[1] = 'C';
      if (write(STDOUT_FILENO, holder, 2) == -1)
	error_exit();
      kill(child, SIGINT); //kill child process on ^C
      break;
    }
    else if (buf[i] == 0x0D || buf[i] == 0x0A) { //cr or lf
      holder[0] = 0x0D;
      holder[1] = 0x0A;
      if (write(STDOUT_FILENO, holder, 2) == -1)
	error_exit(); //echo newline
      if (write(pipe_out, holder+1, 1) == -1)
	error_exit(); //send linefeed
    } else {
      if (write(STDOUT_FILENO, buf+i, 1) == -1) //write to stdout and pipe if normal
	error_exit();
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
  int i;
  int eof_check = 0;
  int cnt = read(pip, buf, 256); //read into buf
  if (cnt == -1)
    error_exit();
  if (cnt == 0) //check for EOF
    return 1;
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

void set_term(struct termios *orig) {
  int err = tcgetattr(0, orig); //get term settings
  if (err == -1)
    error_exit();
  struct termios new_set;
  new_set = *orig; //create new struct, and set term to these specs
  new_set.c_iflag = ISTRIP;
  new_set.c_oflag = 0;
  new_set.c_lflag = 0;
  err = tcsetattr(0, TCSANOW, &new_set);
  if (err == -1)
    error_exit();
}

void exit_proto(struct termios *orig) {
  int err = tcsetattr(0, TCSANOW,  orig); //return to old settings, exit
  if (err == -1)
    error_exit();
  exit(0);
}

int opt_handler(int argc, char**argv) {
  static struct option longopt[] = {
				    {"shell", no_argument, 0, 's'},
				    {0, 0, 0, 0}
  };
  int fork_flag = 0; //used to signal standard or shell
  int opt;
  while ( (opt =  getopt_long(argc, argv, "s", longopt, NULL)) != -1) {
    switch (opt) {
    case 's':
      fork_flag = 1;
      break;
    case '?':
      fprintf(stderr, "Error: invalid option\r\n");
      fprintf(stderr, usage);
      exit(1);
      break;
    }
  }
  if (optind < argc) //print out extra args
  {
    for (; optind < argc; optind++)      
      fprintf(stderr, "invalid argument: %s\n", argv[optind]);
    fprintf(stderr, usage);
    exit(1);
  }
  return fork_flag;
}

void sigpipe_handler() { //exit normally for sigpipe
  exit(0);
}

void error_exit() {
  fprintf(stderr, "Error: %s", strerror(errno));
  exit(1);
}

void close_pipe(int fd) {
  if (close(fd) == -1)
    error_exit();
}

