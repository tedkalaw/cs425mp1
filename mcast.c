#include <string.h>
#include <assert.h>

#include "mp1.h"

void multicast_init(void) {
    unicast_init();
}

/* Basic multicast implementation */
void multicast(const char *message) {
    int i;

    pthread_mutex_lock(&member_lock);
    for (i = 0; i < mcast_num_members; i++) {
        usend(mcast_members[i], message, strlen(message)+1);
    }
    pthread_mutex_unlock(&member_lock);
}

void receive(int source, const char *message, int len) {
    assert(message[len-1] == 0);
    deliver(source, message);
}

void mcast_join(int member) {

}
