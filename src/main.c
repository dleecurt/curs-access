/*
 * main.c - Curses Access: a lettered menu launcher for the terminal.
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

#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "crypto.h"
#include "menu.h"
#include "prefs.h"
#include "screens.h"
#include "security.h"
#include "signals.h"
#include "theme.h"
#include "ui.h"
#include "util.h"

/* --help text. */
static void usage(FILE *fp)
{
    fprintf(fp,
            "Usage: %s [OPTION]...\n"
            "A lettered menu launcher for the terminal.\n\n"
            "  -n, --no-splash       do not show the start-up logo\n"
            "  -r, --recover         use the recovery code to remove forgotten passwords\n"
            "      --reset-security  start over: remove all passwords, make a new\n"
            "                        recovery code (the old usage log is deleted)\n"
            "  -d, --debug           write a debug log to %s\n"
            "  -h, --help            show this help and exit\n"
            "  -V, --version         show version information and exit\n\n"
            "Configuration: ~/.config/curs-access/\n"
            "Logs:          ~/.local/state/curs-access/\n",
            CA_NAME, "~/.local/state/curs-access/debug.log");
}

/* --version text, including the encryption library in use. */
static void version(void)
{
    printf("%s (%s) %s\n"
           "Copyright (C) 2026 the Curses Access project\n"
           "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
           "This is free software: you are free to change and redistribute it.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n"
           "Encryption: %s\n",
           CA_NAME, PACKAGE_NAME, PACKAGE_VERSION, ca_crypto_description());
}

/* --reset-security: confirm on the terminal, then remove the files. */
static int reset_security(void)
{
    char path[CA_PATH_MAX], answer[16];

    printf("This removes the Exit and Menu Maintenance passwords, makes a new\n"
           "recovery code and deletes the encrypted usage log.\n"
           "Menus and settings are kept.  Continue? [y/N] ");
    fflush(stdout);
    if (!fgets(answer, sizeof answer, stdin) || (answer[0] != 'y' && answer[0] != 'Y')) {
        printf("Nothing was changed.\n");
        return -1;
    }
    ca_config_path(path, sizeof path, "security.conf");
    unlink(path);
    ca_state_path(path, sizeof path, "usage.log");
    unlink(path);
    printf("Security data removed.\n");
    return 0;
}

/*
 * Start-up order: options, directories, debug log, encryption library,
 * settings, menus, security data, signal handling, then the screens:
 * splash, first-run recovery code, main menu.  On exit, the optional
 * "Command To Run On Exit" replaces this process.
 */
int main(int argc, char **argv)
{
    static const struct option opts[] = {
        { "no-splash", no_argument, NULL, 'n' },
        { "recover", no_argument, NULL, 'r' },
        { "reset-security", no_argument, NULL, 'R' },
        { "debug", no_argument, NULL, 'd' },
        { "help", no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'V' },
        { NULL, 0, NULL, 0 }
    };
    int no_splash = 0, recover = 0, reset = 0, debug = 0, c, sec;
    char code[CA_RECOVERY_LEN + 8];

    setlocale(LC_ALL, "");
    while ((c = getopt_long(argc, argv, "nrdhV", opts, NULL)) != -1) {
        switch (c) {
        case 'n': no_splash = 1; break;
        case 'r': recover = 1; break;
        case 'R': reset = 1; break;
        case 'd': debug = 1; break;
        case 'h': usage(stdout); return 0;
        case 'V':
            ca_crypto_init();
            version();
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }

    if (ca_paths_init() != 0) {
        fprintf(stderr, "%s: cannot create the configuration directories\n", CA_NAME);
        return 1;
    }
    if (debug)
        ca_debug_open();
    if (ca_crypto_init() != 0) {
        fprintf(stderr, "%s: the encryption library could not be started\n", CA_NAME);
        return 1;
    }
    if (reset && reset_security() != 0)
        return 1;

    prefs_load();
    menu_load();

    sec = sec_load();
    if (sec == SEC_MISMATCH) {
        fprintf(stderr,
                "%s: the security data was made by a build using %s,\n"
                "but this build uses %s.  Use the same build, or run\n"
                "  %s --reset-security\n"
                "to start over (passwords removed, usage log deleted).\n",
                CA_NAME, sec_file_backend(), ca_crypto_backend(), CA_NAME);
        return 1;
    }
    if (sec == SEC_ERROR) {
        fprintf(stderr, "%s: %s/security.conf is damaged.\n"
                        "Run %s --reset-security to start over.\n",
                CA_NAME, ca_config_dir(), CA_NAME);
        return 1;
    }

    srand((unsigned)time(NULL));
    /* ^C, ^\, ^Z and kill must not close the menu (before ui_init). */
    ca_signals_trap();

    ui_init();
    theme_apply(&g_prefs.theme);

    if (g_prefs.splash && !no_splash)
        splash_show();
    if (sec == SEC_NEW) {
        ui_busy("Creating encryption keys...");
        if (sec_setup(code, sizeof code) != 0) {
            ui_end();
            fprintf(stderr, "%s: the security data could not be created\n", CA_NAME);
            return 1;
        }
        recovery_code_show(code, 1);
        ca_wipe(code, sizeof code);
        sec_lock();
    }
    if (recover) {
        access_recover();
        sec_lock();
    }

    main_menu_run();

    ui_end();
    sec_lock();
    menu_free(g_main_menu);
    ca_debug_close();

    /* "Command To Run On Exit" replaces this process, like a shell's exec. */
    if (g_prefs.exit_cmd[0]) {
        ca_signals_default();
        execl("/bin/sh", "sh", "-c", g_prefs.exit_cmd, (char *)NULL);
        perror("curs-access: exit command");
        return 1;
    }
    return 0;
}
