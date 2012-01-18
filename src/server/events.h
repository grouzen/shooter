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

#ifndef __EVENTS_H__
#define __EVENTS_H__

void event_enemy_position(struct player*, uint16_t, uint16_t);
void event_player_position(struct player*);
void event_player_killed(struct player*, struct player*);
void event_player_hit(struct player*, struct player*, uint16_t);
void event_map_explode(uint16_t, uint16_t);
void event_on_bonus(struct player*, struct bonus*);
void event_disconnect_server(void);
void event_disconnect_notify(uint8_t*);
void event_connect_notify(struct player*);
void event_connect_ok(struct player*, uint8_t);
void send_events(void);
void event_disconnect_client(struct msg_queue_node*);
void event_connect_ask(struct msg_queue_node*);
void event_shoot(struct msg_queue_node*);
void event_walk(struct msg_queue_node*);

#endif
