/*
 * cfgscreen.c - Program Configuration and Color Themes screens.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "prefs.h"
#include "screens.h"
#include "security.h"
#include "theme.h"
#include "ui.h"
#include "util.h"

enum { R_TEXT, R_PASSWORD, R_TOGGLE, R_NUMBER, R_HEADING, R_THEME, R_COLOR };

typedef struct {
    int type;
    const char *label;
    void *ptr;          /* char* for text, int* for toggles and numbers */
    size_t len;         /* text buffer size */
    int which;          /* password: 0 exit, 1 maintenance; color: slot */
} row_t;

#define N_ROWS (12 + 1 + SLOT_COUNT)

static int g_nrows;
static row_t g_rows[N_ROWS];

/* Fill g_rows with the settings shown on the Program Configuration screen. */
static void build_rows(void)
{
    int n = 0, s;

#define ROW(t, l, p, ln, w) g_rows[n].type = (t); g_rows[n].label = (l); \
    g_rows[n].ptr = (p); g_rows[n].len = (ln); g_rows[n].which = (w); n++
    ROW(R_TEXT, "Command To Run On Exit", g_prefs.exit_cmd, sizeof g_prefs.exit_cmd, 0);
    ROW(R_TEXT, "Greeting", g_prefs.greeting, sizeof g_prefs.greeting, 0);
    ROW(R_PASSWORD, "Password For 'Exit'", NULL, 0, 0);
    ROW(R_PASSWORD, "Password For 'Menu Maintenance'", NULL, 0, 1);
    ROW(R_TOGGLE, "Clear Screen Before Commands", &g_prefs.clear_before_run, 0, 0);
    ROW(R_TOGGLE, "Usage Tracking", &g_prefs.usage_tracking, 0, 0);
    ROW(R_TOGGLE, "Clock Display", &g_prefs.clock, 0, 0);
    ROW(R_TOGGLE, "24-Hour Clock", &g_prefs.clock_24h, 0, 0);
    ROW(R_TOGGLE, "Menu Border", &g_prefs.menu_border, 0, 0);
    ROW(R_NUMBER, "Screen Blanking Time (In Minutes)", &g_prefs.blank_minutes, 0, 0);
    ROW(R_TOGGLE, "Start-Up Logo", &g_prefs.splash, 0, 0);
    ROW(R_HEADING, "Color Options", NULL, 0, 0);
    ROW(R_THEME, "Color Theme", NULL, 0, 0);
    for (s = 0; s < SLOT_COUNT; s++) {
        ROW(R_COLOR, theme_slot_name(s), NULL, 0, s);
    }
#undef ROW
    g_nrows = n;
}

/* Text inside the [ ] of a row. */
static void row_value(const row_t *r, char *out, size_t len)
{
    switch (r->type) {
    case R_TEXT:
        ca_strlcpy(out, (const char *)r->ptr, len);
        break;
    case R_PASSWORD:
        ca_strlcpy(out, (r->which ? sec_has_maint_password() : sec_has_exit_password())
                            ? "********" : "", len);
        break;
    case R_TOGGLE:
        ca_strlcpy(out, *(int *)r->ptr ? "On" : "Off", len);
        break;
    case R_NUMBER:
        if (*(int *)r->ptr)
            snprintf(out, len, "%d", *(int *)r->ptr);
        else
            ca_strlcpy(out, "Off", len);
        break;
    case R_THEME:
        ca_strlcpy(out, g_prefs.theme.name, len);
        break;
    case R_COLOR:
        ca_strlcpy(out, theme_color_name(g_prefs.theme.color[r->which]), len);
        break;
    default:
        out[0] = '\0';
    }
}

/* Width of the value field of a row. */
static int value_width(const row_t *r)
{
    if (r->type == R_TEXT) {
        int w = COLS - 52;
        return w > 40 ? 40 : w < 12 ? 12 : w;
    }
    return 14;
}

/* Attribute used to show a value (colors show themselves). */
static attr_t value_attr(const row_t *r)
{
    const theme_t *t = &g_prefs.theme;

    if (r->type != R_COLOR)
        return theme_slot(SLOT_TEXT) | A_BOLD;
    switch (r->which) {
    case SLOT_BACKGROUND:
        return theme_attr(t->color[SLOT_TEXT], t->color[SLOT_BACKGROUND]) | A_BOLD;
    case SLOT_BAR:
    case SLOT_BAR_TEXT:
        return theme_bar();
    default:
        return theme_attr(t->color[r->which], t->color[SLOT_BACKGROUND]) | A_BOLD;
    }
}

/* Draw one row at y; returns the x of its value field. */
static int draw_row(int y, const row_t *r, int selected)
{
    char val[600];
    int w = value_width(r), rx = COLS - 8, vx = rx - w, lx = 4, ll, i;
    attr_t la;

    if (r->type == R_HEADING) {
        attrset(theme_slot(SLOT_TITLE) | A_BOLD);
        mvaddstr(y, lx, r->label);
        return 0;
    }
    la = r->type == R_COLOR && r->which != SLOT_BACKGROUND && r->which != SLOT_BAR &&
                 r->which != SLOT_BAR_TEXT
             ? theme_attr(g_prefs.theme.color[r->which], g_prefs.theme.color[SLOT_BACKGROUND]) | A_BOLD
             : theme_slot(SLOT_TEXT) | A_BOLD;
    attrset(la);
    mvaddnstr(y, lx, r->label, vx - lx - 3);
    ll = (int)strlen(r->label);
    attrset(theme_slot(SLOT_TEXT));
    for (i = lx + ll + 1; i < vx - 2; i++)
        mvaddch(y, i, '.');
    attrset(theme_slot(SLOT_TEXT) | A_BOLD);
    mvaddch(y, vx - 1, '[');
    mvaddch(y, rx, ']');
    row_value(r, val, sizeof val);
    ui_field_text(y, vx, w, val, selected ? theme_bar() : value_attr(r));
    return vx;
}

/* Apply the theme in g_prefs to the screen right away. */
static void preview(void)
{
    theme_apply(&g_prefs.theme);
}

/* Choose a theme from the list. */
static void choose_theme(int step)
{
    theme_t all[64];
    const char *names[64];
    int n = theme_list(all, 64), cur = 0, i, k;

    for (i = 0; i < n; i++) {
        names[i] = all[i].name;
        if (ca_strcaseeq(all[i].name, g_prefs.theme.name))
            cur = i;
    }
    if (step)
        k = (cur + step + n) % n;
    else
        k = ui_pick("Color Theme", names, n, cur);
    if (k >= 0) {
        g_prefs.theme = all[k];
        preview();
    }
}

/*
 * Change one color slot: step through the 16 colors with Left/Right
 * (step = -1 / +1) or pick from a list (step = 0).  The theme becomes
 * "Custom" and the screen shows the new color at once.
 */
static void change_color(int slot, int step)
{
    const char *names[C_COUNT];
    int i, k;

    for (i = 0; i < C_COUNT; i++)
        names[i] = theme_color_name(i);
    if (step)
        k = (g_prefs.theme.color[slot] + step + C_COUNT) % C_COUNT;
    else
        k = ui_pick(theme_slot_name(slot), names, C_COUNT, g_prefs.theme.color[slot]);
    if (k >= 0 && k != g_prefs.theme.color[slot]) {
        g_prefs.theme.color[slot] = (short)k;
        ca_strlcpy(g_prefs.theme.name, "Custom", sizeof g_prefs.theme.name);
        preview();
    }
}

/*
 * The Program Configuration screen.  Settings are saved on leaving.
 * Returns 1 for F10 (Save & Run Main Menu), 0 for Esc.
 */
int config_screen(void)
{
    static const fkey_t keys[] = {
        { "Enter", "Change" }, { "F10", "Save & Run Main Menu" }, { "Esc", "Save/Exit" }
    };
    int sel = 0, first = 0, pending = 0;

    build_rows();
    for (;;) {
        int top = 5, rows, i, k, vx = 0;
        row_t *r;

        if (ui_too_small()) {
            if (ui_getkey() == 27)
                break;
            continue;
        }
        rows = LINES - 5 - top;
        if (sel < first)
            first = sel;
        if (sel >= first + rows)
            first = sel - rows + 1;
        ui_frame(0);
        ui_spaced_title(2, "Program Configuration");
        ui_divider(4);
        for (i = 0; i < rows && first + i < g_nrows; i++) {
            int x = draw_row(top + i, &g_rows[first + i], first + i == sel);
            if (first + i == sel)
                vx = x;
        }
        if (g_nrows > rows)
            ui_scrollbar(top, COLS - 3, rows, first, rows, g_nrows);
        ui_divider(LINES - 4);
        ui_fkeys(LINES - 2, 1, COLS - 2, keys, 3);
        refresh();

        r = &g_rows[sel];
        if (pending && r->type == R_TEXT) {
            char buf[600];
            int start = pending == '\n' ? 0 : pending;

            ca_strlcpy(buf, (char *)r->ptr, sizeof buf);
            pending = 0;
            k = ui_edit(top + sel - first, vx, value_width(r), buf, r->len, 0, start);
            if (k != 27)
                ca_strlcpy((char *)r->ptr, buf, r->len);
            if (k == UI_REDRAW)
                pending = '\n';
            else if (k == KEY_UP || k == KEY_DOWN || (k >= KEY_F(1) && k <= KEY_F(12)))
                ungetch(k);
            continue;
        }
        pending = 0;

        k = ui_getkey();
        switch (k) {
        case KEY_UP:
            do {
                sel = (sel + g_nrows - 1) % g_nrows;
            } while (g_rows[sel].type == R_HEADING);
            break;
        case KEY_DOWN:
        case '\t':
            do {
                sel = (sel + 1) % g_nrows;
            } while (g_rows[sel].type == R_HEADING);
            break;
        case KEY_HOME:
            sel = 0;
            break;
        case KEY_END:
            sel = g_nrows - 1;
            break;
        case KEY_PPAGE:
            sel = sel > rows ? sel - rows : 0;
            if (g_rows[sel].type == R_HEADING) sel++;
            break;
        case KEY_NPAGE:
            sel = sel + rows < g_nrows ? sel + rows : g_nrows - 1;
            if (g_rows[sel].type == R_HEADING) sel++;
            break;
        case KEY_LEFT:
        case KEY_RIGHT:
        case ' ':
        case '\n': {
            int step = k == KEY_LEFT ? -1 : k == KEY_RIGHT ? 1 : 0;
            switch (r->type) {
            case R_TEXT:
                if (k == '\n' || k == ' ')
                    pending = '\n';        /* edit the current text */
                break;
            case R_PASSWORD:
                if (k == '\n' || k == ' ')
                    password_change(r->which);
                break;
            case R_TOGGLE:
                *(int *)r->ptr = !*(int *)r->ptr;
                break;
            case R_NUMBER: {
                int *v = (int *)r->ptr;
                if (step) {
                    *v += step;
                    if (*v < 0) *v = 0;
                    if (*v > 99) *v = 99;
                } else {
                    char buf[8];
                    snprintf(buf, sizeof buf, "%d", *v);
                    if (ui_prompt(r->label, "Minutes before the screen blanks\n(0 = Off, up to 99)",
                                  buf, sizeof buf, 0, NULL)) {
                        *v = atoi(buf);
                        if (*v < 0) *v = 0;
                        if (*v > 99) *v = 99;
                    }
                }
                break;
            }
            case R_THEME:
                choose_theme(step);
                break;
            case R_COLOR:
                change_color(r->which, step);
                break;
            }
            break;
        }
        case 27:
            prefs_save();
            return 0;
        case KEY_F(10):
            prefs_save();
            return 1;
        default:
            if (r->type == R_TEXT && k >= 32 && k < 127)
                pending = k;
            break;
        }
    }
    prefs_save();
    return 0;
}

/* ---- Color Themes --------------------------------------------------- */

void themes_screen(void)
{
    static const fkey_t keys[] = {
        { "F2", "Save Current Colors As Theme" }, { "Esc", "Exit" }
    };
    int sel = 0;

    for (;;) {
        theme_t all[MENU_MAX_ITEMS];
        const char *names[MENU_MAX_ITEMS];
        char sub[96];
        int n = theme_list(all, MENU_MAX_ITEMS), i, k;

        for (i = 0; i < n; i++) {
            names[i] = all[i].name;
        }
        snprintf(sub, sizeof sub, "Current Theme: %s", g_prefs.theme.name);
        k = menu_screen("Color Themes", sub, names, n, &sel, keys, 2);
        if (k == '\n') {
            g_prefs.theme = all[sel];
            theme_apply(&g_prefs.theme);
            prefs_save();
        } else if (k == KEY_F(2)) {
            char name[48];
            ca_strlcpy(name, ca_strcaseeq(g_prefs.theme.name, "Custom") ? "" : g_prefs.theme.name,
                       sizeof name);
            if (ui_prompt("Save Theme", "Name for a theme with the current colors", name,
                          sizeof name, 0, NULL) && name[0]) {
                theme_t t = g_prefs.theme, dummy;
                int builtin = 0;
                for (i = 0; i < theme_builtin_count(); i++)
                    if (ca_strcaseeq(theme_builtin(i)->name, name))
                        builtin = 1;
                if (builtin) {
                    ui_message("Save Theme", "Built-in themes cannot be replaced.\nChoose another name.");
                    continue;
                }
                if (theme_find(name, &dummy) == 0 &&
                    !ui_confirm("Save Theme", "A theme with that name exists.\nReplace it?"))
                    continue;
                ca_strlcpy(t.name, name, sizeof t.name);
                if (theme_save(&t) == 0) {
                    g_prefs.theme = t;
                    prefs_save();
                    ui_message("Save Theme", "Theme saved.");
                } else {
                    ui_message("Save Theme", "The theme could not be saved.");
                }
            }
        } else if (k == 27 || k == KEY_F(10)) {
            return;
        }
    }
}
