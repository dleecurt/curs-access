/*
 * theme.c - Color themes: built-ins, user theme files, color pairs.
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

#include "config.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theme.h"
#include "util.h"

static const char *color_names[C_COUNT] = {
    "Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Brown", "Light Gray",
    "Dark Gray", "Light Blue", "Light Green", "Light Cyan", "Light Red",
    "Light Magenta", "Yellow", "White"
};

/* Labels as shown in Program Configuration; keys as written in files. */
static const char *slot_names[SLOT_COUNT] = {
    "Titles & Subscripts", "Text", "Menu Border", "Screen Border",
    "Background", "Selection Bar Text", "Selection Bar", "Function Keys",
    "Key Labels", "Messages"
};
static const char *slot_keys[SLOT_COUNT] = {
    "title", "text", "menu_border", "screen_border", "background",
    "bar_text", "bar", "key", "key_label", "message"
};

/*                       title     text      menuB     scrB      bg       barT     bar      key      label     msg */
static const theme_t builtins[] = {
    { "Classic",      { C_YELLOW, C_LGRAY,  C_CYAN,   C_CYAN,   C_BLACK, C_WHITE, C_RED,   C_WHITE, C_YELLOW, C_LRED } },
    { "Blue Screen",  { C_YELLOW, C_WHITE,  C_LCYAN,  C_LRED,   C_BLUE,  C_WHITE, C_RED,   C_WHITE, C_YELLOW, C_LMAGENTA } },
    { "Monochrome",   { C_WHITE,  C_LGRAY,  C_LGRAY,  C_LGRAY,  C_BLACK, C_BLACK, C_LGRAY, C_WHITE, C_LGRAY,  C_WHITE } },
    { "Amber",        { C_YELLOW, C_BROWN,  C_BROWN,  C_BROWN,  C_BLACK, C_BLACK, C_BROWN, C_YELLOW, C_BROWN, C_YELLOW } },
    { "Green Screen", { C_LGREEN, C_GREEN,  C_GREEN,  C_GREEN,  C_BLACK, C_BLACK, C_GREEN, C_LGREEN, C_GREEN, C_LGREEN } },
    { "Midnight",     { C_LCYAN,  C_LGRAY,  C_BLUE,   C_LBLUE,  C_BLACK, C_WHITE, C_BLUE,  C_WHITE, C_LCYAN,  C_YELLOW } },
};
#define N_BUILTIN ((int)(sizeof builtins / sizeof builtins[0]))

static theme_t g_current;
static int g_colors_ok;      /* terminal supports color */
static int g_sixteen;        /* terminal has at least 16 colors */
static short g_pairs[C_COUNT][C_COUNT];
static short g_next_pair = 1;

const char *theme_color_name(int c)
{
    return (c >= 0 && c < C_COUNT) ? color_names[c] : "?";
}

const char *theme_slot_name(int slot)
{
    return (slot >= 0 && slot < SLOT_COUNT) ? slot_names[slot] : "?";
}

int theme_color_from_name(const char *name)
{
    int i;

    for (i = 0; i < C_COUNT; i++)
        if (ca_strcaseeq(name, color_names[i]))
            return i;
    return -1;
}

int theme_builtin_count(void) { return N_BUILTIN; }

const theme_t *theme_builtin(int i)
{
    return (i >= 0 && i < N_BUILTIN) ? &builtins[i] : NULL;
}

/* Read one theme file.  Missing slots keep the Classic colors. */
static int load_theme_file(const char *path, theme_t *t)
{
    FILE *fp = fopen(path, "r");
    char line[256];
    int s;

    if (!fp)
        return -1;
    *t = builtins[0];
    t->name[0] = '\0';
    while (fgets(line, sizeof line, fp)) {
        char *eq, *key, *val;

        if (line[0] == '#')
            continue;
        eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        key = ca_trim(line);
        val = ca_trim(eq + 1);
        if (!strcmp(key, "name")) {
            ca_strlcpy(t->name, val, sizeof t->name);
            continue;
        }
        for (s = 0; s < SLOT_COUNT; s++) {
            if (!strcmp(key, slot_keys[s])) {
                int c = theme_color_from_name(val);
                if (c >= 0)
                    t->color[s] = (short)c;
            }
        }
    }
    fclose(fp);
    return t->name[0] ? 0 : -1;
}

int theme_list(theme_t *out, int max)
{
    char dir[CA_PATH_MAX];
    DIR *d;
    struct dirent *de;
    int n = 0, i;

    for (i = 0; i < N_BUILTIN && n < max; i++)
        out[n++] = builtins[i];
    ca_config_path(dir, sizeof dir, "themes");
    d = opendir(dir);
    if (!d)
        return n;
    while ((de = readdir(d)) != NULL && n < max) {
        size_t len = strlen(de->d_name);
        char path[CA_PATH_MAX * 2];

        if (len < 7 || strcmp(de->d_name + len - 6, ".theme") != 0)
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        if (load_theme_file(path, &out[n]) == 0)
            n++;
    }
    closedir(d);
    return n;
}

int theme_find(const char *name, theme_t *out)
{
    theme_t all[64];
    int n = theme_list(all, 64), i;

    for (i = 0; i < n; i++) {
        if (ca_strcaseeq(all[i].name, name)) {
            *out = all[i];
            return 0;
        }
    }
    return -1;
}

int theme_save(const theme_t *t)
{
    char file[64], path[CA_PATH_MAX + 80], buf[2048];
    size_t i, o = 0;
    int n, s;

    /* File name from the theme name: letters and digits, rest '-'. */
    for (i = 0; t->name[i] && o + 1 < sizeof file - 7; i++)
        file[o++] = isalnum((unsigned char)t->name[i])
                        ? (char)tolower((unsigned char)t->name[i]) : '-';
    file[o] = '\0';
    if (!o)
        return -1;
    snprintf(path, sizeof path, "%s/themes/%s.theme", ca_config_dir(), file);
    n = snprintf(buf, sizeof buf, "# Curses Access color theme\nname = %s\n", t->name);
    for (s = 0; s < SLOT_COUNT; s++)
        n += snprintf(buf + n, sizeof buf - (size_t)n, "%s = %s\n",
                      slot_keys[s], color_names[t->color[s]]);
    return ca_write_file_atomic(path, buf, (size_t)n);
}

/* ---- Curses colors ------------------------------------------------ */

/* PC color order -> curses color numbers for the eight base colors. */
static const short base_map[8] = {
    COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
    COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE
};

void theme_init_curses(void)
{
    g_colors_ok = has_colors();
    if (g_colors_ok) {
        start_color();
        g_sixteen = COLORS >= 16;
    }
    memset(g_pairs, 0, sizeof g_pairs);
    g_next_pair = 1;
}

void theme_apply(const theme_t *t)
{
    g_current = *t;
}

const theme_t *theme_current(void) { return &g_current; }

attr_t theme_attr(int fg, int bg)
{
    attr_t extra = A_NORMAL;
    short cfg, cbg;

    if (fg < 0 || fg >= C_COUNT)
        fg = C_LGRAY;
    if (bg < 0 || bg >= C_COUNT)
        bg = C_BLACK;
    if (!g_colors_ok) {
        /* No color: light colors bold, light backgrounds reversed. */
        if (bg != C_BLACK && bg != g_current.color[SLOT_BACKGROUND])
            return A_REVERSE;
        return fg >= 8 ? A_BOLD : A_NORMAL;
    }
    if (!g_pairs[fg][bg]) {
        if (g_next_pair >= COLOR_PAIRS || g_next_pair > 255)
            return A_NORMAL;
        cfg = base_map[fg & 7];
        cbg = base_map[bg & 7];
        if (g_sixteen) {
            if (fg >= 8)
                cfg = (short)(cfg + 8);
            if (bg >= 8)
                cbg = (short)(cbg + 8);
        }
        init_pair(g_next_pair, cfg, cbg);
        g_pairs[fg][bg] = g_next_pair++;
    }
    if (!g_sixteen && fg >= 8)
        extra = A_BOLD;
    return (attr_t)COLOR_PAIR(g_pairs[fg][bg]) | extra;
}

attr_t theme_slot(int slot)
{
    return theme_attr(g_current.color[slot], g_current.color[SLOT_BACKGROUND]);
}

attr_t theme_bar(void)
{
    if (!g_colors_ok)
        return A_REVERSE;
    return theme_attr(g_current.color[SLOT_BAR_TEXT], g_current.color[SLOT_BAR]);
}

attr_t theme_field(void)
{
    /* Fields being edited use the bar colors, like the original. */
    return theme_bar();
}
