#ifndef __CDATA_H__
#define __CDATA_H__

#include <stdint.h>

/*
 * Structures which describes message body
 */
enum {
    MSGTYPE_NONE = -1,
    MSGTYPE_WALK,
    MSGTYPE_SHOOT,
    MSGTYPE_CONNECT
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

struct msgtype_connect {
    uint8_t nick[NICK_MAX_LEN]; // null-terminated.
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
        struct msgtype_connect connect;
    } event;
};

/* Optimization for minimizing number of system calls;
 * client --| struct msg |--> server
 * server --| struct msg_batch |--> client
 * Thus we call socket_send() only one time
 * when sending difference between world's states.
 *
 * First byte - size, therefore size is limited about 255.
 */
#define MSGBATCH_INIT_SIZE 64
#define MSGBATCH_BYTES (sizeof(struct msg) * MSGBATCH_INIT_SIZE + sizeof(uint8_t))

enum msg_batch_enum_t {
    MSGBATCH_ERROR = 0,
    MSGBATCH_OK
};

struct msg_batch {
    uint8_t chunks[MSGBATCH_BYTES];
    uint16_t offset;
};

#define MSGBATCH_SIZE(b) ((b)->chunks[0])

#define MAX_PLAYERS 16

struct player {
    struct sockaddr_in addr;
    uint8_t nick[NICK_MAX_LEN];
};

#define TPS 5 /* Ticks per second. */

struct ticks {
    uint64_t offset;
    uint64_t count;
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
