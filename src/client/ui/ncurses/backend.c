#include <ncurses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "../../../cdata.h"
#include "../backend.h"

/* Graphics. */
#define UI_MAP_EMPTY ' '
#define UI_MAP_WALL_FOG '#' | A_DIM | COLOR_PAIR(2)
#define UI_MAP_WALL '#' | A_BOLD | COLOR_PAIR(1)
#define UI_MAP_PLAYER '@' | A_BOLD | COLOR_PAIR(3)
#define UI_MAP_ENEMY '@' | A_BOLD | COLOR_PAIR(4)

WINDOW *window = NULL;
struct screen screen;
uint8_t notify_line_history[NOTIFY_LINE_HISTORY_MAX][NOTIFY_LINE_MAX_LEN];
pthread_mutex_t ui_event_mutex, ui_refresh_mutex;

/* The abstraction for getting
   and setting pressed keys.
*/
#define EVENT_MAX_LEN 16
/* One event can contain several pressed keys.
   This is necessary for example:
   - for diagonal movement;
   - etc.
*/
struct ui_event {
    int keys[EVENT_MAX_LEN];
    int last;
};

struct ui_event event;

static void ui_event_push(struct ui_event *e, int key)
{
    pthread_mutex_lock(&ui_event_mutex);
    if(e->last < EVENT_MAX_LEN) {
        e->keys[e->last] = key;
        e->last++;
    }
    pthread_mutex_unlock(&ui_event_mutex);
}

static int ui_event_pop(struct ui_event *e)
{
    int ret;
    
    pthread_mutex_lock(&ui_event_mutex);
    if(e->last > 0) {
        e->last--;
        ret = e->keys[e->last];
    } else {
        ret = 0;
    }
    pthread_mutex_unlock(&ui_event_mutex);
    
    return ret;
}

int ui_get_event(void)
{
    /* For a while we get only the last pressed key from struct event. */
    
    int lkey = ui_event_pop(&event);
    while(ui_event_pop(&event) != 0);
    
    switch(lkey) {
    case 'a':
        return UI_EVENT_WALK_LEFT;
        break;
    case 'd':
        return UI_EVENT_WALK_RIGHT;
        break;
    case 'w':
        return UI_EVENT_WALK_UP;
        break;
    case 's':
        return UI_EVENT_WALK_DOWN;
        break;
    default:
        return UI_EVENT_NONE;
        break;
    }
}

/* TODO: dbehavior like printf(). */
void ui_notify_line_set(uint8_t *l)
{
    int i;

    for(i = 0; i < NOTIFY_LINE_HISTORY_MAX - 1; i++) {
        strncpy((char *) notify_line_history[i + 1], (char *) notify_line_history[i], NOTIFY_LINE_MAX_LEN);
    }

    strncpy((char *) notify_line_history[0], (char *) l, NOTIFY_LINE_MAX_LEN);
}

static void ui_notify_line_update(void)
{
    int x;

    for(x = 2; x < screen.width; x++) {
        mvwaddch(window, screen.height, x, ' ');
    }

    mvwaddstr(window, screen.height, 2, (char *) notify_line_history[0]);
}

static void ui_screen_update(void)
{
    int w, h, x, y;
    
    /* Update screen's offsets. */
    pthread_mutex_lock(&player_mutex);
    pthread_mutex_lock(&map_mutex);
    
    if(player->pos_x <= screen.width / 2) {
        screen.offset_x = 0;
    } else if(player->pos_x >= map->width - screen.width / 2) {
        screen.offset_x = map->width - screen.width;
    } else {
        screen.offset_x = player->pos_x - screen.width / 2;
    }

    if(player->pos_y <= screen.height / 2) {
        screen.offset_y = 0;
    } else if(player->pos_y >= map->height - screen.height / 2) {
        screen.offset_y = map->height - screen.height + 2;
    } else {
        screen.offset_y = player->pos_y - screen.height / 2;
    }
    
    /* TODO: dispatch and colorize. */
    for(h = 2, y = screen.offset_y; h < screen.height; h++, y++) {
        for(w = 1, x = screen.offset_x; w < screen.width + 1; w++, x++) {
            if(map->objs[y][x] == MAP_PLAYER) {
                mvwaddch(window, h, w, UI_MAP_ENEMY);
            } else {
                mvwaddch(window, h, w, map->objs[y][x] | A_DIM);
            }
        }
    }
    
    mvwaddch(window, player->pos_y + 2 - screen.offset_y,
             player->pos_x + 1 - screen.offset_x, UI_MAP_PLAYER);
    
    pthread_mutex_unlock(&map_mutex);
    pthread_mutex_unlock(&player_mutex);
}

/* Applies all the changes made to screen by other functions */
void ui_refresh(void)
{
    pthread_mutex_lock(&ui_refresh_mutex);
    ui_notify_line_update();
    ui_screen_update();
    wrefresh(window);
    pthread_mutex_unlock(&ui_refresh_mutex);
}
        
enum ui_enum_t ui_init(void)
{
    char key;
    
    /* initializing ncurses mode */
    initscr();

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    setlocale(LC_ALL, "C");
    
    if(has_colors() == FALSE) {
        ui_free();
        fprintf(stderr, "Colors is not supported by this terminal.\n");
        
        return UI_ERROR;
    }
    
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);
    
    curs_set(0); /* Invisible cursor */
    refresh();

    if(LINES < 25 || COLS < 80) {
        ui_free();
        fprintf(stderr, "Terminal should be at least 80x25 in size!\n");
        
        return UI_ERROR;
    } else {
        /* box() would draw *inside* borders, so we need a window that is two
         * rows/cols wider than 80x25 */
        window = newwin(LINES, COLS, 0, 0);
        box(window, 0, 0);
        screen.width = COLS - 2;
        screen.height = LINES - 2;
    }
    
    refresh();
    ui_refresh();

    pthread_mutex_init(&ui_event_mutex, NULL);
    pthread_mutex_init(&ui_refresh_mutex, NULL);
    
    while((key = getch()) != 'q') {
        ui_event_push(&event, key);
    }
    
    return UI_OK;
}

void ui_free(void)
{
    pthread_mutex_destroy(&ui_event_mutex);
    pthread_mutex_destroy(&ui_refresh_mutex);
    endwin();
}
