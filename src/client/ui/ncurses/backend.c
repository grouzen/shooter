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
#define UI_MAP_PLAYER_UP '^' | A_BOLD
#define UI_MAP_PLAYER_DOWN 'v' | A_BOLD
#define UI_MAP_PLAYER_LEFT '>' | A_BOLD
#define UI_MAP_PLAYER_RIGHT '<' | A_BOLD

WINDOW *window = NULL;
struct screen screen;
uint8_t notify_line_history[NOTIFY_LINE_HISTORY_MAX][NOTIFY_LINE_MAX_LEN];
pthread_mutex_t ui_event_mutex;

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
    case 4:
        return UI_EVENT_WALK_LEFT;
        break;
    case 5:
        return UI_EVENT_WALK_RIGHT;
        break;
    case 3:
        return UI_EVENT_WALK_UP;
        break;
    case 2:
        return UI_EVENT_WALK_DOWN;
        break;
    default:
        return UI_EVENT_NONE;
        break;
    }
}

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
    pthread_mutex_unlock(&player_mutex);
    /* TODO: dispatch and colorize. */
    pthread_mutex_lock(&map_mutex);
    for(h = 1, y = screen.offset_y; h < screen.height; h++, y++) {
        for(w = 1, x = screen.offset_x; w < screen.width + 1; w++, x++) {
            mvwaddch(window, h, w, map->objs[y][x] | A_DIM);
        }
    }
    pthread_mutex_unlock(&map_mutex);
    char l[64];
    snprintf(l, 64, "of_x: %u, of_y: %u, p_x: %u, p_y: %u",
             screen.offset_x, screen.offset_y, player->pos_x, player->pos_y);
    ui_notify_line_set(l);
    pthread_mutex_lock(&player_mutex);
    mvwaddch(window, player->pos_y + 1, player->pos_x + 1, UI_MAP_PLAYER_UP | COLOR_PAIR(3));
    pthread_mutex_unlock(&player_mutex);
}

#if 0
/* Updates statusline shown at the top line of viewport */
static void ui_update_status_line(void)
{
    int i, space_width;
    char status_line[screen.width],
        hp[12],
        weapon[WEAPON_NAME_MAX_LEN + 10];
    
    /* Health points block
     * Would look like this: HP: 100/100 */
    snprintf(hp, sizeof(hp) * sizeof(char), "HP: %u/100", player->hp);
    /* Weapon block
     * Would look like this: W: [123456789] Rocket(0-100) */
    snprintf(weapon, sizeof(weapon) * sizeof(char),
             "W: %s(%u-%u)",
             weapons[player->weapons.current].name,
             weapons[player->weapons.current].damage_max,
             weapons[player->weapons.current].damage_min);

    
    /* Concateneting strings together to get resulting status line */
    strcat(status_line, nick);
    /* add spaces to center weapon_str */
    space_width = viewport_width - 2 - strlen(nick) - strlen(hp_str) - strlen(weapon_str);
    space_width = floor(space_width / 2);
    for(i = 0; i < space_width; i++)
        tmp[i] = ' ';
    tmp[space_width] = '\0';
    strcat(status_line, tmp);
    strcat(status_line, weapon_str);
    /* add more space to make hp_str right-aligned */
    space_width = viewport_width - 2 - strlen(nick) - strlen(hp_str) - strlen(weapon_str);
    space_width = ceil(space_width / 2);
    for(i = 0; i < space_width; i++)
        tmp[i] = ' ';
    tmp[space_width] = '\0';
    strcat(status_line, tmp);
    strcat(status_line, hp_str);

    /* remove previous status line from the screen */
    for(i=1; i<viewport_width; i++)
        mvwaddch(window, 1, i, ' ');
    /* put new one onto the same place */
    if(viewport_width > 82 && viewport_height > 27) {
        mvwaddstr(window, 1, 2, status_line);
    } else {
        mvwaddstr(window, 1, 1, status_line);
    }
    
    free(tmp);
    free(hp_str);
    free(weapon_str);
    free(status_line);
}
#endif

/* Applies all the changes made to screen by other functions */
void ui_refresh(void)
{
    ui_notify_line_update();
    ui_screen_update();
    wrefresh(window);
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
    
    while((key = getch()) != 'q') {
        ui_event_push(&event, key);
    }
    
    return UI_OK;
}

void ui_free(void)
{
    pthread_mutex_destroy(&ui_event_mutex);
    endwin();
}
