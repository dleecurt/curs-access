/*
 * prefs.c - Program configuration (<config dir>/curs-access.conf).
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

#include "prefs.h"
#include "util.h"

prefs_t g_prefs;

/* Keys used for the theme colors in the configuration file. */
static const char *color_keys[SLOT_COUNT] = {
    "color.title", "color.text", "color.menu_border", "color.screen_border",
    "color.background", "color.bar_text", "color.bar", "color.key",
    "color.key_label", "color.message"
};

/* Settings used before the configuration file exists. */
static void defaults(void)
{
    memset(&g_prefs, 0, sizeof g_prefs);
    ca_strlcpy(g_prefs.greeting, "Please Enter Your Selection", sizeof g_prefs.greeting);
    g_prefs.clear_before_run = 1;
    g_prefs.usage_tracking = 0;
    g_prefs.clock = 1;
    g_prefs.clock_24h = 0;
    g_prefs.menu_border = 1;
    g_prefs.blank_minutes = 0;
    g_prefs.splash = 1;
    g_prefs.theme = *theme_builtin(0);
}

/* Read on/off, yes/no, true/false or 1/0 from the configuration file. */
static int onoff(const char *v)
{
    return ca_strcaseeq(v, "on") || ca_strcaseeq(v, "yes") ||
           ca_strcaseeq(v, "true") || !strcmp(v, "1");
}

int prefs_load(void)
{
    char path[CA_PATH_MAX], line[1024];
    FILE *fp;
    int s;

    defaults();
    ca_config_path(path, sizeof path, "curs-access.conf");
    fp = fopen(path, "r");
    if (!fp)
        return 0;     /* first run: defaults */
    while (fgets(line, sizeof line, fp)) {
        char *eq, *key, *val;

        if (line[0] == '#')
            continue;
        eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        key = ca_trim(line);
        val = ca_trim(eq + 1);
        if (!strcmp(key, "exit_command"))
            ca_strlcpy(g_prefs.exit_cmd, val, sizeof g_prefs.exit_cmd);
        else if (!strcmp(key, "greeting"))
            ca_strlcpy(g_prefs.greeting, val, sizeof g_prefs.greeting);
        else if (!strcmp(key, "clear_screen"))
            g_prefs.clear_before_run = onoff(val);
        else if (!strcmp(key, "usage_tracking"))
            g_prefs.usage_tracking = onoff(val);
        else if (!strcmp(key, "clock"))
            g_prefs.clock = onoff(val);
        else if (!strcmp(key, "clock_24h"))
            g_prefs.clock_24h = onoff(val);
        else if (!strcmp(key, "menu_border"))
            g_prefs.menu_border = onoff(val);
        else if (!strcmp(key, "blank_minutes"))
            g_prefs.blank_minutes = atoi(val) < 0 ? 0 : atoi(val) > 99 ? 99 : atoi(val);
        else if (!strcmp(key, "splash"))
            g_prefs.splash = onoff(val);
        else if (!strcmp(key, "theme"))
            ca_strlcpy(g_prefs.theme.name, val, sizeof g_prefs.theme.name);
        else
            for (s = 0; s < SLOT_COUNT; s++)
                if (!strcmp(key, color_keys[s])) {
                    int c = theme_color_from_name(val);
                    if (c >= 0)
                        g_prefs.theme.color[s] = (short)c;
                }
    }
    fclose(fp);
    return 0;
}

int prefs_save(void)
{
    char path[CA_PATH_MAX], buf[4096];
    int n, s;

    n = snprintf(buf, sizeof buf,
                 "# Curses Access program configuration\n"
                 "# Edit with F1 Maintenance > Program Configuration.\n"
                 "exit_command = %s\n"
                 "greeting = %s\n"
                 "clear_screen = %s\n"
                 "usage_tracking = %s\n"
                 "clock = %s\n"
                 "clock_24h = %s\n"
                 "menu_border = %s\n"
                 "blank_minutes = %d\n"
                 "splash = %s\n"
                 "theme = %s\n",
                 g_prefs.exit_cmd, g_prefs.greeting,
                 g_prefs.clear_before_run ? "on" : "off",
                 g_prefs.usage_tracking ? "on" : "off",
                 g_prefs.clock ? "on" : "off",
                 g_prefs.clock_24h ? "on" : "off",
                 g_prefs.menu_border ? "on" : "off",
                 g_prefs.blank_minutes,
                 g_prefs.splash ? "on" : "off",
                 g_prefs.theme.name);
    for (s = 0; s < SLOT_COUNT; s++)
        n += snprintf(buf + n, sizeof buf - (size_t)n, "%s = %s\n",
                      color_keys[s], theme_color_name(g_prefs.theme.color[s]));
    ca_config_path(path, sizeof path, "curs-access.conf");
    return ca_write_file_atomic(path, buf, (size_t)n);
}
