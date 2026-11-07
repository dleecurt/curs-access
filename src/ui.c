/*
 * ui.c - The Curses Access screen kit (see ui.h and doc/DESIGN.md).
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
#include <langinfo.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "crypto.h"
#include "prefs.h"
#include "theme.h"
#include "ui.h"
#include "util.h"

static int g_clock_shown;    /* the current screen has a clock */
static time_t g_last_key;    /* for the screen blanker */
static int g_utf8;
static int g_last_dy = -1, g_last_dx, g_last_dh, g_last_dw;   /* previous dialog */

/* ---- Start / stop -------------------------------------------------- */

void ui_init(void)
{
    const char *cs = nl_langinfo(CODESET);

    g_utf8 = cs && (ca_strcaseeq(cs, "UTF-8") || ca_strcaseeq(cs, "utf8"));
    initscr();
    raw();          /* ^C ^\ ^Z ^S ^Q arrive as keys, never as signals */
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    curs_set(0);
    theme_init_curses();
    g_last_key = time(NULL);
}

void ui_end(void)
{
    curs_set(1);
    endwin();
}

int ui_utf8(void) { return g_utf8; }

/* ---- Keys, clock and blanker --------------------------------------- */

/*
 * The screen blanker: an empty screen with a short notice that moves
 * every few seconds so nothing burns in.  Any key ends it; that key is
 * not passed on.
 */
static void blank_screen(void)
{
    const char *msg = "Curses Access  -  Press any key";
    int len = (int)strlen(msg);

    ca_debug("screen blanker on");
    curs_set(0);
    for (;;) {
        int y, x, ch;

        bkgdset(' ' | theme_attr(C_LGRAY, C_BLACK));
        erase();
        y = LINES > 1 ? rand() % LINES : 0;
        x = COLS > len ? rand() % (COLS - len) : 0;
        attrset(theme_attr(C_DGRAY, C_BLACK));
        mvaddstr(y, x, msg);
        refresh();
        timeout(5000);
        ch = getch();
        if (ch != ERR && ch != KEY_RESIZE)
            break;
    }
    attrset(A_NORMAL);
    bkgdset(' ' | theme_slot(SLOT_TEXT));
    clear();
}

/* Shared key loop; blank = allow the blanker. */
static int getkey_common(int blank)
{
    for (;;) {
        int ch;

        timeout(1000);
        ch = getch();
        if (ch == ERR) {
            if (g_clock_shown && g_prefs.clock) {
                ui_draw_clock();
                refresh();
            }
            if (blank && g_prefs.blank_minutes > 0 &&
                time(NULL) - g_last_key >= (time_t)g_prefs.blank_minutes * 60) {
                blank_screen();
                g_last_key = time(NULL);
                return UI_REDRAW;
            }
            continue;
        }
        g_last_key = time(NULL);
        if (ch == KEY_RESIZE)
            return UI_REDRAW;
        if (ch == 27) {
            /* Esc followed quickly by a digit stands for F1..F10. */
            int c2;
            timeout(250);
            c2 = getch();
            if (c2 >= '1' && c2 <= '9')
                return KEY_F(c2 - '0');
            if (c2 == '0')
                return KEY_F(10);
            if (c2 != ERR)
                ungetch(c2);
            return 27;
        }
        if (ch == '\r')
            ch = '\n';
        if (ch == KEY_ENTER)
            ch = '\n';
        return ch;
    }
}

int ui_getkey(void) { return getkey_common(1); }
int ui_getkey_plain(void) { return getkey_common(0); }

/* ---- Basic drawing --------------------------------------------------- */

void ui_clear(void)
{
    bkgdset(' ' | theme_slot(SLOT_TEXT));
    attrset(theme_slot(SLOT_TEXT));
    erase();
    g_clock_shown = 0;
    g_last_dy = -1;
}

int ui_too_small(void)
{
    char msg[96];

    if (LINES >= UI_MIN_LINES && COLS >= UI_MIN_COLS)
        return 0;
    ui_clear();
    snprintf(msg, sizeof msg, "Terminal too small: %dx%d", COLS, LINES);
    ui_center(LINES / 2 - 1, msg, theme_slot(SLOT_MESSAGE));
    snprintf(msg, sizeof msg, "Curses Access needs %dx%d", UI_MIN_COLS, UI_MIN_LINES);
    ui_center(LINES / 2, msg, theme_slot(SLOT_TEXT));
    refresh();
    return 1;
}

void ui_fill(int y, int x, int h, int w, attr_t attr)
{
    int r;

    attrset(attr);
    for (r = 0; r < h; r++)
        mvhline(y + r, x, ' ', w);
}

void ui_field_text(int y, int x, int w, const char *text, attr_t attr)
{
    int len = (int)strlen(text);

    if (w <= 0)
        return;
    attrset(attr);
    mvaddnstr(y, x, text, len < w ? len : w);
    if (len < w)
        hline(' ', w - len);
}

void ui_center(int y, const char *text, attr_t attr)
{
    int len = (int)strlen(text);
    int x = (COLS - len) / 2;

    if (x < 0)
        x = 0;
    attrset(attr);
    mvaddnstr(y, x, text, COLS - x);
}

void ui_spaced_title(int y, const char *text)
{
    char buf[256];
    size_t i, o = 0;

    /* "Main Menu" -> "M A I N   M E N U" */
    for (i = 0; text[i] && o + 4 < sizeof buf; i++) {
        if (i > 0)
            buf[o++] = ' ';
        if (text[i] == ' ') {
            buf[o++] = ' ';       /* word gap: three spaces in all */
            continue;
        }
        buf[o++] = (char)toupper((unsigned char)text[i]);
    }
    buf[o] = '\0';
    if ((int)o > COLS - 4) {
        /* Too wide when spaced out: show it plainly in capitals. */
        for (i = 0; text[i] && i + 1 < sizeof buf; i++)
            buf[i] = (char)toupper((unsigned char)text[i]);
        buf[i] = '\0';
    }
    ui_center(y, buf, theme_slot(SLOT_TITLE) | A_BOLD);
}

void ui_box(int y, int x, int h, int w, attr_t attr)
{
    attrset(attr);
    mvadd_wch(y, x, WACS_D_ULCORNER);
    mvhline_set(y, x + 1, WACS_D_HLINE, w - 2);
    mvadd_wch(y, x + w - 1, WACS_D_URCORNER);
    mvvline_set(y + 1, x, WACS_D_VLINE, h - 2);
    mvvline_set(y + 1, x + w - 1, WACS_D_VLINE, h - 2);
    mvadd_wch(y + h - 1, x, WACS_D_LLCORNER);
    mvhline_set(y + h - 1, x + 1, WACS_D_HLINE, w - 2);
    mvadd_wch(y + h - 1, x + w - 1, WACS_D_LRCORNER);
}

void ui_frame(int clock)
{
    ui_clear();
    ui_box(0, 0, LINES, COLS, theme_slot(SLOT_SCREEN_BORDER));
    g_clock_shown = clock;
    if (clock && g_prefs.clock)
        ui_draw_clock();
}

void ui_draw_clock(void)
{
    char date[64], tim[32], buf[80];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int y = LINES - 1, dx, tx, i;

    if (!g_clock_shown || !g_prefs.clock || COLS < 40)
        return;
    strftime(date, sizeof date, "%A  %B %e, %Y", tm);
    /* "%e" pads with a space; squeeze "February  9" to "February 9". */
    {
        char *p = strstr(date, "  ");
        p = p ? strstr(p + 2, "  ") : NULL;
        if (p)
            memmove(p, p + 1, strlen(p));
    }
    if (g_prefs.clock_24h) {
        strftime(tim, sizeof tim, "%H:%M:%S", tm);
    } else {
        strftime(tim, sizeof tim, "%I:%M:%S %p", tm);
        for (i = 0; tim[i]; i++)
            tim[i] = (char)tolower((unsigned char)tim[i]);
        if (tim[0] == '0')
            memmove(tim, tim + 1, strlen(tim));
    }
    /* Redraw the border line first so a shorter time leaves no trace. */
    attrset(theme_slot(SLOT_SCREEN_BORDER));
    mvhline_set(y, 1, WACS_D_HLINE, COLS - 2);

    snprintf(buf, sizeof buf, " %s ", date);
    dx = COLS * 3 / 10 - (int)strlen(buf) / 2;
    if (dx < 2)
        dx = 2;
    attrset(theme_slot(SLOT_TEXT) | A_BOLD);
    mvaddstr(y, dx, buf);
    snprintf(buf, sizeof buf, " %s ", tim);
    tx = COLS * 4 / 5 - (int)strlen(buf) / 2;
    if (tx + (int)strlen(buf) > COLS - 2)
        tx = COLS - 2 - (int)strlen(buf);
    mvaddstr(y, tx, buf);
}

void ui_divider(int y)
{
    attrset(theme_slot(SLOT_SCREEN_BORDER));
    mvadd_wch(y, 0, WACS_D_LTEE);
    mvhline_set(y, 1, WACS_D_HLINE, COLS - 2);
    mvadd_wch(y, COLS - 1, WACS_D_RTEE);
}

void ui_fkeys(int y, int x1, int x2, const fkey_t *keys, int n)
{
    int total = 0, i, gap, x, avail = x2 - x1 + 1;

    for (i = 0; i < n; i++)
        total += (int)strlen(keys[i].key) + 1 + (int)strlen(keys[i].label);
    gap = (avail - total) / (n + 1);
    if (gap < 1)
        gap = 1;
    x = x1 + gap;
    if (gap == 1)
        x = x1;
    for (i = 0; i < n; i++) {
        int kl = (int)strlen(keys[i].key), ll = (int)strlen(keys[i].label);

        if (x + kl > x2)
            break;
        attrset(theme_slot(SLOT_KEY) | A_BOLD);
        mvaddstr(y, x, keys[i].key);
        x += kl + 1;
        attrset(theme_slot(SLOT_KEY_LABEL) | A_BOLD);
        mvaddnstr(y, x, keys[i].label, x2 - x + 1);
        x += ll + gap;
    }
}

void ui_scrollbar(int y, int x, int h, int first, int visible, int total)
{
    int r, thumb, tsize;
    attr_t a = theme_slot(SLOT_MENU_BORDER);

    if (h < 3)
        return;
    attrset(a);
    mvaddstr(y, x, g_utf8 ? "\xe2\x86\x91" : "^");           /* up arrow */
    mvaddstr(y + h - 1, x, g_utf8 ? "\xe2\x86\x93" : "v");   /* down arrow */
    for (r = 1; r < h - 1; r++)
        mvaddstr(y + r, x, g_utf8 ? "\xe2\x96\x91" : ":");   /* light shade */
    if (total <= visible || total <= 0)
        return;
    tsize = (h - 2) * visible / total;
    if (tsize < 1)
        tsize = 1;
    thumb = (h - 2 - tsize) * first / (total - visible);
    for (r = 0; r < tsize; r++)
        mvaddstr(y + 1 + thumb + r, x, g_utf8 ? "\xe2\x96\x88" : "#");  /* full block */
}

/* ---- Dialogs --------------------------------------------------------- */

/*
 * Draw an empty dialog of h x w centred on the screen with a double
 * border and the title in the top border.  Returns its position.
 */
static void dialog(int h, int w, const char *title, int *py, int *px)
{
    int y, x;

    /* Wipe the previous dialog so two borders never overlap. */
    if (g_last_dy >= 0 && g_last_dy + g_last_dh <= LINES && g_last_dx + g_last_dw <= COLS)
        ui_fill(g_last_dy, g_last_dx, g_last_dh, g_last_dw, theme_slot(SLOT_TEXT));

    if (w > COLS - 2)
        w = COLS - 2;
    if (h > LINES - 2)
        h = LINES - 2;
    y = (LINES - h) / 2;
    x = (COLS - w) / 2;
    ui_fill(y, x, h, w, theme_slot(SLOT_TEXT));
    ui_box(y, x, h, w, theme_slot(SLOT_MENU_BORDER));
    if (title && *title) {
        int tl = (int)strlen(title) + 2;
        attrset(theme_slot(SLOT_TITLE) | A_BOLD);
        mvprintw(y, x + (w - tl) / 2, " %s ", title);
    }
    *py = y;
    *px = x;
    g_last_dy = y;
    g_last_dx = x;
    g_last_dh = h;
    g_last_dw = w;
}

/* Count lines and the longest line of a '\n'-separated text. */
static void measure(const char *text, int *lines, int *longest)
{
    int n = 1, cur = 0, max = 0;

    for (; *text; text++) {
        if (*text == '\n') {
            n++;
            cur = 0;
        } else if (++cur > max) {
            max = cur;
        }
    }
    *lines = n;
    *longest = max;
}

/* Draw '\n'-separated text starting at (y, x), each line centred in w. */
static void draw_lines(int y, int x, int w, const char *text, attr_t attr)
{
    char line[512];
    const char *p = text;

    while (*p) {
        size_t len = strcspn(p, "\n");
        int lx;

        if (len >= sizeof line)
            len = sizeof line - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        lx = x + (w - (int)len) / 2;
        if (lx < x)
            lx = x;
        attrset(attr);
        mvaddnstr(y++, lx, line, w);
        p += strcspn(p, "\n");
        if (*p == '\n')
            p++;
    }
}

void ui_message(const char *title, const char *text)
{
    for (;;) {
        int nl, longest, y, x, w, h, k;

        measure(text, &nl, &longest);
        w = longest + 8;
        if (w < 36)
            w = 36;
        h = nl + 5;
        dialog(h, w, title, &y, &x);
        if (w > COLS - 2)
            w = COLS - 2;
        draw_lines(y + 2, x + 2, w - 4, text, theme_slot(SLOT_TEXT) | A_BOLD);
        attrset(theme_slot(SLOT_KEY_LABEL) | A_BOLD);
        mvaddstr(y + h - 2 > LINES - 2 ? LINES - 2 : y + h - 2, x + (w - 15) / 2, "Press any key...");
        refresh();
        k = ui_getkey_plain();
        if (k == UI_REDRAW) {
            ui_clear();
            continue;
        }
        return;
    }
}

int ui_confirm(const char *title, const char *question)
{
    int sel = 1;     /* start on "No": the safe answer */

    for (;;) {
        int nl, longest, y, x, w, h, k, bx;

        measure(question, &nl, &longest);
        w = longest + 8;
        if (w < 36)
            w = 36;
        h = nl + 6;
        dialog(h, w, title, &y, &x);
        draw_lines(y + 2, x + 2, w - 4, question, theme_slot(SLOT_TEXT) | A_BOLD);
        bx = x + (w - 18) / 2;
        attrset(sel == 0 ? theme_bar() : theme_slot(SLOT_TEXT));
        mvaddstr(y + h - 2, bx, "  Yes  ");
        attrset(sel == 1 ? theme_bar() : theme_slot(SLOT_TEXT));
        mvaddstr(y + h - 2, bx + 11, "  No   ");
        refresh();
        k = ui_getkey_plain();
        switch (k) {
        case 'y': case 'Y':
            return 1;
        case 'n': case 'N': case 27:
            return 0;
        case KEY_LEFT: case KEY_RIGHT: case '\t': case KEY_BTAB:
            sel = !sel;
            break;
        case '\n':
            return sel == 0;
        case UI_REDRAW:
            ui_clear();
            break;
        }
    }
}

int ui_edit(int y, int x, int w, char *buf, size_t buflen, int masked,
            int first_key)
{
    char work[1024];
    int len, cur, off = 0, k;

    ca_strlcpy(work, buf, sizeof work < buflen ? sizeof work : buflen);
    /* Typing straight onto a field replaces it; Enter edits what is there. */
    if (first_key >= 32 && first_key < 127)
        work[0] = '\0';
    len = (int)strlen(work);
    cur = len;
    curs_set(1);
    for (;;) {
        int i;

        /* Keep the cursor inside the visible part of the field. */
        if (cur < off)
            off = cur;
        if (cur - off >= w)
            off = cur - w + 1;
        attrset(theme_field());
        move(y, x);
        for (i = 0; i < w; i++) {
            int p = off + i;
            addch(p < len ? (masked ? '*' : (unsigned char)work[p]) : ' ');
        }
        move(y, x + cur - off);
        refresh();

        if (first_key) {
            k = first_key;
            first_key = 0;
        } else {
            k = ui_getkey_plain();
        }
        switch (k) {
        case KEY_LEFT:
            if (cur > 0) cur--;
            break;
        case KEY_RIGHT:
            if (cur < len) cur++;
            break;
        case KEY_HOME: case 1:          /* Ctrl-A */
            cur = 0;
            break;
        case KEY_END: case 5:           /* Ctrl-E */
            cur = len;
            break;
        case KEY_BACKSPACE: case 127: case 8:
            if (cur > 0) {
                memmove(work + cur - 1, work + cur, (size_t)(len - cur + 1));
                cur--;
                len--;
            }
            break;
        case KEY_DC:
            if (cur < len) {
                memmove(work + cur, work + cur + 1, (size_t)(len - cur));
                len--;
            }
            break;
        case 21:                        /* Ctrl-U: clear the field */
            work[0] = '\0';
            len = cur = 0;
            break;
        case 27:
            if (masked)
                ca_wipe(work, sizeof work);
            curs_set(0);
            return 27;
        case UI_REDRAW:
            /* Keep what was typed; the caller redraws and edits again. */
            ca_strlcpy(buf, work, buflen);
            curs_set(0);
            return UI_REDRAW;
        default:
            if (k >= 32 && k < 127) {
                if (len + 1 < (int)buflen && len + 1 < (int)sizeof work) {
                    memmove(work + cur + 1, work + cur, (size_t)(len - cur + 1));
                    work[cur++] = (char)k;
                    len++;
                }
            } else if (k == '\n' || k == '\t' || k == KEY_BTAB || k == KEY_UP ||
                       k == KEY_DOWN || k == KEY_PPAGE || k == KEY_NPAGE ||
                       (k >= KEY_F(1) && k <= KEY_F(12))) {
                ca_strlcpy(buf, work, buflen);
                if (masked)
                    ca_wipe(work, sizeof work);   /* no password copies left behind */
                curs_set(0);
                return k;
            }
            break;
        }
    }
}

/* Shared body of ui_prompt and ui_password. */
static int prompt_dialog(const char *title, const char *prompt, char *buf,
                         size_t buflen, int masked, const char *hint,
                         int f4_recovery)
{
    for (;;) {
        int nl, longest, y, x, w, h, fw, k, hl = 0, hlong = 0;

        measure(prompt, &nl, &longest);
        if (hint)
            measure(hint, &hl, &hlong);
        w = longest > hlong ? longest : hlong;
        w += 8;
        if (w < 50)
            w = 50;
        if (w > COLS - 4)
            w = COLS - 4;
        h = nl + 5 + (hint ? hl + 1 : 0);
        dialog(h, w, title, &y, &x);
        draw_lines(y + 2, x + 2, w - 4, prompt, theme_slot(SLOT_TEXT) | A_BOLD);
        if (hint)
            draw_lines(y + nl + 4, x + 2, w - 4, hint, theme_slot(SLOT_KEY_LABEL));
        fw = w - 6;
        k = ui_edit(y + nl + 2, x + 3, fw, buf, buflen, masked, 0);
        if (k == UI_REDRAW) {
            ui_clear();
            continue;
        }
        if (k == 27)
            return PW_CANCEL;
        if (k == KEY_F(4) && f4_recovery)
            return PW_RECOVERY;
        if (k == '\n')
            return PW_ENTERED;
    }
}

int ui_prompt(const char *title, const char *prompt, char *buf,
              size_t buflen, int masked, const char *hint)
{
    return prompt_dialog(title, prompt, buf, buflen, masked, hint, 0) == PW_ENTERED;
}

int ui_password(const char *title, const char *prompt, char *buf,
                size_t buflen, int allow_recovery)
{
    buf[0] = '\0';
    return prompt_dialog(title, prompt, buf, buflen, 1,
                         allow_recovery ? "F4  Forgot it? Use your recovery code" : NULL,
                         allow_recovery);
}

int ui_pick(const char *title, const char *const *options, int n, int current)
{
    int sel = current < 0 || current >= n ? 0 : current, first = 0;

    for (;;) {
        int longest = (int)strlen(title) + 4, i, y, x, w, h, vis, k;

        for (i = 0; i < n; i++)
            if ((int)strlen(options[i]) > longest)
                longest = (int)strlen(options[i]);
        w = longest + 8;
        vis = n;
        if (vis > LINES - 6)
            vis = LINES - 6;
        h = vis + 4;
        if (sel < first)
            first = sel;
        if (sel >= first + vis)
            first = sel - vis + 1;
        dialog(h, w, title, &y, &x);
        for (i = 0; i < vis; i++) {
            int idx = first + i;
            attr_t a = idx == sel ? theme_bar() : theme_slot(SLOT_TEXT) | A_BOLD;
            ui_field_text(y + 2 + i, x + 3, w - 6, "", a);
            attrset(a);
            mvaddnstr(y + 2 + i, x + 4, options[idx], w - 8);
        }
        if (n > vis)
            ui_scrollbar(y + 1, x + w - 2, h - 2, first, vis, n);
        refresh();
        k = ui_getkey_plain();
        switch (k) {
        case KEY_UP:
            if (sel > 0) sel--;
            break;
        case KEY_DOWN:
            if (sel < n - 1) sel++;
            break;
        case KEY_HOME:
            sel = 0;
            break;
        case KEY_END:
            sel = n - 1;
            break;
        case KEY_PPAGE:
            sel = sel > vis ? sel - vis : 0;
            break;
        case KEY_NPAGE:
            sel = sel + vis < n ? sel + vis : n - 1;
            break;
        case '\n':
            return sel;
        case 27:
            return -1;
        case UI_REDRAW:
            ui_clear();
            break;
        }
    }
}

int ui_viewer(const char *title, const char *heading, char **lines, int n,
              const fkey_t *keys, int nkeys, const int *exit_keys)
{
    int first = 0, hoff = 0;

    for (;;) {
        int top, bottom, vis, i, k, longest = 0;

        if (ui_too_small()) {
            if (ui_getkey() == 27)
                return 27;
            continue;
        }
        ui_frame(1);
        ui_spaced_title(2, title);
        ui_divider(4);
        top = 5;
        if (heading) {
            attrset(theme_slot(SLOT_TITLE) | A_BOLD);
            mvaddnstr(5, 3, heading, COLS - 6);
            top = 6;
        }
        bottom = LINES - 5;
        vis = bottom - top + 1;
        for (i = 0; i < n; i++)
            if ((int)strlen(lines[i]) > longest)
                longest = (int)strlen(lines[i]);
        if (first > n - vis)
            first = n - vis;
        if (first < 0)
            first = 0;
        for (i = 0; i < vis && first + i < n; i++) {
            const char *s = lines[first + i];
            int sl = (int)strlen(s);
            attrset(theme_slot(SLOT_TEXT) | A_BOLD);
            if (hoff < sl)
                mvaddnstr(top + i, 3, s + hoff, COLS - 7);
        }
        if (n == 0)
            ui_center(top + 1, "(nothing to show)", theme_slot(SLOT_TEXT));
        ui_scrollbar(top, COLS - 3, vis, first, vis, n);
        ui_divider(LINES - 4);
        ui_fkeys(LINES - 2, 1, COLS - 2, keys, nkeys);
        refresh();
        k = ui_getkey();
        switch (k) {
        case KEY_UP: if (first > 0) first--; break;
        case KEY_DOWN: if (first < n - vis) first++; break;
        case KEY_PPAGE: first -= vis; if (first < 0) first = 0; break;
        case KEY_NPAGE: first += vis; break;
        case KEY_HOME: first = 0; hoff = 0; break;
        case KEY_END: first = n; break;
        case KEY_LEFT: hoff = hoff > 8 ? hoff - 8 : 0; break;
        case KEY_RIGHT: if (hoff + 8 < longest) hoff += 8; break;
        case 27:
            return 27;
        default:
            if (exit_keys)
                for (i = 0; exit_keys[i]; i++)
                    if (k == exit_keys[i])
                        return k;
            break;
        }
    }
}

void ui_busy(const char *text)
{
    int y, x, w = (int)strlen(text) + 8;

    dialog(5, w < 30 ? 30 : w, NULL, &y, &x);
    attrset(theme_slot(SLOT_MESSAGE) | A_BOLD);
    mvaddstr(y + 2, x + ((w < 30 ? 30 : w) - (int)strlen(text)) / 2, text);
    refresh();
}
