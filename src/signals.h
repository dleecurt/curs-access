/*
 * signals.h - Signal handling for Curses Access.
 *
 * The menu must not be closed from the keyboard or with a plain kill:
 *   SIGINT (^C), SIGQUIT (^\), SIGTSTP (^Z) and SIGTERM are ignored
 *   (and SIGTTOU, so the menu can take the terminal back from a program).
 *   SIGHUP (the terminal went away) restores the terminal and exits.
 *   SIGKILL cannot be caught by any program.
 * The terminal is also put in raw mode (see ui_init), so ^C, ^\, ^Z and
 * ^S/^Q reach the program as ordinary keys rather than as signals or
 * flow control.
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

#ifndef CA_SIGNALS_H
#define CA_SIGNALS_H

/*
 * Install the menu's signal settings.  Call before ui_init(): ncurses
 * only installs its own SIGINT/SIGTSTP/SIGTERM handlers for signals
 * still at their default, so ignoring them first keeps it from doing so.
 */
void ca_signals_trap(void);

/*
 * Put every signal the menu changed back to its default.  Used in a
 * child process before it runs a menu entry (ignored signals stay
 * ignored across exec, so programs would otherwise not see ^C), and
 * before the "Command To Run On Exit" replaces the program.
 */
void ca_signals_default(void);

#endif /* CA_SIGNALS_H */
