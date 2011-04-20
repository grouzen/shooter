#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include "../cdata.h"
#include "ui/backend.h"
#include "client.h"

pthread_t ui_mngr_thread, recv_mngr_thread;
pthread_attr_t common_attr;
pthread_mutex_t msgqueue_mutex;
pthread_cond_t queue_mngr_cond;
struct ticks *ui_mngr_ticks;
struct sockaddr_in server_addr;
struct msg_queue *msgqueue = NULL;
struct player *player = NULL;
struct map *map = NULL;
int sd;

struct msg_queue *msgqueue_init(void)
{
    struct msg_queue *q;
    int i;

    q = malloc(sizeof(struct msg_queue));
    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        q->data[i] = malloc(sizeof(struct msg));
    }

    q->top = -1;

    return q;
}

void msgqueue_free(struct msg_queue *q)
{
    int i;

    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        free(q->data[i]);
    }

    free(q);
}

enum msg_queue_enum_t msgqueue_push(struct msg_queue *q, struct msg *m)
{
    if(q->top < MSGQUEUE_INIT_SIZE - 1) {
        q->top++;
        memcpy(q->data[q->top], m, sizeof(struct msg));

        return MSGQUEUE_OK;
    }

    return MSGQUEUE_ERROR;
}

struct msg *msgqueue_pop(struct msg_queue *q)
{
    if(q->top >= 0) {
        return q->data[q->top--];
    }

    return NULL;
}

void quit(int signum)
{    
    if(signum > 0) {
        pthread_cancel(recv_mngr_thread);
        pthread_cancel(ui_mngr_thread);
    }
    
    pthread_join(recv_mngr_thread, NULL);
    pthread_join(ui_mngr_thread, NULL);

    event_disconnect_client();
    
    close(sd);
    msgqueue_free(msgqueue);
    player_free(player);
    pthread_mutex_destroy(&msgqueue_mutex);
    pthread_cond_destroy(&queue_mngr_cond);
    pthread_attr_destroy(&common_attr);
    pthread_exit(NULL);
}

void send_event(struct msg *m)
{
    uint8_t buf[sizeof(struct msg)];
    
    m->header.id = player->id;
    m->header.seq = player->seq;

    msg_pack(m, buf);
    
    sendto(sd, buf, sizeof(struct msg),
           0, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in));
}

void event_disconnect_client(void)
{
    struct msg msg;

    msg.type = MSGTYPE_DISCONNECT_CLIENT;
    msg.event.disconnect_client.stub = 1;

    send_event(&msg);
}

void event_connect_ask(void)
{
    struct msg msg;

    msg.type = MSGTYPE_CONNECT_ASK;
    /* TODO: get nick from config or args. */
    strncpy((char *) msg.event.connect_ask.nick, "somenick", NICK_MAX_LEN);

    send_event(&msg);
}

/* Init ui_backend. */
void *ui_mngr_func(void *arg)
{
    if(ui_init() == UI_ERROR) {
        fprintf(stderr, "UI couldn't init.\n");
    }
    
    quit(1);
}

void *recv_mngr_func(void *arg)
{
    ssize_t recvbytes;
    
    while("zombies walk") {
        uint8_t buf[sizeof(struct msg_batch)];
        struct msg_batch msgbatch;
        uint8_t *chunk;
        
        if((recvbytes = recvfrom(sd, buf, sizeof(struct msg_batch),
                                 0, NULL, NULL)) < 0) {
            perror("client: recvfrom");
            continue;
        }
        
        msgbatch.offset = (buf[0] * sizeof(struct msg));
        memcpy(msgbatch.chunks, buf, msgbatch.offset + 1);

        pthread_mutex_lock(&msgqueue_mutex);
        while((chunk = msg_batch_pop(&msgbatch)) != NULL) {
            struct msg m;
            
            msg_unpack(chunk, &m);
            if(msgqueue_push(msgqueue, &m) == MSGQUEUE_ERROR) {
                fprintf(stderr, "client: msgqueue_push: couldn't push data into queue.\n");
            }
        }
        pthread_mutex_unlock(&msgqueue_mutex);
    }

}

void *queue_mngr_func(void *arg)
{
    struct msg *m;
    
    sleep(1);

    while(1) {
        pthread_cond_wait(&queue_mngr_cond, &msgqueue_mutex);
        
        while((m = msgqueue_pop(msgqueue)) != NULL) {
            /* TODO: just fucking do it!. */
        }

        pthread_mutex_unlock(&msgqueue_mutex);
    }
}

int main(int argc, char **argv)
{
    // TODO: get the address from argv or config, or other place
    struct hostent *host = gethostbyname((char *) "127.0.0.1");

    signal(SIGINT, quit);
    signal(SIGHUP, quit);
    signal(SIGQUIT, quit);

    msgqueue = msgqueue_init();
    player = player_init();
    
    ui_mngr_ticks = ticks_start();
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6006);
    server_addr.sin_addr = *((struct in_addr *) host->h_addr);

    sd = socket(server_addr.sin_family, SOCK_DGRAM, 0);
    if(sd < 0) {
        perror("client: socket");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&msgqueue_mutex, NULL);
    pthread_cond_init(&queue_mngr_cond, NULL);
    
    pthread_attr_init(&common_attr);
    pthread_attr_setdetachstate(&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&recv_mngr_thread, &common_attr, recv_mngr_func, NULL);
    pthread_create(&ui_mngr_thread, &common_attr, ui_mngr_func, NULL);

    event_connect_ask();
    
    quit(0);
    
    return 0;
}
