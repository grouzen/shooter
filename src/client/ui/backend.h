#ifndef __BACKEND_H__
#define __BACKEND_H__

/* UI event types. */
enum ui_event_enum_t {
    UI_EVENT_NONE,
    UI_EVENT_WALK_UP,
    UI_EVENT_WALK_DOWN,
    UI_EVENT_WALK_LEFT,
    UI_EVENT_WALK_RIGHT
};

struct screen {
    uint16_t width;
    uint16_t height;
    uint16_t offset_x;
    uint16_t offset_y;
};

#define NOTIFY_LINE_HISTORY_MAX 8
#define NOTIFY_LINE_MAX_LEN 64

struct notify_line {
    uint8_t history[NOTIFY_LINE_HISTORY_MAX][NOTIFY_LINE_MAX_LEN];
    uint8_t count;
};

enum ui_enum_t {
    UI_ERROR = 0,
    UI_OK
};

/* Backend's API. These functions should implement each backend. */
void ui_refresh(void);
enum ui_event_enum_t ui_get_event(void);
enum ui_enum_t ui_init(void);
void ui_notify_line_set(uint8_t*);

/* Export some global variables from client.c file. */
extern struct player *player;
extern struct map *map;

#endif
