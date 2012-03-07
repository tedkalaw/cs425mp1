#include "mp1.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

int *mcast_members;
int mcast_num_members;
int my_id;
static int mcast_mem_alloc;
pthread_mutex_t member_lock = PTHREAD_MUTEX_INITIALIZER;

static int sock;

pthread_t receive_thread;


/* Add a potential new member to the list */
void new_member(int member) {
    int i;
    pthread_mutex_lock(&member_lock);
    for (i = 0; i < mcast_num_members; i++) {
        if (member == mcast_members[i])
            break;
    }
    if (i == mcast_num_members) { /* really is a new member */
        fprintf(stderr, "New group member: %d\n", member);
        if (mcast_num_members == mcast_mem_alloc) { /* make sure there's enough space */
            mcast_mem_alloc *= 2;
            mcast_members = realloc(mcast_members, mcast_mem_alloc * sizeof(mcast_members[0]));
            if (mcast_members == NULL) {
                perror("realloc");
                exit(1);
            }
        }
        mcast_members[mcast_num_members++] = member;
        pthread_mutex_unlock(&member_lock);
        mcast_join(member);
    } else {
        pthread_mutex_unlock(&member_lock);
    }
}

void *receive_thread_main(void *discard) {
    struct sockaddr_in fromaddr;
    socklen_t len;
    int nbytes;
    char buf[1000];

    for (;;) {
        int source;
        len = sizeof(fromaddr);
        nbytes = recvfrom(sock,buf,1000,0,(struct sockaddr *)&fromaddr,&len);
        if (nbytes < 0) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            exit(1);
        }

        source = ntohs(fromaddr.sin_port);

        fprintf(stderr, "Received %d bytes from %d\n", nbytes, source);

        if (nbytes == 0) {
            /* A first message from someone */
            new_member(source);
        } else {
            receive(source, buf, nbytes);
        }
    }
}


void unicast_init(void) {
    struct sockaddr_in servaddr;
    socklen_t len;
    int fd;
    char buf[128];
    int i,n;

    /* set up UDP listening socket */
    sock = socket(AF_INET,SOCK_DGRAM,0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port=htons(0);     /* let the operating system choose a port for us */

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        exit(1);
    }

    /* obtain a port number */
    len = sizeof(servaddr);
    if (getsockname(sock, (struct sockaddr *) &servaddr, &len) < 0) {
        perror("getsockname");
        exit(1);
    }

    my_id = ntohs(servaddr.sin_port);
    fprintf(stderr, "Our port number: %d\n", my_id);

    /* allocate initial member arrary */
    mcast_mem_alloc = 16;
    mcast_members = malloc(sizeof(mcast_members[0]) * mcast_mem_alloc);
    if (mcast_members == NULL) {
        perror("malloc");
        exit(1);
    }
    mcast_num_members = 0;

    /* start receiver thread to make sure we obtain announcements from anyone who tries to contact us */
    if (pthread_create(&receive_thread, NULL, &receive_thread_main, NULL) != 0) {
        fprintf(stderr, "Error in pthread_create\n");
        exit(1);
    }

    /* add self to group file */

    fd = open(GROUP_FILE, O_WRONLY | O_APPEND | O_CREAT, 0600); /* read-write by the user */
    if (fd < 0) {
        perror("open(group file, 'a')");
        exit(1);
    }

    if (write(fd, &my_id, sizeof(my_id)) != sizeof(my_id)) {
        perror("write");
        exit(1);
    }
    close(fd);


    /* now read in the group file */
    fd = open(GROUP_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open(group file, 'r')");
        exit(1);
    }

     do {
         int *member;
         n = read(fd, buf, sizeof(buf));
         if (n < 0) {
             break;
         }

         for (member = (int *) buf; ((char *) member) - buf < n; member++) {
             new_member(*member);
         }
     } while (n == sizeof(buf));

     close(fd);

     /* announce ourselves to everyone */

     pthread_mutex_lock(&member_lock);
     for (i = 0; i < mcast_num_members; i++) {
         struct sockaddr_in destaddr;

         if (mcast_members[i] == my_id)
             continue;  /* don't announce to myself */

         memset(&destaddr, 0, sizeof(destaddr));
         destaddr.sin_family = AF_INET;
         destaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
         destaddr.sin_port = htons(mcast_members[i]);

         sendto(sock, buf, 0, 0, (struct sockaddr *) &destaddr, sizeof(destaddr));
     }
     pthread_mutex_unlock(&member_lock);
}

struct message {
    struct message *next;
    double delivery_time;
    char *message;
    int len;
    int dest;
};

struct message *head = NULL;

void insert_message(struct message *new) {
    struct message **p = &head;

    while (*p && (*p)->delivery_time < new->delivery_time)
        p = &(*p)->next;

    new->next = *p;
    *p = new;
}

void catch_alarm(int sig);

void setsignal(void) {
    struct sigaction action;

    action.sa_handler = catch_alarm;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGALRM, &action, NULL);
}

void setalarm(useconds_t seconds) {
    struct itimerval timer;
    timer.it_value.tv_usec = seconds % 1000000;
    timer.it_value.tv_sec = seconds / 1000000;
    timer.it_interval.tv_usec = 0;
    timer.it_interval.tv_sec = 0;

    setitimer(ITIMER_REAL, &timer, NULL);
}


void catch_alarm(int sig) {
    struct timeval tim;
    double now;

    gettimeofday(&tim, NULL);
    now = tim.tv_sec * 1000000.0 + tim.tv_usec;

    while (head && head->delivery_time <= now) {
        struct message *temp;
        struct sockaddr_in destaddr;

        memset(&destaddr, 0, sizeof(destaddr));
        destaddr.sin_family = AF_INET;
        destaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        destaddr.sin_port = htons(head->dest);

        sendto(sock, head->message, head->len, 0, (struct sockaddr *) &destaddr, sizeof(destaddr));

        temp = head;
        free(head->message);
        head = head->next;
        free(temp);
    }

    if (head) {
	setsignal();
        setalarm((useconds_t) head->delivery_time - now);
    }
}



void usend(int dest, const char *message, int len) {
    //double delay = MINDELAY + (MAXDELAY-MINDELAY) * ((double) rand()) / RAND_MAX;
    double delay = 100000;
    struct timeval tim;
    struct message *new;

    if (rand() < P_DROP * RAND_MAX) {
        fprintf(stderr, "Message to %d dropped\n", dest);
        return;
    } else {
        fprintf(stderr, "Message to %d delayed by %.3fms\n", dest, delay / 1000);
    }

    new = malloc(sizeof(struct message));
    new->message = malloc(len);
    memcpy(new->message, message, len);
    new->len = len;
    new->dest = dest;

    gettimeofday(&tim, NULL);
    new->delivery_time = tim.tv_sec * 1000000.0  + tim.tv_usec + delay;

    insert_message(new);

    if (new == head) {
        setsignal();
        setalarm((useconds_t) delay);
    }
}

