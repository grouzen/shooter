#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "cdata.h"

#define MSGQUEUE_INIT_SIZE 64

struct msg_queue_node {
    struct msg *data;
    struct sockaddr_in *addr;
};

struct msg_queue {
    struct msg_queue_node *nodes[MSGQUEUE_INIT_SIZE];
    ssize_t top;
};

struct msg_queue *msgqueue_init(void)
{
    struct msg_queue *q;
    int i;

    q = malloc(sizeof(struct msg_queue));
    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        q->nodes[i] = malloc(sizeof(struct msg_queue_node));
        q->nodes[i]->data = malloc(sizeof(struct msg));
        q->nodes[i]->addr = malloc(sizeof(struct sockaddr_in));
    }

    q->top = -1;

    return q;
}

void msgqueue_free(struct msg_queue *q)
{
    int i;

    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        free(q->nodes[i]->data);
        free(q->nodes[i]->addr);
        free(q->nodes[i]);
    }
    
    free(q);
}

enum msg_queue_enum_t msgqueue_push(struct msg_queue *q, struct msg_queue_node *qnode)
{
    if(q->top < MSGQUEUE_INIT_SIZE - 1) {
        q->top++;
        memcpy(q->nodes[q->top]->addr, qnode->addr, sizeof(struct sockaddr_in));
        memcpy(q->nodes[q->top]->data, qnode->data, sizeof(struct msg));
        
        return MSGQUEUE_OK;
    }

    return MSGQUEUE_ERROR;
}

struct msg_queue_node *msgqueue_pop(struct msg_queue *q)
{
    if(q->top >= 0) {
        return q->nodes[q->top--];
    }

    return NULL;
}

struct msg_queue *msgqueue = NULL;
struct players *players = NULL;
pthread_mutex_t queue_mngr_mutex;
pthread_cond_t queue_mngr_cond;
pthread_t recv_mngr_thread, queue_mngr_thread;
pthread_attr_t common_attr;
struct ticks *queue_mngr_ticks;
int sd;

/* This thread must do the only one
 * thing - recieves messages from many
 * clients and then pushs them to msgqueue.
 */
void *recv_mngr_func(void *arg)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct msg_queue_node *qnode;
        
    qnode = malloc(sizeof(struct msg_queue_node));
    qnode->data = malloc(sizeof(struct msg));
    qnode->addr = malloc(sizeof(struct sockaddr_in));

    queue_mngr_ticks = ticks_start();
    
    while("hope is not dead") {
        uint8_t buf[sizeof(struct msg)];
        
        if(ticks_get_diff(queue_mngr_ticks) > 1000 / FPS) {
            ticks_update(queue_mngr_ticks);
            pthread_cond_signal(&queue_mngr_cond);
        }
        
        if(recvfrom(sd, buf, sizeof(struct msg), 0,
                    (struct sockaddr *) &client_addr, &client_addr_len) < 0) {
            perror("server: recvfrom");
            continue;
        }
        
        msg_unpack(buf, qnode->data);
        qnode->addr = &client_addr;
        pthread_mutex_lock(&queue_mngr_mutex);
        if(msgqueue_push(msgqueue, qnode) == MSGQUEUE_ERROR) {
            fprintf(stderr, "server: msgqueue_push: couldn't push data into queue.\n");
        }
        pthread_mutex_unlock(&queue_mngr_mutex);
    }
}

/* TODO: remove this fucking shit.
enum event_enum_t {
    EVENT_NEXT = 0,
    EVENT_FURTHER
};
*/

void event_connect_ask(struct msg_queue_node *qnode)
{
    struct player player;
    struct msg msg;
    int i;
    
    player.addr = qnode->addr;
    player.nick = qnode->data->event.connect_ask.nick;

    /* We will send MSGTYPE_CONNECT_OK message's type to the client. */
    msg.type = MSGTYPE_CONNECT_OK;
                
    if(players_occupy(players, &player) == PLAYERS_ERROR) {
        struct msg_batch msgbatch;
                    
        printf("There are no free slots. Server maintains %d players\n", MAX_PLAYERS);

        msg.event.connect_ok.ok = 0;
                    
        msg_batch_push(&msgbatch, &msg);
        sendto(sd, msgbatch.chunks, msgbatch.offset + 1, 0, (struct sockaddr *) qnode->addr, sizeof(struct sockaddr_in));
    } else {
        printf("Player has been connected with nick: %s, total players = %u\n",
               players->slots[players->count - 1].nick, players->count);

        /* TODO: set seq. Set pos_x and pos_y. */
                    
        /* Set order number for the new player. */
        msg.header.player = players->count;
        msg.event.connect_ok.ok = 1;

        /* Push to the new player. */
        msg_batch_push(&(players->slots[players->count - 1].msgbatch), &msg);
        /* Notify rest players about new player's connection. */
        memset(&msg, 0, sizeof(struct msg));
        msg.type = MSGTYPE_CONNECT_NOTIFY;
        memcpy(msg.event.connect_notify.nick, players->slots[players->count - 1].nick, NICK_MAX_LEN);
        for(i = 0; i < players->count - 1; i++) {
            msg_batch_push(&(players->slots[i].msgbatch), &msg);
        }
    }
}
    
void *queue_mngr_func(void *arg)
{
    struct msg_queue_node *qnode;

    sleep(1);
    
    while("teh internetz exists") {
        int i;
        
        pthread_cond_wait(&queue_mngr_cond, &queue_mngr_mutex);

        /* Refresh msgbatch for each player. */
        for(i = 0; i < players->count; i++) {
            memset(&(players->slots[i].msgbatch), 0, sizeof(struct msg_batch));
        }
        
        /* Handle messages(events). */
        while((qnode = msgqueue_pop(msgqueue)) != NULL) {
            /* TODO: check seq. */

            /* TODO: for each case to write the function. */
            switch(qnode->data->type) {
            case MSGTYPE_CONNECT_ASK:
                event_connect_ask(qnode);
                
                break;
            default:
                printf("Unknown event\n");
                break;
            }
        }
        
        /* Send diff to each player. */
        for(i = 0; i < players->count; i++) {
            struct player *p = &(players->slots[i]);
            
            if(MSGBATCH_SIZE(&(p->msgbatch)) > 0) {
                sendto(sd, p->msgbatch.chunks, p->msgbatch.offset + 1, 0, (struct sockaddr *) p->addr, sizeof(struct sockaddr_in));
            }
        }
        
        pthread_mutex_unlock(&queue_mngr_mutex);
    }
}

void quit(int signum)
{
    if(signum > 0) {
        pthread_cancel(recv_mngr_thread);
        pthread_cancel(queue_mngr_thread);
    }
    
    pthread_join(recv_mngr_thread, NULL);
    pthread_join(queue_mngr_thread, NULL);
    close(sd);
    msgqueue_free(msgqueue);
    players_free(players);
    pthread_attr_destroy(&common_attr);
    pthread_mutex_destroy(&queue_mngr_mutex);
    pthread_cond_destroy(&queue_mngr_cond);
    pthread_exit(NULL);
}

/* TODO: write function sync_mngr_func()
 * which will be check seq number of
 * each client and if necessary send
 * to him current world state.
 */
int main(int argc, char **argv)
{
    struct sockaddr_in server_addr;
    
    signal(SIGINT, quit);
    signal(SIGHUP, quit);
    signal(SIGQUIT, quit);
     
    msgqueue = msgqueue_init();
    players = players_init();

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
    
    pthread_mutex_init(&queue_mngr_mutex, NULL);
    pthread_cond_init(&queue_mngr_cond, NULL);
    
    pthread_attr_init(&common_attr);
    pthread_attr_setdetachstate(&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&recv_mngr_thread, &common_attr, recv_mngr_func, NULL);
    pthread_create(&queue_mngr_thread, &common_attr, queue_mngr_func, NULL);

    quit(0);
    
    return 0;
}
