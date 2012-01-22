/* Copyright (c) 2011 Michael Nedokushev <grouzen.hexy@gmail.com>
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
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#include "../cdata.h"
#include "server.h"

void event_enemy_position(struct player *p, uint16_t x, uint16_t y)
{
    struct msg msg;
    
    p->seq++;
    
    msg.type = MSGTYPE_ENEMY_POSITION;
    msg.event.enemy_position.pos_x = x;
    msg.event.enemy_position.pos_y = y;
    msg_batch_push(&(p->msgbatch), &msg);
}

void event_player_position(struct player *p)
{
    struct msg msg;

    p->seq++;
    
    msg.type = MSGTYPE_PLAYER_POSITION;
    msg.event.player_position.pos_x = p->pos_x;
    msg.event.player_position.pos_y = p->pos_y;
    msg_batch_push(&(p->msgbatch), &msg);
}

void event_player_killed(struct player *ptarget, struct player *pkiller)
{
    INFO("Player %s kills %s.\n", pkiller->nick, ptarget->nick);
    return;
}

void event_player_hit(struct player *ptarget, struct player *pkiller, uint16_t damage)
{
    struct msg msg;

    
    INFO("Player %s hits %s, damage: %u\n", pkiller->nick, ptarget->nick, damage);
    
    ptarget->seq++;

    if(ptarget->armor > damage / 2) {
        ptarget->armor -= damage / 2;
        damage -= damage / 2;
    } else if(ptarget->armor > 0) {
        damage -= damage - ptarget->armor;
        ptarget->armor = 0;
    }

    if(ptarget->hp > damage) {
        ptarget->hp -= damage;
    } else {
        ptarget->hp = 0;
        event_player_killed(ptarget, pkiller);

        return;
    }

    msg.type = MSGTYPE_PLAYER_HIT;
    msg.event.player_hit.hp = ptarget->hp;
    msg.event.player_hit.armor = ptarget->armor;
    msg_batch_push(&(ptarget->msgbatch), &msg);
}

void event_map_explode(uint16_t w, uint16_t h)
{
    struct msg msg;
    struct players_slot *slot = players->root;

    msg.type = MSGTYPE_MAP_EXPLODE;
    msg.event.map_explode.w = w;
    msg.event.map_explode.h = h;

    INFO("Map has been destroyed at %ux%u.\n", w, h);
    
    while(slot != NULL) {
        struct player *p = slot->p;

        p->seq++;

        msg_batch_push(&(p->msgbatch), &msg);
        
        slot = slot->next;
    }
}

void event_on_bonus(struct player *p, struct bonus *bonus)
{
    struct msg msg;
    
    p->seq++;

    switch(bonus->type) {
    case BONUSTYPE_WEAPON:
        p->weapons.bullets[bonus->index] = weapons[bonus->index].bullets_count;
        INFO("Player %s get bonus: weapon - %s.\n",
             p->nick, weapons[bonus->index].name);
        break;
    default:
        break;
    }
    
    msg.type = MSGTYPE_ON_BONUS;
    msg.event.on_bonus.type = bonus->type;
    msg.event.on_bonus.index = bonus->index;
    msg_batch_push(&(p->msgbatch), &msg);
}

void event_disconnect_server(void)
{
    struct players_slot *slot = players->root;
    struct msg msg;

    msg.type = MSGTYPE_DISCONNECT_SERVER;
    msg.event.disconnect_server.stub = 1;
    
    while(slot != NULL) {
        struct player *p = slot->p;

        p->seq++;
        msg_batch_push(&(p->msgbatch), &msg);
        
        slot = slot->next;
    }
}

void event_disconnect_notify(uint8_t *nick)
{
    struct msg msg;
    struct players_slot *slot = players->root;
    
    msg.type = MSGTYPE_DISCONNECT_NOTIFY;
    strncpy((char *) msg.event.disconnect_notify.nick, (char *) nick, NICK_MAX_LEN);    
    
    slot = players->root;
    while(slot != NULL) {
        struct player *p = slot->p;
        
        p->seq++;
        msg_batch_push(&(p->msgbatch), &msg);
        
        slot = slot->next;
    }
}

void event_connect_notify(struct player *p)
{
    struct msg msg;
    struct players_slot *slot = players->root;
    
    msg.type = MSGTYPE_CONNECT_NOTIFY;
    strncpy((char *) msg.event.connect_notify.nick, (char *) p->nick, NICK_MAX_LEN);
    
    while(slot != NULL) {
        struct player *lp = slot->p;
            
        if(lp != p) {
            lp->seq++;
            msg_batch_push(&(lp->msgbatch), &msg);
        }
            
        slot = slot->next;
    }
}

void event_connect_ok(struct player *p, uint8_t ok)
{
    struct msg msg;
    
    p->seq++;

    msg.type = MSGTYPE_CONNECT_OK;
    msg.event.connect_ok.id = p->id;
    msg.event.connect_ok.ok = ok;
    strncpy((char *) msg.event.connect_ok.mapname, (char *) map->name, MAP_NAME_MAX_LEN);
    msg_batch_push(&(p->msgbatch), &msg);
}

void send_events(void)
{
    struct players_slot *slot = players->root;

    bullets_proceed(bullets);
    
    /* Send diff to each player. */
    slot = players->root;
    while(slot != NULL) {
        struct player *p = slot->p;
        struct players_slot *lslot = players->root;

        while(lslot != NULL) {
            struct player *lp = lslot->p;

            if(IN_PLAYER_VIEWPORT(lp->pos_x, lp->pos_y, p->pos_x, p->pos_y)) {
                event_enemy_position(p, lp->pos_x, lp->pos_y);
            }

            lslot = lslot->next;
        }
        
        if(MSGBATCH_SIZE(&(p->msgbatch)) > 0) {
            sendto(sd, p->msgbatch.chunks, p->msgbatch.size + 1,
                   0, (struct sockaddr *) p->addr, sizeof(struct sockaddr_in));
        }
        
        slot = slot->next;
    }

    slot = players->root;
    /* Refresh msgbatch for each player. */
    while(slot != NULL) {
        memset(&(slot->p->msgbatch), 0, sizeof(struct msg_batch));
        slot = slot->next;
    }
        
}

void event_disconnect_client(struct msg_queue_node *qnode)
{
    uint8_t nick[NICK_MAX_LEN];

    /* Copy nick of the disconnected player. */
    if(players->slots[qnode->data->header.id] != NULL) {
        strncpy((char *) nick, (char *) players->slots[qnode->data->header.id]->p->nick, NICK_MAX_LEN);
    }

    if(players_release(players, qnode->data->header.id) == PLAYERS_ERROR) {
        WARN("Couldn't remove the player from slots: %u\n", qnode->data->header.id);
        return;
    }

    INFO("Player %s disconnect.\n", nick);

    event_disconnect_notify(nick);    
}

void event_connect_ask(struct msg_queue_node *qnode)
{
    struct player player;
    struct player *newplayer;
    
    player.addr = qnode->addr;
    player.nick = qnode->data->event.connect_ask.nick;

    newplayer = players_occupy(players, &player);
    
    if(newplayer == NULL) {
        struct msg msg;
        struct msg_batch msgbatch;
        
        WARN("There are no free slots. Server maintains maximum %u players\n", MAX_PLAYERS);

        msg.type = MSGTYPE_CONNECT_OK;
        msg.event.connect_ok.ok = 0;

        memset(&msgbatch, 0, sizeof(struct msg_batch));
        msg_batch_push(&msgbatch, &msg);
        
        sendto(sd, msgbatch.chunks, msgbatch.size + 1,
               0, (struct sockaddr *) qnode->addr, sizeof(struct sockaddr_in));
    } else {
        struct map_respawn *respawn;
        struct bonus bonus = {
            .type = BONUSTYPE_WEAPON,
            .index = WEAPON_GUN,
            .x = 0,
            .y = 0
        };
        
        INFO("New player connected with nick %s. Total players: %u.\n",
             newplayer->nick, players->count);
        
        /* Connect new player. */
        event_connect_ok(newplayer, 1);
        
        /* Get random respawn point. */
        srand((unsigned int) time(NULL));
        respawn = &(map->respawns[0 + rand() % map->respawns_count]);
        newplayer->pos_x = respawn->w + 1;
        newplayer->pos_y = respawn->h + 1;
        
        event_player_position(newplayer);
        
        /* Give to the player the weapon. */
        event_on_bonus(newplayer, &bonus);
        
        /* Notify rest players about new player. */
        event_connect_notify(newplayer);
    }
}


void event_shoot(struct msg_queue_node *qnode)
{
    struct player *p = players->slots[qnode->data->header.id]->p;
    struct bullet b = {
        .player = p,
        .type = p->weapons.current,
        .x = p->pos_x,
        .y = p->pos_y,
        .sx = p->pos_x,
        .sy = p->pos_y,
        .direction = p->direction
    };

    if(p->weapons.bullets[p->weapons.current] > 0) {
        p->weapons.bullets[p->weapons.current]--;
        bullets_add(bullets, &b);
    }
}


void event_walk(struct msg_queue_node *qnode)
{
    struct player *p = players->slots[qnode->data->header.id]->p;
    uint16_t px, py;

    px = p->pos_x;
    py = p->pos_y;
    
    p->direction = qnode->data->event.walk.direction;
    
    switch(p->direction) {
    case DIRECTION_LEFT:
        p->pos_x--;
        break;
    case DIRECTION_RIGHT:
        p->pos_x++;
        break;
    case DIRECTION_UP:
        p->pos_y--;
        break;
    case DIRECTION_DOWN:
        p->pos_y++;
        break;
    default:
        break;
    }

    if(collision_check_player(p, map, players) != COLLISION_NONE) {
        p->pos_x = px;
        p->pos_y = py;
    }
    
    event_player_position(p);
}
