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

pthread_t ui_mngr_thread, ui_event_mngr_thread, recv_mngr_thread, queue_mngr_thread;
pthread_attr_t common_attr;
pthread_mutex_t msgqueue_mutex, map_mutex, player_mutex;
pthread_cond_t queue_mngr_cond;
sem_t queue_mngr_sem;
struct msg_queue *msgqueue = NULL;
struct player *player = NULL;
struct map *map = NULL;
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
    uint8_t buf[sizeof(struct msg)];

    pthread_mutex_lock(&player_mutex);
    m->header.id = player->id;
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

    msg.type = MSGTYPE_CONNECT_ASK;
    /* TODO: get nick from config or args. */
    strncpy((char *) msg.event.connect_ask.nick, "somenick", NICK_MAX_LEN);

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
        pthread_mutex_lock(&map_mutex);
        map = map_load(mapname);
        pthread_mutex_unlock(&map_mutex);
        if(map == NULL) {
            WARN("Map couldn't be loaded: %s. Trying to load it from server...\n",
                 mapname);
            /* TODO: call event_map_load_ask() which calls map_load(). */
            quit(1);
        }

        ui_notify_line_set("Connected with id: %u, map: %s.",
                           m->event.connect_ok.id, (char *) mapname);

        pthread_mutex_lock(&player_mutex);
        player->id = m->event.connect_ok.id;
        pthread_mutex_unlock(&player_mutex);

        /* TODO: move this call to event_map_load_finish() for example. */
        pthread_create(&ui_mngr_thread, &common_attr, ui_mngr_func, NULL);
    } else {
        ui_notify_line_set("Connection failed.");
    }
}

void event_connect_notify(struct msg *m)
{
    ui_notify_line_set("New player has been connected with nick: %s.",
                       m->event.connect_notify.nick);
}

void event_disconnect_notify(struct msg *m)
{
    ui_notify_line_set("Player has been disconnected: %s.",
                       m->event.disconnect_notify.nick);
}

void event_disconnect_server(struct msg *m)
{
    ui_notify_line_set("Server has been halted. Disconnecting...");
    ui_refresh();
    sleep(3);

    quit(1);
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
    map->objs[m->event.enemy_position.pos_y - 1][m->event.enemy_position.pos_x - 1] = MAP_PLAYER;
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
    pthread_create(&ui_event_mngr_thread, &common_attr, ui_event_mngr_func, NULL);

    if(ui_init() == UI_ERROR) {
        WARN("UI couldn't init.\n");
    }

    quit(1);
}

void *recv_mngr_func(void *arg)
{
    /* TODO: ticks_finish(). */
    struct ticks *ticks;

    sem_wait(&queue_mngr_sem);
    sem_destroy(&queue_mngr_sem);

    ticks = ticks_start();

    while("zombies walk") {
        uint8_t buf[sizeof(struct msg_batch)];
        struct msg_batch msgbatch;
        uint8_t *chunk, id;

        if(recvfrom(sd, buf, sizeof(struct msg_batch), 0, NULL, NULL) < 0) {
            perror("recvfrom");
            continue;
        }

        msgbatch.size = (buf[0] * sizeof(struct msg));
        memcpy(msgbatch.chunks, buf, msgbatch.size + 1);

        pthread_mutex_lock(&msgqueue_mutex);
        while((chunk = msg_batch_pop(&msgbatch)) != NULL) {
            struct msg m;

            msg_unpack(chunk, &m);
            if(msgqueue_push(msgqueue, &m) == MSGQUEUE_ERROR) {
                WARN("msgqueue_push: couldn't push data into queue.\n");
            } else {
                pthread_mutex_lock(&player_mutex);
                player->seq++;
                pthread_mutex_unlock(&player_mutex);
            }
        }

        pthread_mutex_lock(&player_mutex);
        id = player->id;
        pthread_mutex_unlock(&player_mutex);

        if(ticks_get_diff(ticks) > 1000 / FPS || !id) {
            ticks_update(ticks);
            pthread_cond_signal(&queue_mngr_cond);
        }

        pthread_mutex_unlock(&msgqueue_mutex);

    }

    return arg;
}

void *queue_mngr_func(void *arg)
{
    struct msg *m;

    while(1) {
        uint16_t w, h;

        sem_post(&queue_mngr_sem);

        pthread_mutex_lock(&msgqueue_mutex);
        pthread_cond_wait(&queue_mngr_cond, &msgqueue_mutex);

        /* Clean map from players, bullets, bonuses and other objects. */
        if(map != NULL) {
            pthread_mutex_lock(&map_mutex);
            for(h = 0; h < map->height; h++) {
                for(w = 0; w < map->width; w++) {
                    uint8_t o = map->objs[h][w];

                    if(o == MAP_PLAYER || o == MAP_BULLET) {
                        map->objs[h][w] = MAP_EMPTY;
                    }
                }
            }
            pthread_mutex_unlock(&map_mutex);
        }

        while((m = msgqueue_pop(msgqueue)) != NULL) {
            /* TODO: just fucking do it!. */
            /* TODO: check player->id, if it's 0 than warn. */

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

        pthread_mutex_unlock(&msgqueue_mutex);

        ui_refresh();
    }
}

void quit(int signum)
{
    if(signum > 0) {
        pthread_cancel(recv_mngr_thread);
        pthread_cancel(ui_mngr_thread);
        pthread_cancel(ui_event_mngr_thread);
        pthread_cancel(queue_mngr_thread);
    }

    pthread_join(recv_mngr_thread, NULL);
    pthread_join(ui_mngr_thread, NULL);
    pthread_join(ui_event_mngr_thread, NULL);
    pthread_join(queue_mngr_thread, NULL);

    ui_free();
    event_disconnect_client();

    close(sd);
    if(map != NULL) {
        map_unload(map);
    }
    msgqueue_free(msgqueue);
    player_free(player);
    pthread_mutex_destroy(&msgqueue_mutex);
    pthread_mutex_destroy(&map_mutex);
    pthread_mutex_destroy(&player_mutex);
    pthread_cond_destroy(&queue_mngr_cond);
    pthread_attr_destroy(&common_attr);
    pthread_exit(NULL);
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
    player = player_init();

    pthread_mutex_init(&msgqueue_mutex, NULL);
    pthread_mutex_init(&map_mutex, NULL);
    pthread_mutex_init(&player_mutex, NULL);
    pthread_cond_init(&queue_mngr_cond, NULL);
    sem_init(&queue_mngr_sem, 0, 0);

    pthread_attr_init(&common_attr);
    pthread_attr_setdetachstate(&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&queue_mngr_thread, &common_attr, queue_mngr_func, NULL);
    pthread_create(&recv_mngr_thread, &common_attr, recv_mngr_func, NULL);

    event_connect_ask();

    quit(0);

    return 0;
}
