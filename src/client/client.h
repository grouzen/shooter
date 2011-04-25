#ifndef __CLIENT_H__
#define __CLIENT_H__

#define MSGQUEUE_INIT_SIZE (MSGBATCH_INIT_SIZE * 3)

struct msg_queue {
    struct msg *data[MSGQUEUE_INIT_SIZE];
    ssize_t top;
};

struct msg_queue *msgqueue_init(void);
void msgqueue_free(struct msg_queue*);
enum msg_queue_enum_t msgqueue_push(struct msg_queue*, struct msg*);
struct msg *msgqueue_pop(struct msg_queue*);
void quit(int);
void send_event(struct msg*);
void event_disconnect_client(void);
void event_connect_ask(void);
void event_disconnect_server(struct msg*);
void event_connect_ok(struct msg*);
void event_connect_notify(struct msg*);
void event_disconnect_notify(struct msg*);
void *recv_mngr_func(void*);
void *queue_mngr_func(void*);
void *ui_mngr_func(void*);


#endif
