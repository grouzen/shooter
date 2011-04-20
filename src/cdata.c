#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "cdata.h"

struct weapon weapons[] = {
    {"gun", 20, 5, 0, 0, 0},
    {"rocket", 90, 40, 3, 40, 10}
};

/* TODO: pack_float(). */
static void pack_int32(uint8_t *buf, uint32_t x)
{
    *buf++ = x >> 24;
    *buf++ = x >> 16;
    *buf++ = x >> 8;
    *buf++ = x;
}

static uint32_t unpack_int32(uint8_t *buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static void pack16_int(uint8_t *buf, uint16_t x)
{
    *buf++ = x >> 8;
    *buf++ = x;
}

static uint16_t unpack16_int(uint8_t *buf)
{
    return (buf[0] << 8) | buf[1];
}

/* Here we have a couple routines for
 * packing subtypes(member `event')
 * of the structure `msg'.
 */
static void msgtype_walk_pack(struct msg *m, uint8_t *buf)
{
    *buf++ = m->event.walk.direction;
}

static void msgtype_player_position_pack(struct msg *m, uint8_t *buf)
{
    uint16_t pos_x = m->event.player_position.pos_x;
    uint16_t pos_y = m->event.player_position.pos_y;

    pack16_int(buf, htons(pos_x));
    buf += 2;
    pack16_int(buf, htons(pos_y));
    buf += 2;
}

static void msgtype_shoot_pack(struct msg *m, uint8_t *buf)
{
    *buf++ = m->event.shoot.gun_type;
}

static void msgtype_connect_ask_pack(struct msg *m, uint8_t *buf)
{
    int i;

    for(i = 0; m->event.connect_ask.nick[i] != '\0'; i++) {
        *buf++ = m->event.connect_ask.nick[i];
    }
    *buf++ = '\0';
}

static void msgtype_connect_ok_pack(struct msg *m, uint8_t *buf)
{
    *buf++ = m->event.connect_ok.ok;
    *buf++ = m->event.connect_ok.id;
}

static void msgtype_connect_notify_pack(struct msg *m, uint8_t *buf)
{
    int i;

    for(i = 0; m->event.connect_notify.nick[i] != '\0'; i++) {
        *buf++ = m->event.connect_notify.nick[i];
    }
    *buf++ = '\0';
}

static void msgtype_disconnect_server_pack(struct msg *m, uint8_t *buf)
{
    *buf++ = m->event.disconnect_server.stub;
}

static void msgtype_disconnect_client_pack(struct msg *m, uint8_t *buf)
{
    *buf++ = m->event.disconnect_client.stub;
}

static void msgtype_disconnect_notify_pack(struct msg *m, uint8_t *buf)
{
    int i;
    
    for(i = 0; m->event.disconnect_notify.nick[i] != '\0'; i++) {
        *buf++ = m->event.disconnect_notify.nick[i];
    }
    *buf++ = '\0';
}

/* ... and for unpacking. */
static void msgtype_walk_unpack(uint8_t *buf, struct msg *m)
{
    m->event.walk.direction = *buf++;
}

static void msgtype_player_position_unpack(uint8_t *buf, struct msg *m)
{
    m->event.player_position.pos_x = ntohs(unpack16_int(buf));
    buf += 2;
    m->event.player_position.pos_y = ntohs(unpack16_int(buf));
    buf += 2;
}

static void msgtype_shoot_unpack(uint8_t *buf, struct msg *m)
{
    m->event.shoot.gun_type = (uint8_t) *buf++;
}

static void msgtype_connect_ask_unpack(uint8_t *buf, struct msg *m)
{
    int i;

    for(i = 0; buf[i] != '\0'; i++) {
        m->event.connect_ask.nick[i] = buf[i];
    }
    m->event.connect_ask.nick[i] = '\0';
}

static void msgtype_connect_ok_unpack(uint8_t *buf, struct msg *m)
{
    m->event.connect_ok.ok = (uint8_t) *buf++;
    m->event.connect_ok.id = (uint8_t) *buf++;
}

static void msgtype_connect_notify_unpack(uint8_t *buf, struct msg *m)
{
    int i;

    for(i = 0; buf[i] != '\0'; i++) {
        m->event.connect_notify.nick[i] = buf[i];
    }
    m->event.connect_notify.nick[i] = '\0';
}

static void msgtype_disconnect_server_unpack(uint8_t *buf, struct msg *m)
{
    m->event.disconnect_server.stub = (uint8_t) *buf++;
}

static void msgtype_disconnect_client_unpack(uint8_t *buf, struct msg *m)
{
    m->event.disconnect_client.stub = (uint8_t) *buf++;
}

static void msgtype_disconnect_notify_unpack(uint8_t *buf, struct msg *m)
{
    int i;

    for(i = 0; buf[i] != '\0'; i++) {
        m->event.disconnect_notify.nick[i] = buf[i];
    }
    m->event.disconnect_notify.nick[i] = '\0';
}

/* Because I hate switches and all these condition statements
 * I prefer to use calls table. They must be synced with enum
 * declared in cdata.h.
 */
intptr_t msgtype_pack_funcs[] = {
    (intptr_t) msgtype_walk_pack,
    (intptr_t) msgtype_player_position_pack,
    (intptr_t) msgtype_shoot_pack,
    (intptr_t) msgtype_connect_ask_pack,
    (intptr_t) msgtype_connect_ok_pack,
    (intptr_t) msgtype_connect_notify_pack,
    (intptr_t) msgtype_disconnect_server_pack,
    (intptr_t) msgtype_disconnect_client_pack,
    (intptr_t) msgtype_disconnect_notify_pack
};

intptr_t msgtype_unpack_funcs[] = {
    (intptr_t) msgtype_walk_unpack,
    (intptr_t) msgtype_player_position_unpack,
    (intptr_t) msgtype_shoot_unpack,
    (intptr_t) msgtype_connect_ask_unpack,
    (intptr_t) msgtype_connect_ok_unpack,
    (intptr_t) msgtype_connect_notify_unpack,
    (intptr_t) msgtype_disconnect_server_unpack,
    (intptr_t) msgtype_disconnect_client_unpack,
    (intptr_t) msgtype_disconnect_notify_unpack
};

/* General packing/unpacking functions. */
void msg_pack(struct msg *m, uint8_t *buf)
{
    void (*msgtype_func)(struct msg*, uint8_t*);
    uint32_t seq = m->header.seq;

    pack_int32(buf, htonl(seq));

    buf += 4;
    *buf++ = m->header.id;
    *buf++ = m->type;

    msgtype_func = (void *) msgtype_pack_funcs[m->type];
    msgtype_func(m, buf);
}

void msg_unpack(uint8_t *buf, struct msg *m)
{
    void (*msgtype_func)(uint8_t*, struct msg*);

    m->header.seq = ntohl(unpack_int32(buf));
    buf += 4;
    m->header.id = *buf++;
    m->type = *buf++;

    msgtype_func = (void *) msgtype_unpack_funcs[m->type];
    msgtype_func(buf, m);
}

enum msg_batch_enum_t msg_batch_push(struct msg_batch *b, struct msg *m)
{
    if(MSGBATCH_SIZE(b) < MSGBATCH_INIT_SIZE) {
        msg_pack(m, &(b->chunks[b->offset + 1]));
        b->offset += sizeof(struct msg);
        MSGBATCH_SIZE(b)++;
        
        return MSGBATCH_OK;
    }

    return MSGBATCH_ERROR;
}

uint8_t *msg_batch_pop(struct msg_batch *b)
{
    if(MSGBATCH_SIZE(b) > 0) {
        b->offset -= sizeof(struct msg);
        MSGBATCH_SIZE(b)--;
        
        return &(b->chunks[b->offset + 1]);
    }

    return NULL;
}

/* These functions work in fact with ms. */
uint64_t ticks_get(void)
{
    struct timeval t;
    
    if(gettimeofday(&t, NULL) < 0) {
        perror("cdata: gettimeofday");
        exit(EXIT_FAILURE);
    }

    return (((uint64_t) t.tv_sec) * 1000) + (((uint64_t) t.tv_usec) / 1000);
}

struct ticks *ticks_start(void)
{
    struct ticks *tc;

    tc = malloc(sizeof(struct ticks));

    tc->offset = ticks_get();
    tc->count = 0;

    return tc;
}

void ticks_finish(struct ticks *tc)
{
    free(tc);
}

void ticks_update(struct ticks *tc)
{
    tc->count = ticks_get() - tc->offset;
}

uint64_t ticks_get_diff(struct ticks *tc)
{

    return ticks_get() - (tc->offset + tc->count);
}

struct player *player_init(void)
{
    struct player *p;

    p = malloc(sizeof(struct player));
    memset(p, 0, sizeof(struct player));
    p->nick = malloc(sizeof(uint8_t) * NICK_MAX_LEN);
#ifdef _SERVER_
    p->addr = malloc(sizeof(struct sockaddr_in));
#endif
    
    return p;
}

void player_free(struct player *p)
{
    free(p->nick);
#ifdef _SERVER_
    free(p->addr);
#endif
    free(p);
}

/* TODO: load map from file. */
struct map *map_load(void)
{
    struct map *m = malloc(sizeof(struct map));

    memset(m, 0, sizeof(struct map));
    
    m->width = 100 + rand() % 500;
    m->height = 100 + rand() % 500;
    
    return m;
}

void map_unload(struct map *m)
{
    free(m);
}
