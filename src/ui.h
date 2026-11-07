/*
 * ui.h - The Curses Access screen kit.
 *
 * These are the building blocks every screen is made from: the screen
 * frame with its clock, letter-spaced titles, double-line boxes, the
 * function key bar, selection bars, dialogs, input fields and text
 * viewers.  Layout is computed from LINES and COLS each time a screen is
 * drawn, so every screen follows the size of the terminal.
 * See doc/DESIGN.md for the design rules these functions implement.
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

#ifndef CA_UI_H
#define CA_UI_H

#include <curses.h>

/* Smallest terminal the screens are laid out for. */
#define UI_MIN_COLS 64
#define UI_MIN_LINES 20

/*
 * Extra key codes returned by ui_getkey():
 *   UI_REDRAW  the screen must be redrawn (after the blanker or a resize).
 */
#define UI_REDRAW (KEY_MAX + 1)

/* Result of a password prompt. */
enum { PW_CANCEL = 0, PW_ENTERED = 1, PW_RECOVERY = 2 };

/* One entry of a function key bar: key name and what it does. */
typedef struct {
    const char *key;     /* "F1", "Esc" */
    const char *label;   /* "Maintenance" */
} fkey_t;

/* Start and stop curses. */
void ui_init(void);
void ui_end(void);

/* Non-zero when the terminal can show UTF-8 block characters. */
int ui_utf8(void);

/*
 * Read a key.  Waits up to a second at a time, keeping the clock
 * current and starting the screen blanker after the idle time set in
 * Program Configuration.  "Esc 1".."Esc 0" are read as F1..F10 for
 * terminals that use the function keys themselves.
 */
int ui_getkey(void);

/* Read a key without the blanker (inside dialogs). */
int ui_getkey_plain(void);

/* Clear the screen to the theme background. */
void ui_clear(void);

/* Too-small terminal: show a notice and return 1. */
int ui_too_small(void);

/*
 * Screen frame: double border around the whole screen.  When clock is
 * set the date and time sit in the bottom border, as in the original.
 */
void ui_frame(int clock);
void ui_draw_clock(void);

/* Horizontal double divider across the frame at row y (with tees). */
void ui_divider(int y);

/* Text centred on row y. */
void ui_center(int y, const char *text, attr_t attr);

/* Letter-spaced title ("W E L C O M E") centred on row y. */
void ui_spaced_title(int y, const char *text);

/* Double-line box. */
void ui_box(int y, int x, int h, int w, attr_t attr);

/* Fill a rectangle with spaces in attr. */
void ui_fill(int y, int x, int h, int w, attr_t attr);

/* Write at most w columns of text, padded with spaces to w. */
void ui_field_text(int y, int x, int w, const char *text, attr_t attr);

/* Function key bar spread evenly across row y between x1 and x2. */
void ui_fkeys(int y, int x1, int x2, const fkey_t *keys, int n);

/* Vertical scroll bar with arrows, at column x from row y, h rows. */
void ui_scrollbar(int y, int x, int h, int first, int visible, int total);

/* Message box; waits for a key.  Lines are separated by '\n'. */
void ui_message(const char *title, const char *text);

/* Yes / No question.  Returns 1 for Yes, 0 for No or Esc. */
int ui_confirm(const char *title, const char *question);

/*
 * Edit a one-line text field in place at (y, x), w columns wide.
 * masked: show '*' for each character.  Editing keys: Left, Right,
 * Home, End, Backspace, Delete, Ctrl-U (clear).  Returns the key that
 * ended editing (Enter = '\n', 27 = Esc, KEY_UP, KEY_DOWN, '\t',
 * KEY_BTAB or a function key).  On Esc the buffer is left unchanged.
 * If first_key is non-zero it is processed as the first keystroke; a
 * printable first key replaces the old text (spreadsheet style).
 */
int ui_edit(int y, int x, int w, char *buf, size_t buflen, int masked,
            int first_key);

/*
 * Dialog with a prompt and one input field.  Returns 1 when Enter was
 * pressed, 0 for Esc.  hint, if not NULL, is shown under the field.
 */
int ui_prompt(const char *title, const char *prompt, char *buf,
              size_t buflen, int masked, const char *hint);

/*
 * Password dialog.  When allow_recovery is set, F4 offers the recovery
 * code and PW_RECOVERY is returned.
 */
int ui_password(const char *title, const char *prompt, char *buf,
                size_t buflen, int allow_recovery);

/* Pick one of n options in a dialog; returns the index or -1. */
int ui_pick(const char *title, const char *const *options, int n, int current);

/*
 * Scrollable text viewer filling the frame.  heading may be NULL.
 * keys/nkeys describe extra function keys; when one of the keys in
 * exit_keys (0-terminated) is pressed it is returned.  Esc returns 27.
 */
int ui_viewer(const char *title, const char *heading, char **lines, int n,
              const fkey_t *keys, int nkeys, const int *exit_keys);

/* Busy notice for slow operations (key derivation). */
void ui_busy(const char *text);

#endif /* CA_UI_H */
