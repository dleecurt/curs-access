/*
 * launch.c - Run a menu entry's program and come back to the menu.
 *
 * Curses is suspended, the command runs through /bin/sh in its working
 * directory, and when it ends the menu is redrawn.  When usage tracking
 * is on, an encrypted log entry is written.
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

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "crypto.h"
#include "prefs.h"
#include "signals.h"
#include "screens.h"
#include "ui.h"
#include "usage.h"
#include "util.h"

/* Ask for an entry's own password, if it has one.  1 = may run. */
static int entry_password_ok(const item_t *it)
{
    char pw[128];
    int ok;

    if (!it->pwhash[0])
        return 1;
    if (ui_password(it->desc, "This entry is password protected.\nEnter its password",
                    pw, sizeof pw, 0) != PW_ENTERED)
        return 0;
    ui_busy("Checking password...");
    ok = ca_pw_verify(it->pwhash, pw) == 1;
    ca_wipe(pw, sizeof pw);
    if (!ok) {
        sleep(1);
        ui_message(it->desc, "Incorrect password.");
    }
    return ok;
}

/*
 * Resume every stopped process in process group pgid.  A program run
 * through "sh -c" is usually a grandchild of the menu, so waitpid()
 * never reports it stopping: look at /proc instead.
 */
static void continue_stopped(pid_t pgid)
{
    DIR *d = opendir("/proc");
    struct dirent *de;
    int found = 0;

    if (!d)
        return;
    while ((de = readdir(d)) != NULL && !found) {
        char path[300], buf[512], *p, state;
        long pgrp;
        FILE *fp;

        if (de->d_name[0] < '0' || de->d_name[0] > '9')
            continue;
        snprintf(path, sizeof path, "/proc/%s/stat", de->d_name);
        fp = fopen(path, "r");
        if (!fp)
            continue;
        if (fgets(buf, sizeof buf, fp)) {
            /* "pid (comm) state ppid pgrp ..."; comm may hold spaces. */
            p = strrchr(buf, ')');
            if (p && sscanf(p + 1, " %c %*d %ld", &state, &pgrp) == 2 &&
                pgrp == (long)pgid && (state == 'T' || state == 't'))
                found = 1;
        }
        fclose(fp);
    }
    closedir(d);
    if (found) {
        ca_debug("program was stopped (^Z): continuing it");
        kill(-pgid, SIGCONT);
    }
}

/*
 * Run cmd through /bin/sh in dir and wait for it.  Returns the child's
 * pid (-1 if it could not start) and its exit code in *code.
 *
 * When the menu owns the terminal, the program gets a process group of
 * its own and is made the terminal's foreground group: ^C, ^\ and ^Z
 * then go to the program alone.  The menu does not allow programs to be
 * suspended (it would be left waiting with no way back), so a ^Z stop
 * is undone at once.
 */
static pid_t run_command(const char *dir, const char *cmd, int *code)
{
    int tty = isatty(STDIN_FILENO) && tcgetpgrp(STDIN_FILENO) == getpgrp();
    int sync[2] = { -1, -1 };
    int status = 0;
    pid_t pid;

    *code = -1;
    if (tty && pipe(sync) != 0)
        tty = 0;
    pid = fork();
    if (pid == 0) {
        if (tty) {
            char c;
            setpgid(0, 0);
            close(sync[1]);
            /* Wait until the parent has handed over the terminal. */
            while (read(sync[0], &c, 1) < 0 && errno == EINTR)
                ;
            close(sync[0]);
        }
        /* The menu ignores ^C and friends; the program it runs must not. */
        ca_signals_default();
        if (chdir(dir) != 0)
            fprintf(stderr, "curs-access: cannot change to %s: %s\n", dir, strerror(errno));
        setenv("CURS_ACCESS", "1", 1);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    if (pid < 0) {
        if (tty) {
            close(sync[0]);
            close(sync[1]);
        }
        return -1;
    }
    if (tty) {
        setpgid(pid, pid);              /* also done by the child: no race */
        tcsetpgrp(STDIN_FILENO, pid);
        close(sync[0]);
        close(sync[1]);                 /* lets the child go on */
    }

    for (;;) {
        pid_t w = waitpid(pid, &status, WUNTRACED | WNOHANG);

        if (w < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (w == pid && !WIFSTOPPED(status))
            break;                      /* finished */
        continue_stopped(tty ? pid : getpgrp());
        usleep(200000);                 /* check five times a second */
    }
    *code = WIFEXITED(status) ? WEXITSTATUS(status)
          : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;

    /* Take the terminal back (SIGTTOU is ignored, so this is allowed). */
    if (tty)
        tcsetpgrp(STDIN_FILENO, getpgrp());
    return pid;
}

/*
 * Run a program entry: entry password, then the command with curses
 * suspended, the optional pause, and a usage log entry if tracking is on.
 */
void launch_item(const menu_t *m, const item_t *it)
{
    char dir[DIR_LEN + 256], path[512];
    time_t start, end;
    pid_t pid;
    int code = -1;

    if (!it->cmd[0]) {
        ui_message(it->desc, "This entry has no command yet.\nAdd one in Menu Maintenance.");
        return;
    }
    if (!entry_password_ok(it))
        return;

    ca_expand_home(dir, sizeof dir, it->dir[0] ? it->dir : "~");
    menu_path(m, it, path, sizeof path);
    ca_debug("run entry: %s", it->desc);

    /* Hand the terminal back to the program. */
    def_prog_mode();
    endwin();
    printf("\n");
    fflush(stdout);

    start = time(NULL);
    pid = run_command(dir, it->cmd, &code);
    end = time(NULL);

    if (it->pause || code == 127) {
        int c;
        if (code == 127)
            printf("\ncurs-access: the command could not be run (not found?)\n");
        printf("\nPress Enter to return to Curses Access...");
        fflush(stdout);
        while ((c = getchar()) != '\n' && c != EOF)
            ;
        clearerr(stdin);
    }

    /* Back to the menu. */
    reset_prog_mode();
    clearok(curscr, TRUE);
    refresh();

    if (pid < 0)
        ui_message(it->desc, "The program could not be started.");
    if (g_prefs.usage_tracking)
        usage_record(path, start, (long)(end - start), code);
    ca_debug("entry finished, exit status %d", code);
}
