// NAME: Collin Prince
// ID: 505091865
// EMAIL: cprince99@g.ucla.edu


#include "SortedList.h"
#include <pthread.h>

//circular list with dummy node, empty list has both list->next and list->prev point to itself

//insert new element between two nodes
void insert_between_nodes (SortedListElement_t *left, SortedListElement_t *right, SortedListElement_t *elem) {
  left->next = elem;
  elem->prev = left;
  right->prev = elem;
  elem->next = right;
}

//pass in two nodes to connect
void delete_node(SortedListElement_t *left, SortedListElement_t *right) {
  left->next = right; 
  right->prev =left;
}


void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
  int inserted = 0; //flag to mark when inserted
  if (opt_yield & INSERT_YIELD) //critical section before we check list's pointer
    sched_yield();
  if (list->next == list) {     //empty list, set head's next and prev to be the new element    
    insert_between_nodes(list, list, element);
    return;
  }
  SortedListElement_t* cur = list->next;
  while (cur != list) {
    if (opt_yield & INSERT_YIELD) //critical section before we compare each key
      sched_yield();
    if ( *(element->key) <= *(cur->key)) { //compare keys and mark if we need to insert
      inserted = 1;
      break;
    }
    cur = cur->next;
  }
  if (opt_yield & INSERT_YIELD) // critical section before we insert
    sched_yield();
  if (inserted == 0) //if inserted == 0, it is greatest element in list
    insert_between_nodes(list->prev, list, element);
  else //otherwise, inserted was marked and we broke from loop, insert between cur->prev and cur
    insert_between_nodes(cur->prev, cur, element);
  return;
}


int SortedList_delete(SortedListElement_t *element) {
  if (opt_yield & DELETE_YIELD) //critical section before we check pointers
    sched_yield();
  while (element->prev->next == element->next->prev) {
    delete_node(element->prev, element->next); //if our pointers match up, insert
    return 0;
  }
  return 1;
}


SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
  if (opt_yield & LOOKUP_YIELD) //critical section before we check list's pointers
    sched_yield();
  while (list->next == list) //empty
    return NULL;
  SortedListElement_t* cur = list->next;
  do {
    if (opt_yield & LOOKUP_YIELD)
      sched_yield();
    while ( *(cur->key) == *(key)) //critical section as we compare cur and our key
      return cur;
    cur = cur->next;
  } while (cur != list); //loop until we hit end of list

  return NULL; //at this point, value not there
}

int SortedList_length(SortedList_t *list) {
  if (opt_yield & LOOKUP_YIELD) //critical section as we check list's next ptr for empty list
    sched_yield();
  if (list->next == list) //empty list
    return 0;
  else if (list->next == list->prev) //one elem list
    return 1;
  else {
    SortedListElement_t* cur = list->next;
    while (cur->prev->next != cur->next->prev)
      return -1;
    int total = 0;
    do {
      if (opt_yield & LOOKUP_YIELD) //critical section before we change to our next pointer
	sched_yield();
      total += 1; //increment for another valid node
      cur = cur->next;
    } while (cur != list); //loop until end of list
    return total;
  }
}
