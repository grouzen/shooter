#ifndef __CDATA_H__
#define __CDATA_H__

#include <stdint.h>

/*
 * Structures which describes message body
 */
enum {
    MSGTYPE_NONE = -1,
    MSGTYPE_WALK,
    MSGTYPE_SHOOT
};

struct msgtype_walk {
    uint8_t direction;
    uint16_t pos_x;
    uint16_t pos_y;
};

struct msgtype_shoot {
    uint8_t gun_type;
};

struct msgtype_connect {
    
}
    
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
};

#define TPS 5 /* Ticks per second. */

#endif
