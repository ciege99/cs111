// NAME: Collin Prince
// ID: 505091865
// EMAIL: cprince99@g.ucla.edu


#include "SortedList.h"
#include <pthread.h>

//circular list with dummy node, empty list has both list->next and list->prev point to itself


void insert_between_nodes (SortedListElement_t *left, SortedListElement_t *right, SortedListElement_t *elem) {
  left->next = elem;
  elem->prev = left;
  right->prev = elem;
  elem->next = right;
}

void delete_node(SortedListElement_t *left, SortedListElement_t *right) {
  left->next = right;
  right->prev =left;
}


void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
  int inserted = 0; //flag to mark when inserted
  if (opt_yield & INSERT_YIELD)
    sched_yield();
  if (list->next == list) {     //empty list, set head's next and prev to be the new element    
    insert_between_nodes(list, list, element);
    inserted = 1;
  }
  else {
    SortedListElement_t* cur = list->next;
    do { 
      if (element->key < cur->key) {
	insert_between_nodes(cur->prev, cur, element);
	inserted = 1;
	break;
      }
      cur = cur->next;
    } while (cur != list);

    //at this point, the val is greater than all other values
    while (inserted == 0) {
      insert_between_nodes(list->prev, list, element);
      break;
    }
  }
}


//not sure about this one, only an if for the first if?
int SortedList_delete(SortedListElement_t *element) {
  if (opt_yield & DELETE_YIELD)
    sched_yield();
  while (element->prev->next != element->next->prev)
    return 1;
  delete_node(element->prev, element->next);
  return 0;
}


SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
  if (opt_yield & LOOKUP_YIELD)
    sched_yield();
  while (list->next == list) //empty
    return NULL;
  SortedListElement_t* cur = list->next;
  do {
    while (cur->key == key) 
      return cur;
    cur = cur->next;
  } while (cur != list);

  return NULL; //at this point, value not there
}

int SortedList_length(SortedList_t *list) {
  if (opt_yield & LOOKUP_YIELD)
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
      total += 1;
      cur = cur->next;
    } while (cur != list);
    return total;
  }
}
