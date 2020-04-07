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
  "--sync=[ms]            optional argument to ensure synchronization of threads\n"
  "--lists=NUM            optional argument to partition list into sublists\n;";

void yield_option_check(char * arg);
void exit_and_free(int ret_val);

long time_diff(struct timespec* start, struct timespec* end);
void initialize_elements(SortedListElement_t* elem_arr, char* keys, int threads, int iterations);
void *thread_func(void* args);
void standard_thread(void* args);
void mutex_thread(void* args);
void spin_thread(void * args);

int i_flag = 0; //flags for each of the yield options -- this is yield i option
int d_flag = 0; //yield d option
int l_flag = 0; // yield l option
int m_flag = 0; //flags for each of the sync options -- this is mutex
int s_flag = 0; // spinlock
int list_flag = 0; //flag for list variable
int iter_op = 0; //global int for keeping track of number of iterations

int list_op; //global var for keeping track of list count
SortedList_t* list_arr; //global array of lists
int *spin_arr; //array for spinlocks
pthread_mutex_t *mutex; //mutex pointer for mutex array
int mutex_set = 0; //flag if mutex has been set
int spinlock = 0; //spinlock int for spinlock option



//args to be used for creation of pthread
struct List_args {
  SortedListElement_t* ptr; //ptr to where each list's partition begins
  long lock_time; //lock time for each thread
};
void safe_gettime(struct timespec *time); //get time for a timespec


int main(int argc, char ** argv) {
  int thread_opt = 1;
  iter_op = 1;
  list_op = 1;
  signal(SIGSEGV, seg_handler); //segfault signal handler
  static struct option longopts[] = {
    {"threads", required_argument, 0, 't'},
    {"iterations", required_argument, 0, 'i'},
    {"yield", required_argument, 0, 'y'},
    {"sync", required_argument, 0, 's'},
    {"lists", required_argument, 0, 'l'},
    {0, 0, 0, 0}
  };
  int opt = 0;
  while ( (opt =  getopt_long(argc, argv, "t:i:y:s:l:", longopts, NULL)) != -1) {
    switch(opt) {
    case 't':
      if (optarg)
	thread_opt = atoi(optarg);
      if (thread_opt <= 0) {
	fprintf(stderr, "Incorrect argument given to --threads=NUM option\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      break;
    case 'i':
      if (optarg)
	iter_op = atoi(optarg);
      if (iter_op <= 0) {
	fprintf(stderr, "Incorrect argument given to --iterations=NUM option\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      break;
    case 'y':
      yield_option_check(optarg);
      break;
    case 's':
      if (strlen(optarg) > 1) {
	fprintf(stderr, "Can only have one option used for --sync=OPT\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      
      if (optarg[0] == 'm')
	m_flag = 1;
      else if (optarg[0] == 's')
	s_flag = 1;
      else {
	fprintf(stderr, "Invalid value for --sync=NUM\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      break;
    case 'l':
      if (optarg)
	list_op = atoi(optarg);
      if (list_op <= 0) {
	fprintf(stderr, "Incorrect argument given to --lists=NUM operation\n");
	fprintf(stderr, "%s", usage);
	exit(1);
      }
      list_flag = 1;
      break;  
    case '?':
      fprintf(stderr, "Invalid option\n");
      fprintf(stderr, "%s", usage);
      exit(1);
      break;
    }
  }
  if (optind < argc) //print for extra invalid arguments
  {
    for (; optind < argc; optind++)      
          fprintf(stderr, "invalid argument: %s\n", argv[optind]);
    fprintf(stderr, "%s", usage);
    exit(1);
  }

  pthread_mutex_t mutex_arr[list_op]; //initialize mutex locks
  if (m_flag) { //set mutex flag
    int i;
    for (i = 0; i < list_op; i++) //init each lock
      pthread_mutex_init(mutex_arr+i, NULL);
    mutex = mutex_arr;
    mutex_set = 1;
  }

  int spins[list_op]; //initialize spinlocks if needed
  if (s_flag) {
    int i;
    for (i = 0; i < list_op; i++) 
      spins[i] = 0;
  }
  spin_arr = spins; //assign array to global pointer
  long num_lock_ops = 0; //keep track of number of locks
  if (m_flag || s_flag) { //one of the locks has been chosen
    num_lock_ops = (iter_op*thread_opt*2) + (thread_opt*list_op); //lock used 2x per elem, list*thread
  }

  struct timespec start, end; //set declare times and get start time
  if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    exit_and_free(1);

  pthread_t thread_arr[thread_opt]; //create array for threads
  SortedList_t lists[list_op]; //create array for lists
  int i;
  for (i = 0; i < list_op; i++) { //initialize each list
    lists[i].next = lists+i;
    lists[i].prev = lists+i;
    lists[i].key = NULL;
  }
  list_arr = lists; //assign list array to global
  const int total_elems = thread_opt * iter_op; //total number of elements to be initialized/added
  SortedListElement_t elem_arr[total_elems]; //array of elements for list
  char keys[total_elems];
  initialize_elements(elem_arr, keys, thread_opt, iter_op); //call to randomly initialize elements
  struct List_args args_arr[thread_opt]; //declare args for each thread
  
  //create threads
  int ret; //use for return values
  for (i = 0; i < thread_opt; i++) {
    args_arr[i].ptr = elem_arr+(i*iter_op); //give each thread a segment of the total mem
    args_arr[i].lock_time = 0; //keep track of each lock
    ret = pthread_create(thread_arr+i, NULL, thread_func, (void*) &args_arr[i]);
    if (ret == -1) {
      fprintf(stderr, "Could not create thread\n");
      exit_and_free(1);
    }
  }

  long long total_lock_time = 0;
  //wait for threads
  for (i = 0; i < thread_opt; i++) {
    ret = pthread_join(thread_arr[i], NULL);
    if (ret == -1) {
      fprintf(stderr, "Could not rejoin thread in main\n");
      exit_and_free(1);
    }
    total_lock_time += args_arr[i].lock_time;
  }

  //get end time
  if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
    fprintf(stderr, "Could not get end time\n");
    exit_and_free(1);
  }

  long long avg_lock_time = 0;
  if (m_flag || s_flag) {
    avg_lock_time = total_lock_time / num_lock_ops;
  }
  //format ops, times for final print msg
  const long operations = (thread_opt * iter_op * 3);
  long runtime = time_diff(&start, &end);
  long avgtime = runtime/operations;
  char name[15]; //longest possible name is 14 chars + 1 null byte
  msg_set(name); //set name to be printed
  printf("%s,%d,%d,%d,%ld,%ld,%ld,%lld\n", name, thread_opt, iter_op, list_op, operations, runtime, avgtime, avg_lock_time); //print to stdout
  //print to file
  int fd = open("lab2b_list.csv", O_WRONLY|O_APPEND|O_CREAT, 0666);
  if (fd == -1) {
    fprintf(stderr, "Could not open/create lab2b_list.csv\n");
    exit_and_free(1);
  }
  dprintf(fd, "%s,%d,%d,%d,%ld,%ld,%ld,%lld\n", name, thread_opt, iter_op, list_op, operations, runtime, avgtime, avg_lock_time);
  ret = close(fd); //printed and close
  if (ret == -1) {
    fprintf(stderr, "Could not close lab2b_list.csv\n");
    exit_and_free(1);
  }
  exit_and_free(0); //exit successfully
}


//this algorithm from a source https://www.beck-ipc.com/api_files/sc145/libc/Elapsed-Time.html
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
  if (mutex_set) { //destroy locks if necessary on exit
    int i;
    for (i = 0; i < list_op; i++) //destroy each lock
      pthread_mutex_destroy(mutex+i);
  }
  exit(ret_val);
}


void yield_option_check(char * arg) { //check yield options are valid
  int length = strlen(arg);
  if (length > 3) {
    fprintf(stderr, "Too many arguments passed to --yield=OPT\n");
    fprintf(stderr, "%s", usage);
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
      fprintf(stderr, "%s", usage);
      exit_and_free(1);
      break;
    }
  }
}


void initialize_elements(SortedListElement_t* elem_arr, char* keys, int threads, int iterations) {
  int i;
  const int total_elems = threads*iterations; //total elements in wholel ist
  for (i = 0; i < total_elems; i++) {
    keys[i] = (char) (rand() % 128); //give each element random key
    elem_arr[i].key = (keys+i); //assing address of key to each element
  }
}

//modified dbj2 algorithm from http://www.cse.yorku.ca/~oz/hash.html
unsigned int hash_func(const char *key) {
  if (list_op == 1) //simple case
     return 0;
  unsigned int hash = 5381;
  hash = ((hash << 5) + hash) + *key;
  return hash % list_op; 
}

void *thread_func(void* args) { //this thread function handles which control flow to use
  void (*protocol) (void *);
  if (m_flag)
    protocol = mutex_thread; //make the function run mutex control flow
  else if (s_flag)
    protocol = spin_thread; //make the function run spin control flow
  else
    protocol = standard_thread; //make the function run control flow w/o locks
  protocol(args); //run the control flow that was selected
  pthread_exit(NULL); //complete thread
}

void standard_thread(void *args) { //thread func for no locks
  int i;
  struct List_args* arg = (struct List_args*) args;
  SortedListElement_t* elem_arr = arg->ptr;
  SortedListElement_t* cur;
  unsigned int hash;
  for (i = 0; i < iter_op; i++) {
    cur = elem_arr+i;
    hash = hash_func(cur->key); //get hash for key
    SortedList_insert(list_arr+hash, cur); //insert each element
  }
  int length = 0;
  for (i = 0; i < list_op; i++)
    length += SortedList_length(list_arr+i);
  if (length == -1) {
    fprintf(stderr, "Error: Mismatched pointers in length\n");
    exit_and_free(2);
  }
  for (i = 0; i < iter_op; i++) {
    SortedListElement_t* elem = elem_arr+i;
    hash = hash_func(elem->key); //get hash for key
    SortedListElement_t* cur = SortedList_lookup(list_arr+hash, elem->key);
    if (cur == NULL) {
      fprintf(stderr, "Error: Key not found when it should be in list\n");
      exit_and_free(2);
    }
    int ret = SortedList_delete(cur);
    if (ret == 1) {
      fprintf(stderr, "Error: Mismatched pointers in delete\n");
      exit_and_free(2);
    }
  }
}

void mutex_thread(void* args) {
  struct List_args* arg = (struct List_args*) args;
  SortedListElement_t* elem_arr = arg->ptr;
  struct timespec before, after;
  int i;
  for (i = 0; i < iter_op; i++) {
    SortedListElement_t* cur = elem_arr+i;
    unsigned int hash = hash_func(cur->key); //get hash for key
    safe_gettime(&before); //measure the lock operation
    pthread_mutex_lock(mutex+hash);
    safe_gettime(&after);
    arg->lock_time += time_diff(&before, &after);
    SortedList_insert(list_arr+hash, cur); //insert and unlock
    pthread_mutex_unlock(mutex+hash);
  }
  

  for (i = 0; i < list_op; i++) { //lock each list
    safe_gettime(&before);
    pthread_mutex_lock(mutex+i);
    safe_gettime(&after);
    arg->lock_time += time_diff(&before, &after);
  }
  int length = 0;
  for (i = 0; i < list_op; i++) { //get length of each list
    length += SortedList_length(list_arr+i);
  }
  for (i = 0; i < list_op; i++) //unlock each list
    pthread_mutex_unlock(mutex+i);

  if (length == -1) {
    fprintf(stderr, "Error: Mismatched pointers in length\n");
    exit_and_free(2);
  }
  unsigned int hash;
  SortedListElement_t *elem, *cur;
  for (i = 0; i < iter_op; i++) {
    elem = elem_arr+i;
    hash = hash_func(elem->key); //get hash for element
    safe_gettime(&before); //measure lock
    pthread_mutex_lock(mutex+hash);
    safe_gettime(&after);
    arg->lock_time += time_diff(&before, &after);
    cur = SortedList_lookup(list_arr+hash, elem->key);
    if (cur == NULL) {
      fprintf(stderr, "Error: Key not found when it should be in list\n");
      exit_and_free(2);
    }
    int ret = SortedList_delete(cur);
    if (ret == 1) {
      fprintf(stderr, "Error: Mismatched pointers in delete\n");
      exit_and_free(2);
    }
    pthread_mutex_unlock(mutex+hash); //unlock list's lock
  }
}

void spin_thread(void * args) {
  struct List_args* arg = (struct List_args*) args; //convert args to List_args ptr
  struct timespec before, after; //use these timespecs to calculate elapsed time
  int i;
  for (i = 0; i < iter_op; i++) {
    int hash = hash_func(arg->ptr[i].key); //get hash for elem
    safe_gettime(&before); //lock and measure time
    while (__sync_lock_test_and_set(spin_arr + hash, 1) == 1) //spinlock
	;
    safe_gettime(&after);
    arg->lock_time += time_diff(&before, &after); //calculate time after
    SortedList_insert(list_arr + hash, &arg->ptr[i]); //insert and unlock
    __sync_lock_release(spin_arr+hash); 
  }

  int length = 0;
  for (i = 0; i < list_op; i++) {
    safe_gettime(&before); //lock each list
    while (__sync_lock_test_and_set(spin_arr + i, 1) == 1)
      ;
    safe_gettime(&after);
    arg->lock_time += time_diff(&before, &after);
  }
  for (i = 0; i < list_op; i++) //add up length of each list
        length += SortedList_length(list_arr+i);
  for (i = 0; i < list_op; i++) //unlock each list
    __sync_lock_release(spin_arr + i);

  if (length == -1) {
    fprintf(stderr, "Error: Mismatched pointers in length\n");
    exit_and_free(2);
  }
  int hash;
  for (i = 0; i < iter_op; i++) {
    SortedListElement_t* cur;
    hash = hash_func(arg->ptr[i].key); //get hash for each elem
    safe_gettime(&before);
    while (__sync_lock_test_and_set(spin_arr+hash, 1) == 1) //lock respective list
      ;
    safe_gettime(&after);
    arg->lock_time += time_diff(&before, &after);
    cur = SortedList_lookup(list_arr+hash, arg->ptr[i].key); //look up value
    if (cur == NULL) {
      fprintf(stderr, "Error: Key not found when it should be in list\n");
      exit_and_free(2);
    }
    int ret = SortedList_delete(cur); //delete found value
    if (ret == 1) {
      fprintf(stderr, "Error: Mismatched pointers in delete\n");
      exit_and_free(2);
    }
    __sync_lock_release(spin_arr + hash); //release lists lock
  }  
}


void msg_set(char *arg) { //parser to set exit message
  char yield[5]; //holds the yield option part of message
  char sync[5]; //holds the sync option part of message
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

void safe_gettime(struct timespec *time) { //get time and check for error
  if (clock_gettime(CLOCK_MONOTONIC, time) == -1)
    exit_and_free(1);
}

void seg_handler() { //seg fault handler
  fprintf(stderr, "Error: Segmentation fault\n");
  exit_and_free(2);
}

