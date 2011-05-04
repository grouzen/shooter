#ifndef __CDATA_H__
#define __CDATA_H__

#include <stdint.h>

/* Must be less than 25 because terminal's geometry is 80x25. */
#define PLAYER_VIEWPORT_WIDTH 21
#define PLAYER_VIEWPORT_HEIGHT 21

#define MAP_EMPTY ' '
#define MAP_WALL '#'

#define MAP_NAME_MAX_LEN 16

struct map {
    /* On client part if name doesn't set,
       means that map is not loaded yet.
    */
    uint8_t name[MAP_NAME_MAX_LEN];
    uint8_t **objs;
    uint16_t width;
    uint16_t height;
};

/*
 * Structures which describes message body
 */
enum {
    //MSGTYPE_NONE = -1,
    MSGTYPE_WALK = 0,
    MSGTYPE_PLAYER_POSITION,
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
};

struct msgtype_player_position {
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
    uint8_t id;
    uint8_t mapname[MAP_NAME_MAX_LEN];
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
    uint8_t id;
};

struct msg {
    struct msg_header header;
    uint8_t type;
    union {
        struct msgtype_walk walk;
        struct msgtype_player_position player_position;
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

#define WEAPON_NAME_MAX_LEN 8

struct weapon {
    uint8_t name[WEAPON_NAME_MAX_LEN];
    uint8_t damage_max;
    uint8_t damage_min;
    uint8_t bullet_speed;
    uint8_t bullet_distance;
    uint8_t bullet_count;
};

#define WEAPON_SLOTS_MAX 9

struct weapon_slots {
    /* Bit array where 1 means that weapon exists. */
    uint8_t slots[WEAPON_SLOTS_MAX];
    /* Number of bullets for each weapon. */
    uint8_t bullets[WEAPON_SLOTS_MAX];
    /* Selected weapon. */
    uint8_t current;
};

extern struct weapon weapons[];

enum {
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
    DIRECTION_UP,
    DIRECTION_DOWN
};

struct player {
#ifdef _SERVER_
    struct sockaddr_in *addr;
    struct msg_batch msgbatch;
#endif
    uint8_t id; /* slot's number. */
    uint8_t *nick;
    uint32_t seq;
    uint8_t direction;
    uint16_t pos_x;
    uint16_t pos_y;
    uint8_t hp;
    struct weapon_slots weapons;
};

#define FPS 10

struct ticks {
    uint64_t offset;
    uint64_t count;
};

/* For now I've different implementation
   of the msgqueue in client and server.
*/
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
struct player *player_init(void);
void player_free(struct player*);
struct map *map_load(uint8_t*);
void map_unload(struct map*);

#endif
