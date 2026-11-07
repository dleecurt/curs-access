/*
 * test_files.c - Tests for the files kept in the user's home
 *                directory: configuration, menus, themes, security data and
 *                the usage log.  Runs in a temporary directory, never in the
 *                real home (run by make check).
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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "crypto.h"
#include "menu.h"
#include "prefs.h"
#include "security.h"
#include "theme.h"
#include "usage.h"
#include "util.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
    else printf("ok   %s\n", #cond); } while (0)

/* Run every check; the exit status is non-zero if any failed. */
int main(void)
{
    char tmpl[] = "/tmp/curs-access-test-XXXXXX", dir[CA_PATH_MAX], code[64], code2[64];
    char **lines;
    struct stat st;
    theme_t t;
    menu_t *m;
    int n;

    CHECK(mkdtemp(tmpl) != NULL);
    snprintf(dir, sizeof dir, "%s/config", tmpl);
    setenv("XDG_CONFIG_HOME", dir, 1);
    snprintf(dir, sizeof dir, "%s/state", tmpl);
    setenv("XDG_STATE_HOME", dir, 1);
    CHECK(ca_paths_init() == 0);
    CHECK(ca_crypto_init() == 0);

    /* Preferences round trip. */
    CHECK(prefs_load() == 0);
    CHECK(g_prefs.clock == 1 && g_prefs.usage_tracking == 0);
    g_prefs.usage_tracking = 1;
    g_prefs.blank_minutes = 7;
    strcpy(g_prefs.exit_cmd, "echo bye");
    g_prefs.theme.color[SLOT_TITLE] = C_LMAGENTA;
    CHECK(prefs_save() == 0);
    memset(&g_prefs, 0, sizeof g_prefs);
    CHECK(prefs_load() == 0);
    CHECK(g_prefs.usage_tracking == 1 && g_prefs.blank_minutes == 7);
    CHECK(strcmp(g_prefs.exit_cmd, "echo bye") == 0);
    CHECK(g_prefs.theme.color[SLOT_TITLE] == C_LMAGENTA);

    /* Menus: starter menu, then nested sub-menus survive a round trip. */
    CHECK(menu_load() == 0);
    CHECK(g_main_menu->count == 4 && g_main_menu->items[2].type == ITEM_SUBMENU);
    m = g_main_menu->items[2].sub;
    CHECK(menu_insert(m, m->count) == 0);
    strcpy(m->items[m->count - 1].desc, "Deeper");
    CHECK(menu_make_submenu(m, m->count - 1) == 0);
    CHECK(menu_insert(m->items[m->count - 1].sub, 0) == 0);
    strcpy(m->items[m->count - 1].sub->items[0].desc, "Level Three");
    strcpy(m->items[m->count - 1].sub->items[0].cmd, "true");
    CHECK(menu_insert(g_main_menu, 4) == 0);
    strcpy(g_main_menu->items[4].desc, "After Sub-Menu");
    CHECK(menu_save() == 0);
    menu_free(g_main_menu);
    CHECK(menu_load() == 0);
    CHECK(g_main_menu->count == 5);
    CHECK(strcmp(g_main_menu->items[4].desc, "After Sub-Menu") == 0);
    m = g_main_menu->items[2].sub;
    CHECK(m && m->count == 5 && m->items[4].type == ITEM_SUBMENU);
    CHECK(m->items[4].sub && m->items[4].sub->level == 3 &&
          strcmp(m->items[4].sub->items[0].desc, "Level Three") == 0);
    menu_move(g_main_menu, 0, 4);
    CHECK(strcmp(g_main_menu->items[4].desc, "Command Shell") == 0);
    menu_delete(g_main_menu, 1);   /* frees its sub-menu, if any */
    CHECK(g_main_menu->count == 4);

    /* Themes. */
    t = *theme_builtin(1);
    strcpy(t.name, "My Test Theme");
    CHECK(theme_save(&t) == 0);
    CHECK(theme_find("my test theme", &t) == 0 && t.color[SLOT_BACKGROUND] == C_BLUE);

    /* Security: first run, passwords, recovery. */
    CHECK(sec_load() == SEC_NEW);
    CHECK(sec_setup(code, sizeof code) == 0);
    printf("recovery code: %s\n", code);
    CHECK(strlen(code) == 39);
    CHECK(sec_load() == SEC_OK);
    {
        char path[CA_PATH_MAX];
        ca_config_path(path, sizeof path, "security.conf");
        CHECK(stat(path, &st) == 0 && (st.st_mode & 077) == 0);
    }
    CHECK(sec_unlock("") == 0);
    CHECK(sec_set_maint_password("open sesame") == 0);
    CHECK(sec_set_exit_password("bye") == 0);
    sec_lock();
    CHECK(sec_load() == SEC_OK);
    CHECK(sec_has_maint_password() && sec_has_exit_password());
    CHECK(sec_check_exit("bye") && !sec_check_exit("hello"));
    CHECK(sec_unlock("wrong") != 0 && !sec_is_unlocked());

    /* Usage log: written without a password, read after unlocking. */
    CHECK(usage_record("Main Menu > Editor", time(NULL), 12, 0) == 0);
    CHECK(usage_record("Main Menu > Shell", time(NULL), 3, 1) == 0);
    CHECK(usage_read(&lines) == -1);                 /* locked */
    CHECK(sec_unlock("open sesame") == 0);
    n = usage_read(&lines);
    CHECK(n == 2 && strstr(lines[0], "Editor") && strstr(lines[1], "exit 1"));
    usage_free(lines, n);
    sec_lock();

    /* Recovery code: case and dashes do not matter; clears passwords. */
    {
        size_t i, o = 0;
        for (i = 0; code[i]; i++)
            if (code[i] != '-')
                code2[o++] = (char)(code[i] | 0x20);   /* lower case, no dashes */
        code2[o] = '\0';
    }
    CHECK(sec_check_recovery(code2));
    CHECK(sec_recover("AAAA-BBBB") != 0);
    CHECK(sec_recover(code2) == 0);
    CHECK(!sec_has_maint_password() && !sec_has_exit_password());
    n = usage_read(&lines);
    CHECK(n == 2);                                  /* log still readable */
    usage_free(lines, n);
    sec_lock();
    CHECK(sec_load() == SEC_OK && sec_unlock("") == 0);

    /* New recovery code replaces the old one. */
    CHECK(sec_new_recovery(code2, sizeof code2) == 0);
    CHECK(!sec_check_recovery(code) && sec_check_recovery(code2));

    {
        char cmd[CA_PATH_MAX + 16];
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmpl);
        if (system(cmd) != 0)
            printf("note: could not remove %s\n", tmpl);
    }
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
