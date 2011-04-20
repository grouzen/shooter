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

enum ui_enum_t {
    UI_ERROR = 0,
    UI_OK
};

/* Backend's API. These functions should implement each backend. */
void ui_refresh(void);
enum ui_event_enum_t ui_get_event(void);
enum ui_enum_t ui_init(void);

/* Export some global variables from client.c file. */
extern struct player *player;
extern struct map *map;

#endif
