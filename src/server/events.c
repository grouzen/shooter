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

void event_seqnotify_all (struct msg *msg, struct player *exclude)
{
    register size_t i = 0u;
    for (; i < MAX_PLAYERS; i++)
    {
        /* skip user, if nick not defined: user not allocated */
        if (players->player[i].nick ||\
                (exclude && exclude->id != players->player[i].id))
        {
            players->player[i].seq++;
            msg->header.seq = players->player[i].seq;
            memcpy (msg->header.key, players->player[i].key,\
                     sizeof (msg->header.key));
            msg_batch_push (&players->player[i].msgbatch, msg);
        }
    }
}

void event_enemy_position(struct player *p, uint16_t x, uint16_t y)
{
    struct msg msg;

    memcpy (msg.header.key, p->key, sizeof (msg.header.key));
    msg.header.seq = p->seq++;

    msg.type = MSGTYPE_ENEMY_POSITION;
    msg.event.enemy_position.pos_x = x;
    msg.event.enemy_position.pos_y = y;
    msg_batch_push(&(p->msgbatch), &msg);
}

void event_player_position(struct player *p)
{
    struct msg msg;

    memcpy (msg.header.key, p->key, sizeof (msg.header.key));
    msg.header.seq = p->seq++;

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

    memcpy (msg.header.key, ptarget->key, sizeof (msg.header.key));
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

    msg.type = MSGTYPE_MAP_EXPLODE;
    msg.event.map_explode.w = w;
    msg.event.map_explode.h = h;

    INFO("Map has been destroyed at %ux%u.\n", w, h);
    event_seqnotify_all (&msg, NULL);
}

void event_on_bonus(struct player *p, struct bonus *bonus)
{
    struct msg msg;

    memcpy (msg.header.key, p->key, sizeof (msg.header.key));
    msg.header.seq = p->seq++;

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
    struct msg msg;

    msg.type = MSGTYPE_DISCONNECT_SERVER;
    msg.event.disconnect_server.stub = 1;

    event_seqnotify_all (&msg, NULL);
 }

void event_disconnect_notify(uint8_t *nick)
{
    struct msg msg;

    msg.type = MSGTYPE_DISCONNECT_NOTIFY;
    strncpy((char *) msg.event.disconnect_notify.nick,
            (char *) nick, NICK_MAX_LEN);

    event_seqnotify_all (&msg, NULL);
}

void event_connect_notify(struct player *p)
{
    struct msg msg;

    msg.type = MSGTYPE_CONNECT_NOTIFY;
    strncpy((char *) msg.event.connect_notify.nick,
            (char *) p->nick, NICK_MAX_LEN);

    event_seqnotify_all (&msg, p);
}

void event_connect_ok(struct player *p)
{
    struct msg msg;

    memcpy (msg.header.key, p->key, sizeof (msg.header.key));
    msg.header.seq = p->seq++;


    /* TODO: change this event.
       Map name is not neccesary any more, because map
       must be loaded from server each time.
     */
    msg.type = MSGTYPE_CONNECT_OK;
    msg.event.connect_ok.ok = 1;
    strncpy((char *) msg.event.connect_ok.mapname,
            (char *) map->name, MAP_NAME_MAX_LEN);
    msg_batch_push(&(p->msgbatch), &msg);
}

void send_events(void)
{
    register size_t li;
    register size_t ri;
    register struct player *rplayer;
    register struct player *lplayer;
    bullets_proceed(bullets);

    /* Send diff to each player. */
    for (li = 0u; li < MAX_PLAYERS; li++)
    {
        /* skip unused slot */
        if (!players->player[li].nick)
            continue;
        lplayer = &players->player[li];
        for (ri = 0u; ri < MAX_PLAYERS; ri++)
        {
            if (!players->player[ri].nick)
                continue;
            rplayer = &players->player[ri];
            if (IN_PLAYER_VIEWPORT (rplayer->pos_x, rplayer->pos_y,\
                        lplayer->pos_x, lplayer->pos_y))
            {
                event_enemy_position (lplayer, rplayer->pos_x, rplayer->pos_y);
            }
        }
        /* flush batch */
        if (MSGBATCH_SIZE (&lplayer->msgbatch) > 0)
        {
            send_to_player (lplayer->msgbatch.chunks,\
                            lplayer->msgbatch.size + 1, lplayer);
        }
        /* Refresh msgbatch for each player. */
        memset (&lplayer->msgbatch, 0, sizeof (struct msg_batch));
    }
}

void event_disconnect_client(struct msg_queue_node *qnode)
{
    uint8_t nick[NICK_MAX_LEN];

    /* Copy nick of the disconnected player. */
    if(qnode->player->nick) {
        strncpy((char *) nick,\
                (const char *)qnode->player->nick,\
                NICK_MAX_LEN);
    }

    if(players_release(players, qnode->player->id) == PLAYERS_ERROR) {
        WARN("Couldn't remove the player from slots: %u\n", qnode->player->id);
        return;
    }

    INFO("Player %s disconnect.\n", nick);

    event_disconnect_notify(nick);
}

void event_connect_ask(struct msg_queue_node *qnode)
{
    struct player *newplayer = NULL;
    newplayer = players_occupy(players, qnode->addr, qnode->data->event.connect_ask.nick);

    if(newplayer == NULL) {
        struct msg msg;
        struct msg_batch msgbatch;

        WARN("There are no free slots. Server maintains maximum %u players\n", MAX_PLAYERS);

        msg.type = MSGTYPE_CONNECT_OK;
        msg.event.connect_ok.ok = 0;

        memset(&msgbatch, 0, sizeof(struct msg_batch));
        msg_batch_push(&msgbatch, &msg);

        send_to(msgbatch.chunks, msgbatch.size + 1,
               (struct sockaddr *) qnode->addr,
               sizeof(struct sockaddr_storage));
    } else {
        struct map_respawn *respawn;
        struct bonus bonus = {
            .type = BONUSTYPE_WEAPON,
            .index = WEAPON_GUN,
            .x = 0,
            .y = 0
        };

        INFO ("New player, %s, count: %u\n", newplayer->nick, players->count);
        INFO ("| assign key: '" KEY_FORMAT "'\n", KEY_EXPAND (newplayer->key));

        /* Connect new player. */
        event_connect_ok(newplayer);

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
    struct player *p = qnode->player;
    struct bullet b = {
        .player = p,
        .type = p->weapons.current,
        .x = p->pos_x,
        .y = p->pos_y,
        .sx = p->pos_x,
        .sy = p->pos_y,
        .direction = qnode->data->event.shoot.direction
    };

    if(p->weapons.bullets[p->weapons.current] > 0) {
        p->weapons.bullets[p->weapons.current]--;
        bullets_add(bullets, &b);
    }
}


void event_walk(struct msg_queue_node *qnode)
{
    struct player *p = qnode->player;
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
/* vim:set expandtab: */
