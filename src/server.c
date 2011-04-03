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

#define MAX_PLAYERS 16

enum players_enum_t {
    PLAYERS_ERROR = 0,
    PLAYERS_OK
};

struct player {
    struct sockaddr_in *addr;
    struct msg_batch msgbatch;
    uint8_t player; /* slot's number. */
    uint8_t *nick;
    uint32_t seq;
    uint16_t pos_x;
    uint16_t pos_y;
};

struct players_slot {
    struct players_slot *next;
    struct players_slot *prev;
    struct player *p;
};

struct players_slots {
    struct players_slot *root;
    uint8_t count;
    /* Describes what slots are free and occupied.
       It is an array of pointers to slot,
       if it equals NULL than slot is free.
    */
    struct players_slot *slots[MAX_PLAYERS];
};

struct players_slots *players_init(void)
{
    struct players_slots *slots;
    
    slots = malloc(sizeof(struct players_slots));
    slots->root = NULL;
    slots->count = 0;
    memset(slots->slots, 0, MAX_PLAYERS * sizeof(struct players_slot *));

    return slots;
}

void players_free(struct players_slots *slots)
{
    struct players_slot *cslot, *nslot;

    nslot = slots->root;

    while(nslot != NULL) {
        cslot = nslot;
        nslot = nslot->next;

        free(cslot->p->addr);
        free(cslot->p->nick);
        free(cslot->p);
        free(cslot);
    }
    
    free(slots);
}

struct player *players_occupy(struct players_slots *slots, struct player *p)
{
    if(slots->count < MAX_PLAYERS - 1) {
        struct players_slot *oslot = slots->root;
        struct players_slot *new = malloc(sizeof(struct players_slot));
        struct players_slot *pslot = NULL;
        int i;

        memset(new, 0, sizeof(struct players_slot));
        slots->count++;

        while(oslot != NULL) {
            pslot = oslot;
            oslot = oslot->next;
        }
        
        oslot = new;
        oslot->prev = pslot;
        oslot->next = NULL;
        oslot->p = malloc(sizeof(struct player));
        memset(oslot->p, 0, sizeof(struct player));
        oslot->p->addr = malloc(sizeof(struct sockaddr_in));
        oslot->p->nick = malloc(sizeof(uint8_t) * NICK_MAX_LEN);
        memcpy(oslot->p->addr, p->addr, sizeof(struct sockaddr_in));
        strncpy((char *) oslot->p->nick, (char *) p->nick, NICK_MAX_LEN);

        if(slots->root == NULL) {
            slots->root = oslot;
        } else {
            pslot->next = oslot;
        }
        
        for(i = 0; i < MAX_PLAYERS; i++) {
            if(slots->slots[i] == NULL) {
                slots->slots[i] = oslot;
                oslot->p->player = (uint8_t) i;
                
                return oslot->p;
            }
        }
    }

    return NULL;
}

enum players_enum_t players_release(struct players_slots *slots, uint8_t player)
{
    if(slots->count > 0 && slots->slots[player] != NULL) {
        struct players_slot *cslot, *pslot, *nslot;
        slots->count--;
        
        cslot = slots->slots[player];
        nslot = cslot->next;
        pslot = cslot->prev;
        
        if(cslot == slots->root) {
            slots->root = nslot;
        } else {
            pslot->next = nslot;
        }

        if(nslot != NULL) {
            nslot->prev = pslot;
        }

        /* Make slot free. */
        free(cslot->p->addr);
        free(cslot->p->nick);
        free(cslot->p);
        free(cslot);
        
        slots->slots[player] = NULL;

        return PLAYERS_OK;
    }

    return PLAYERS_ERROR;
}

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
struct players_slots *players = NULL;
pthread_mutex_t msgqueue_mutex;
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
        pthread_mutex_lock(&msgqueue_mutex);
        if(msgqueue_push(msgqueue, qnode) == MSGQUEUE_ERROR) {
            fprintf(stderr, "server: msgqueue_push: couldn't push data into queue.\n");
        }
        pthread_mutex_unlock(&msgqueue_mutex);
    }
}

void event_disconnect_server(void)
{
    struct players_slot *slot = players->root;
    struct msg_batch msgbatch;
    struct msg msg;

    msg.type = MSGTYPE_DISCONNECT_SERVER;
    msg.event.disconnect_server.stub = 1;
    
    memset(&msgbatch, 0, sizeof(struct msg_batch));
    msg_batch_push(&msgbatch, &msg);
    while(slot != NULL) {
        struct player *p = slot->p;
        
        sendto(sd, msgbatch.chunks, msgbatch.offset + 1,
               0, (struct sockaddr *) p->addr, sizeof(struct sockaddr_in));

        slot = slot->next;
    }
}

void event_disconnect_client(struct msg_queue_node *qnode)
{
    struct players_slot *slot;
    struct msg_batch msgbatch;
    struct msg msg;

    msg.type = MSGTYPE_DISCONNECT_NOTIFY;
    if(players->slots[qnode->data->header.player] != NULL) {
        strncpy((char *) msg.event.disconnect_notify.nick,
                (char *) players->slots[qnode->data->header.player]->p->nick, NICK_MAX_LEN);
    }

    if(players_release(players, qnode->data->header.player) == PLAYERS_ERROR) {
        printf("Couldn't remove the player from slots: %u\n", qnode->data->header.player);
        
        return;
    }

    printf("Player has been disconnected: %s\n", msg.event.disconnect_notify.nick);
    
    memset(&msgbatch, 0, sizeof(struct msg_batch));
    msg_batch_push(&msgbatch, &msg);

    slot = players->root;
    while(slot != NULL) {
        struct player *p = slot->p;
        
        sendto(sd, msgbatch.chunks, msgbatch.offset + 1,
               0, (struct sockaddr *) p->addr, sizeof(struct sockaddr_in));

        slot = slot->next;
    }

}

void event_connect_ask(struct msg_queue_node *qnode)
{
    struct player player;
    struct player *newplayer;
    struct msg msg;
    
    player.addr = qnode->addr;
    player.nick = qnode->data->event.connect_ask.nick;

    /* We will send MSGTYPE_CONNECT_OK message's type to the client. */
    msg.type = MSGTYPE_CONNECT_OK;
    newplayer = players_occupy(players, &player);
    
    if(newplayer == NULL) {
        struct msg_batch msgbatch;
        
        printf("There are no free slots. Server maintains maximum %u players\n", MAX_PLAYERS);

        msg.event.connect_ok.ok = 0;

        memset(&msgbatch, 0, sizeof(struct msg_batch));
        msg_batch_push(&msgbatch, &msg);
        sendto(sd, msgbatch.chunks, msgbatch.offset + 1,
               0, (struct sockaddr *) qnode->addr, sizeof(struct sockaddr_in));
    } else {
        struct players_slot *slot = players->root;
        
        printf("Player has been connected with nick: %s, total players: %u\n",
               newplayer->nick, players->count);

        /* TODO: set seq. Set pos_x and pos_y. */
                    
        /* Set order number for the new player. */
        msg.header.seq = 0;
        msg.event.connect_ok.player = newplayer->player;
        msg.event.connect_ok.ok = 1;

        /* Push to the new player. */
        msg_batch_push(&(newplayer->msgbatch), &msg);
        /* Notify rest players about new player's connection. */
        memset(&msg, 0, sizeof(struct msg));
        msg.type = MSGTYPE_CONNECT_NOTIFY;
        strncpy((char *) msg.event.connect_notify.nick, (char *) newplayer->nick, NICK_MAX_LEN);
        while(slot != NULL) {
            if(slot->p != newplayer) {
                msg.header.seq = ++(slot->p->seq);
                msg_batch_push(&(slot->p->msgbatch), &msg);
            }
            slot = slot->next;
        }
    }
}
    
void *queue_mngr_func(void *arg)
{
    struct msg_queue_node *qnode;

    sleep(1);
    
    while("teh internetz exists") {
        struct players_slot *slot = players->root;
        
        pthread_cond_wait(&queue_mngr_cond, &msgqueue_mutex);

        /* Refresh msgbatch for each player. */
        while(slot != NULL) {
            memset(&(slot->p->msgbatch), 0, sizeof(struct msg_batch));
            slot = slot->next;
        }
        
        /* Handle messages(events). */
        while((qnode = msgqueue_pop(msgqueue)) != NULL) {
            /* TODO: check seq. */

            switch(qnode->data->type) {
            case MSGTYPE_CONNECT_ASK:
                event_connect_ask(qnode);                
                break;
            case MSGTYPE_DISCONNECT_CLIENT:
                event_disconnect_client(qnode);
                break;
            default:
                printf("Unknown event\n");
                break;
            }
        }

        /* Send diff to each player. */
        slot = players->root;
        while(slot != NULL) {
            struct player *p = slot->p;
            
            if(MSGBATCH_SIZE(&(p->msgbatch)) > 0) {
                sendto(sd, p->msgbatch.chunks, p->msgbatch.offset + 1,
                       0, (struct sockaddr *) p->addr, sizeof(struct sockaddr_in));
            }
            
            slot = slot->next;
        }
        
        pthread_mutex_unlock(&msgqueue_mutex);
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
    
    event_disconnect_server();
    
    close(sd);
    msgqueue_free(msgqueue);
    players_free(players);
    pthread_attr_destroy(&common_attr);
    pthread_mutex_destroy(&msgqueue_mutex);
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
    
    pthread_mutex_init(&msgqueue_mutex, NULL);
    pthread_cond_init(&queue_mngr_cond, NULL);
    
    pthread_attr_init(&common_attr);
    pthread_attr_setdetachstate(&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&recv_mngr_thread, &common_attr, recv_mngr_func, NULL);
    pthread_create(&queue_mngr_thread, &common_attr, queue_mngr_func, NULL);

    quit(0);
    
    return 0;
}
