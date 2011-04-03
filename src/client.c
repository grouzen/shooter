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

#include "cdata.h"

#ifdef UI_BACKEND_NCURSES
#include "ui/ncurses/backend.h"
#elif UI_BACKEND_SDL
#include "ui/sdl/backend.h"
#endif

#define MSGQUEUE_INIT_SIZE (MSGBATCH_INIT_SIZE * 3)

struct msg_queue {
    struct msg *data[MSGQUEUE_INIT_SIZE];
    ssize_t top;
};

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

pthread_t ui_mngr_thread, recv_mngr_thread;
pthread_attr_t common_attr;
struct ticks *ui_mngr_ticks;
struct sockaddr_in server_addr;
struct msg_queue *msgqueue = NULL;
uint8_t player; /* Your slot number in `struct players`. */
uint32_t seq; /* Your seq number in `struct players`. */
int sd;

void send_event(struct msg *m)
{
    uint8_t buf[sizeof(struct msg)];
    
    m->header.player = player;
    m->header.seq = seq;

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

/* Init ui_backend. */
void *ui_mngr_func(void *arg)
{
    ui_init();
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

        while((chunk = msg_batch_pop(&msgbatch)) != NULL) {
            struct msg m;
            
            msg_unpack(chunk, &m);
            if(msgqueue_push(msgqueue, &m) == MSGQUEUE_ERROR) {
                fprintf(stderr, "client: msgqueue_push: couldn't push data into queue.\n");
            }
        }
    }

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
    pthread_attr_destroy(&common_attr);
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    // TODO: get the address from argv or config, or other place
    struct hostent *host = gethostbyname((char *) "127.0.0.1");

    signal(SIGINT, quit);
    signal(SIGHUP, quit);
    signal(SIGQUIT, quit);

    msgqueue = msgqueue_init();
    
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

    pthread_attr_init(&common_attr);
    pthread_attr_setdetachstate(&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&recv_mngr_thread, &common_attr, recv_mngr_func, NULL);
    pthread_create(&ui_mngr_thread, &common_attr, ui_mngr_func, NULL);

    quit(0);
    
    return 0;
}
