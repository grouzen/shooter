#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "cdata.h"

#define MAX_RECVBUFLEN 64
#define MSGQUEUE_INIT_SIZE 64

struct msg_queue {
    struct msg *data[MSGQUEUE_INIT_SIZE];
    ssize_t top;
};

enum msg_queue_enum_t {
    MSGQUEUE_ERROR = 0,
    MSGQUEUE_OK
};

void msgqueue_init(struct msg_queue *q)
{
    int i;

    q = malloc(sizeof(struct msg_queue));
    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        q->data[i] = malloc(sizeof(struct msg));
    }
    memset(q->data, 0, sizeof(struct msg) * MSGQUEUE_INIT_SIZE);

    q->top = 0;
}

void msgqueue_clean(struct msg_queue *q)
{
    int i;

    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        free(q->data[i]);
    }
    
   free(q);
}

msg_queue_enum_t msgqueue_push(struct msg_queue *q, uint8_t *buf)
{
    if(msgqueue->top < MSGQUEUE_INIT_SIZE - 1) {
        msg_unpack(buf, q->top++);
        return MSGQUEUE_OK;
    }

    return MSGQUEUE_ERROR;
}

struct msg *msgqueue_pop(struct msg_queue *q)
{
    if(msgqueue->top) {
        return msgqueue->data[msgqueue->top--];
    }

    return NULL;
}

struct msg_queue *msgqueue = NULL;
struct player players[MAX_PLAYERS];
pthread_mutex_t queue_mngr_mutex;
pthread_cond_t queue_mngr_cond;
pthread_t recv_mngr_thread, queue_mngr_thread;
uint64_t queue_mngr_ticks;

/* This thread must do the only one
 * thing - recieve messages from many
 * clients and then push them to msgqueue.
 */
void *recv_mngr_func(void *arg)
{
    int sd;
    uint8_t buf[MAX_RECVBUFLEN];
    ssize_t recvbytes;
    struct sockaddr_in server_addr, client_addr;
    struct socklen_t client_addr_len;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6006);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sd = socket(server_addr.sin_family, SOCK_DGRAM, 0);
    if(sd < 0) {
        perror("server: socket");
        exit(EXIT_FAILURE);
    }

    if(bind(sd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("server: bind");
        exit(EXIT_FAILURE);
    }

    while("hope is not dead") {
        if(ticks_get() - queue_mngr_ticks > 1000 / TPS) {
            queue_mngr_ticks = ticks_get();
            pthread_cond_signal(&queue_mngr_cond);
        }
        
        if((recvbytes = recvfrom(sd, buf, MAX_RECVBUFLEN, 0,
                                 (struct sockaddr *) &server_addr,
                                 &client_addr_len)) < 0) {
            perror("server: recvfrom");
        }
        
        pthread_mutex_lock(&queue_mngr_mutex);
        if(msgqueue_push(msgqueue, buf) == MSGQUEUE_ERROR) {
            fprintf(stderr, "server: recvfrom: couldn't push data into queue.\n");
        }
        pthread_mutex_unlock(&queue_mngr_mutex);
    }

    close(sd);
}


void *queue_mngr_func(void *arg)
{
    while("teh internetz exists") {
        pthread_cond_wait(&queue_mngr_cond, &queue_mngr_mutex);

        /* Handle messages. */
        
        pthread_mutex_unlock(&queue_mngr_mutex);
    }
}

/* TODO: write function sync_mngr_func()
 * which will be check seq number of
 * each client and if necessary send
 * to him current world state.
 */
int main(int argc, char **argv)
{
    pthread_attr_t common_attr;

    queue_mngr_ticks = ticks_start();
    /* Init once messages queue. */
    msgqueue_init(msgqueue);
    pthread_mutex_init(&queue_mngr_mutex, NULL);
    pthread_cond_init(&queue_mngr_cond, NULL);
    
    pthread_attr_init(&common_attr);
    pthread_attr_setdetachstate(&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&recv_mngr_thread, &common_attr, recv_mngr_func, NULL);
    pthread_create(&queue_mngr_thread, &common_attr, queue_mngr_func, NULL);

    pthread_join(recv_mngr_thread);
    pthread_join(queue_mngr_thread);
    msgqueue_clean(msgqueue);
    pthread_attr_destroy(&common_attr);
    pthread_mutex_destroy(&queue_mngr_mutex);
    pthread_cond_destroy(&queue_mngr_cond);
    pthread_exit(NULL);
    
    return 0;
}
