/* Copyright (c) 2011 Michael Nedokushev <grouzen.hexy@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __CDATA_H__
#define __CDATA_H__

#include <stdint.h>

#ifdef _DEBUG_
#define DEBUG(format, ...)                              \
    do {                                                \
        printf("[ DEBUG ]: " format, ##__VA_ARGS__);    \
    } while(0)
#else
#define DEBUG(format, ...) do { } while(0)
#endif

#define INFO(format, ...)                                        \
    do {                                                         \
        printf("[ INFO ]: " format, ##__VA_ARGS__);              \
    } while(0)

#define WARN(format, ...)                                        \
    do {                                                         \
        fprintf(stderr, "[ WARN ]: " format, ##__VA_ARGS__);     \
    } while(0)

/* Must be less than 25 because terminal's geometry is 80x25
 * minus status lines on top and bottom of screen.
 */
#define PLAYER_VIEWPORT_WIDTH 21
#define PLAYER_VIEWPORT_HEIGHT 21

#define NICK_MAX_LEN 16

#define IN_PLAYER_VIEWPORT(x, y, px, py)            \
    ((x) >= (px) - PLAYER_VIEWPORT_WIDTH / 2 &&     \
     (x) <= (px) + PLAYER_VIEWPORT_WIDTH / 2 &&     \
     (y) >= (py) - PLAYER_VIEWPORT_HEIGHT / 2 &&    \
     (y) <= (py) + PLAYER_VIEWPORT_HEIGHT / 2)
        

/* Each object on the map is represented by ascii symbol. */
#define MAP_EMPTY ' '
#define MAP_WALL '#'
#define MAP_PLAYER '@'
#define MAP_BULLET '*'
#define MAP_RESPAWN '!'
#define MAP_NAME_MAX_LEN 32

/* TODO: rewrite this comment */
/* On server side `**objs` can contain only MAP_WALL and MAP_EMPTY symbols,
 * because information about bullets, players, etc is in actual state and
 * full detailed on arrays, structes and lists.
 * Client must draw objects on a screen only and nothing more, that's why
 * all information about world can be presented in simple form(2d array).
 */
#ifdef _SERVER_
#define MAP_RESPAWNS_MAX 16

/* This struct is needed to optimise a search of respawn points when new player
 * connects.
 */
struct map_respawn {
    uint16_t w;
    uint16_t h;
};
#endif

struct map {
    /* On client's side: if name isn't set, then map isn't loaded yet */
    uint8_t name[MAP_NAME_MAX_LEN];
    uint8_t **objs;
    uint16_t width;
    uint16_t height;
#ifdef _SERVER_
    struct map_respawn respawns[MAP_RESPAWNS_MAX];
    uint8_t respawns_count;
#endif
};

/* Structures that describe message body. */
enum {
    //MSGTYPE_NONE = -1,
    MSGTYPE_WALK = 0,
    MSGTYPE_PLAYER_POSITION,
    MSGTYPE_PLAYER_HIT,
    MSGTYPE_PLAYER_KILLED,
    MSGTYPE_ENEMY_POSITION,
    MSGTYPE_SHOOT,
    MSGTYPE_CONNECT_ASK,
    MSGTYPE_CONNECT_OK,
    MSGTYPE_CONNECT_NOTIFY,
    MSGTYPE_DISCONNECT_SERVER,
    MSGTYPE_DISCONNECT_CLIENT,
    MSGTYPE_DISCONNECT_NOTIFY,
    MSGTYPE_ON_BONUS,
    MSGTYPE_MAP_EXPLODE
};

struct msgtype_walk {
    uint8_t direction;
};

struct msgtype_player_position {
    uint16_t pos_x;
    uint16_t pos_y;
};

struct msgtype_player_hit {
    uint16_t hp;
    uint16_t armor;
};

struct msgtype_player_killed {
    uint8_t some;
};

struct msgtype_enemy_position {
    uint16_t pos_x;
    uint16_t pos_y;
};

struct msgtype_shoot {
    uint8_t stub;
};

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

struct msgtype_on_bonus {
    uint8_t type;
    uint8_t index;
};

struct msgtype_map_explode {
    uint16_t w;
    uint16_t h;
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
        struct msgtype_player_hit player_hit;
        struct msgtype_player_killed player_killed;
        struct msgtype_enemy_position enemy_position;
        struct msgtype_shoot shoot;
        struct msgtype_connect_ask connect_ask;
        struct msgtype_connect_ok connect_ok;
        struct msgtype_connect_notify connect_notify;
        struct msgtype_disconnect_server disconnect_server;
        struct msgtype_disconnect_client disconnect_client;
        struct msgtype_disconnect_notify disconnect_notify;
        struct msgtype_on_bonus on_bonus;
        struct msgtype_map_explode map_explode;
    } event;
};

/* msg_batch is an optimisation to minimize number of socket_send() calls.
 * Instead of invoking it for each change in the world, we first pack those
 * changes to the structure and then send it using single syscall.
 *
 * Interaction between client and server goes like this:
 *
 * client --| struct msg |--> server
 * server --| struct msg_batch |--> client
 *
 * First byte of the chunks[] is number of chunks contained in the batch,
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
    /* size is number of bytes in chunks[] occupied by data */
    uint16_t size;
};

#define MSGBATCH_SIZE(b) ((b)->chunks[0])

enum {
    BONUSTYPE_WEAPON = 0,
    BONUSTYPE_HEALTH,
    BONUSTYPE_ARMOR
};

#define HEALTH_NAME_MAX_LEN 8

enum {
    HEALTH_BIG = 0,
    HEALTH_SMALL
};

struct health {
    uint8_t name[HEALTH_NAME_MAX_LEN];
    uint8_t index;
    uint16_t hp;
};

extern struct health healths[];

#define ARMOR_NAME_MAX_LEN 8

enum {
    ARMOR_HEAVY = 0,
    ARMOR_LIGHT
};

struct armor {
    uint8_t name[ARMOR_NAME_MAX_LEN];
    uint8_t index;
    uint16_t armor;
};

extern struct armor armors[];

#define WEAPON_NAME_MAX_LEN 8

enum {
    WEAPON_GUN = 0,
    WEAPON_ROCKET
};

struct weapon {
    uint8_t name[WEAPON_NAME_MAX_LEN];
    uint8_t index;
    uint8_t damage_max;
    uint8_t damage_min;
    uint8_t bullets_speed;
    uint8_t bullets_distance;
    uint16_t bullets_count;
    uint8_t explode_map;
    uint8_t explode_radius;
};

#define WEAPON_SLOTS_MAX 9

struct weapon_slots {
    /* Bit array where 1 means that weapon exists. */
    uint8_t slots[WEAPON_SLOTS_MAX];
    /* Number of bullets for each weapon. */
    uint16_t bullets[WEAPON_SLOTS_MAX];
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
    uint16_t pos_x; /* [1..65535] */
    uint16_t pos_y;
    uint16_t hp;
    uint16_t armor;
    struct weapon_slots weapons;
};

#ifdef _SERVER_
#define MAX_PLAYERS 16

enum player_enum_t {
    PLAYERS_ERROR = 0,
    PLAYERS_OK
};

struct players_slot {
    struct players_slot *next;
    struct players_slot *prev;
    struct player *p;
};

struct players_slots {
    struct players_slot *root;
    uint8_t count;
    /* Describes which slots are free and which are occupied.
       It is an array of pointers to slots.
       If pointer is NULL then slot is free.
    */
    struct players_slot *slots[MAX_PLAYERS];
};
#endif

/* For collisions detection on server and client (movement prediction). */
enum collision_enum_t {
    COLLISION_NONE = 0,
    COLLISION_WALL,
    COLLISION_PLAYER,
    COLLISION_BONUS,
    COLLISION_BULLET
};

#define FPS 10

struct ticks {
    uint64_t offset;
    uint64_t count;
};

/* TODO: *understand* and rewrite this comment. */
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
#ifdef _SERVER_
enum collision_enum_t collision_check_player(struct player*,
                                             struct map*,
                                             struct players_slots*);
#elif _CLIENT_
enum collision_enum_t collision_check_player(struct player*, struct map*);
#endif

#endif
