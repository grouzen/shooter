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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <error.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#include "../cdata.h"
#include "ui/backend.h"
#include "client.h"

/* locks */
pthread_mutex_t player_mutex, map_mutex, queue_mutex;
pthread_cond_t queue_cond;
sem_t showflow;

/* thread */
bool ui_mngr, ui_event_mngr;
pthread_t ui_mngr_thread, ui_event_mngr_thread, queue_mngr_thread;
pthread_attr_t common_attr;

/* game */
struct msg_queue *msgqueue;
struct player *player;
struct map *map;
int sd;

struct msg_queue *msgqueue_init(void)
{
    struct msg_queue *q;
    int i;

    q = malloc(sizeof(struct msg_queue));
    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        q->data[i] = malloc(sizeof(struct msg));
    }

    q->top = -1;

    return q;
}

void msgqueue_free(struct msg_queue *q)
{
    int i;

    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        free(q->data[i]);
    }

    free(q);
}

enum msg_queue_enum_t msgqueue_push(struct msg_queue *q, struct msg *m)
{
    if(q->top < MSGQUEUE_INIT_SIZE - 1) {
        q->top++;
        memcpy(q->data[q->top], m, sizeof(struct msg));

        return MSGQUEUE_OK;
    }

    return MSGQUEUE_ERROR;
}

struct msg *msgqueue_pop(struct msg_queue *q)
{
    if(q->top >= 0) {
        return q->data[q->top--];
    }

    return NULL;
}

void send_event(struct msg *m)
{
    uint8_t buf[sizeof(struct msg)] = {0};

    pthread_mutex_lock(&player_mutex);
    memcpy (m->header.key, player->key, sizeof (player->key));
    m->header.seq = player->seq;
    pthread_mutex_unlock(&player_mutex);

    msg_pack(m, buf);
    write(sd, buf, sizeof(struct msg));
}

void event_disconnect_client(void)
{
    struct msg msg;

    msg.type = MSGTYPE_DISCONNECT_CLIENT;
    msg.event.disconnect_client.stub = 1;

    send_event(&msg);
}

void event_connect_ask(void)
{
    struct msg msg;
    char *nick = getenv ("USER");
    msg.type = MSGTYPE_CONNECT_ASK;
    /* TODO: get nick from config or args. */
    strncpy((char *) msg.event.connect_ask.nick, nick ? nick : "somenick", NICK_MAX_LEN);

    send_event(&msg);
}

void event_connect_ok(struct msg *m)
{
    uint8_t *mapname = m->event.connect_ok.mapname;

    if(m->event.connect_ok.ok) {
        /* TODO: fix bug with map.
           Now client loads the map and use it as is,
           but map is changing during the game, thus client
           must load the map always from server.
        */
        INFO ("recive permissive message from server, load map: %s\n", mapname);
        pthread_mutex_lock(&map_mutex);
        map = map_load(mapname);
        pthread_mutex_unlock(&map_mutex);
        if(map == NULL) {
            WARN("Map couldn't be loaded: %s. Trying to load it from server...\n",
                 mapname);
            /* TODO: call event_map_load_ask() which calls map_load(). */
            quit (SIGHUP);
            return;
        }

        INFO ("Connected with key: " KEY_FORMAT ", map: %s\n",
                           KEY_EXPAND (m->header.key), (char *) mapname);

        pthread_mutex_lock(&player_mutex);
        memcpy (player->key, m->header.key, sizeof (player->key));
        player->connected = true;
        pthread_mutex_unlock(&player_mutex);

        /* TODO: move this call to event_map_load_finish() for example. */
        ui_mngr = true;
        pthread_create(&ui_mngr_thread, &common_attr, ui_mngr_func, NULL);
    } else {
        INFO ("connection failed\n");
    }
}

void event_connect_notify(struct msg *m)
{
    INFO ("New player has been connected with nick: %s\n",
                       m->event.connect_notify.nick);
}

void event_disconnect_notify(struct msg *m)
{
    INFO ("Player has been disconnected: %s\n.",
                       m->event.disconnect_notify.nick);
}

void event_disconnect_server(struct msg *m)
{
    INFO ("Server has been halted. Disconnecting... (seq: %ld)",
        m->header.seq);
    ui_refresh();
    sleep(3);

    quit (SIGHUP);
}

void event_player_hit(struct msg *m)
{
    pthread_mutex_lock(&player_mutex);
    player->hp = m->event.player_hit.hp;
    player->armor = m->event.player_hit.armor;
    pthread_mutex_unlock(&player_mutex);
}

void event_map_explode(struct msg *m)
{
    pthread_mutex_lock(&map_mutex);
    map->objs[m->event.map_explode.h][m->event.map_explode.w] = MAP_EMPTY;
    pthread_mutex_unlock(&map_mutex);
}

void event_on_bonus(struct msg *m)
{
    uint8_t type = m->event.on_bonus.type;
    uint8_t index = m->event.on_bonus.index;

    switch(type) {
    case BONUSTYPE_WEAPON:
        pthread_mutex_lock(&player_mutex);
        player->weapons.slots[index] = 1;
        player->weapons.bullets[index] = weapons[index].bullets_count;
        pthread_mutex_unlock(&player_mutex);

        break;
    default:
        break;
    }
}

void event_player_position(struct msg *m)
{
    pthread_mutex_lock(&player_mutex);
    player->pos_x = m->event.player_position.pos_x;
    player->pos_y = m->event.player_position.pos_y;
    pthread_mutex_unlock(&player_mutex);
}

void event_enemy_position(struct msg *m)
{
    pthread_mutex_lock(&map_mutex);
    if (!m->event.enemy_position.pos_y || !m->event.enemy_position.pos_x ||
        m->event.enemy_position.pos_y > map->height ||\
           m->event.enemy_position.pos_x > map->width) {
        WARN ("enemy position not in (%u, %u)\n", map->height, map->width);
    } else {
        map->objs[m->event.enemy_position.pos_y - 1]\
            [m->event.enemy_position.pos_x - 1] = MAP_PLAYER;
    }
    pthread_mutex_unlock(&map_mutex);
}

void event_shoot(void)
{
    struct msg msg;

    pthread_mutex_lock(&player_mutex);
    msg.type = MSGTYPE_SHOOT;
    msg.event.shoot.direction = player->direction;
    pthread_mutex_unlock(&player_mutex);

    send_event(&msg);
}

void event_walk(void)
{
    struct msg msg;

    pthread_mutex_lock(&player_mutex);
    msg.type = MSGTYPE_WALK;
    msg.event.walk.direction = player->direction;
    pthread_mutex_unlock(&player_mutex);

    send_event(&msg);
}

void *ui_event_mngr_func(void *arg)
{
    while(1) {
        int ui_event;
        struct timespec req;
        uint16_t px, py;

        req.tv_sec = 1000 / FPS / 1000;
        req.tv_nsec = 1000 / FPS * 1000000;

        nanosleep(&req, NULL);

        ui_event = ui_get_event();

        pthread_mutex_lock(&player_mutex);

        /* Just remember previous parameters. */
        px = player->pos_x;
        py = player->pos_y;

        switch(ui_event) {
        case UI_EVENT_WALK_LEFT:
            player->direction = DIRECTION_LEFT;
            player->pos_x--;
            break;
        case UI_EVENT_WALK_RIGHT:
            player->direction = DIRECTION_RIGHT;
            player->pos_x++;
            break;
        case UI_EVENT_WALK_UP:
            player->direction = DIRECTION_UP;
            player->pos_y--;
            break;
        case UI_EVENT_WALK_DOWN:
            player->direction = DIRECTION_DOWN;
            player->pos_y++;
            break;
        case UI_EVENT_SHOOT_UP:
        case UI_EVENT_SHOOT_DOWN:
        case UI_EVENT_SHOOT_LEFT:
        case UI_EVENT_SHOOT_RIGHT:
            if(player->weapons.bullets[player->weapons.current] > 0) {
                (player->weapons.bullets[player->weapons.current])--;
            }

            break;
        default:
            break;
        }

        pthread_mutex_unlock(&player_mutex);

        switch(ui_event) {
        case UI_EVENT_WALK_LEFT:
        case UI_EVENT_WALK_RIGHT:
        case UI_EVENT_WALK_UP:
        case UI_EVENT_WALK_DOWN:
            pthread_mutex_lock(&player_mutex);
            if(collision_check_player(player, map) != COLLISION_NONE) {
                player->pos_x = px;
                player->pos_y = py;
            }
            pthread_mutex_unlock(&player_mutex);

            event_walk();
            break;
        case UI_EVENT_SHOOT_UP:
            player->direction = DIRECTION_UP;
            event_shoot();
            break;
        case UI_EVENT_SHOOT_DOWN:
            player->direction = DIRECTION_DOWN;
            event_shoot();
            break;
        case UI_EVENT_SHOOT_LEFT:
            player->direction = DIRECTION_LEFT;
            event_shoot();
            break;
        case UI_EVENT_SHOOT_RIGHT:
            player->direction = DIRECTION_RIGHT;
            event_shoot();
            break;
        default:
            break;
        }

        ui_refresh();
    }
}

/* Init ui backend. */
void *ui_mngr_func(void *arg)
{
    ui_event_mngr = true;
    pthread_create(&ui_event_mngr_thread, &common_attr, ui_event_mngr_func, NULL);

    if(ui_init() == UI_ERROR) {
        WARN("UI couldn't init.\n");
    }

    quit (SIGHUP);
    return NULL;
}

void *queue_mngr_func(void *arg)
{
    struct ticks *ticks = ticks_start ();
    struct msg *m;
    register uint16_t w, h;
    register uint8_t o;
    while(1) {

        pthread_mutex_lock(&queue_mutex);
        pthread_cond_wait(&queue_cond, &queue_mutex);
        ticks_update (ticks);

        /* Clean map from players, bullets, bonuses and other objects. */
        if(map != NULL) {
            /* lock map and go */
            pthread_mutex_lock(&map_mutex);
            for(h = 0; h < map->height; h++) {
                for(w = 0; w < map->width; w++) {
                    o = map->objs[h][w];

                    if(o == MAP_PLAYER || o == MAP_BULLET) {
                        map->objs[h][w] = MAP_EMPTY;
                    }
                }
            }
            pthread_mutex_unlock(&map_mutex);
        }

        while((m = msgqueue_pop(msgqueue)) != NULL) {
            /* TODO: just fucking do it!. */
            /* check message key */
            pthread_mutex_lock (&player_mutex);
            if (!player->connected && m->type != MSGTYPE_CONNECT_OK) {
                WARN ("outside package, handshake not completed. Type %s, seq %lu\n",\
                     msgtype_str (m->type), m->header.seq);
                m->type = MSGTYPE_NONE;
            } else if (player->connected) {
                if (memcmp (m->header.key, player->key, KEY_LEN)) {
                    WARN ("receive message witout true key '" KEY_FORMAT ","\
                           " Type %s, seq %lu\n", KEY_EXPAND (m->header.key),\
                           msgtype_str (m->type), m->header.seq);
                    m->type = MSGTYPE_NONE;
                }
            }
            pthread_mutex_unlock (&player_mutex);
            if (m->type != MSGTYPE_NONE) {
                DEBUG ("receive msg, seq: %lu, type %s\n", m->header.seq,\
                         msgtype_str (m->type));
            }
            /* TODO: figure out how to return this case into switch.
               Problem in that we cann't start a game until
               map is not loaded
            */

            switch(m->type) {
            case MSGTYPE_CONNECT_OK:
                event_connect_ok(m);
                break;
            case MSGTYPE_CONNECT_NOTIFY:
                event_connect_notify(m);
                break;
            case MSGTYPE_DISCONNECT_NOTIFY:
                event_disconnect_notify(m);
                break;
            case MSGTYPE_DISCONNECT_SERVER:
                event_disconnect_server(m);
                break;
            case MSGTYPE_PLAYER_POSITION:
                event_player_position(m);
                break;
            case MSGTYPE_ENEMY_POSITION:
                event_enemy_position(m);
                break;
            case MSGTYPE_ON_BONUS:
                event_on_bonus(m);
                break;
            case MSGTYPE_PLAYER_HIT:
                event_player_hit(m);
                break;
            case MSGTYPE_MAP_EXPLODE:
                event_map_explode(m);
                break;
            default:
                break;
            }
        }

        pthread_mutex_unlock(&queue_mutex);
        if (ticks_get_diff (ticks) > 1000 / FPS)
            ui_refresh();
    }
    ticks_finish (ticks);
}

void
quit (int signum) {
    WARN ("abnormal quit with signum %d\n", signum);
    sem_post (&showflow);
    INFO ("threads terminate\n");
    if (ui_mngr)
        pthread_cancel (ui_mngr_thread);
    if (ui_event_mngr)
        pthread_cancel (ui_event_mngr_thread);
    pthread_cancel (queue_mngr_thread);
}

void
loop ()
{
    uint8_t buf[sizeof (struct msg_batch)];
    struct msg_batch msgbatch;
    uint8_t *chunk;
    struct msg m;
    ssize_t recvsize;
    size_t messages;
    fd_set rfds;
    struct timeval tv;
    int rv;
    /* begin */
    event_connect_ask();
    DEBUG ("entering main loop\n");
    for (; sem_trywait (&showflow); ) {
        /* without full blocking */
        FD_ZERO (&rfds);
        FD_SET (sd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        rv = select (sd + 1, &rfds, NULL, NULL, &tv);
        if (rv <= 0) {
            if (rv < 0)
                perror ("select");
            continue;
        }
        /* recv data, select returns ok */
        recvsize = recvfrom (sd, buf, sizeof (msgbatch), 0, NULL, NULL);
        if (recvsize <= 0) {
            perror ("recvfrom");
            continue;
        }
        msgbatch.size = (buf[0] * sizeof (struct msg));
        if (msgbatch.size > (size_t)recvsize) {
            WARN ("recvfrom: defect in packet, size %u, received %ld\n",\
                msgbatch.size, recvsize);
            continue;
        }
        /* send to queue */
        memcpy (msgbatch.chunks, buf, msgbatch.size + 1);

        messages = 0u;
        pthread_mutex_lock (&queue_mutex);
        while ((chunk = msg_batch_pop (&msgbatch)) != NULL) {
            if (!msg_unpack (chunk, &m)) {
                WARN ("msg_unpack: packet malformed\n");
            } else {
                if (msgqueue_push (msgqueue, &m) == MSGQUEUE_ERROR) {
                    WARN ("msgqueue_push: couldn't push data into queue\n");
                } else {
                    messages++;
                }
            }
        }
        pthread_mutex_unlock (&queue_mutex);
        /* event to event_mngr, if new message is coming */
        if (messages) {
            pthread_cond_signal (&queue_cond);
        }
    }
    DEBUG ("leave main loop\n");
}

int main(int argc, char **argv)
{
    struct addrinfo *addr_res = NULL;
    struct addrinfo hints, *addr = NULL;
    int errcode;
    memset(&hints, '\0', sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_socktype = SOCK_DGRAM;
    // TODO: get the address from argv or config, or other place
    errcode = getaddrinfo("localhost", "6006", &hints, &addr_res);
    if(errcode != 0 ) {
        error(EXIT_FAILURE, 0, "getaddrinfo: %s", gai_strerror(errcode));
    }
    addr = addr_res;
    for(; addr != NULL; addr = addr->ai_next) {
        sd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if(sd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        } else {
            if(connect(sd, addr->ai_addr, addr->ai_addrlen) != 0) {
                perror("connect");
                close(sd);
            } else {
                break;
            }
        }
    }
    freeaddrinfo(addr_res);

    signal(SIGINT, quit);
    signal(SIGHUP, quit);
    signal(SIGQUIT, quit);

    msgqueue = msgqueue_init();
    player = player_init(NULL);
    if (!player)
    {
        perror ("malloc");
        return 1;
    }
    pthread_mutex_init (&player_mutex, NULL);
    pthread_mutex_init (&map_mutex, NULL);
    pthread_mutex_init (&queue_mutex, NULL);
    pthread_cond_init (&queue_cond, NULL);

    sem_init (&showflow, 0, 0);

    pthread_attr_init (&common_attr);
    pthread_attr_setdetachstate (&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create (&queue_mngr_thread, &common_attr, queue_mngr_func, NULL);

    /* main loop */
    loop ();
    /* deinit */

    INFO ("threads wait\n");
    if (ui_mngr)
        pthread_join (ui_mngr_thread, NULL);
    if (ui_event_mngr)
        pthread_join (ui_event_mngr_thread, NULL);
    pthread_join (queue_mngr_thread, NULL);

    INFO ("free ui\n");
    ui_free ();

    INFO ("send disconnect info\n");
    event_disconnect_client ();

    if (map != NULL) {
        INFO ("unload map\n");
        map_unload (map);
    }

    msgqueue_free (msgqueue);

    player_free (player);

    pthread_mutex_destroy (&queue_mutex);
    pthread_mutex_destroy (&map_mutex);
    pthread_mutex_destroy (&player_mutex);

    pthread_cond_destroy (&queue_cond);
    pthread_attr_destroy (&common_attr);

    INFO ("fully exit\n");
    pthread_exit (NULL);
    /* TODO */
    return 0;
}
/* vim:set expandtab: */
