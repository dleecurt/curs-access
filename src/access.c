/*
 * access.c - Password checks for Exit and Menu Maintenance, and the
 *            recovery code.
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

#include <string.h>
#include <unistd.h>

#include "crypto.h"
#include "screens.h"
#include "security.h"
#include "ui.h"
#include "util.h"

#define TRIES 3

/*
 * Ask for the recovery code and, if it is right, clear the passwords and
 * unlock the usage log key.  Returns 1 on success.
 */
static int recover_dialog(void)
{
    char code[64] = "";

    if (!ui_prompt("Recovery Code",
                   "Type the recovery code you wrote down when\n"
                   "Curses Access was first started.",
                   code, sizeof code, 0, "Dashes and spaces are optional"))
        return 0;
    ui_busy("Checking recovery code...");
    if (sec_recover(code) == 0) {
        ca_wipe(code, sizeof code);
        ui_message("Recovery",
                   "The recovery code is correct.\n\n"
                   "The Exit and Menu Maintenance passwords\n"
                   "have been removed.  Set new ones in\n"
                   "Program Configuration.");
        return 1;
    }
    ca_wipe(code, sizeof code);
    sleep(1);
    ui_message("Recovery", "That recovery code is not correct.");
    return 0;
}

/* --recover: ask for the recovery code at start-up (see screens.h). */
int access_recover(void)
{
    return recover_dialog();
}

/*
 * F1: check the Menu Maintenance password (if one is set) and unlock the
 * usage log key.  F4 at the prompt switches to the recovery code.
 * Returns 1 when Maintenance may be opened.
 */
int access_maintenance(void)
{
    char pw[128];
    int t;

    if (!sec_has_maint_password()) {
        ui_busy("Opening Menu Maintenance...");
        if (sec_unlock("") != 0) {
            ui_message("Menu Maintenance",
                       "The usage log key could not be opened.\n"
                       "The usage report will not be readable.");
        }
        return 1;
    }
    for (t = 0; t < TRIES; t++) {
        int r = ui_password("Menu Maintenance",
                            "Enter the Menu Maintenance password", pw,
                            sizeof pw, 1);

        if (r == PW_CANCEL)
            break;
        if (r == PW_RECOVERY) {
            ca_wipe(pw, sizeof pw);
            return recover_dialog();
        }
        ui_busy("Checking password...");
        if (sec_unlock(pw) == 0) {
            ca_wipe(pw, sizeof pw);
            ca_debug("maintenance unlocked");
            return 1;
        }
        ca_debug("wrong maintenance password");
        sleep(1);       /* slow down guessing */
        ui_message("Menu Maintenance", "Incorrect password.");
    }
    ca_wipe(pw, sizeof pw);
    return 0;
}

/*
 * F10 at the main menu: check the Exit password, if one is set.
 * Returns 1 when the program may exit.  Using the recovery code here
 * clears the passwords but returns to the menu.
 */
int access_exit(void)
{
    char pw[128];
    int t;

    if (!sec_has_exit_password())
        return 1;
    for (t = 0; t < TRIES; t++) {
        int r = ui_password("Exit", "Enter the Exit password", pw, sizeof pw, 1);

        if (r == PW_CANCEL)
            break;
        if (r == PW_RECOVERY) {
            ca_wipe(pw, sizeof pw);
            if (recover_dialog())
                sec_lock();
            return 0;   /* back to the menu; passwords are now cleared */
        }
        ui_busy("Checking password...");
        if (sec_check_exit(pw)) {
            ca_wipe(pw, sizeof pw);
            return 1;
        }
        ca_debug("wrong exit password");
        sleep(1);
        ui_message("Exit", "Incorrect password.");
    }
    ca_wipe(pw, sizeof pw);
    return 0;
}
