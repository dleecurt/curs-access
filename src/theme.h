/*
 * theme.h - Color themes.
 *
 * A theme gives a color to each "slot" (titles, text, borders...).
 * Colors use the sixteen classic PC text-mode names.  On terminals with
 * only eight colors the light colors are drawn as bold, and light
 * background colors fall back to their dark versions.
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

#ifndef CA_THEME_H
#define CA_THEME_H

#include <curses.h>

/* The sixteen colors, in PC text-mode order. */
enum {
    C_BLACK, C_BLUE, C_GREEN, C_CYAN, C_RED, C_MAGENTA, C_BROWN, C_LGRAY,
    C_DGRAY, C_LBLUE, C_LGREEN, C_LCYAN, C_LRED, C_LMAGENTA, C_YELLOW, C_WHITE,
    C_COUNT
};

/* Color slots of a theme. */
enum {
    SLOT_TITLE,          /* screen titles, column headings, subscripts */
    SLOT_TEXT,           /* ordinary text and menu entries */
    SLOT_MENU_BORDER,    /* box around the menu and dialogs */
    SLOT_SCREEN_BORDER,  /* double border around the whole screen */
    SLOT_BACKGROUND,     /* background of every screen */
    SLOT_BAR_TEXT,       /* text on the selection bar */
    SLOT_BAR,            /* selection bar background */
    SLOT_KEY,            /* function key names ("F1") */
    SLOT_KEY_LABEL,      /* what the key does ("Maintenance") */
    SLOT_MESSAGE,        /* warnings and prompts */
    SLOT_COUNT
};

typedef struct {
    char name[48];
    short color[SLOT_COUNT];
} theme_t;

/* Display names. */
const char *theme_color_name(int c);
const char *theme_slot_name(int slot);
/* Color index from a name ("Light Cyan"); -1 if unknown. */
int theme_color_from_name(const char *name);

/* Built-in themes. */
int theme_builtin_count(void);
const theme_t *theme_builtin(int i);

/*
 * All themes: built-ins followed by user themes from <config>/themes/NAME.theme.
 * theme_list fills up to max entries and returns how many were found.
 */
int theme_list(theme_t *out, int max);

/* Find a theme by name (built-in or user file).  0 on success. */
int theme_find(const char *name, theme_t *out);

/* Save a theme to <config>/themes/<name>.theme.  0 on success. */
int theme_save(const theme_t *t);

/* The active theme (a copy lives in the configuration). */
void theme_apply(const theme_t *t);
const theme_t *theme_current(void);

/* Curses set-up once colors are available. */
void theme_init_curses(void);

/* Attribute for foreground color fg on background bg. */
attr_t theme_attr(int fg, int bg);

/* Attribute for a slot drawn on the theme background. */
attr_t theme_slot(int slot);

/* Attribute for the selection bar. */
attr_t theme_bar(void);

/* Attribute for input fields being edited. */
attr_t theme_field(void);

#endif /* CA_THEME_H */
