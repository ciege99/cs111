/*
Name: Collin Prince
Email: cprince99@g.ucla.edu
ID: 505091865
*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

void catch_seg() {
  char * msg = "Segmentation fault caught by catch argument\n";
  write(2, msg, 44);
  exit(4);
}

void open_error() {
  fprintf(stderr, "open error from --input due to: %s\n", strerror(errno));
  exit(2);
}

void creat_error() {
  fprintf(stderr, "creat error from --output due to: %s\n", strerror(errno));
  exit(3);
}

char * usage_msg = "correct usage:\n"
          "--input=filename : use use the specified file as standard input\n"
          "--output=filename : create the specified file and use it as standard output\n" 
          "--segfault : force a segmentation fault\n"
          "--catch : catch a segmentation fault\n";


int main(int argc, char **argv) {
  int fd0 = 0;
  int fd1 = 1;
  static int flag_seg = 0;
  static int flag_catch = 0;
  static struct option longopts[] = {
    {"input", required_argument, 0, 'i'},
    {"output", required_argument, 0, 'o'}, 
    {"segfault", no_argument, 0, 's'},
    {"catch", no_argument, 0, 'c'},
    {0, 0, 0, 0}
  };
  int opt = 0;
  while ( (opt =  getopt_long(argc, argv, "sci:o:", longopts, NULL)) != -1) {
    switch(opt) {
      case 'i':
        fd0 = open(optarg, O_RDONLY);
        if (fd0 == -1)
          open_error();
        close(0);
        dup(fd0);
        break;
      case 'o':
        fd1 = creat(optarg, 0666);
        if (fd1 == -1)
          creat_error();
        close(1);
        dup(fd1);
        break;
      case 's':
        flag_seg = 1;
        break;
      case 'c':
        flag_catch = 1;
        break;
      case '?':
        fprintf(stderr, "invalid option\n");
        fprintf(stderr, usage_msg);
        exit(1);
        break;
        
    }
  }
  if (optind < argc)
  {
    for (; optind < argc; optind++)      
          fprintf(stderr, "invalid argument: %s\n", argv[optind]);
    fprintf(stderr, usage_msg);
    exit(1);
  }

  if (flag_catch) {
    signal(SIGSEGV, catch_seg);
  }

  if (flag_seg) {
    char * fault;
    fault = NULL;
    *fault = '0';
  }

  int x;
  char buf;
  while (1) {
    x = read(0, &buf, 1);
    if (x == 0) 
      break;
    x = write(1, &buf, 1);
  }
  exit(0);
}
