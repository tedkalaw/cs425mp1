#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "mp1.h"
typedef struct {
  int timestamp_length;
  int sender;
  int seq_number;
  char* message;
} tagged_message;

typedef struct{
  int pid;
  int seq_number;
} timestamp_t;

//Data structures
int seq_number = 0;
int* vector_timestamp;
//On failure, we can remove the key from our hash
GHashTable* hash = NULL;

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

    int i;
    size_t num_timestamps = mcast_num_members * sizeof(timestamp_t);
    char* to_send = malloc(num_timestamps + strlen(message) + 1);

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
    //not sure if this is ncessary or not yet
    to_send[strlen(message)] = '\0';
    //reset pointer to point to beginning of whole array
    to_send -= (num_timestamps);
    cur = (timestamp_t*)to_send;

    pthread_mutex_lock(&member_lock);
    for (i = 0; i < mcast_num_members; i++) {
        usend(mcast_members[i], to_send, num_timestamps + strlen(message)+1);
    }
    pthread_mutex_unlock(&member_lock);
    //TODO:
    //free(to_send);
}

void deliver_tagged_message(int source, const char* message, int len){
    deliver(source, message);
}

void receive(int source, const char *message, int len) {
    //check to make sure that string is correctly null terminated
    assert(message[len-1] == 0);
    timestamp_t* cur = (timestamp_t*)message;
    int i;
    for(i=0; i<mcast_num_members; i++){
      cur = (timestamp_t*)message;
      g_hash_table_insert(hash, cur->pid, cur->seq_number);
      debugprintf("%d: %d\n", cur->pid, cur->seq_number);
      message += sizeof(timestamp_t);
      
    }
    deliver_tagged_message(source, message, len);
}

void mcast_join(int member) {
  if(!hash)
    hash = g_hash_table_new(g_direct_hash, g_direct_equal);

  g_hash_table_insert(hash, member, 0);
}
