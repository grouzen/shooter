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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#define MSGQUEUE_INIT_SIZE (MSGBATCH_INIT_SIZE * 3)

struct msg_queue {
    struct msg *data[MSGQUEUE_INIT_SIZE];
    ssize_t top;
};

struct msg_queue *msgqueue_init(void);
void msgqueue_free(struct msg_queue*);
enum msg_queue_enum_t msgqueue_push(struct msg_queue*, struct msg*);
struct msg *msgqueue_pop(struct msg_queue*);
void quit(int);
void send_event(struct msg*);
void event_disconnect_client(void);
void event_connect_ask(void);
void event_disconnect_server(struct msg*);
void event_connect_ok(struct msg*);
void event_connect_notify(struct msg*);
void event_disconnect_notify(struct msg*);
void *recv_mngr_func(void*);
void *queue_mngr_func(void*);
void *ui_mngr_func(void*);


#endif
