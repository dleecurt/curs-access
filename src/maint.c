/*
 * maint.c - Menu Maintenance: the maintenance menu, the Create / Modify
 *           menu editor, the menu tree, the usage report and the
 *           Passwords & Recovery screen.
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

#include "crypto.h"
#include "menu.h"
#include "prefs.h"
#include "screens.h"
#include "security.h"
#include "theme.h"
#include "ui.h"
#include "usage.h"
#include "util.h"

/* Set by F10 in the editor: leave every nested editor at once. */
static int g_leave_editor;

/* ---- Passwords ------------------------------------------------------ */

void password_change(int maintenance)
{
    char a[128], b[128];
    const char *what = maintenance ? "Menu Maintenance" : "Exit";
    char prompt[96];
    int rc;

    snprintf(prompt, sizeof prompt,
             "New %s password\n(leave empty for no password)", what);
    if (ui_password(what, prompt, a, sizeof a, 0) != PW_ENTERED)
        return;
    if (a[0]) {
        if (ui_password(what, "Type the new password again", b, sizeof b, 0) != PW_ENTERED) {
            ca_wipe(a, sizeof a);
            return;
        }
        if (strcmp(a, b) != 0) {
            ca_wipe(a, sizeof a);
            ca_wipe(b, sizeof b);
            ui_message(what, "The two passwords do not match.\nThe password was not changed.");
            return;
        }
        ca_wipe(b, sizeof b);
    } else if (!ui_confirm(what, "Remove this password?")) {
        return;
    }
    {
        int removing = !a[0];

        ui_busy("Saving password...");
        rc = maintenance ? sec_set_maint_password(a) : sec_set_exit_password(a);
        ca_wipe(a, sizeof a);
        ui_message(what, rc != 0 ? "The password could not be saved."
                         : removing ? "The password has been removed."
                                    : "The password has been changed.");
    }
}

/* Replace the recovery code after a Yes / No question, then show it. */
static void new_recovery_code(void)
{
    char code[CA_RECOVERY_LEN + 8];

    if (!ui_confirm("Recovery Code",
                    "Make a new recovery code?\nThe old code will stop working."))
        return;
    ui_busy("Making a new recovery code...");
    if (sec_new_recovery(code, sizeof code) != 0) {
        ui_message("Recovery Code", "The new recovery code could not be saved.");
        return;
    }
    recovery_code_show(code, 0);
    ca_wipe(code, sizeof code);
}

/* Maintenance > Passwords & Recovery. */
static void passwords_screen(void)
{
    static const char *const entries[] = {
        "Change Exit Password", "Change Menu Maintenance Password",
        "Make A New Recovery Code", "Return To Maintenance Menu"
    };
    static const fkey_t keys[] = { { "Esc", "Exit" } };
    int sel = 0;

    for (;;) {
        char sub[128];
        int k;

        snprintf(sub, sizeof sub, "Exit Password: %s     Maintenance Password: %s",
                 sec_has_exit_password() ? "Set" : "None",
                 sec_has_maint_password() ? "Set" : "None");
        k = menu_screen("Passwords & Recovery", sub, entries, 4, &sel, keys, 1);
        if (k == 27 || k == KEY_F(10) || (k == '\n' && sel == 3))
            return;
        if (k != '\n')
            continue;
        if (sel == 0)
            password_change(0);
        else if (sel == 1)
            password_change(1);
        else if (sel == 2)
            new_recovery_code();
    }
}

/* ---- Usage report ------------------------------------------------- */

static void usage_report(void)
{
    static const fkey_t keys[] = { { "F2", "Clear Log" }, { "Esc", "Exit" } };
    static const int exits[] = { KEY_F(2), 0 };
    char heading[96];

    snprintf(heading, sizeof heading, "%-11s%-10s%-8s%-10s%s",
             "Date", "Time", "Secs", "Status", "Menu Entry");
    for (;;) {
        char **lines = NULL, **show;
        int n, k, extra = 0, i;

        if (!sec_log_sk()) {
            ui_message("Usage Report", "The usage log key is locked.");
            return;
        }
        ui_busy("Decrypting usage log...");
        n = usage_read(&lines);
        if (n < 0) {
            ui_message("Usage Report", "The usage log could not be read.");
            return;
        }
        show = calloc((size_t)n + 2, sizeof *show);
        if (!show) {
            usage_free(lines, n);
            return;
        }
        if (!g_prefs.usage_tracking) {
            show[0] = "(Usage tracking is off: turn it on in Program Configuration.)";
            show[1] = "";
            extra = 2;
        }
        for (i = 0; i < n; i++)
            show[extra + i] = lines[i];
        k = ui_viewer("Usage Report", heading, show, n + extra, keys, 2, exits);
        free(show);
        usage_free(lines, n);
        if (k == KEY_F(2)) {
            if (ui_confirm("Usage Report", "Delete every entry in the usage log?"))
                usage_clear();
            continue;
        }
        return;
    }
}

/* ---- Menu tree ------------------------------------------------------ */

static void tree_lines(const menu_t *m, int depth, char ***lines, int *n, int *cap)
{
    int i;

    for (i = 0; i < m->count; i++) {
        const item_t *it = &m->items[i];
        char buf[1024];

        if (it->type == ITEM_SUBMENU)
            snprintf(buf, sizeof buf, "%*s%c) %s  >>", depth * 4, "", 'A' + i, it->desc);
        else
            snprintf(buf, sizeof buf, "%*s%c) %-*s  %s", depth * 4, "", 'A' + i,
                     DESC_LEN - 1, it->desc, it->cmd);
        if (*n == *cap) {
            char **nl;
            *cap = *cap ? *cap * 2 : 64;
            nl = realloc(*lines, (size_t)*cap * sizeof **lines);
            if (!nl)
                return;
            *lines = nl;
        }
        (*lines)[(*n)++] = strdup(buf);
        if (it->type == ITEM_SUBMENU && it->sub)
            tree_lines(it->sub, depth + 1, lines, n, cap);
    }
}

/* F7: the whole menu tree, indented by level, in a viewer. */
void tree_show(void)
{
    static const fkey_t keys[] = { { "Esc", "Exit" } };
    char **lines = NULL;
    int n = 0, cap = 0, i;

    tree_lines(g_main_menu, 0, &lines, &n, &cap);
    ui_viewer("Menu Tree", NULL, lines, n, keys, 1, NULL);
    for (i = 0; i < n; i++)
        free(lines[i]);
    free(lines);
}

/* ---- Create / Modify menu editor ---------------------------------- */

/* Column positions, worked out from the terminal width. */
typedef struct {
    int lx;                 /* "A)" label */
    int x[3], w[3];         /* description, directory, command */
    int top, rows;          /* first entry row and visible rows */
    int two_key_rows;
} cols_t;

/*
 * Column positions and visible rows of the Create / Modify table for
 * the current terminal size.  The description column grows on wide
 * terminals; directory and command share the rest 2:3.
 */
static void editor_layout(cols_t *c)
{
    int avail, dw, rw;

    c->lx = 2;
    avail = COLS - 5 - 4 - 4;         /* label, scroll bar, gaps */
    dw = 24 + (COLS > 80 ? (COLS - 80) / 4 : 0);
    if (dw > DESC_LEN - 1)
        dw = DESC_LEN - 1;
    rw = (avail - dw) * 2 / 5;
    c->x[0] = 5;
    c->w[0] = dw;
    c->x[1] = c->x[0] + dw + 2;
    c->w[1] = rw;
    c->x[2] = c->x[1] + rw + 2;
    c->w[2] = COLS - 4 - c->x[2];
    c->two_key_rows = COLS < 100;
    c->top = 8;
    c->rows = (LINES - (c->two_key_rows ? 6 : 5)) - c->top;
    if (c->rows < 1)
        c->rows = 1;
}

/* "1st" .. "6th" for the "(2nd Level)" note under the title. */
static const char *level_name(int level)
{
    static const char *names[] = { "", "1st", "2nd", "3rd", "4th", "5th", "6th" };
    return level >= 1 && level <= 6 ? names[level] : "";
}

/* Text shown in a cell. */
static const char *cell_text(const menu_t *m, int row, int col, char *tmp, size_t len)
{
    const item_t *it;

    if (row >= m->count)
        return "";
    it = &m->items[row];
    if (col == 0)
        return it->desc;
    if (it->type == ITEM_SUBMENU) {
        if (col == 1)
            return "<Sub-Menu>";
        snprintf(tmp, len, "-> %s", it->sub ? it->sub->title : "");
        return tmp;
    }
    return col == 1 ? it->dir : it->cmd;
}

/*
 * Draw the Create / Modify screen: title, menu title and level, column
 * headings, the visible rows (current cell, or the whole row while
 * moving, in the selection bar colors) and the function keys.
 */
static void editor_draw(const menu_t *m, const cols_t *c, int row, int col,
                        int first, int moving)
{
    static const fkey_t keys1[] = {
        { "F1", "Ins" }, { "F2", "Del" }, { "F3", "Mov" }, { "F5", "Custom" },
        { "F7", "Tree" }, { "F8", "Options" }, { "F9", "Sub-Menu" },
        { "Esc", "Exit" }, { "F10", "Main" }
    };
    static const fkey_t row1[] = {
        { "F1", "Insert" }, { "F2", "Delete" }, { "F3", "Move" },
        { "F5", "Custom Title" }, { "F7", "Tree" }
    };
    static const fkey_t row2[] = {
        { "F8", "Options" }, { "F9", "Sub-Menu" }, { "Esc", "Exit" },
        { "F10", "Maintenance Menu" }
    };
    static const char *heads[3] = { "Program Description", "Directory", "Command" };
    char line[160], tmp[80];
    int i, j, total = m->count < MENU_MAX_ITEMS ? m->count + 1 : MENU_MAX_ITEMS;

    ui_frame(0);
    ui_spaced_title(2, m->level == 1 ? "Create / Modify Main Menu"
                                     : "Create / Modify Sub-Menus");
    if (moving) {
        ui_center(3, "Moving: Up / Down to place the entry, Enter to drop, Esc to cancel",
                  theme_slot(SLOT_MESSAGE) | A_BOLD);
    } else {
        int len, x;
        snprintf(line, sizeof line, "The Title Of This %s Is '%s'.   (%s Level)",
                 m->level == 1 ? "Menu" : "Sub-Menu", m->title, level_name(m->level));
        len = (int)strlen(line);
        x = (COLS - len) / 2;
        ui_center(3, line, theme_slot(SLOT_TEXT) | A_BOLD);
        /* Show the title itself in the title color. */
        {
            const char *q = strchr(line, '\'');
            if (q && x >= 0) {
                attrset(theme_slot(SLOT_TITLE) | A_BOLD);
                mvaddnstr(3, x + (int)(q - line) + 1, m->title, COLS - 2);
            }
        }
    }
    ui_divider(4);

    for (j = 0; j < 3; j++) {
        attrset(theme_slot(SLOT_TITLE) | A_BOLD);
        mvaddnstr(6, c->x[j], heads[j], c->w[j]);
        attrset(theme_slot(SLOT_TEXT));
        mvhline(7, c->x[j], '-', c->w[j]);
    }
    for (i = 0; i < c->rows && first + i < MENU_MAX_ITEMS; i++) {
        int r = first + i, y = c->top + i;

        attrset(theme_slot(SLOT_TEXT) | A_BOLD);
        mvprintw(y, c->lx, "%c)", 'A' + r);
        if (r >= total)
            continue;
        for (j = 0; j < 3; j++) {
            const char *t = cell_text(m, r, j, tmp, sizeof tmp);
            attr_t a;

            if (r == row && (j == col || moving))
                a = theme_bar();
            else if (r < m->count && m->items[r].type == ITEM_SUBMENU && j > 0)
                a = theme_slot(SLOT_TITLE);
            else
                a = theme_slot(SLOT_TEXT) | A_BOLD;
            ui_field_text(y, c->x[j], c->w[j], t, a);
        }
        if (r < m->count && m->items[r].pwhash[0]) {
            attrset(theme_slot(SLOT_MESSAGE) | A_BOLD);
            mvaddstr(y, c->x[0] - 1, "*");     /* password protected */
        }
    }
    ui_scrollbar(c->top, COLS - 3, c->rows, first, c->rows, MENU_MAX_ITEMS);

    if (c->two_key_rows) {
        ui_divider(LINES - 5);
        ui_fkeys(LINES - 3, 1, COLS - 2, row1, 5);
        ui_fkeys(LINES - 2, 1, COLS - 2, row2, 4);
    } else {
        ui_divider(LINES - 4);
        ui_fkeys(LINES - 2, 1, COLS - 2, keys1, 9);
    }
}

/* Options dialog (F8) for a program entry. */
static void entry_options(item_t *it)
{
    int sel = 0;

    for (;;) {
        char o0[64], o1[64];
        const char *opts[3];
        int k;

        snprintf(o0, sizeof o0, "Pause After Program Ends ... [%s]", it->pause ? "Yes" : "No ");
        snprintf(o1, sizeof o1, "Entry Password ............. [%s]", it->pwhash[0] ? "Set " : "None");
        opts[0] = o0;
        opts[1] = o1;
        opts[2] = "Done";
        k = ui_pick("Entry Options", opts, 3, sel);
        if (k < 0 || k == 2)
            break;
        sel = k;
        if (k == 0) {
            it->pause = !it->pause;
        } else if (k == 1) {
            char a[128], b[128];
            if (ui_password("Entry Password", "New password for this entry\n(leave empty for none)",
                            a, sizeof a, 0) != PW_ENTERED)
                continue;
            if (!a[0]) {
                it->pwhash[0] = '\0';
                continue;
            }
            if (ui_password("Entry Password", "Type the password again", b, sizeof b, 0) != PW_ENTERED ||
                strcmp(a, b) != 0) {
                ui_message("Entry Password", "The passwords do not match.");
            } else {
                ui_busy("Saving password...");
                if (ca_pw_hash(a, it->pwhash, sizeof it->pwhash) != 0)
                    it->pwhash[0] = '\0';
            }
            ca_wipe(a, sizeof a);
            ca_wipe(b, sizeof b);
        }
    }
    menu_save();
}

/* Store an edited cell; creates the entry when typing on the new row. */
static void cell_store(menu_t *m, int row, int col, const char *text)
{
    item_t *it;

    if (row >= m->count) {
        if (!text[0] || menu_insert(m, m->count) != 0)
            return;
        ca_strlcpy(m->items[row].dir, "~", DIR_LEN);
    }
    it = &m->items[row];
    if (col == 0)
        ca_strlcpy(it->desc, text, DESC_LEN);
    else if (col == 1)
        ca_strlcpy(it->dir, text, DIR_LEN);
    else
        ca_strlcpy(it->cmd, text, CMD_LEN);
    menu_save();
}

/*
 * The Create / Modify editor for one menu.  F9 on a sub-menu entry
 * opens a nested editor for it; Esc returns one level, F10 returns to
 * the Maintenance menu from any depth (g_leave_editor).  Every change
 * is saved to menus.conf straight away.
 */
void menu_editor(menu_t *m)
{
    int row = 0, col = 0, first = 0, moving = 0, move_from = 0;
    int pending = 0;          /* key that starts editing */

    for (;;) {
        cols_t c;
        int k, total;

        if (g_leave_editor)
            return;
        if (ui_too_small()) {
            if (ui_getkey() == 27)
                return;
            continue;
        }
        editor_layout(&c);
        total = m->count < MENU_MAX_ITEMS ? m->count + 1 : MENU_MAX_ITEMS;
        if (row >= total)
            row = total - 1;
        if (row < first)
            first = row;
        if (row >= first + c.rows)
            first = row - c.rows + 1;
        editor_draw(m, &c, row, col, first, moving);

        /* Typing (or Enter) on a text cell edits it in place. */
        if (pending && !moving) {
            item_t *it = row < m->count ? &m->items[row] : NULL;
            if (it && it->type == ITEM_SUBMENU && col > 0) {
                pending = 0;
                continue;
            }
            {
                char buf[CMD_LEN];
                size_t max = col == 0 ? DESC_LEN : col == 1 ? DIR_LEN : CMD_LEN;
                char tmp[80];
                int y = c.top + row - first;
                int start = pending == '\n' ? 0 : pending;

                ca_strlcpy(buf, cell_text(m, row, col, tmp, sizeof tmp), sizeof buf);
                pending = 0;
                if (row >= m->count && col > 0) {
                    ui_message("Create / Modify", "Type the program description first.");
                    col = 0;
                    continue;
                }
                k = ui_edit(y, c.x[col], c.w[col], buf, max, 0, start);
                if (k == 27)
                    continue;
                cell_store(m, row, col, buf);
                if (k == UI_REDRAW) {
                    pending = '\n';         /* resume editing after redraw */
                    continue;
                }
                /* Keys that end editing also move. */
                if (k == '\n' || k == '\t') {
                    if (col < 2 && row < m->count && m->items[row].type == ITEM_PROGRAM)
                        col++;
                    else {
                        col = 0;
                        if (row + 1 < (m->count < MENU_MAX_ITEMS ? m->count + 1 : MENU_MAX_ITEMS))
                            row++;
                    }
                    continue;
                }
                if (k == KEY_BTAB) {
                    if (col > 0) col--;
                    continue;
                }
                /* Up / Down / function keys: handled below. */
                ungetch(k);
                continue;
            }
        }

        k = ui_getkey();
        if (moving) {
            switch (k) {
            case KEY_UP:
                if (row > 0) { menu_move(m, row, row - 1); row--; }
                break;
            case KEY_DOWN:
                if (row < m->count - 1) { menu_move(m, row, row + 1); row++; }
                break;
            case '\n':
                moving = 0;
                menu_save();
                break;
            case 27:
                menu_move(m, row, move_from);
                row = move_from;
                moving = 0;
                break;
            }
            continue;
        }
        switch (k) {
        case KEY_UP:
            if (row > 0) row--;
            break;
        case KEY_DOWN:
            if (row < total - 1) row++;
            break;
        case KEY_PPAGE:
            row = row > c.rows ? row - c.rows : 0;
            break;
        case KEY_NPAGE:
            row = row + c.rows < total ? row + c.rows : total - 1;
            break;
        case KEY_HOME:
            row = 0;
            col = 0;
            break;
        case KEY_END:
            row = total - 1;
            break;
        case KEY_LEFT:
        case KEY_BTAB:
            if (col > 0) col--;
            break;
        case KEY_RIGHT:
        case '\t':
            if (col < 2) col++;
            break;
        case '\n':
            if (row < m->count && m->items[row].type == ITEM_SUBMENU && col > 0) {
                menu_editor(m->items[row].sub);
            } else {
                pending = '\n';
            }
            break;
        case KEY_F(1):                              /* insert */
            if (m->count >= MENU_MAX_ITEMS)
                ui_message("Insert", "A menu holds at most 26 entries (A to Z).");
            else if (row <= m->count) {
                menu_insert(m, row);
                ca_strlcpy(m->items[row].dir, "~", DIR_LEN);
                ca_strlcpy(m->items[row].desc, "New Entry", DESC_LEN);
                col = 0;
                menu_save();
            }
            break;
        case KEY_F(2):                              /* delete */
            if (row < m->count) {
                char q[160];
                snprintf(q, sizeof q, "Delete '%s'%s?", m->items[row].desc,
                         m->items[row].type == ITEM_SUBMENU
                             ? "\nand everything in its sub-menus" : "");
                if (ui_confirm("Delete", q)) {
                    menu_delete(m, row);
                    menu_save();
                }
            }
            break;
        case KEY_F(3):                              /* move */
            if (row < m->count && m->count > 1) {
                moving = 1;
                move_from = row;
            }
            break;
        case KEY_F(5): {                            /* custom title */
            char t[TITLE_LEN];
            ca_strlcpy(t, m->title, sizeof t);
            if (ui_prompt("Custom Title", "Title shown at the top of this menu", t,
                          sizeof t, 0, NULL)) {
                ca_strlcpy(m->title, t, TITLE_LEN);
                menu_save();
            }
            break;
        }
        case KEY_F(7):
            tree_show();
            break;
        case KEY_F(8):
            if (row < m->count && m->items[row].type == ITEM_PROGRAM)
                entry_options(&m->items[row]);
            else if (row < m->count)
                ui_message("Options", "Options apply to program entries only.");
            break;
        case KEY_F(9):                              /* sub-menu */
            if (row >= m->count) {
                ui_message("Sub-Menu", "Type a description for the entry first.");
            } else if (m->items[row].type == ITEM_SUBMENU) {
                menu_editor(m->items[row].sub);
            } else if (m->level >= MENU_MAX_LEVELS) {
                ui_message("Sub-Menu", "Sub-menus can be at most six levels deep.");
            } else if (!m->items[row].cmd[0] ||
                       ui_confirm("Sub-Menu", "Turn this entry into a sub-menu?\n"
                                              "Its command will be removed.")) {
                menu_make_submenu(m, row);
                menu_save();
                menu_editor(m->items[row].sub);
            }
            break;
        case 27:
            menu_save();
            return;
        case KEY_F(10):
            menu_save();
            g_leave_editor = 1;
            return;
        case UI_REDRAW:
            break;
        default:
            if (k >= 32 && k < 127)
                pending = k;               /* start editing with this key */
            break;
        }
    }
}

/* ---- Maintenance menu ---------------------------------------------- */

void maintenance_run(void)
{
    static const char *const entries[] = {
        "Create / Modify Main Menu", "Program Configuration", "Color Themes",
        "Usage Report", "Passwords & Recovery", "Return To Main Menu"
    };
    static const fkey_t keys[] = { { "F7", "Menu Tree" }, { "Esc", "Return To Main Menu" } };
    int sel = 0;

    ca_debug("maintenance opened");
    for (;;) {
        int k = menu_screen("Menu Maintenance", "Please Enter Your Selection",
                            entries, 6, &sel, keys, 2);

        if (k == 27 || k == KEY_F(10) || (k == '\n' && sel == 5))
            return;
        if (k == KEY_F(7)) {
            tree_show();
            continue;
        }
        if (k != '\n')
            continue;
        switch (sel) {
        case 0:
            g_leave_editor = 0;
            menu_editor(g_main_menu);
            g_leave_editor = 0;
            break;
        case 1:
            if (config_screen())
                return;
            break;
        case 2:
            themes_screen();
            break;
        case 3:
            usage_report();
            break;
        case 4:
            passwords_screen();
            break;
        }
    }
}
