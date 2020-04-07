// NAME: Collin Prince
// ID: 505091865
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
#include <signal.h>

//forward declarations
struct SortedListElement {
	struct SortedListElement *prev;
	struct SortedListElement *next;
	const char *key;
};
typedef struct SortedListElement SortedList_t;
typedef struct SortedListElement SortedListElement_t;
void SortedList_insert(SortedList_t *list, SortedListElement_t *element);
int SortedList_delete( SortedListElement_t *element);
SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key);
int SortedList_length(SortedList_t *list);
int opt_yield = 0; //define global var
void msg_set(char *arg);
void seg_handler();

char usage[] = "./lab2_list [--threads=THRD_NUM] [--iterations=ITER_NUM] [--yield=[idl]] [--sync=[ms]]\n"
  "Create a program to build a sorted linked list and then destroy it\n"
  "Options: \n"
  "--threads=THRD_NUM     optional argument to specify number of threads, default of 1\n"
  "--iterations=THRD_NUM  optional argument to specify number of iterations, default of 1\n"
  "--yield=[idl]          optional argument to yield threads at critical sections\n"
  "--sync=[ms]            optional argument to ensure synchronization of threads\n";

void yield_option_check(char * arg);
void exit_and_free(int ret_val);

long time_diff(struct timespec* start, struct timespec* end);
void initialize_elements(SortedListElement_t* elem_arr, int threads, int iterations);
void *thread_func(void* args);
void standard_thread(void* args);
void mutex_thread(void* args);
void spin_thread(void * args);

int i_flag = 0; //flags for each of the yield options
int d_flag = 0;
int l_flag = 0;
int m_flag = 0; //flags for each of the sync options
int s_flag = 0;

int mutex_set = 0; //flag if mutex has been set
int spinlock = 0; //spinlock int for spinlock option
pthread_mutex_t mutex; //mutex lock for --sync=m option

//args to be used for creation of pthread
struct List_args {
  SortedListElement_t* ptr;
  SortedList_t* list;
  int num_iters;
};

int main(int argc, char ** argv) {
  int thread_opt = 1;
  int iter_opt = 1;
  signal(SIGSEGV, seg_handler);
  static struct option longopts[] = {
    {"threads", optional_argument, 0, 't'},
    {"iterations", optional_argument, 0, 'i'},
    {"yield", required_argument, 0, 'y'},
    {"sync", required_argument, 0, 's'},
    {0, 0, 0, 0}
  };
  int opt = 0;
  while ( (opt =  getopt_long(argc, argv, "t::i::y:s:", longopts, NULL)) != -1) {
    switch(opt) {
    case 't':
      if (optarg)
	thread_opt = atoi(optarg);
      if (thread_opt <= 0) {
	fprintf(stderr, "Incorrect argument given to --threads=NUM option\n");
	fprintf(stderr, usage);
	exit(1);
      }
      break;
    case 'i':
      if (optarg)
	iter_opt = atoi(optarg);
      if (iter_opt <= 0) {
	fprintf(stderr, "Incorrect argument given to --iterations=NUM option\n");
	fprintf(stderr, usage);
	exit(1);
      }
      break;
    case 'y':
      yield_option_check(optarg);
      break;
    case 's':
      if (strlen(optarg) > 1) {
	fprintf(stderr, "Can only have one option used for --sync=OPT\n");
	fprintf(stderr, usage);
	exit(1);
      }
      
      if (optarg[0] == 'm')
	m_flag = 1;
      else if (optarg[0] == 's')
	s_flag = 1;
      else {
	fprintf(stderr, "Invalid value for --sync=NUM\n");
	fprintf(stderr, usage);
	exit(1);
      }
      break;
    case '?':
      fprintf(stderr, "Invalid option\n");
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

  if (m_flag) { //set mutex flag
    pthread_mutex_init(&mutex, NULL);
    mutex_set = 1;
  }

  struct timespec start, end; //set declare times and get start time
  if (clock_gettime(CLOCK_REALTIME, &start) == -1)
    exit_and_free(1);

  pthread_t thread_arr[thread_opt]; //create array for threads

  const int total_elems = thread_opt * iter_opt; //total number of elements to be initialized/added
  SortedList_t list; //dummy node for list
  SortedListElement_t elem_arr[total_elems]; //array of elements for list
  list.next = &list; //initialize list head
  list.prev = &list;
  char key = '0';
  list.key = &key;
  initialize_elements(elem_arr, thread_opt, iter_opt); //call to randomly initialize elements
  struct List_args args_arr[thread_opt]; //declare args for each thread
  
  //create threads
  int ret; //use for return values
  int i;
  for (i = 0; i < thread_opt; i++) {
    args_arr[i].num_iters = iter_opt; //define each of the variables in arg
    args_arr[i].list = &list;
    args_arr[i].ptr = &elem_arr[i*iter_opt]; //give each thread a segment of the total mem
    ret = pthread_create(&thread_arr[i], NULL, thread_func, (void*) &args_arr[i]);
    if (ret == -1) {
      fprintf(stderr, "Could not create thread\n");
      exit_and_free(1);
    }
  }
  //wait for threads
  for (i = 0; i < thread_opt; i++) {
    ret = pthread_join(thread_arr[i], NULL);
    if (ret == -1) {
      fprintf(stderr, "Could not rejoin thread in main\n");
      exit_and_free(1);
    }
  }

  //get end time
  if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
    fprintf(stderr, "Could not get end time\n");
    exit_and_free(1);
  }
  //format ops, times for final print msg
  const long operations = (thread_opt * iter_opt * 3);
  long runtime = time_diff(&start, &end);
  long avgtime = runtime/operations;
  char name[15]; //longest possible name is 14 chars + 1 null byte
  msg_set(name); //set name to be printed
  printf("%s,%d,%d,1,%ld,%ld,%ld\n", name, thread_opt, iter_opt, operations, runtime, avgtime);
  //print to file
  int fd = open("lab2_list.csv", O_WRONLY|O_APPEND|O_CREAT, 0666);
  if (fd == -1) {
    fprintf(stderr, "Could not open/create lab2_list.csv\n");
    exit_and_free(1);
  }
  dprintf(fd, "%s,%d,%d,1,%ld,%ld,%ld\n", name, thread_opt, iter_opt, operations, runtime, avgtime);
  ret = close(fd);
  if (ret == -1) {
    fprintf(stderr, "Could not close lab2_list.csv\n");
    exit_and_free(1);
  }

  exit_and_free(0); //exit
}


//this algorithm from a source
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


void yield_option_check(char * arg) {
  int length = strlen(arg);
  if (length > 3) {
    fprintf(stderr, "Too many arguments passed to --yield=OPT\n");
    fprintf(stderr, usage);
    exit_and_free(1);
  }

  int i;
  for (i = 0; i < length; i++) {
    switch(arg[i]) {
    case 'i':
      opt_yield = (opt_yield | 0x01);
      i_flag = 1;
      break;
    case 'd':
      opt_yield = (opt_yield | 0x02);
      d_flag = 1;
      break;
    case 'l':
      opt_yield = (opt_yield | 0x04);
      l_flag = 1;
      break;
    default: //incorrect arg given
      fprintf(stderr, "Incorrect argument given to --yield=OPT\n");
      fprintf(stderr, usage);
      exit_and_free(1);
      break;
    }
  }
}


void initialize_elements(SortedListElement_t* elem_arr, int threads, int iterations) {
  int i;
  const int total_elems = threads*iterations;
  for (i = 0; i < total_elems; i++) {
    char key = (char) (rand() % 128);
    elem_arr[i].key = &key;
  }
}

void *thread_func(void* args) {
  void (*protocol) (void *);
  if (m_flag)
    protocol = mutex_thread;
  else if (s_flag)
    protocol = spin_thread;
  else
    protocol = standard_thread;
  protocol(args);
  pthread_exit(NULL);
}

void standard_thread(void *args) {
  int i;
  struct List_args* arg = (struct List_args*) args;
  for (i = 0; i < arg->num_iters; i++) {
    SortedList_insert(arg->list, &arg->ptr[i]);
  }
  int length = SortedList_length(arg->list);
  if (length == -1) {
    fprintf(stderr, "Error: Mismatched pointers in length\n");
    exit(2);
  }
  for (i = 0; i < arg->num_iters; i++) {
    SortedListElement_t* cur = SortedList_lookup(arg->list, arg->ptr[i].key);
    if (cur == NULL) {
      fprintf(stderr, "Error: Key not found when it should be in list\n");
      exit(2);
    }
    int ret = SortedList_delete(cur);
    if (ret == 1) {
      fprintf(stderr, "Error: Mismatched pointers in delete\n");
      exit(2);
    }
  }
}

void mutex_thread(void* args) {
  struct List_args* arg = (struct List_args*) args;
  int i;
  for (i = 0; i < arg->num_iters; i++) {
    pthread_mutex_lock(&mutex);
    SortedList_insert(arg->list, &arg->ptr[i]);
    pthread_mutex_unlock(&mutex);
  }
  pthread_mutex_lock(&mutex);
  int length = SortedList_length(arg->list);
  pthread_mutex_unlock(&mutex);
  if (length == -1) {
    fprintf(stderr, "Error: Mismatched pointers in length\n");
    exit(2);
  }
  for (i = 0; i < arg->num_iters; i++) {
    pthread_mutex_lock(&mutex);
    SortedListElement_t* cur = SortedList_lookup(arg->list, arg->ptr[i].key);
    if (cur == NULL) {
      fprintf(stderr, "Error: Key not found when it should be in list\n");
      exit(2);
    }
    int ret = SortedList_delete(cur);
    if (ret == 1) {
      fprintf(stderr, "Error: Mismatched pointers in delete\n");
      exit(2);
    }
    pthread_mutex_unlock(&mutex);
  }
}

void spin_thread(void * args) {
  struct List_args* arg = (struct List_args*) args;
  int i;
  for (i = 0; i < arg->num_iters; i++) {
    while (__sync_lock_test_and_set(&spinlock, 1) == 1)
      ;
    SortedList_insert(arg->list, &arg->ptr[i]);
    __sync_lock_release(&spinlock);
  }
  while (__sync_lock_test_and_set(&spinlock, 1) == 1)
    ;
  int length = SortedList_length(arg->list);
  __sync_lock_release(&spinlock);
  if (length == -1) {
    fprintf(stderr, "Error: Mismatched pointers in length\n");
    exit(2);
  }
  for (i = 0; i < arg->num_iters; i++) {
    while (__sync_lock_test_and_set(&spinlock, 1) == 1)
      ;
    SortedListElement_t* cur = SortedList_lookup(arg->list, arg->ptr[i].key);
    if (cur == NULL) {
      fprintf(stderr, "Error: Key not found when it should be in list\n");
      exit(2);
    }
    int ret = SortedList_delete(cur);
    if (ret == 1) {
      fprintf(stderr, "Error: Mismatched pointers in delete\n");
      exit(2);
    }
    __sync_lock_release(&spinlock);
  }  
}


void msg_set(char *arg) {
  char yield[5];
  char sync[5];
  switch (opt_yield) {
  case 0x01:
    strcpy(yield, "i"); 
    break;
  case 0x02:
    strcpy(yield, "d"); 
    break;
  case 0x04:
    strcpy(yield, "l");
    break;
  case 0x03:
    strcpy(yield, "id");
    break;
  case 0x05:
    strcpy(yield, "il");
    break;
  case 0x06:
    strcpy(yield, "dl");
    break;
  case 0x07:
    strcpy(yield, "idl");
    break;
  default:
    strcpy(yield, "none");
    break;
  }

  if (m_flag)
    strcpy(sync, "m");
  else if (s_flag)
    strcpy(sync, "s");
  else
    strcpy(sync, "none");
  
  sprintf(arg, "list-%s-%s", yield, sync);
}


void seg_handler() {
  fprintf(stderr, "Error: Segmentation fault\n");
  exit_and_free(2);
}
