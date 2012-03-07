#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "mp1.h"
typedef struct {
  int sender;
  int seq_number;
  char* message;
} tagged_message;

int seq_number = 0;
void multicast_init(void) {
    unicast_init();
}

/* Basic multicast implementation */
void multicast(const char *message) {
    int i;
    char* to_send = malloc((sizeof(int) * 2) + strlen(message) + 1);
    tagged_message* numbers = (tagged_message*)to_send;
    numbers->sender = my_id;
    numbers->seq_number = seq_number;
    to_send += (2*sizeof(int));
    strncpy(to_send, message, strlen(message));
    to_send[strlen(message)] = '\0';
    to_send -= (2*sizeof(int));

    seq_number++;
    pthread_mutex_lock(&member_lock);
    for (i = 0; i < mcast_num_members; i++) {
        usend(mcast_members[i], to_send, (2*sizeof(int)) + strlen(message)+1);
    }
    pthread_mutex_unlock(&member_lock);
    //update number of messages sent
    free(to_send);
}

void receive(int source, const char *message, int len) {
    assert(message[len-1] == 0);
    tagged_message *incoming_message = (tagged_message*)message;
    const char* text = message += (2*sizeof(int));
    deliver(source, text);
}

void mcast_join(int member) {

}
