/*
 * mainmenu.c - The lettered menu screen and the main menu loop.
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
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "prefs.h"
#include "screens.h"
#include "security.h"
#include "theme.h"
#include "ui.h"
#include "util.h"

/* First half of the greeting, by the hour of the local time. */
const char *greeting_time(void)
{
    time_t now = time(NULL);
    int h = localtime(&now)->tm_hour;

    return h < 12 ? "Good Morning" : h < 18 ? "Good Afternoon" : "Good Evening";
}

/*
 * Vertical layout of a menu screen, worked out from the terminal size.
 * On a 25-line terminal this matches the classic layout: title on row 2,
 * greeting on row 5, the box in the middle, version on row 18 and the
 * function keys on row 21.
 */
typedef struct {
    int title_y, greet_y, box_top, box_bottom, ver_y, fk_y;
} layout_t;

/* Work out the rows of a menu screen for the current terminal height. */
static void menu_layout(layout_t *L)
{
    int big = LINES >= 24;

    L->title_y = big ? 2 : 1;
    L->greet_y = big ? 5 : 3;
    L->fk_y = big ? LINES - 4 : LINES - 3;
    L->ver_y = L->fk_y - (big ? 3 : 2);
    L->box_top = L->greet_y + 2;
    L->box_bottom = L->ver_y - 2;
}

int menu_screen(const char *title, const char *subtitle,
                const char *const *entries, int n, int *sel,
                const fkey_t *keys, int nkeys)
{
    int first = 0;

    if (*sel >= n)
        *sel = n > 0 ? n - 1 : 0;
    for (;;) {
        layout_t L;
        int longest = 0, i, avail, pad, vis, bh, bw, by, bx, ex, k;
        char ver[64];

        if (ui_too_small()) {
            k = ui_getkey();
            if (k == 27 || (k >= KEY_F(1) && k <= KEY_F(12)))
                return k;
            continue;
        }
        menu_layout(&L);
        ui_frame(1);
        ui_spaced_title(L.title_y, title);
        if (subtitle)
            ui_center(L.greet_y, subtitle, theme_slot(SLOT_TEXT) | A_BOLD);

        /* Size the box: entries plus a blank row above and below. */
        for (i = 0; i < n; i++)
            if ((int)strlen(entries[i]) > longest)
                longest = (int)strlen(entries[i]);
        if (n == 0)
            longest = 44;
        avail = L.box_bottom - L.box_top + 1;
        pad = 1;
        vis = n > 0 ? n : 1;
        if (vis + 2 + 2 * pad > avail)
            pad = 0;
        if (vis + 2 + 2 * pad > avail)
            vis = avail - 2;
        if (vis < 1)
            vis = 1;
        bh = vis + 2 + 2 * pad;
        bw = longest + 3 + 16;
        if (bw < 50)
            bw = 50;
        if (bw > COLS - 6)
            bw = COLS - 6;
        if (longest + 3 > bw - 4)
            longest = bw - 7;
        by = L.box_top + (avail - bh) / 2;
        bx = (COLS - bw) / 2;
        ex = bx + (bw - (longest + 3)) / 2;
        if (g_prefs.menu_border)
            ui_box(by, bx, bh, bw, theme_slot(SLOT_MENU_BORDER));

        /* Keep the selection on screen. */
        if (*sel < first)
            first = *sel;
        if (*sel >= first + vis)
            first = *sel - vis + 1;
        for (i = 0; i < vis && first + i < n; i++) {
            int idx = first + i;
            char line[DESC_LEN + 8];
            attr_t a = idx == *sel ? theme_bar() : theme_slot(SLOT_TEXT);

            snprintf(line, sizeof line, "%c) %s", 'A' + idx, entries[idx]);
            ui_field_text(by + 1 + pad + i, ex - 1, longest + 5, "", a);
            attrset(a);
            mvaddnstr(by + 1 + pad + i, ex, line, longest + 3);
        }
        if (n == 0)
            ui_center(by + 1 + pad, "(This menu is empty: press F1 to add entries)",
                      theme_slot(SLOT_TEXT));
        if (n > vis && g_prefs.menu_border)
            ui_scrollbar(by + 1, bx + bw - 1, bh - 2, first, vis, n);

        snprintf(ver, sizeof ver, "CURSES ACCESS   Version %s", PACKAGE_VERSION);
        ui_center(L.ver_y, ver, theme_slot(SLOT_TEXT) | A_BOLD);
        ui_fkeys(L.fk_y, 2, COLS - 3, keys, nkeys);
        refresh();

        k = ui_getkey();
        if (k < 256 && isalpha(k)) {
            int idx = toupper(k) - 'A';
            if (idx < n) {
                *sel = idx;
                return '\n';
            }
            continue;
        }
        switch (k) {
        case KEY_UP:
            if (n) *sel = (*sel + n - 1) % n;
            break;
        case KEY_DOWN:
            if (n) *sel = (*sel + 1) % n;
            break;
        case KEY_HOME:
            *sel = 0;
            break;
        case KEY_END:
            *sel = n > 0 ? n - 1 : 0;
            break;
        case KEY_PPAGE:
            *sel = *sel > vis ? *sel - vis : 0;
            break;
        case KEY_NPAGE:
            if (n) *sel = *sel + vis < n ? *sel + vis : n - 1;
            break;
        case '\n':
            if (n > 0)
                return '\n';
            break;
        case UI_REDRAW:
            break;
        default:
            if (k == 27 || (k >= KEY_F(1) && k <= KEY_F(12)))
                return k;
            break;
        }
    }
}

/*
 * The main menu loop: walks the menu tree, runs entries and handles
 * F1 Maintenance, F3 System Info, F5 Logo, Esc (back) and F10 (exit).
 * Returns when the user has been allowed to exit.
 */
void main_menu_run(void)
{
    static const fkey_t main_keys[] = {
        { "F1", "Maintenance" }, { "F3", "System Info" },
        { "F5", "Logo" }, { "F10", "Exit" }
    };
    static const fkey_t sub_keys[] = {
        { "F1", "Maintenance" }, { "F3", "System Info" },
        { "Esc", "Previous Menu" }, { "F10", "Main Menu" }
    };
    menu_t *m = g_main_menu;
    int sel = 0;
    int saved[MENU_MAX_LEVELS + 1] = { 0 };   /* selection at each level */

    for (;;) {
        const char *entries[MENU_MAX_ITEMS];
        char sub[128];
        int i, k;

        for (i = 0; i < m->count; i++)
            entries[i] = m->items[i].desc;
        snprintf(sub, sizeof sub, "%s...%s", greeting_time(), g_prefs.greeting);
        k = menu_screen(m->title[0] ? m->title : "Main Menu", sub, entries,
                        m->count, &sel, m->parent ? sub_keys : main_keys, 4);

        if (k == '\n') {
            item_t *it = &m->items[sel];
            if (it->type == ITEM_SUBMENU && it->sub) {
                saved[m->level] = sel;
                m = it->sub;
                sel = 0;
            } else if (it->type == ITEM_PROGRAM) {
                launch_item(m, it);
            }
        } else if (k == KEY_F(1)) {
            if (access_maintenance()) {
                maintenance_run();
                sec_lock();
                /* Menus may have changed: start again at the top. */
                m = g_main_menu;
                sel = 0;
            }
        } else if (k == KEY_F(3)) {
            sysinfo_show();
        } else if (k == KEY_F(5) && !m->parent) {
            splash_show();
        } else if (k == 27 && m->parent) {
            m = m->parent;
            sel = saved[m->level];
        } else if (k == KEY_F(10)) {
            if (m->parent) {
                m = g_main_menu;
                sel = saved[1];
            } else if (access_exit()) {
                return;
            }
        }
    }
}
