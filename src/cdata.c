#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#include "cdata.h"

/* Bonuses. */
struct weapon weapons[WEAPON_SLOTS_MAX] = {
    /*   NAME   SNAME   DMAX   DMIN   BSPEED   BDIST   BCOUNT */
    {   "gun",   'G',    20,     5,      0,      0,     100   },
    {  "rocket", 'R',    90,    40,      3,     40,      10   }
};

struct health healths[] = {
    /*  NAME    SNAME   HP */
    {  "hbig",   'H',  100 },
    {  "hsmall", 'h',   50 }
};

struct armor armors[] = {
    /* NAME     SNAME  ARMOR */
    { "aheavy",  'A',   100  },
    { "alight",  'a',    50  }
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

static void msgtype_enemy_position_pack(struct msg *m, uint8_t *buf)
{
    uint16_t pos_x = m->event.enemy_position.pos_x;
    uint16_t pos_y = m->event.enemy_position.pos_y;

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
    strncpy((char *) buf, (char *) m->event.connect_ask.nick, NICK_MAX_LEN);
}

static void msgtype_connect_ok_pack(struct msg *m, uint8_t *buf)
{
    *buf++ = m->event.connect_ok.ok;
    *buf++ = m->event.connect_ok.id;

    strncpy((char *) buf, (char *) m->event.connect_ok.mapname, MAP_NAME_MAX_LEN);
}

static void msgtype_connect_notify_pack(struct msg *m, uint8_t *buf)
{
    strncpy((char *) buf, (char *) m->event.connect_notify.nick, NICK_MAX_LEN);
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
    strncpy((char *) buf, (char *) m->event.disconnect_notify.nick, NICK_MAX_LEN);
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

static void msgtype_enemy_position_unpack(uint8_t *buf, struct msg *m)
{
    m->event.enemy_position.pos_x = ntohs(unpack16_int(buf));
    buf += 2;
    m->event.enemy_position.pos_y = ntohs(unpack16_int(buf));
    buf += 2;
}

static void msgtype_shoot_unpack(uint8_t *buf, struct msg *m)
{
    m->event.shoot.gun_type = (uint8_t) *buf++;
}

static void msgtype_connect_ask_unpack(uint8_t *buf, struct msg *m)
{
    strncpy((char *) m->event.connect_ask.nick, (char *) buf, NICK_MAX_LEN);
}

static void msgtype_connect_ok_unpack(uint8_t *buf, struct msg *m)
{
    m->event.connect_ok.ok = (uint8_t) *buf++;
    m->event.connect_ok.id = (uint8_t) *buf++;

    strncpy((char *) m->event.connect_ok.mapname, (char *) buf, MAP_NAME_MAX_LEN);
}

static void msgtype_connect_notify_unpack(uint8_t *buf, struct msg *m)
{
    strncpy((char *) m->event.connect_notify.nick, (char *) buf, NICK_MAX_LEN);
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
    strncpy((char *) m->event.disconnect_notify.nick, (char *) buf, NICK_MAX_LEN);
}

/* Because I hate switches and all these condition statements
 * I prefer to use calls table. They must be synced with enum
 * declared in cdata.h.
 */
intptr_t msgtype_pack_funcs[] = {
    (intptr_t) msgtype_walk_pack,
    (intptr_t) msgtype_player_position_pack,
    (intptr_t) msgtype_enemy_position_pack,
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
    (intptr_t) msgtype_enemy_position_unpack,
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
    p->hp = 100;

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
struct map *map_load(uint8_t *name)
{
    struct map *m = malloc(sizeof(struct map));
    char path[4096], c;
    FILE *fmap;
    int w, h;

    memset(m, 0, sizeof(struct map));
    
    snprintf(path, 4096, "data/maps/%s", name);
    if((fmap = fopen(path, "r")) == NULL) {
#ifdef _SERVER_
        free(m);
        return NULL;
#elif _CLIENT_
        /* If map doesn't exist try to load it.
         * When map loaded, msgqueue_mngr_func() calls
         * map_load() and than loading is finished.
         */
        
        /* Recieved map dump into `path`. */
        return NULL; // TODO: remove it.
#endif
    }

    /* Determine size of a map and load it.
     * This algorithm I took from my project `snake-sdl` ;^).
     */
    while((c = getc(fmap)) != '\n') {
        m->width++;
    }

    m->height++;
    
    while((c = getc(fmap)) != EOF) {
        w = 0;
        
        while(c != '\n') {
            c = getc(fmap);
            w++;
        }

        if(w != m->width) {
            printf("Map has an incorrect geometry: %s.\n", name);
            fclose(fmap);
            free(m);
            
            return NULL;
        }

        m->height++;
    }
    
    fseek(fmap, 0L, SEEK_SET);
    
    m->objs = malloc(sizeof(uint8_t *) * m->height);
    for(h = 0; h < m->height; h++) {
        m->objs[h] = malloc(sizeof(uint8_t) * m->width);
    }

    h = 0;
    while((c = getc(fmap)) != EOF) {
        w = 0;

        while(c != '\n') { 
            if(c == MAP_EMPTY || c == MAP_WALL) {
                m->objs[h][w] = c;
            } else if(c == MAP_RESPAWN) {
#ifdef _SERVER_
                if(m->respawns_count < MAP_RESPAWNS_MAX - 1) {
                    m->respawns[m->respawns_count].w = w;
                    m->respawns[m->respawns_count].h = h;
                    m->respawns_count++;
                } else {
                    printf("%c, %d\n", c, w);
                    printf("Max count of respawns was reached: %d.\n", MAP_RESPAWNS_MAX);
                }
#endif
                m->objs[h][w] = MAP_EMPTY;
            } else {
                printf("Incorrect symbol '%c' has been found at %dx%d.\n", c, w, h);
                fclose(fmap);
                map_unload(m);
                
                return NULL;
            }
            
            c = fgetc(fmap);
            w++;
        }
        
        h++;
    }
    
    strncpy((char *) m->name, (char *) name, MAP_NAME_MAX_LEN);
    
    fclose(fmap);
    
    return m;
}

void map_unload(struct map *m)
{
    int h;
    
    for(h = 0; h < m->height; h++) {
        free(m->objs[h]);
    }
    
    free(m->objs);
    free(m);
}

#ifdef _SERVER_
/* TODO: add `struct bonuses_list`, bullets, etc. */
enum collision_enum_t collision_check_player(struct player *p,
                                             struct map *m,
                                             struct players_slots *s)
#elif _CLIENT_
/* On client we need check only MAP_WALL and MAP_PLAYER cases,
 * because on other cases we can put player point on bullet or bonus
 * and anything terrible doesn't happen.
 */
enum collision_enum_t collision_check_player(struct player *p, struct map *m)
#endif
{
#ifdef _SERVER_
    struct players_slot *slot = s->root;
#endif
    if((int16_t) p->pos_x < 0 || (int16_t) p->pos_y < 0 || p->pos_x >= m->width ||
       p->pos_y >= m->height || m->objs[p->pos_y][p->pos_x] == MAP_WALL) {
        return COLLISION_WALL;
    }
#ifdef _SERVER_
    while(slot != NULL) {
        struct player *sp = slot->p;

        if(sp != p && sp->pos_x == p->pos_x && sp->pos_y == p->pos_y) {
            return COLLISION_PLAYER;
        }
        
        slot = slot->next;
    }
#elif _CLIENT_
    if(m->objs[p->pos_y][p->pos_x] == MAP_PLAYER) {
        return COLLISION_PLAYER;
    }
#endif
    
    return COLLISION_NONE;
}
