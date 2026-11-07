/*
 * menu.c - The menu tree and its file.
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

#include "menu.h"
#include "util.h"

menu_t *g_main_menu;

menu_t *menu_new(int level, menu_t *parent, const char *title)
{
    menu_t *m = calloc(1, sizeof *m);

    if (!m)
        return NULL;
    m->level = level;
    m->parent = parent;
    ca_strlcpy(m->title, title ? title : "", sizeof m->title);
    return m;
}

void menu_free(menu_t *m)
{
    int i;

    if (!m)
        return;
    for (i = 0; i < m->count; i++)
        menu_free(m->items[i].sub);
    free(m);
}

/* Fill an item as a program entry. */
static void set_program(item_t *it, const char *desc, const char *dir,
                        const char *cmd, int pause)
{
    memset(it, 0, sizeof *it);
    it->type = ITEM_PROGRAM;
    ca_strlcpy(it->desc, desc, sizeof it->desc);
    ca_strlcpy(it->dir, dir, sizeof it->dir);
    ca_strlcpy(it->cmd, cmd, sizeof it->cmd);
    it->pause = pause;
}

/* A small starter menu so a first run shows something useful. */
static void starter_menu(void)
{
    menu_t *m = menu_new(1, NULL, "Welcome To The Main Menu");
    menu_t *u;

    g_main_menu = m;
    set_program(&m->items[0], "Command Shell", "~", "exec \"${SHELL:-/bin/sh}\"", 0);
    set_program(&m->items[1], "Text Editor", "~", "\"${EDITOR:-nano}\"", 0);
    m->items[2].type = ITEM_SUBMENU;
    ca_strlcpy(m->items[2].desc, "System Utilities", DESC_LEN);
    u = menu_new(2, m, "System Utilities");
    m->items[2].sub = u;
    set_program(&m->items[3], "Curses Access Manual", "~", "man curs-access", 0);
    m->count = 4;

    set_program(&u->items[0], "Disk Space", "~", "df -h", 1);
    set_program(&u->items[1], "Running Processes", "~", "top", 0);
    set_program(&u->items[2], "Who Is Logged In", "~", "who", 1);
    set_program(&u->items[3], "Calendar", "~", "cal -3", 1);
    u->count = 4;
}

/* Split a line on tabs in place; returns the number of fields. */
static int split_tabs(char *line, char **fields, int max)
{
    int n = 0;
    char *p = line;

    while (n < max) {
        fields[n++] = p;
        p = strchr(p, '\t');
        if (!p)
            break;
        *p++ = '\0';
    }
    return n;
}

int menu_load(void)
{
    char path[CA_PATH_MAX], line[2048];
    FILE *fp;
    menu_t *cur[MENU_MAX_LEVELS + 2] = { 0 };   /* menu open at each level */

    ca_config_path(path, sizeof path, "menus.conf");
    fp = fopen(path, "r");
    if (!fp) {
        starter_menu();
        ca_debug("no menus.conf: starter menu created");
        return menu_save();
    }
    g_main_menu = menu_new(1, NULL, "Welcome To The Main Menu");
    cur[1] = g_main_menu;
    while (fgets(line, sizeof line, fp)) {
        char *f[8];
        int n, level;
        menu_t *m;
        item_t *it;

        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#')
            continue;
        n = split_tabs(line, f, 8);
        if (n < 3)
            continue;
        level = atoi(f[0]);
        if (f[1][0] == 'T' && level == 0) {
            ca_strlcpy(g_main_menu->title, f[2], TITLE_LEN);
            continue;
        }
        if (level < 1 || level > MENU_MAX_LEVELS || !cur[level])
            continue;           /* orphan line: skip it */
        m = cur[level];
        if (m->count >= MENU_MAX_ITEMS)
            continue;
        it = &m->items[m->count];
        memset(it, 0, sizeof *it);
        ca_strlcpy(it->desc, f[2], DESC_LEN);
        if (f[1][0] == 'S') {
            it->type = ITEM_SUBMENU;
            if (level < MENU_MAX_LEVELS) {
                it->sub = menu_new(level + 1, m, n > 3 ? f[3] : f[2]);
                cur[level + 1] = it->sub;
            }
        } else {
            it->type = ITEM_PROGRAM;
            if (n > 3) ca_strlcpy(it->dir, f[3], DIR_LEN);
            if (n > 4) ca_strlcpy(it->cmd, f[4], CMD_LEN);
            if (n > 5) it->pause = atoi(f[5]);
            if (n > 6 && strcmp(f[6], "-") != 0)
                ca_strlcpy(it->pwhash, f[6], CA_HASH_MAX);
        }
        m->count++;
        /* A new entry at this level closes the deeper sub-menus
         * (a new sub-menu entry keeps only its own new level open). */
        {
            int l = level + (it->type == ITEM_SUBMENU ? 2 : 1);
            for (; l <= MENU_MAX_LEVELS; l++)
                cur[l] = NULL;
        }
    }
    fclose(fp);
    ca_debug("menus loaded: %d main menu entries", g_main_menu->count);
    return 0;
}

/* Append one menu's entries (recursively) to the buffer. */
static void save_menu(const menu_t *m, char **buf, size_t *len, size_t *cap)
{
    int i;

    for (i = 0; i < m->count; i++) {
        const item_t *it = &m->items[i];
        char line[2048];
        int n;

        if (it->type == ITEM_SUBMENU)
            n = snprintf(line, sizeof line, "%d\tS\t%s\t%s\n", m->level,
                         it->desc, it->sub ? it->sub->title : it->desc);
        else
            n = snprintf(line, sizeof line, "%d\tP\t%s\t%s\t%s\t%d\t%s\n",
                         m->level, it->desc, it->dir, it->cmd, it->pause,
                         it->pwhash[0] ? it->pwhash : "-");
        if (n < 0)
            continue;
        if ((size_t)n >= sizeof line)
            n = (int)sizeof line - 1;
        if (*len + (size_t)n + 1 > *cap) {
            char *nb;
            *cap = (*cap + (size_t)n + 1) * 2;
            nb = realloc(*buf, *cap);
            if (!nb)
                return;
            *buf = nb;
        }
        memcpy(*buf + *len, line, (size_t)n);
        *len += (size_t)n;
        if (it->type == ITEM_SUBMENU && it->sub)
            save_menu(it->sub, buf, len, cap);
    }
}

int menu_save(void)
{
    char path[CA_PATH_MAX];
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    int rc, n;

    if (!buf)
        return -1;
    n = snprintf(buf, cap,
                 "# Curses Access menus - edit with F1 Maintenance.\n"
                 "# level<TAB>P<TAB>description<TAB>directory<TAB>command<TAB>pause<TAB>password\n"
                 "# level<TAB>S<TAB>description<TAB>sub-menu title\n"
                 "0\tT\t%s\n", g_main_menu->title);
    len = (size_t)n;
    save_menu(g_main_menu, &buf, &len, &cap);
    ca_config_path(path, sizeof path, "menus.conf");
    rc = ca_write_file_atomic(path, buf, len);
    free(buf);
    return rc;
}

int menu_insert(menu_t *m, int pos)
{
    if (m->count >= MENU_MAX_ITEMS || pos < 0 || pos > m->count)
        return -1;
    memmove(&m->items[pos + 1], &m->items[pos],
            (size_t)(m->count - pos) * sizeof(item_t));
    memset(&m->items[pos], 0, sizeof(item_t));
    m->items[pos].type = ITEM_PROGRAM;
    m->count++;
    return 0;
}

void menu_delete(menu_t *m, int pos)
{
    if (pos < 0 || pos >= m->count)
        return;
    menu_free(m->items[pos].sub);
    memmove(&m->items[pos], &m->items[pos + 1],
            (size_t)(m->count - pos - 1) * sizeof(item_t));
    m->count--;
    memset(&m->items[m->count], 0, sizeof(item_t));
}

void menu_move(menu_t *m, int from, int to)
{
    item_t tmp;

    if (from < 0 || from >= m->count || to < 0 || to >= m->count || from == to)
        return;
    tmp = m->items[from];
    if (from < to)
        memmove(&m->items[from], &m->items[from + 1], (size_t)(to - from) * sizeof(item_t));
    else
        memmove(&m->items[to + 1], &m->items[to], (size_t)(from - to) * sizeof(item_t));
    m->items[to] = tmp;
}

int menu_make_submenu(menu_t *m, int pos)
{
    item_t *it;

    if (pos < 0 || pos >= m->count || m->level >= MENU_MAX_LEVELS)
        return -1;
    it = &m->items[pos];
    if (it->type == ITEM_SUBMENU && it->sub)
        return 0;
    it->type = ITEM_SUBMENU;
    it->dir[0] = it->cmd[0] = it->pwhash[0] = '\0';
    it->pause = 0;
    it->sub = menu_new(m->level + 1, m, it->desc);
    return it->sub ? 0 : -1;
}

void menu_path(const menu_t *m, const item_t *it, char *out, size_t outlen)
{
    const menu_t *chain[MENU_MAX_LEVELS + 1];
    int n = 0, i;
    size_t o = 0;

    for (; m && n <= MENU_MAX_LEVELS; m = m->parent)
        chain[n++] = m;
    out[0] = '\0';
    for (i = n - 1; i >= 0; i--) {
        const menu_t *c = chain[i];
        const char *name = "Main Menu";
        int k;

        /* A sub-menu is named by the entry that opens it. */
        if (c->parent)
            for (k = 0; k < c->parent->count; k++)
                if (c->parent->items[k].sub == c)
                    name = c->parent->items[k].desc;
        o += (size_t)snprintf(out + o, o < outlen ? outlen - o : 0, "%s%s",
                              i == n - 1 ? "" : " > ", name);
        if (o >= outlen)
            return;
    }
    if (it)
        snprintf(out + o, o < outlen ? outlen - o : 0, " > %s", it->desc);
}
