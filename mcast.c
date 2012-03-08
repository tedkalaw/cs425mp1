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

//Data structures
int seq_number = 0;
int* vector_timestamp;
//On failure, we can remove the key from our hash
GHashTable* hash = NULL;
GSList* holdback_queue = NULL;
GSList* received = NULL;

void multicast_init(void) {
    unicast_init();
    //init our global hash to store timestamps in
}

void atomic_seq_inc(){
    pthread_mutex_lock(&member_lock);
    g_hash_table_insert(hash, my_id, ++seq_number);
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

gboolean comp_vectors(const char* vector1, const char* vector2){
  return TRUE;
}

void is_deliverable(const char* message){
  tag_t* tag = (tag_t*)message;
  timestamp_t* cur;
  int pid = tag->pid;
  int length = tag->length;
  debugprintf("PID: %d, Length: %d\n", pid, length);

  int i;
  message += sizeof(tag_t);
  for(i=0; i<length; i++){
    cur = (timestamp_t*)message;
    //this is the special case where we check if the one we got is +1
    if(cur->pid == pid){
      printf("In this item: %d\n", cur->seq_number);
      printf("Our value: %d\n", g_hash_table_lookup(hash, pid));
    }
    else{

    }

  }

  //first, check if the value for 


}
//make sure that when a new process joins, that it doesn't wait for msgs it
//doesn't need
void deliver_tagged_message(int source, const char* message, int len){
    //regardless of whether or not it's deliverable, we add to queue
    g_slist_append(holdback_queue, message);


    GSList* head;
    gboolean adding = TRUE;
    int pid;
    tag_t* cur;
    while(adding){
      /*
      head = holdback_queue;
      while(head != NULL){
        cur = (tag_t*)(head->data);
        pid = head->pid;
        if(delivers){
          deliver
          continue;
        }
      }
      */
      adding = FALSE;
    }

    is_deliverable(message);
    
    //so it works
    message += (sizeof(tag_t) + (sizeof(timestamp_t) * len));
    deliver(source, message);
}

void receive(int source, const char *message, int len) {
    //check to make sure that string is correctly null terminated
    assert(message[len-1] == 0);
    tag_t* tag = (tag_t*)message;
    timestamp_t* cur;
    int num_members = tag->length;
    char* mover = message+(sizeof(tag_t));
    int i;
    for(i=0; i<num_members; i++){
      cur = (timestamp_t*)mover;
      //we don't update values until later
      //g_hash_table_insert(hash, cur->pid, cur->seq_number);
      debugprintf("%d: %d\n", cur->pid, cur->seq_number);
      mover += sizeof(timestamp_t);
    }

    deliver_tagged_message(source, message, mcast_num_members);
}

void mcast_join(int member) {
  //if this process is just starting, make the hash
  if(!hash)
    hash = g_hash_table_new(g_direct_hash, g_direct_equal);

  g_hash_table_insert(hash, member, 0);
}
