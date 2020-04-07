// NAME: Collin Prince
// ID: 50509165
// EMAIL: cprince99@g.ucla.edu

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char usage[] = "./lab2_add [--threads=THRD_NUM] [--iterations=ITER_NUM]\n"
  "Create a program to add to a global counter w/ optional multi-thread capabilities\n"
  "Options: \n"
  "--threads=THRD_NUM     optional argument to specify number of threads, default of 1\n"
  "--iterations=THRD_NUM  optional argument to specify number of iterations, default of 1\n";


void add(long long *pointer, long long value);
void *add_wrapper(void * args);
void exit_and_free(int ret_val);

long time_diff(struct timespec* start, struct timespec* end);

//pthread_t * thread_arr; //thread arr to keep track of threads
int malloc_flag = 0; //use to know whether malloc of ptr has occurred
int opt_yield = 0; //use for --yield option
int m_flag = 0; //flags for each of the sync options
int s_flag = 0;
int c_flag = 0;

int mutex_set = 0; //flag if mutex has been set
int spinlock = 0; //spinlock int for spinlock option

pthread_mutex_t mutex;

struct Add_args {
  long long *ptr;
  int num_iters;
};



int main(int argc, char ** argv) {
  int thread_opt = 1;
  int iter_opt = 1;
  static struct option longopts[] = {
    {"threads", optional_argument, 0, 't'},
    {"iterations", optional_argument, 0, 'i'},
    {"yield", no_argument, 0, 'y'},
    {"sync", required_argument, 0, 's'},
    {0, 0, 0, 0}
  };
  int opt = 0;
  while ( (opt =  getopt_long(argc, argv, "s:t::i::y", longopts, NULL)) != -1) {
    switch(opt) {
    case 't':
      if (optarg)
	thread_opt = atoi(optarg);
      if (thread_opt == 0) {
	exit(1);
      }
      break;
    case 'i':
      if (optarg)
	iter_opt = atoi(optarg);
      if (iter_opt == 0) {
	exit(1);
      }
      break;
    case 'y':
      opt_yield = 1;
      break;
    case 's':
      if (strlen(optarg) > 1) {
	fprintf(stderr, "Error: incorrect option usage for sync");
	fprintf(stderr, usage);
	exit(1);
      }
      if (optarg[0] == 'm')
	m_flag = 1;
      else if (optarg[0] == 's')
	s_flag = 1;
      else if (optarg[0] == 'c')
	c_flag = 1;
      else {
	fprintf(stderr, "Incorrect option given to --sync=OPT\n");
	fprintf(stderr, usage);
	exit(1);
      }
      break;
    case '?':
      fprintf(stderr, "invalid option\n");
      fprintf(stderr, usage);
      exit(1);
      break;
        
    }
  }
  if (optind < argc)
  {
    for (; optind < argc; optind++)      
          fprintf(stderr, "invalid argument: %s\n", argv[optind]);
    fprintf(stderr, usage);
    exit(1);
  }

  //set mutex flag
  if (m_flag) {
    pthread_mutex_init(&mutex, NULL);
    mutex_set = 1;
  }


  //global counter flow
  long long counter = 0;

  //timer section
  struct timespec start, end;
  if (clock_gettime(CLOCK_REALTIME, &start) == -1)
    exit_and_free(1);

  //define args to be passed
  struct Add_args func_args;
  func_args.ptr = &counter;
  func_args.num_iters = iter_opt;
  pthread_t thread_arr[thread_opt];

  //create threads
  int ret; //use for return values
  int i;
  for (i = 0; i < thread_opt; i++) {
    ret = pthread_create(&thread_arr[i], NULL, add_wrapper, (void*) &func_args);
    if (ret == -1)
      exit_and_free(1);
  }
  //wait for threads
  for (i = 0; i < thread_opt; i++) {
    ret = pthread_join(thread_arr[i], NULL);
    if (ret == -1) {
      exit_and_free(1);
    }
  }

  //get end time
  if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
    exit_and_free(1);
  }
    
  //format ops, times for final print msg
  const long operations = (thread_opt * iter_opt * 2);
  long runtime = time_diff(&start, &end);
  long avgtime = runtime/operations;


  //set name to print
  char name[15]; //longest possible name is 14 chars + 1 null byte
  if (opt_yield) {
    if (m_flag == 1)
      strcpy(name, "add-yield-m");
    else if (s_flag == 1)
      strcpy(name, "add-yield-s");
    else if (c_flag == 1)
      strcpy(name, "add-yield-c");
    else
      strcpy(name, "add-yield-none");
  }
  else {
    if (m_flag == 1)
      strcpy(name, "add-m");
    else if (s_flag == 1)
      strcpy(name, "add-s");
    else if (c_flag == 1)
      strcpy(name, "add-c");
    else
      strcpy(name, "add-none");
  }
  
  printf("%s,%d,%d,%ld,%ld,%ld,%lld\n", name, thread_opt, iter_opt, operations, runtime, avgtime, counter);
  //print to file
  int fd = open("lab2_add.csv", O_WRONLY|O_APPEND|O_CREAT, 0666);
  if (fd == -1) {
    fprintf(stderr, "Could not open/create lab2_add.csv\n");
    exit_and_free(1);
  }
  dprintf(fd, "%s,%d,%d,%ld,%ld,%ld,%lld\n", name, thread_opt, iter_opt, operations, runtime, avgtime, counter);
  close(fd);
  //exit
  exit_and_free(0);
}


//test and set for spinlock
int test_and_set(int *old, int new) {
  int original = *old;
  *old = new;
  return original;

}

void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (opt_yield)
    sched_yield();
  *pointer = sum;
}

void add_mutex(long long *pointer, long long value) {
  pthread_mutex_lock(&mutex);
  add(pointer, value);
  pthread_mutex_unlock(&mutex);
}

void add_spin(long long *pointer, long long value) {
  while (__sync_lock_test_and_set(&spinlock, 1) == 1)
    ;
  add(pointer, value);
  __sync_lock_release(&spinlock);
}

void add_comp(long long *pointer, long long value) {
  int bool = 0;
  while (bool == 0) {
    int old = *pointer;
    int new = old + value;
    if (opt_yield)
      sched_yield();
   int ret =  __sync_val_compare_and_swap(pointer, old, new);
   if (ret == old)
     bool = 1;
  }
}

void *add_wrapper(void  * args) {
  int i;
  struct Add_args * arg = (struct Add_args *) args;

  void (*add_func) (long long *x, long long y);
  if (m_flag == 1)
    add_func = add_mutex;
  else if (s_flag == 1)
    add_func = add_spin;
  else if (c_flag == 1)
    add_func = add_comp;
  else
    add_func = add;
  for (i = 0; i < arg->num_iters; i++) {
    add_func(arg->ptr, 1);
    add_func(arg->ptr, -1);
  }
  
  pthread_exit(NULL);
}

//algorithm from source
long time_diff(struct timespec* start, struct timespec* end) {
  if (end->tv_nsec < start->tv_nsec) {
    int nsec = (start->tv_nsec - end->tv_nsec)/1000000000 + 1;
    start->tv_nsec -= nsec*1000000000;
    start->tv_sec += nsec;
  }
  if (end->tv_nsec - start->tv_nsec > 1000000000) {
    int nsec = (end->tv_nsec - start->tv_nsec)/1000000000 + 1;
    start->tv_nsec += nsec*1000000000;
    start->tv_sec -= nsec;
  }
  long sec_diff = end->tv_sec - start->tv_sec;
  long nano_diff = end->tv_nsec - start->tv_nsec;
  long total;
  total = nano_diff + (1000000000*sec_diff);
  return total;
}


void exit_and_free(int ret_val) {
  if (mutex_set)
    pthread_mutex_destroy(&mutex);
  exit(ret_val);
}
