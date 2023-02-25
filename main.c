/*
 * Copyright 2022-2023 Canonical Ltd.
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

/*
 * This program presents a menu of installation ISOs that we can possibly
 * chain-boot to.  It uses JSON information obtained from SimpleStreams to
 * provide the list of ISOs with a friendly label.
 *
 * The menu is styled to have an appearance that is as close to Subiquity as
 * possible.
 *
 * Input is a file or files similar to
 * http://cdimage.ubuntu.com/streams/v1/com.ubuntu.cdimage.daily:ubuntu-server.json
 *
 * The chosen ISO is output in a format friendly for the /bin/sh source
 * built-in - sample output:
 *
 * MEDIA_URL="https://releases.ubuntu.com/kinetic/ubuntu-22.10-live-server-amd64.iso"
 * MEDIA_LABEL="Ubuntu Server 22.10 (Kinetic Kudu)"
 * MEDIA_SIZE="1642631168"
 */

#include "common.h"

#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdnoreturn.h>
#include <sys/param.h>

#include <ncurses.h>
#include <menu.h>

#include "args.h"
#include "json.h"

noreturn void usage(char *prog)
{
    fprintf(stderr,
            "usage: %s <output path> <input json> [<input json> ...]\n",
            prog);
    exit(1);
}

typedef struct _colors {
    int ubuntu_orange;
    int text_white;
    int back_green;
    int black;

    short black_orange;
    short white_orange;
    short white_green;
} resources_t;

choices_t *read_iso_choices(args_t *args)
{
    choices_t *choices = choices_create(args->num_infiles);
    for(int i = 0; i < args->num_infiles; i++) {
        choices->values[i] = get_newest_iso(args->infiles[i], ARCH);
    }
    return choices;
}

int horizontal_center(int len)
{
    return (COLS - len) / 2;
}

int vertical_center(int len)
{
    /* accounts for 3 line banner */
    return 3 + (LINES - 3 - len) / 2;
}

void draw_line(int y, wchar_t *wchar, short color)
{
    cchar_t cell;
    setcchar(&cell, wchar, 0, color, NULL);
    mvhline_set(y, 0, &cell, COLS);
}

void top_banner(resources_t *resources, char *label)
{
    /* Simulate the banner from Subiquity.
     * - draw black on orange for the half-block rows
     * - draw white on orange for the text row */

    /* half-block upper */
    draw_line(0, L"\u2580", resources->black_orange);
    draw_line(1, L" ", resources->white_orange);
    /* half-block lower */
    draw_line(2, L"\u2584", resources->black_orange);

    attron(COLOR_PAIR(resources->white_orange));
    mvaddstr(1, horizontal_center(strlen(label)), label);
    attroff(COLOR_PAIR(resources->white_orange));
}

ITEM *button_item(char *label, int textwidth)
{
    /* Simulate the appearance of buttons in Subiquity.
     * The unicode character is the right-pointing smaller tringle arrow. */
    char *text = saprintf("[ %-*s \u25b8 ]", textwidth, label);
    return new_item(text, NULL);
}

typedef struct _menu {
    WINDOW *window;
    MENU *menu;
    ITEM **items;
    choices_t *choices;
} menu_t;

iso_data_t *menu_get_selected_item(menu_t *menu)
{
    int idx = item_index(current_item(menu->menu));
    return menu->choices->values[idx];
}

void menu_free(menu_t *menu)
{
    if(!menu) return;

    for(int i = 0; menu->items[i]; i++) {
        free((void *)item_name(menu->items[i]));
        free_item(menu->items[i]);
    }

    delwin(menu->window);
    free_menu(menu->menu);
    free(menu->items);
    free(menu);
}

menu_t *menu_create(resources_t *resources, choices_t *choices)
{
    menu_t *ret = calloc(sizeof(menu_t), 1);
    ret->choices = choices;
    ret->items = calloc(sizeof(ITEM *), choices->len + 1);

    int longest = 0;
    for(int i = 0; i < choices->len; i++) {
        longest = MAX(longest, (int)strlen(choices->values[i]->label));
    }
    for(int i = 0; i < choices->len; i++) {
        ret->items[i] = button_item(choices->values[i]->label, longest);
    }

    /* The +6 accounts for the button text around the label. */
    int width = longest + 6;

    int center_x = horizontal_center(width);
    int center_y = vertical_center(choices->len);

    ret->window = newwin(choices->len, width, center_y, center_x);
    keypad(ret->window, TRUE);

    ret->menu = new_menu(ret->items);
    set_menu_win(ret->menu, ret->window);
    set_menu_sub(ret->menu, ret->window);
    set_menu_mark(ret->menu, NULL);
    set_menu_fore(ret->menu, COLOR_PAIR(resources->white_green));
    post_menu(ret->menu);

    return ret;
}

int color_byte_to_ncurses(uint8_t color_byte)
{
    return color_byte / 255.0 * 1000;
}

void init_color_from_bytes(short color,
                           uint8_t byte_r, uint8_t byte_g, uint8_t byte_b)
{
    int nc_r = color_byte_to_ncurses(byte_r);
    int nc_g = color_byte_to_ncurses(byte_g);
    int nc_b = color_byte_to_ncurses(byte_b);
    init_color(color, nc_r, nc_g, nc_b);
}

void write_output(char *fname, iso_data_t *iso_data)
{
    FILE *f = fopen(fname, "w");
    if(!f) {
        syslog(LOG_ERR, "failed to open output file [%s]: %m", fname);
        exit(1);
    }

    syslog(LOG_DEBUG, "selected: %s", iso_data->label);

    fprintf(f, "MEDIA_URL=\"%s\"\n", iso_data->url);
    fprintf(f, "MEDIA_LABEL=\"%s\"\n", iso_data->label);
    fprintf(f, "MEDIA_256SUM=\"%s\"\n", iso_data->sha256sum);
    fprintf(f, "MEDIA_SIZE=\"%" PRId64 "\"\n", iso_data->size);
    fclose(f);
}

void exit_cb(void)
{
    erase();
    refresh();
    endwin();
}

void resources_free(resources_t *resources)
{
    free(resources);
}

resources_t *resources_create()
{
    resources_t *ret = calloc(sizeof(resources_t), 1);
    ret->ubuntu_orange = COLOR_RED;
    ret->text_white = COLOR_WHITE;
    ret->back_green = COLOR_GREEN;
    ret->black = COLOR_BLACK;

    syslog(LOG_DEBUG, "can_change_color [%d]", can_change_color());
    if(can_change_color()) {
        init_color_from_bytes(ret->ubuntu_orange, 0xE9, 0x54, 0x20);
        init_color_from_bytes(ret->text_white, 0xFF, 0xFF, 0xFF);
        init_color_from_bytes(ret->back_green, 0x0E, 0x84, 0x20);
        init_color_from_bytes(ret->black, 0x00, 0x00, 0x00);
    } else {
        /* These are terminal 256 color codes, see
         * https://www.ditig.com/256-colors-cheat-sheet for an example. */
        ret->ubuntu_orange = 202;  /* not really but kinda close */
        ret->text_white = 231;
        ret->back_green = 28;
        ret->black = 0;
    }

    short color_pair_ids = 1;
    ret->black_orange = color_pair_ids++;
    init_pair(ret->black_orange, ret->black, ret->ubuntu_orange);
    ret->white_orange = color_pair_ids++;
    init_pair(ret->white_orange, ret->text_white, ret->ubuntu_orange);
    ret->white_green = color_pair_ids++;
    init_pair(ret->white_green, ret->text_white, ret->back_green);
    return ret;
}

int main(int argc, char **argv)
{
    args_t *args = args_create(argc, argv);
    if(!args) usage(argv[0]);

    setlocale(LC_ALL, "C.UTF-8");

    choices_t *iso_info = read_iso_choices(args);
    if(!iso_info) {
        syslog(LOG_ERR, "failed to read JSON data");
        return 1;
    }

    if(!initscr()) {
        syslog(LOG_ERR, "initscr failure");
        return 1;
    }

    atexit(exit_cb);

    noecho();

    if(!has_colors()) {
        syslog(LOG_ERR, "has_colors failure");
        return 1;
    }

    if(start_color() == ERR) {
        syslog(LOG_ERR, "start_color failure");
        return 1;
    }

    keypad(stdscr, TRUE);
    cbreak();
    curs_set(0); /* hide */

    resources_t *resources = resources_create();
    top_banner(resources, "Choose an Ubuntu version to install");

    menu_t *menu = menu_create(resources, iso_info);
    refresh();

    bool continuing = true;
    while(continuing) {
        wrefresh(menu->window);
        switch(wgetch(menu->window)) {
            case KEY_DOWN:
                menu_driver(menu->menu, REQ_DOWN_ITEM);
                break;
            case KEY_UP:
                menu_driver(menu->menu, REQ_UP_ITEM);
                break;
            case KEY_ENTER:
            case '\r':
            case '\n':
            case ' ':
                continuing = false;
                break;
            default:
                break;
        }
    }

    write_output(args->outfile, menu_get_selected_item(menu));

    menu_free(menu);
    choices_free(iso_info);
    resources_free(resources);
    args_free(args);

    return 0;
}
