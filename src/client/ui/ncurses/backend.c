#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../../cdata.h"
#include "../backend.h"

WINDOW *window = NULL;
struct ui_event *event;
struct screen screen;
struct notify_line notify_line;

void ui_notify_line_set(uint8_t *l)
{
    int i;

    for(i = 0; i < NOTIFY_LINE_HISTORY_MAX - 1; i++) {
        strncpy((char *) notify_line.history[i + 1], (char *) notify_line.history[i], NOTIFY_LINE_MAX_LEN);
    }

    strncpy((char *) notify_line.history[0], (char *) l, NOTIFY_LINE_MAX_LEN);
}

static void ui_notify_line_update(void)
{
    int i;

    for(i = 2; i < screen.width; i++) {
        mvwaddch(window, screen.height, i, ' ');
    }

    mvwaddstr(window, screen.height, 2, (char *) notify_line.history[0]);
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

/* The abstraction for getting
   and setting pressed keys.
*/
#define EVENT_MAX_LEN 16
/* One event can contain several pressed keys.
   This is necessary for example:
   - for diagonal movement;
   - etc.
*/
/* TODO: rewrite without heap usage. */
struct ui_event {
    int keys[EVENT_MAX_LEN];
    int last;
};

static struct ui_event *ui_event_init(void)
{
    struct ui_event *e = malloc(sizeof(struct ui_event));
    e->last = -1;
    
    return e;
}

static void ui_event_free(struct ui_event *e)
{
    free(e);
}

static void ui_event_push(struct ui_event *e, int key)
{
    if(e->last < EVENT_MAX_LEN - 1) {
        e->last++;
        e->keys[e->last] = key;
    }
}

static int ui_event_pop(struct ui_event *e)
{
    if(e->last >= 0) {
        return e->keys[e->last--];
    }

    return 0;
}

enum ui_event_enum_t ui_get_event(void)
{
    /* For a while we get only the last pressed key from struct event. */
    
    int lkey = ui_event_pop(event);
    while(ui_event_pop(event) != 0);

    switch(lkey) {
    case KEY_LEFT:
        return UI_EVENT_WALK_LEFT;
        break;
    case KEY_RIGHT:
        return UI_EVENT_WALK_RIGHT;
        break;
    case KEY_UP:
        return UI_EVENT_WALK_UP;
        break;
    case KEY_DOWN:
        return UI_EVENT_WALK_DOWN;
        break;
    default:
        return UI_EVENT_NONE;
        break;
    }
}

/* Applies all the changes made to screen by other functions */
void ui_refresh(void)
{
    ui_notify_line_update();
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

    if(has_colors() == FALSE) {
        endwin();
        fprintf(stderr, "Colors is not supported by this terminal.\n");
        
        return UI_ERROR;
    }
    
    start_color();

    curs_set(0); /* Invisible cursor */
    refresh();

    if(LINES < 25 || COLS < 80) {
        endwin();
        fprintf(stderr, "Terminal should be at least 80x25 in size!\n");
        
        return UI_ERROR;
    } else if (LINES >= 27 && COLS >= 82) {
        /* box() would draw *inside* borders, so we need a window that is two
         * rows/cols wider than 80x25 */
        window = newwin(LINES, COLS, 0, 0);
        box(window, 0, 0);
        screen.width = COLS - 2;
        screen.height = LINES - 2;
    } else {
        /* terminal is about 80x25 in size, so no borders are needed */
        window = newwin(LINES, COLS, 0, 0);
        screen.width = COLS;
        screen.height = LINES;
    }
    
    wrefresh(window);
    refresh();
    event = ui_event_init();
    
    while((key = getch()) != 'q') {
        ui_event_push(event, key);
    }
    
    ui_event_free(event);
    endwin();
    
    return UI_OK;
}
