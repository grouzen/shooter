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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h> /* time() */
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#include "../cdata.h"
#include "server.h"
#include "events.h"

struct msg_queue *msgqueue = NULL;
struct players_slots *players = NULL;
struct bonuses *bonuses = NULL;
struct bullets *bullets = NULL;
pthread_mutex_t msgqueue_mutex;
pthread_cond_t queue_mngr_cond;
pthread_t recv_mngr_thread, queue_mngr_thread;
pthread_attr_t common_attr;
struct ticks *queue_mngr_ticks;
struct map *map;
int sd;

struct players_slots *players_init(void)
{
    struct players_slots *slots;
    
    slots = malloc(sizeof(struct players_slots));
    slots->root = NULL;
    slots->count = 0;
    memset(slots->slots, 0, MAX_PLAYERS * sizeof(struct players_slot *));

    return slots;
}

void players_free(struct players_slots *slots)
{
    struct players_slot *cslot, *nslot;

    nslot = slots->root;

    while(nslot != NULL) {
        cslot = nslot;
        nslot = nslot->next;

        player_free(cslot->p);
        free(cslot);
    }
    
    free(slots);
}

struct player *players_occupy(struct players_slots *slots, struct player *p) 
{
    if(slots->count < MAX_PLAYERS - 1) {
        struct players_slot *oslot = slots->root;
        struct players_slot *new = malloc(sizeof(struct players_slot));
        struct players_slot *pslot = NULL;
        int i;

        memset(new, 0, sizeof(struct players_slot));
        slots->count++;

        while(oslot != NULL) {
            pslot = oslot;
            oslot = oslot->next;
        }
        
        oslot = new;
        oslot->prev = pslot;
        oslot->next = NULL;
        oslot->p = player_init();
        memcpy(oslot->p->addr, p->addr, sizeof(struct sockaddr_in));
        strncpy((char *) oslot->p->nick, (char *) p->nick, NICK_MAX_LEN);

        if(slots->root == NULL) {
            slots->root = oslot;
        } else {
            pslot->next = oslot;
        }
        
        for(i = 0; i < MAX_PLAYERS; i++) {
            if(slots->slots[i] == NULL) {
                slots->slots[i] = oslot;
                oslot->p->id = (uint8_t) i;
                
                return oslot->p;
            }
        }
    }

    return NULL;
}

enum player_enum_t players_release(struct players_slots *slots, uint8_t id)
{
    if(slots->count > 0 && slots->slots[id] != NULL) {
        struct players_slot *cslot, *pslot, *nslot;
        slots->count--;
        
        cslot = slots->slots[id];
        nslot = cslot->next;
        pslot = cslot->prev;
        
        if(cslot == slots->root) {
            slots->root = nslot;
        } else {
            pslot->next = nslot;
        }

        if(nslot != NULL) {
            nslot->prev = pslot;
        }

        /* Make slot free. */
        player_free(cslot->p);
        free(cslot);
        
        slots->slots[id] = NULL;

        return PLAYERS_OK;
    }

    return PLAYERS_ERROR;
}

struct msg_queue *msgqueue_init(void)
{
    struct msg_queue *q;
    int i;

    q = malloc(sizeof(struct msg_queue));
    
    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        q->nodes[i].data = malloc(sizeof(struct msg));
        q->nodes[i].addr = malloc(sizeof(struct sockaddr_in));
    }
    
    q->top = -1;

    return q;
}

void msgqueue_free(struct msg_queue *q)
{
    int i;

    for(i = 0; i < MSGQUEUE_INIT_SIZE; i++) {
        free(q->nodes[i].data);
        free(q->nodes[i].addr);
    }
    
    free(q);
}

enum msg_queue_enum_t msgqueue_push(struct msg_queue *q, struct msg_queue_node *qnode)
{
    if(q->top < MSGQUEUE_INIT_SIZE - 1) {
        q->top++;
        
        memcpy(q->nodes[q->top].addr, qnode->addr, sizeof(struct sockaddr_in));
        memcpy(q->nodes[q->top].data, qnode->data, sizeof(struct msg));
        
        return MSGQUEUE_OK;
    }

    return MSGQUEUE_ERROR;
}

struct msg_queue_node *msgqueue_pop(struct msg_queue *q)
{
    if(q->top >= 0) {
        return &(q->nodes[q->top--]);
    }

    return NULL;
}

void bullet_explode(struct bullet *b)
{
    int w, h;
    
    for(w = b->x - weapons[b->type].explode_radius, h = b->y - weapons[b->type].explode_radius;
        w <= b->x + weapons[b->type].explode_radius;
        w++, h++) {
        struct players_slot *slot = players->root;
        
        if(w <= 0 || h <= 0 || w > map->width + 1 || h > map->height + 1) {
            continue;
        }

        if(map->objs[h - 1][w - 1] == MAP_WALL) {
            if(weapons[b->type].explode_map) {
                map->objs[h - 1][w - 1] = MAP_EMPTY;
                
                event_map_explode(w - 1, h - 1);
            }

            continue;
        }
        
        while(slot != NULL) {
            struct player *p = slot->p;

            if(p->pos_x == w && p->pos_y == h) {
                uint16_t damage;

                srand((unsigned int) time(NULL));

                damage = weapons[b->type].damage_min + rand() % weapons[b->type].damage_max;
                DEBUG("bullet_explode(): damage: %u, damage_min: %u, damage_max: %u.\n",
                      damage, weapons[b->type].damage_min, weapons[b->type].damage_max);
                event_player_hit(p, b->player, damage);
                

                break;
            }
            
            slot = slot->next;
        }
    }
}

struct bullets *bullets_init(void)
{
    struct bullets *bullets;

    bullets = malloc(sizeof(struct bullets));
    bullets->root = NULL;
    bullets->last = NULL;

    return bullets;
}

void bullets_free(struct bullets *bullets)
{
    struct bullets_node *cbullet, *nbullet;

    nbullet = bullets->root;

    while(nbullet != NULL) {
        cbullet = nbullet;
        nbullet = nbullet->next;

        free(cbullet->b);
        free(cbullet);
    }

    free(bullets);
}

struct bullet *bullets_add(struct bullets *bullets, struct bullet *b)
{
    struct bullets_node *last, *new;

    last = bullets->root == NULL ? bullets->root : bullets->last;

    new = malloc(sizeof(struct bullets_node));
    new->b = malloc(sizeof(struct bullet));
    new->prev = last;
    new->next = NULL;
    
    new->b->type = b->type;
    new->b->x = b->x;
    new->b->y = b->y;
    new->b->sx = b->sx;
    new->b->sy = b->sy;
    new->b->direction = b->direction;
    
    bullets->last = new;

    if(bullets->root == NULL) {
        bullets->root = new;
    }

    return new->b;
}

enum bullets_enum_t bullets_remove(struct bullets *bullets, struct bullet *b)
{
    struct bullets_node *bullet = bullets->root;
    
    while(bullet != NULL) {
        struct bullet *cb = bullet->b;

        if(cb->x == b->x && cb->y == b->y &&
           cb->type == b->type && cb->direction == b->direction &&
           cb->sx == b->sx && cb->sy == b->sy) {
            struct bullets_node *nbullet, *pbullet;

            nbullet = bullet->next;
            pbullet = bullet->prev;

            if(nbullet != NULL) {
                nbullet->prev = pbullet;
            } else {
                bullets->last = pbullet;
            }

            if(pbullet != NULL) {
                pbullet->next = nbullet;
            } else {
                bullets->root = NULL;
            }
            
            free(bullet->b);
            free(bullet);
            
            return BULLETS_OK;
        }

        bullet = bullet->next;
    }
    
    return BULLETS_ERROR;
}

void bullets_proceed(struct bullets *bullets)
{
    struct bullets_node *bullet = bullets->root;
    
    while(bullet != NULL) {
        struct bullet *b = bullet->b;
        struct weapon *w = &(weapons[b->type]);
        int bx = b->x;
        int by = b->y;
        
        for(;;) {
            struct players_slot *slot = players->root;
            
            switch(b->direction) {
            case DIRECTION_LEFT:
                if(w->bullets_distance > 0 && b->x < bx - w->bullets_speed) {
                    goto outer;
                }

                if(w->bullets_distance > 0 && b->x < b->sx - w->bullets_distance) {
                    bullet = bullet->next;
                    bullets_remove(bullets, b);
                    goto outer;
                }
                
                b->x--;
                
                break;
            case DIRECTION_RIGHT:
                if(w->bullets_distance > 0 && b->x > bx + w->bullets_speed) {
                    goto outer;
                }

                if(w->bullets_distance > 0 && b->x > b->sx + w->bullets_distance) {
                    bullet = bullet->next;
                    bullets_remove(bullets, b);
                    goto outer;
                }
                
                b->x++;
                
                break;
            case DIRECTION_UP:
                if(w->bullets_distance > 0 && b->y < by - w->bullets_speed) {
                    goto outer;
                }

                if(w->bullets_distance > 0 && b->y < b->sy - w->bullets_distance) {
                    bullet = bullet->next;
                    bullets_remove(bullets, b);
                    goto outer;
                }
                
                b->y--;
                
                break;
            case DIRECTION_DOWN:
                if(w->bullets_distance > 0 && b->y > by + w->bullets_speed) {
                    goto outer;
                }

                if(w->bullets_distance > 0 && b->y > b->sy + w->bullets_distance) {
                    bullet = bullet->next;
                    bullets_remove(bullets, b);
                    goto outer;
                }
                
                b->y++;
                
                break;
            default:
                break;
            }

            if(b->y <= 0 || b->x <= 0 ||
               b->y >= map->height - 1 || b->x >= map->width - 1 ||
               map->objs[b->y - 1][b->x - 1] == MAP_WALL) {
                bullet = bullet->next;
                bullet_explode(b);
                bullets_remove(bullets, b);
                goto outer;
            }

            while(slot != NULL) {
                struct player *p = slot->p;

                if(p->pos_x == b->x && p->pos_y == b->y) {
                    bullet = bullet->next;
                    bullet_explode(b);
                    bullets_remove(bullets, b);
                    goto outer;
                }

                slot = slot->next;
            }
        }
        // TODO: remove this hack
        bullet = bullet->next;
    outer:
        continue;
    }
}

struct bonuses *bonuses_init(void)
{
    struct bonuses *bonuses;

    bonuses = malloc(sizeof(struct bonuses));
    bonuses->root = NULL;
    bonuses->count = 0;

    return bonuses;
}

void bonuses_free(struct bonuses *bonuses)
{
    struct bonuses_node *cbonus, *nbonus;

    nbonus = bonuses->root;
    
    while(nbonus != NULL) {
        cbonus = nbonus;
        nbonus = nbonus->next;

        free(cbonus->b);
        free(cbonus);
    }

    free(bonuses);
}

struct bonus *bonuses_search(struct bonuses *bonuses, uint16_t x, uint16_t y)
{
    struct bonuses_node *bonus = bonuses->root;

    while(bonus != NULL) {
        struct bonus *b = bonus->b;

        if(b->x == x && b->y == y) {
            return b;
        }

        bonus = bonus->next;
    }

    return NULL;
}

struct bonus *bonuses_add(struct bonuses *bonuses, struct bonus *b)
{
    struct bonuses_node *bonus = bonuses->root;
    struct bonuses_node *pbonus = NULL;

    for(;;) {
        if(bonus == NULL) {
            bonus = malloc(sizeof(struct bonuses_node));
            bonus->prev = pbonus;
            bonus->next = NULL;
            bonus->b = malloc(sizeof(struct bonus));
            memcpy(bonus->b, b, sizeof(struct bonus));

            break;
        }

        pbonus = bonus;
        bonus = bonus->next;
    }

    return bonus->b;
}

enum bonuses_enum_t bonuses_remove(struct bonuses *bonuses, struct bonus *b)
{
    struct bonuses_node *bonus = bonuses->root;

    while(bonus != NULL) {
        if(bonus->b == b) {
            struct bonuses_node *prev, *next;

            prev = bonus->prev;
            next = bonus->next;

            prev->next = next;
            next->prev = prev;

            free(bonus->b);
            free(bonus);

            return BONUSES_OK;
        }

        bonus = bonus->next;
    }

    return BONUSES_ERROR;
}

/* This thread must do the only one thing - recieves
 * messages from many clients and then pushs them to msgqueue.
 */
void *recv_mngr_func(void *arg)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct msg_queue_node *qnode;
        
    qnode = malloc(sizeof(struct msg_queue_node));
    qnode->data = malloc(sizeof(struct msg));
    qnode->addr = malloc(sizeof(struct sockaddr_in));

    queue_mngr_ticks = ticks_start();
    
    while("hope is not dead") {
        uint8_t buf[sizeof(struct msg)];
        
        if(ticks_get_diff(queue_mngr_ticks) > 1000 / FPS) {
            ticks_update(queue_mngr_ticks);
            pthread_cond_signal(&queue_mngr_cond);
        }
        
        if(recvfrom(sd, buf, sizeof(struct msg), 0,
                    (struct sockaddr *) &client_addr, &client_addr_len) < 0) {
            perror("server: recvfrom");
            continue;
        }
        
        msg_unpack(buf, qnode->data);
        qnode->addr = &client_addr;
        pthread_mutex_lock(&msgqueue_mutex);
        if(msgqueue_push(msgqueue, qnode) == MSGQUEUE_ERROR) {
            INFO("server: msgqueue_push: couldn't push data into queue.\n");
        }
        pthread_mutex_unlock(&msgqueue_mutex);
    }

    return arg;
}
    
void *queue_mngr_func(void *arg)
{
    struct msg_queue_node *qnode;

    sleep(1);
    
    while("teh internetz exists") {        
        pthread_cond_wait(&queue_mngr_cond, &msgqueue_mutex);

        /* Handle messages(events). */
        while((qnode = msgqueue_pop(msgqueue)) != NULL) {
            /* TODO: check seq. */

            switch(qnode->data->type) {
            case MSGTYPE_CONNECT_ASK:
                event_connect_ask(qnode);                
                break;
            case MSGTYPE_DISCONNECT_CLIENT:
                event_disconnect_client(qnode);
                break;
            case MSGTYPE_WALK:
                event_walk(qnode);
                break;
            case MSGTYPE_SHOOT:
                event_shoot(qnode);
                break;
            default:
                INFO("Unknown event\n");
                break;
            }
        }

        send_events();
        
        pthread_mutex_unlock(&msgqueue_mutex);
    }
}

void quit(int signum)
{
    if(signum > 0) {
        pthread_cancel(recv_mngr_thread);
        pthread_cancel(queue_mngr_thread);
    }
    
    pthread_join(recv_mngr_thread, NULL);
    pthread_join(queue_mngr_thread, NULL);
    
    event_disconnect_server();
    send_events();
    
    close(sd);
    map_unload(map);
    msgqueue_free(msgqueue);
    players_free(players);
    bonuses_free(bonuses);
    pthread_attr_destroy(&common_attr);
    pthread_mutex_destroy(&msgqueue_mutex);
    pthread_cond_destroy(&queue_mngr_cond);
    pthread_exit(NULL);
}

/* TODO: write function sync_mngr_func()
 * which will be check seq number of
 * each client and if necessary send
 * to him current world state.
 */
int main(int argc, char **argv)
{
    struct sockaddr_in server_addr;
    
    signal(SIGINT, quit);
    signal(SIGHUP, quit);
    signal(SIGQUIT, quit);

    /* TODO: from config or args. */
    map = map_load((uint8_t *) "default.map");
    if(map == NULL) {
        INFO("Map couldn't be loaded: %s.\n", "maze.map");
        exit(EXIT_FAILURE);
    }
    
    msgqueue = msgqueue_init();
    players = players_init();
    bonuses = bonuses_init();
    bullets = bullets_init();

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6006);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sd = socket(server_addr.sin_family, SOCK_DGRAM, 0);
    if(sd < 0) {
        perror("server: socket");
        exit(EXIT_FAILURE);
    }
    
    if(bind(sd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("server: bind");
        exit(EXIT_FAILURE);
    }
    
    pthread_mutex_init(&msgqueue_mutex, NULL);
    pthread_cond_init(&queue_mngr_cond, NULL);
    
    pthread_attr_init(&common_attr);
    pthread_attr_setdetachstate(&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&recv_mngr_thread, &common_attr, recv_mngr_func, NULL);
    pthread_create(&queue_mngr_thread, &common_attr, queue_mngr_func, NULL);

    quit(0);
    
    return 0;
}
