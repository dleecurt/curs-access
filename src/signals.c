/*
 * signals.c - Signal handling for Curses Access (see signals.h).
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

#include <curses.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "signals.h"

/* Signals the menu ignores. */
/* SIGTTOU too: the menu takes the terminal back after a program ends. */
static const int ignored[] = { SIGINT, SIGQUIT, SIGTSTP, SIGTERM, SIGTTOU };
#define N_IGNORED ((int)(sizeof ignored / sizeof ignored[0]))

/*
 * SIGHUP: the terminal is gone, so nobody can use the menu any more.
 * Restore the terminal settings (in case the line comes back) and exit.
 */
static void on_hangup(int sig)
{
    (void)sig;
    endwin();
    _exit(129);
}

/* Set one signal's handler (SIG_IGN, SIG_DFL or a function). */
static void set(int sig, void (*handler)(int))
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

void ca_signals_trap(void)
{
    int i;

    for (i = 0; i < N_IGNORED; i++)
        set(ignored[i], SIG_IGN);
    set(SIGHUP, on_hangup);
}

void ca_signals_default(void)
{
    int i;

    for (i = 0; i < N_IGNORED; i++)
        set(ignored[i], SIG_DFL);
    set(SIGHUP, SIG_DFL);
}
