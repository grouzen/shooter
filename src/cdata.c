#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "cdata.h"

/* TODO: pack_float(), rename un|pack[0-9]+() to un|pack_int[0-9]+(). */
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
 * packing subtypes(member `body')
 * of the structure `msg'.
 */
static void msgtype_walk_pack(struct msg *m, uint8_t *buf)
{
    uint16_t pos_x = m->body.walk.pos_x;
    uint16_t pos_y = m->body.walk.pos_y;
    
    *buf++ = m->body.walk.direction;

    pack16_int(buf, htons(pos_x));
    buf += 2;
    pack16_int(buf, htons(pos_y));
    buf += 2;
}

static void msgtype_shoot_pack(struct msg *m, uint8_t *buf)
{
    *buf++ = m->body.shoot.gun_type;
    //uint8_t gun_type = m->body.shoot.gun_type;
    //pack8_int(buf, htons(gun_type));
}

/* ... and for unpacking. */
static void msgtype_walk_unpack(uint8_t *buf, struct msg *m)
{
    m->body.walk.direction = (uint8_t) *buf++;
    m->body.walk.pos_x = ntohs(unpack16_int(buf));
    buf += 2;
    m->body.walk.pos_y = ntohs(unpack16_int(buf));
    buf += 2;
}

static void msgtype_shoot_unpack(uint8_t *buf, struct msg *m)
{
    //m->body.shoot.gun_type = ntohs(unpack16_int(buf));
    m->body.shoot.gun_type = (uint8_t) *buf++;
}

/* Because I hate switches and all these condition statements
 * I prefer to use calls table. They must be synced with enum
 * declared in cdata.h.
 */
intptr_t msgtype_pack_funcs[] = {
    msgtype_walk_pack,
    msgtype_shoot_pack
};

intptr_t msgtype_unpack_funcs[] = {
    msgtype_walk_unpack,
    msgtype_shoot_unpack
};

/* General packing/unpacking functions. */
void msg_pack(struct msg *m, uint8_t *buf)
{
    void (*msgtype_func)(struct msg*, uint8_t*);
    uint32_t seq = m->header.seq;
    
    pack_int32(buf, htonl(seq));
    buf += 4;
    *buf++ = m->header.player;
    *buf++ = (uint8_t) m->type;

    if(m->type != MSGTYPE_NONE) {
        msgtype_func = (void *) msgtype_pack_funcs[m->type];
        msgtype_func(m, buf);
    }
}

void msg_unpack(uint8_t *buf, struct msg *m)
{
    void (*msgtype_func)(uint8_t*, struct msg*);

    m->header.seq = ntohl(unpack_int32(buf));
    buf += 4;
    m->header.player = (uint8_t) *buf++;
    m->type = (uint8_t) *buf++;

    if(m->type != MSGTYPE_NONE) {
        msgtype_func = (void *) msgtype_unpack_funcs[m->type];
        msgtype_func(buf, m);
    }
}

msg_batch_enum_t msg_batch_push(struct msg_batch *b, struct msg *m)
{
    if(b->offset <= MSGBATCH_BYTES - sizeof(struct msg)) {
        msg_pack(m, &(b->chunks[b->offset]));
        b->offset += sizeof(struct msg);
        return MSGBATCH_OK;
    }

    return MSGBATCH_ERROR;
}

uint8_t *msg_batch_pop(struct msg_batch *b)
{
    if(b->offset > sizeof(struct msg)) {
        offset -= sizeof(struct msg);
        return &(b->chunks[b->offset]);
    }

    return NULL;
}

/* These functions work in fact with microseconds. */
uint64_t start_ticks;

#define GET_MICROSECS(t) (((t).tv_sec * 1000000) + ((t).tv_usec / 1000))

uint64_t ticks_start(void)
{
    struct timeval t;

    if(gettimeofday(&t, NULL) < 0) {
        perror("cdata: gettimeofday");
        exit(EXIT_FAILURE);
    }

    start_ticks = GET_MICROSECS(t);

    return start_ticks;
}

uint64_t ticks_get(void)
{
    struct timeval t;

    if(gettimeofday(&t, NULL) < 0) {
        perror("cdata: gettimeofday");
        exit(EXIT_FAILURE);
    }

    return GET_MICROSECS(t) - start_ticks;
}
