#include <string.h>
#include <assert.h>

#include "mp1.h"
typedef struct {
  int sender;
  int seq_number;
  char* message;
} tagged_message;

int seq_number = 0;
void multicast_init(void) {
    unicast_init();
    debugprintf("This is a test\n");
}

/* Basic multicast implementation */
void multicast(const char *message) {
    int i;
    tagged_message *new_message = malloc(sizeof(tagged_message));
    new_message->sender = my_id;
    new_message->seq_number = seq_number;

    pthread_mutex_lock(&member_lock);
    for (i = 0; i < mcast_num_members; i++) {
        usend(mcast_members[i], (char*)new_message, strlen(message)+1);
    }
    pthread_mutex_unlock(&member_lock);
    //update number of messages sent
    seq_number++;
}

void receive(int source, const char *message, int len) {
    assert(message[len-1] == 0);
    tagged_message *incoming_message = (tagged_message*)message;
    debugprintf("%d: %d\n", incoming_message->sender, incoming_message->seq_number);
    //deliver(source, message);
}

void mcast_join(int member) {

}
