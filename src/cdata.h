#ifndef __CDATA_H__
#define __CDATA_H__

#include <stdint.h>

/*
 * Structures which describes message body
 */
enum {
    //MSGTYPE_NONE = -1,
    MSGTYPE_WALK = 0,
    MSGTYPE_SHOOT,
    MSGTYPE_CONNECT_ASK,
    MSGTYPE_CONNECT_OK,
    MSGTYPE_CONNECT_NOTIFY,
    MSGTYPE_DISCONNECT_SERVER,
    MSGTYPE_DISCONNECT_CLIENT,
    MSGTYPE_DISCONNECT_NOTIFY
};

struct msgtype_walk {
    uint8_t direction;
    uint16_t pos_x;
    uint16_t pos_y;
};

struct msgtype_shoot {
    uint8_t gun_type;
};

#define NICK_MAX_LEN 16

struct msgtype_connect_ask {
    uint8_t nick[NICK_MAX_LEN]; // null-terminated.
};

struct msgtype_connect_ok {
    uint8_t ok; // > 0 - ok.
    uint8_t player;
};

struct msgtype_connect_notify {
    /* TODO: set postition and so on. */
    uint8_t nick[NICK_MAX_LEN];
};

struct msgtype_disconnect_server {
    uint8_t stub;
};

struct msgtype_disconnect_client {
    uint8_t stub;
};

struct msgtype_disconnect_notify {
    uint8_t nick[NICK_MAX_LEN];
};

/*
 * General message structures
 */
struct msg_header {
    uint32_t seq;
    uint8_t player;
};

struct msg {
    struct msg_header header;
    uint8_t type;
    union {
        struct msgtype_walk walk;
        struct msgtype_shoot shoot;
        struct msgtype_connect_ask connect_ask;
        struct msgtype_connect_ok connect_ok;
        struct msgtype_connect_notify connect_notify;
        struct msgtype_disconnect_server disconnect_server;
        struct msgtype_disconnect_client disconnect_client;
        struct msgtype_disconnect_notify disconnect_notify;
    } event;
};

/* msg_batch:
 * Optimization for minimizing number of system calls;
 * client --| struct msg |--> server
 * server --| struct msg_batch |--> client
 * Thus we call socket_send() only one time
 * when sending difference between world's states.
 *
 * First byte of the chunks array is size,
 * therefore `struct msg_batch` can contains maximum 255 chunks.
 */
#define MSGBATCH_INIT_SIZE 255
#define MSGBATCH_BYTES (sizeof(struct msg) * MSGBATCH_INIT_SIZE + sizeof(uint8_t))

enum msg_batch_enum_t {
    MSGBATCH_ERROR = 0,
    MSGBATCH_OK
};

struct msg_batch {
    uint8_t chunks[MSGBATCH_BYTES];
    /* offset describes current offset in bytes
       from the beginning of the chunks array
       without first byte which describes number
       of occupied chunks.
    */
    uint16_t offset;
};

#define MSGBATCH_SIZE(b) ((b)->chunks[0])

#define PLAYER_VIEWPORT_WIDTH 30
#define PLAYER_VIEWPORT_HEIGHT 30

#define FPS 10

struct ticks {
    uint64_t offset;
    uint64_t count;
};

enum msg_queue_enum_t {
    MSGQUEUE_ERROR = 0,
    MSGQUEUE_OK
};

void msg_pack(struct msg*, uint8_t*);
void msg_unpack(uint8_t*, struct msg*);
enum msg_batch_enum_t msg_batch_push(struct msg_batch*, struct msg*);
uint8_t *msg_batch_pop(struct msg_batch*);
uint64_t ticks_get(void);
struct ticks *ticks_start(void);
void ticks_update(struct ticks*);
void ticks_finish(struct ticks*);
uint64_t ticks_get_diff(struct ticks*);

#endif
