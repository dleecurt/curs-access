/*
 * splash.c - Start-up logo, System Info and the recovery code screen.
 *
 * The logo is an original design: the letters "CA" drawn in horizontal
 * rainbow stripes made from upper-half block characters.  A user logo in
 * <config dir>/logo.txt replaces it (each line gets the next rainbow
 * color).
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

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "crypto.h"
#include "prefs.h"
#include "screens.h"
#include "theme.h"
#include "ui.h"
#include "util.h"

/* Rainbow, top to bottom.  Brown shows as orange on most terminals. */
static const int rainbow[] = {
    C_RED, C_LRED, C_BROWN, C_YELLOW, C_LGREEN,
    C_GREEN, C_LCYAN, C_LBLUE, C_BLUE, C_LMAGENTA
};
#define N_RAINBOW ((int)(sizeof rainbow / sizeof rainbow[0]))

/* 8 x 10 bitmaps of "C" and "A"; each pixel is two columns wide. */
static const char *glyph_c[10] = {
    ".######.", "########", "###..###", "###.....", "###.....",
    "###.....", "###.....", "###..###", "########", ".######."
};
static const char *glyph_a[10] = {
    "..####..", ".######.", "###..###", "###..###", "###..###",
    "########", "########", "###..###", "###..###", "###..###"
};
#define LOGO_ROWS 10
#define LOGO_W    34          /* 16 + 2 + 16 columns */

/* Up to 20 lines of a user logo. */
static char g_user_logo[20][128];
static int g_user_rows, g_user_w;

/* Read <config dir>/logo.txt, if present, to replace the built-in logo. */
static void load_user_logo(void)
{
    char path[CA_PATH_MAX], line[256];
    FILE *fp;

    g_user_rows = g_user_w = 0;
    ca_config_path(path, sizeof path, "logo.txt");
    fp = fopen(path, "r");
    if (!fp)
        return;
    while (g_user_rows < 20 && fgets(line, sizeof line, fp)) {
        int w;
        line[strcspn(line, "\r\n")] = '\0';
        ca_strlcpy(g_user_logo[g_user_rows], line, sizeof g_user_logo[0]);
        w = (int)strlen(line);   /* bytes: close enough for layout */
        if (w > g_user_w)
            g_user_w = w;
        g_user_rows++;
    }
    fclose(fp);
}

/* Draw one glyph row: '#' pixels as colored stripes. */
static void glyph_row(int y, int x, const char *bits, attr_t a)
{
    const char *px = ui_utf8() ? "\xe2\x96\x80\xe2\x96\x80" : "==";  /* "▀▀" */
    int i;

    attrset(a);
    for (i = 0; bits[i]; i++)
        if (bits[i] == '#')
            mvaddstr(y, x + i * 2, px);
}

/* Draw the built-in "CA" logo, or the user's logo, with (y, x) top left. */
static void draw_logo(int y, int x)
{
    int r;

    if (g_user_rows) {
        for (r = 0; r < g_user_rows; r++) {
            attrset(theme_attr(rainbow[r % N_RAINBOW], g_prefs.theme.color[SLOT_BACKGROUND]) | A_BOLD);
            mvaddnstr(y + r, x, g_user_logo[r], COLS - x - 1);
        }
        return;
    }
    for (r = 0; r < LOGO_ROWS; r++) {
        attr_t a = theme_attr(rainbow[r], g_prefs.theme.color[SLOT_BACKGROUND]);
        glyph_row(y + r, x, glyph_c[r], a);
        glyph_row(y + r, x + 18, glyph_a[r], a);
    }
}

/*
 * The start-up screen: logo beside the text block, or above it when the
 * terminal is too narrow.  Any key closes it.
 */
void splash_show(void)
{
    static const char *text[] = {
        "C U R S E S   A C C E S S",
        "A menu launcher for the terminal",
        "",
        "Version " PACKAGE_VERSION,
        "(C) 2026 the Curses Access project",
        "GNU GPL version 3 or later",
        "",
        "Put your own logo in logo.txt!",
        "Press any key..."
    };
    const int nt = (int)(sizeof text / sizeof text[0]);

    load_user_logo();
    for (;;) {
        int lw = g_user_rows ? g_user_w : LOGO_W;
        int lh = g_user_rows ? g_user_rows : LOGO_ROWS;
        int tw = 34, i, ly, lx, ty, tx, side, k;

        ui_clear();
        side = COLS >= lw + tw + 8;
        if (side) {
            int total = lw + 6 + tw;
            lx = (COLS - total) / 2;
            tx = lx + lw + 6;
            ly = (LINES - lh) / 2;
            ty = (LINES - nt) / 2;
        } else {
            lx = (COLS - lw) / 2;
            tx = (COLS - tw) / 2;
            ly = (LINES - lh - nt - 1) / 2;
            ty = ly + lh + 1;
            if (ly < 0 || lx < 0) {          /* no room for the logo */
                lh = 0;
                ty = (LINES - nt) / 2;
            }
        }
        if (lh)
            draw_logo(ly < 0 ? 0 : ly, lx < 0 ? 0 : lx);
        for (i = 0; i < nt; i++) {
            attr_t a;
            int len = (int)strlen(text[i]);

            if (i == 0)
                a = theme_slot(SLOT_TITLE) | A_BOLD;
            else if (i == 1 || i == 7)
                a = theme_slot(SLOT_MENU_BORDER) | A_BOLD;
            else if (i == nt - 1)
                a = theme_slot(SLOT_KEY_LABEL) | A_BOLD;
            else
                a = theme_slot(SLOT_TEXT) | A_BOLD;
            attrset(a);
            if (ty + i >= 0 && ty + i < LINES)
                mvaddnstr(ty + i, tx + (tw - len) / 2 < 0 ? 0 : tx + (tw - len) / 2,
                          text[i], COLS);
        }
        refresh();
        k = ui_getkey_plain();
        if (k != UI_REDRAW)
            return;
    }
}

/* ---- System Info ---------------------------------------------------- */

/* First "model name" line of /proc/cpuinfo. */
static void cpu_model(char *out, size_t len)
{
    FILE *fp = fopen("/proc/cpuinfo", "r");
    char line[512];

    ca_strlcpy(out, "unknown", len);
    if (!fp)
        return;
    while (fgets(line, sizeof line, fp)) {
        if (!strncmp(line, "model name", 10) || !strncmp(line, "Model", 5) ||
            !strncmp(line, "cpu model", 9)) {
            char *c = strchr(line, ':');
            if (c) {
                ca_strlcpy(out, ca_trim(c + 1), len);
                break;
            }
        }
    }
    fclose(fp);
}

/* MemTotal and MemAvailable from /proc/meminfo, in MiB. */
static void memory(long *total, long *avail)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    char line[256];
    long v;

    *total = *avail = -1;
    if (!fp)
        return;
    while (fgets(line, sizeof line, fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &v) == 1)
            *total = v / 1024;
        else if (sscanf(line, "MemAvailable: %ld kB", &v) == 1)
            *avail = v / 1024;
    }
    fclose(fp);
}

/* Append a "Label ...... value" line to the System Info list. */
static void add(char **lines, int *n, const char *label, const char *value)
{
    char buf[512];
    int dots = 30 - (int)strlen(label);

    snprintf(buf, sizeof buf, "%s %.*s %s", label, dots > 0 ? dots : 1,
             "..............................", value);
    lines[(*n)++] = strdup(buf);
}

/* F3 System Info: host, system, processor, memory, disk and terminal. */
void sysinfo_show(void)
{
    char *lines[32], v[512], host[256];
    struct utsname u;
    struct statvfs sv;
    struct passwd *pw = getpwuid(getuid());
    long mt, ma;
    double load[3];
    FILE *fp;
    int n = 0, i;
    static const fkey_t keys[] = { { "Esc", "Return To Menu" } };

    if (gethostname(host, sizeof host) != 0)
        strcpy(host, "unknown");
    host[sizeof host - 1] = '\0';
    add(lines, &n, "Host Name", host);
    if (uname(&u) == 0) {
        snprintf(v, sizeof v, "%s %s", u.sysname, u.release);
        add(lines, &n, "Operating System", v);
        add(lines, &n, "Machine", u.machine);
    }
    cpu_model(v, sizeof v);
    add(lines, &n, "Processor", v);
    snprintf(v, sizeof v, "%ld", sysconf(_SC_NPROCESSORS_ONLN));
    add(lines, &n, "Processors Online", v);
    memory(&mt, &ma);
    snprintf(v, sizeof v, "%ld MiB total, %ld MiB available", mt, ma);
    add(lines, &n, "Memory", v);
    fp = fopen("/proc/uptime", "r");
    if (fp) {
        double up = 0;
        if (fscanf(fp, "%lf", &up) == 1) {
            long s = (long)up;
            snprintf(v, sizeof v, "%ld days, %ld:%02ld", s / 86400, (s % 86400) / 3600,
                     (s % 3600) / 60);
            add(lines, &n, "Up Time", v);
        }
        fclose(fp);
    }
    if (getloadavg(load, 3) == 3) {
        snprintf(v, sizeof v, "%.2f  %.2f  %.2f", load[0], load[1], load[2]);
        add(lines, &n, "Load Average", v);
    }
    add(lines, &n, "User", pw ? pw->pw_name : "unknown");
    add(lines, &n, "Shell", getenv("SHELL") ? getenv("SHELL") : "unknown");
    if (statvfs(getenv("HOME") ? getenv("HOME") : "/", &sv) == 0) {
        snprintf(v, sizeof v, "%.1f GiB free of %.1f GiB",
                 (double)sv.f_bavail * sv.f_frsize / 1073741824.0,
                 (double)sv.f_blocks * sv.f_frsize / 1073741824.0);
        add(lines, &n, "Home Disk", v);
    }
    snprintf(v, sizeof v, "%s, %d x %d, %d colors", getenv("TERM") ? getenv("TERM") : "?",
             COLS, LINES, has_colors() ? COLORS : 0);
    add(lines, &n, "Terminal", v);
    add(lines, &n, "Encryption", ca_crypto_description());
    add(lines, &n, "Configuration", ca_config_dir());
    add(lines, &n, "Logs", ca_state_dir());
    snprintf(v, sizeof v, "%s %s", PACKAGE_NAME, PACKAGE_VERSION);
    add(lines, &n, "Program", v);

    ui_viewer("System Information", NULL, lines, n, keys, 1, NULL);
    for (i = 0; i < n; i++)
        free(lines[i]);
}

/* ---- Recovery code -------------------------------------------------- */

void recovery_code_show(const char *code, int first_run)
{
    static const fkey_t keys[] = { { "Enter", "I Have Written It Down" } };

    for (;;) {
        int y;

        if (ui_too_small()) {
            ui_getkey_plain();
            continue;
        }
        ui_frame(0);
        ui_spaced_title(2, "Recovery Code");
        y = LINES >= 24 ? 5 : 4;
        if (first_run)
            ui_center(y++, "Welcome to Curses Access.", theme_slot(SLOT_TEXT) | A_BOLD);
        ui_center(y++, "This is your recovery code.  Write it down and keep it safe:",
                  theme_slot(SLOT_TEXT) | A_BOLD);
        y++;
        ui_box(y, (COLS - 47) / 2, 3, 47, theme_slot(SLOT_MENU_BORDER));
        ui_center(y + 1, code, theme_slot(SLOT_TITLE) | A_BOLD);
        y += 4;
        ui_center(y++, "It is shown only now and is never stored.  If you forget the",
                  theme_slot(SLOT_TEXT));
        ui_center(y++, "Exit or Menu Maintenance password, press F4 at the password",
                  theme_slot(SLOT_TEXT));
        ui_center(y++, "prompt and type this code: the passwords are removed and the",
                  theme_slot(SLOT_TEXT));
        ui_center(y++, "encrypted usage report can still be read.",
                  theme_slot(SLOT_TEXT));
        if (LINES >= 24)
            y++;
        if (y < LINES - 4)
            ui_center(y, "A new code can be made in Maintenance > Passwords & Recovery.",
                      theme_slot(SLOT_MENU_BORDER) | A_BOLD);
        ui_divider(LINES - 4);
        ui_fkeys(LINES - 2, 1, COLS - 2, keys, 1);
        refresh();
        if (ui_getkey_plain() == '\n')
            return;
    }
}
