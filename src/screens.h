/*
 * screens.h - The screens of Curses Access.
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

#ifndef CA_SCREENS_H
#define CA_SCREENS_H

#include "menu.h"
#include "ui.h"

/*
 * The lettered menu screen used by the main menu, its sub-menus and the
 * maintenance menus: spaced title, greeting, boxed A) B) C) entries,
 * version line and function key bar.  Letters choose an entry at once;
 * arrows and Enter work too.  Returns '\n' with *sel set when an entry
 * was chosen, otherwise the key pressed (Esc, a function key...).
 */
int menu_screen(const char *title, const char *subtitle,
                const char *const *entries, int n, int *sel,
                const fkey_t *keys, int nkeys);

/* "Good Morning", "Good Afternoon" or "Good Evening". */
const char *greeting_time(void);

/* splash.c */
void splash_show(void);                 /* logo; waits for a key */
void sysinfo_show(void);                /* F3 System Info */
void recovery_code_show(const char *code, int first_run);

/* mainmenu.c */
void main_menu_run(void);               /* returns when the user exits */

/* access.c */
int access_maintenance(void);           /* password check, unlocks the log key */
int access_exit(void);                  /* Exit password check */
int access_recover(void);               /* --recover: ask for the code */

/* launch.c */
void launch_item(const menu_t *m, const item_t *it);

/* maint.c */
void maintenance_run(void);
void menu_editor(menu_t *m);
void tree_show(void);
void password_change(int maintenance);  /* 0 = Exit password, 1 = Maintenance */

/* cfgscreen.c */
int config_screen(void);                /* returns 1 for "Save & Run Main Menu" */
void themes_screen(void);

#endif /* CA_SCREENS_H */
