/*
 * prefs.h - Program configuration (<config dir>/curs-access.conf).
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

#ifndef CA_PREFS_H
#define CA_PREFS_H

#include "theme.h"

typedef struct {
    char exit_cmd[512];     /* shell command run after "Exit" (may be empty) */
    char greeting[64];      /* text after "Good Morning..." */
    int clear_before_run;   /* blank the screen before running a command */
    int usage_tracking;     /* write encrypted usage log entries */
    int clock;              /* date and time on the bottom border */
    int clock_24h;          /* 24-hour clock instead of am/pm */
    int menu_border;        /* box around the menu */
    int blank_minutes;      /* screen blanker after N idle minutes, 0 = off */
    int splash;             /* show the start-up logo */
    theme_t theme;          /* active colors */
} prefs_t;

extern prefs_t g_prefs;

/* Defaults, then values from the file if it exists.  0 on success. */
int prefs_load(void);
int prefs_save(void);

#endif /* CA_PREFS_H */
