#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "mp1.h"

typedef struct{
  int pid;
  int seq_number;
} timestamp_t;

typedef struct{
  int pid;
  int length;
} tag_t;

//TODO: interesting case...relationship between timestamp length and validity?
//TODO: case where a new process joins - what happens to its timestamps?
//that is to say, how does it initialize its timestamps?

//Data structures
int seq_number = 0;
int* vector_timestamp;
//On failure, we can remove the key from our hash
GHashTable* hash = NULL;
GSList* holdback_queue = NULL;
GSList* received = NULL;
pthread_mutex_t queue_access;

void multicast_init(void) {
    unicast_init();
    //init our global hash to store timestamps in
}

void atomic_seq_inc(){
    pthread_mutex_lock(&member_lock);
    ++seq_number;
    pthread_mutex_unlock(&member_lock);
}

/* Basic multicast implementation */
void multicast(const char *message) {
    atomic_seq_inc();

    //FOR NOW, MCAST NUM MEMBERS IS ONLY INCREMENTING - WHICH IS FINE
    //THIS GUARANTEES THAT OUR TIMESTAMPS WILL GROW
    int i;
    tag_t* tag;
    size_t num_timestamps = mcast_num_members * sizeof(timestamp_t);
    size_t length = sizeof(tag_t) + num_timestamps + strlen(message) + 1;
    char* to_send = malloc(length);

    tag = (tag_t*)to_send;
    tag->pid = my_id;
    tag->length = mcast_num_members;

    to_send += sizeof(tag_t);

    timestamp_t* cur;
    GList* keys = g_hash_table_get_keys(hash);
    while(keys != NULL){
      cur = (timestamp_t*)to_send;
      cur->pid = keys->data;
      if(cur->pid == my_id)
        cur->seq_number = seq_number;
      else
        cur->seq_number = g_hash_table_lookup(hash, keys->data);
      to_send += sizeof(timestamp_t);
      keys = keys->next;
    }

    strncpy(to_send, message, strlen(message));
    to_send[strlen(message)] = '\0';
    //reset pointer to point to beginning of whole array
    to_send -= (sizeof(tag_t) + (num_timestamps));

    pthread_mutex_lock(&member_lock);
    for (i = 0; i < mcast_num_members; i++) {
        usend(mcast_members[i], to_send, length);
    }
    pthread_mutex_unlock(&member_lock);
    //TODO:
    //free(to_send);
}


/*
 * Checks if a given message is deliverable; returns 0 if it IS NOT currently 
 * deliverable, and returns the new timestamp value of the PID that sent the
 * message if it IS deliverable
 */
int is_deliverable(const char* message){
  tag_t* tag = (tag_t*)message;
  timestamp_t* cur;
  int pid = tag->pid;
  int i, update=0;

  cur = (timestamp_t*)(tag+1);
  for(i=0; i<tag->length; i++){
    int val = g_hash_table_lookup(hash, cur->pid);
    //this is the special case where we check if the one we got is +1
    if(pid == cur->pid){
      if((val+1) == cur->seq_number){
        update = cur->seq_number;
      }
      else return 0;
    }
    else {
      if(val <= cur->seq_number){
        return 0;
      }
    }
    cur++;
  }

  return update;
}


//returns new head of list
GSList* modified_delivery(GSList* queue, const char* message, int new_value){
  tag_t* cur;
  cur = (tag_t*)message;
  int length = cur->length;
  deliver(cur->pid, message + sizeof(tag_t) + (sizeof(timestamp_t) * length));
  g_hash_table_insert(hash, cur->pid, new_value);
  g_slist_append(received, message);
  return g_slist_remove(queue, message);
}

//The next two functions are for debugging purposes
void print_queue_vectors(GSList* queue){
  while(queue != NULL){
    print_timestamp(queue);
    queue = queue->next;
  }
}
void print_timestamp(GSList* node){
  tag_t* cur;
  char* message = node->data;
  cur = (tag_t*)(message);
  int length = cur->length;
  message += sizeof(tag_t);
  timestamp_t* timestamp;
  int i;
  for(i=0; i<length; i++){
    timestamp = (timestamp_t*)(message);
    debugprintf("<%d - %d> ", timestamp->pid, timestamp ->seq_number);
    message += sizeof(timestamp_t);
  }
  debugprintf("\n");
}
//make sure that when a new process joins, that it doesn't wait for msgs it
//doesn't need


void deliver_tagged_message(int source, const char* message, int len){
    //IF WE DON'T COPY THIS, MESSAGE WILL KEEP CHANGING
    //that is, it'll take on the value of the newly recv msg
    //I CAN'T REMEMBER C SO WILL HAVE TO ASK THE TA
    char* new_message = malloc(len);
    memcpy(new_message, message, len);
    
    //regardless of whether or not it's deliverable, we add to queue
    holdback_queue = g_slist_append(holdback_queue, new_message);

    tag_t* cur;
    GSList* head = holdback_queue;

    int new_value=0;
    while(head != NULL){
      cur = (tag_t*)(head->data);
      new_value = is_deliverable(head->data);
      if(new_value){
        holdback_queue = modified_delivery(holdback_queue, head->data, new_value);
        head = holdback_queue;
      }
      else{
        head = head->next;
      }
    }
}

void receive(int source, const char *message, int len) {
    //check to make sure that string is correctly null terminated
    //assert(message[len-1] == 0);
    tag_t* tag = (tag_t*)message;
    timestamp_t* cur;
    int num_members = tag->length;
    int i;


    deliver_tagged_message(source, message, len);
}

void mcast_join(int member) {
  //if this process is just starting, make the hash
  if(!hash)
    hash = g_hash_table_new(g_direct_hash, g_direct_equal);

  g_hash_table_insert(hash, member, 0);
}
