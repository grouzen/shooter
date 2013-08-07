/* Copyright (c) 2011, 2012 Michael Nedokushev <grouzen.hexy@gmail.com>
 * Copyright (c) 2011, 2012 Alexander Batischev <eual.jp@gmail.com>
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

#ifndef __SERVER_H__
#define __SERVER_H__

#define MSGQUEUE_INIT_SIZE 64

struct msg_queue_node {
    struct msg *data;
    struct player *player;
    struct sockaddr_storage *addr;
    socklen_t addr_len;
};

struct msg_queue {
    struct msg_queue_node nodes[MSGQUEUE_INIT_SIZE];
    ssize_t top;
};

enum bullets_enum_t {
    BULLETS_ERROR = 0,
    BULLETS_OK
};

struct bullet {
    struct player *player;
    uint8_t type;
    uint16_t x;
    uint16_t y;
    uint16_t sx;
    uint16_t sy;
    uint8_t direction;
};

struct bullets_node {
    struct bullets_node *next;
    struct bullets_node *prev;
    struct bullet *b;
};

struct bullets {
    struct bullets_node *root;
    struct bullets_node *last;
};

enum bonuses_enum_t {
    BONUSES_ERROR = 0,
    BONUSES_OK
};

struct bonus {
    uint8_t type;
    uint8_t index;
    uint16_t x;
    uint16_t y;
};

struct bonuses_node {
    struct bonuses_node *next;
    struct bonuses_node *prev;
    struct bonus *b;
};

struct bonuses {
    struct bonuses_node *root;
    uint8_t count;
};

struct players_slots *players_init(void);
void players_free(struct players_slots*);
struct player *players_occupy(struct players_slots*, struct sockaddr_storage*, uint8_t*);
enum player_enum_t players_release(struct players_slots*, uint8_t);
struct msg_queue *msgqueue_init(void);
void msgqueue_free(struct msg_queue*);
enum msg_queue_enum_t msgqueue_push(struct msg_queue*, struct msg_queue_node*);
struct msg_queue_node *msgqueue_pop(struct msg_queue*);
void bullet_explode(struct bullet*);
struct bullets *bullets_init(void);
void bullets_free(struct bullets*);
struct bullet *bullets_add(struct bullets*, struct bullet*);
enum bullets_enum_t bullets_remove(struct bullets*, struct bullet*);
void bullets_proceed(struct bullets*);
struct bonuses *bonuses_init(void);
void bonuses_free(struct bonuses*);
struct bonus *bonuses_search(struct bonuses*, uint16_t, uint16_t);
struct bonus *bonuses_add(struct bonuses*, struct bonus*);
enum bonuses_enum_t bonuses_remove(struct bonuses*, struct bonus*);
void send_to_player (const void *buf, size_t len, struct player *player);
void send_to(const void *buf, size_t len, const struct sockaddr *dest,
        socklen_t addrlen);

extern struct msg_queue *msgqueue;
extern struct players_slots *players;
extern struct bonuses *bonuses;
extern struct bullets *bullets;
extern struct map *map;
extern struct pollfd *fds;
extern int nfds;
extern int *fd_families;

#endif
/* vim:set expandtab: */
