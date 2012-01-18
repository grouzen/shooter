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

#ifndef __BACKEND_H__
#define __BACKEND_H__

/* UI event types. */
enum {
    UI_EVENT_NONE,
    UI_EVENT_WALK_UP,
    UI_EVENT_WALK_DOWN,
    UI_EVENT_WALK_LEFT,
    UI_EVENT_WALK_RIGHT,
    UI_EVENT_SHOOT
};

struct screen {
    uint16_t width;
    uint16_t height;
    uint16_t offset_x;
    uint16_t offset_y;
};

enum ui_enum_t {
    UI_ERROR = 0,
    UI_OK
};

/* Backend's API. These functions should implement each backend. */
void ui_refresh(void);
int ui_get_event(void);
enum ui_enum_t ui_init(void);
void ui_free(void);
void ui_notify_line_set(char *format, ...);

#define NOTIFY_LINE_HISTORY_MAX 8
#define NOTIFY_LINE_MAX_LEN 64

/* Export some global variables from client.c file. */
extern struct player *player;
extern struct map *map;
extern pthread_mutex_t map_mutex, player_mutex;

#endif
