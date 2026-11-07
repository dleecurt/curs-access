/*
 * menu.h - The menu tree: a main menu of up to 26 entries (A-Z), where
 * each entry runs a program or opens a sub-menu, up to six levels deep.
 *
 * Stored in <config dir>/menus.conf, one entry per line, tab separated:
 *   level  T  title                                (title of the menu
 *                                                   that the following
 *                                                   level+1 lines fill)
 *   level  P  description  directory  command  pause  password-hash
 *   level  S  description  sub-menu title
 * Level 1 lines belong to the main menu; a level N "S" line opens a
 * sub-menu filled by the level N+1 lines that follow it.
 *
 * Copyright (C) 2026 Lee Curtis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CA_MENU_H
#define CA_MENU_H

#include "crypto.h"

#define MENU_MAX_ITEMS  26      /* A to Z */
#define MENU_MAX_LEVELS 6       /* main menu is level 1 */
#define DESC_LEN        41      /* 40 characters + NUL */
#define TITLE_LEN       49
#define DIR_LEN         256
#define CMD_LEN         512

enum { ITEM_PROGRAM, ITEM_SUBMENU };

typedef struct menu menu_t;

typedef struct {
    int type;                    /* ITEM_PROGRAM or ITEM_SUBMENU */
    char desc[DESC_LEN];         /* text shown on the menu */
    char dir[DIR_LEN];           /* working directory (programs) */
    char cmd[CMD_LEN];           /* shell command line (programs) */
    int pause;                   /* wait for a key after the program ends */
    char pwhash[CA_HASH_MAX];    /* optional entry password (hash) */
    menu_t *sub;                 /* sub-menu (ITEM_SUBMENU) */
} item_t;

struct menu {
    char title[TITLE_LEN];       /* custom title ("Welcome To ...") */
    int level;                   /* 1 = main menu */
    int count;
    item_t items[MENU_MAX_ITEMS];
    menu_t *parent;
};

/* The main menu (always present after menu_load). */
extern menu_t *g_main_menu;

/* Load menus.conf, or create a starter menu.  0 on success. */
int menu_load(void);
int menu_save(void);
void menu_free(menu_t *m);

/* New empty menu at the given level. */
menu_t *menu_new(int level, menu_t *parent, const char *title);

/* Insert a blank program entry at pos (shifting the rest down). */
int menu_insert(menu_t *m, int pos);
/* Delete entry pos (and any sub-menu under it). */
void menu_delete(menu_t *m, int pos);
/* Move entry from -> to. */
void menu_move(menu_t *m, int from, int to);

/* Make entry pos a sub-menu (creates an empty one).  0 on success. */
int menu_make_submenu(menu_t *m, int pos);

/* "Main Menu > Utilities > Editors" style path for m (and entry). */
void menu_path(const menu_t *m, const item_t *it, char *out, size_t outlen);

#endif /* CA_MENU_H */
