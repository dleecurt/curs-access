/*
 * util.c - Small helpers shared by every part of Curses Access.
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

static char g_config_dir[CA_PATH_MAX];
static char g_state_dir[CA_PATH_MAX];
static FILE *g_debug_fp;

size_t ca_strlcpy(char *dst, const char *src, size_t dstsize)
{
    size_t len = strlen(src);

    if (dstsize > 0) {
        size_t n = (len >= dstsize) ? dstsize - 1 : len;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return len;
}

char *ca_trim(char *s)
{
    char *start = s;
    size_t len;

    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
    return s;
}

int ca_strcaseeq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

/* Find the user's home directory: $HOME first, then the password file. */
static const char *home_dir(void)
{
    const char *h = getenv("HOME");
    struct passwd *pw;

    if (h && *h)
        return h;
    pw = getpwuid(getuid());
    return (pw && pw->pw_dir) ? pw->pw_dir : "/tmp";
}

/* mkdir -p with mode 0700 for every directory we create. */
static int mkdir_p(const char *path)
{
    char tmp[CA_PATH_MAX];
    char *p;

    ca_strlcpy(tmp, path, sizeof tmp);
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0700) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/*
 * Pick an XDG base directory if it is set to an absolute path,
 * otherwise use the given default below the home directory.
 */
static void xdg_dir(char *out, size_t outlen, const char *env,
                    const char *fallback)
{
    const char *v = getenv(env);

    if (v && v[0] == '/')
        snprintf(out, outlen, "%s/%s", v, CA_NAME);
    else
        snprintf(out, outlen, "%s/%s/%s", home_dir(), fallback, CA_NAME);
}

int ca_paths_init(void)
{
    xdg_dir(g_config_dir, sizeof g_config_dir, "XDG_CONFIG_HOME", ".config");
    xdg_dir(g_state_dir, sizeof g_state_dir, "XDG_STATE_HOME", ".local/state");
    if (mkdir_p(g_config_dir) != 0 || mkdir_p(g_state_dir) != 0)
        return -1;
    {
        char themes[CA_PATH_MAX];
        ca_config_path(themes, sizeof themes, "themes");
        mkdir_p(themes);
    }
    return 0;
}

const char *ca_config_dir(void) { return g_config_dir; }
const char *ca_state_dir(void) { return g_state_dir; }

void ca_config_path(char *out, size_t outlen, const char *name)
{
    if (snprintf(out, outlen, "%s/%s", g_config_dir, name) >= (int)outlen)
        ca_debug("path too long: %s", name);
}

void ca_state_path(char *out, size_t outlen, const char *name)
{
    if (snprintf(out, outlen, "%s/%s", g_state_dir, name) >= (int)outlen)
        ca_debug("path too long: %s", name);
}

void ca_expand_home(char *out, size_t outlen, const char *in)
{
    if (in[0] == '~' && (in[1] == '\0' || in[1] == '/'))
        snprintf(out, outlen, "%s%s", home_dir(), in + 1);
    else
        ca_strlcpy(out, in, outlen);
}

int ca_write_file_atomic(const char *path, const char *data, size_t len)
{
    char tmp[CA_PATH_MAX + 8];
    int fd;
    ssize_t w;

    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return -1;
    while (len > 0) {
        w = write(fd, data, len);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            close(fd);
            unlink(tmp);
            return -1;
        }
        data += w;
        len -= (size_t)w;
    }
    if (fsync(fd) != 0 || close(fd) != 0) {
        unlink(tmp);
        return -1;
    }
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

/* ---- Base64 -------------------------------------------------------- */

static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t ca_b64_len(size_t n)
{
    return ((n + 2) / 3) * 4;
}

void ca_b64_encode(const uint8_t *in, size_t n, char *out)
{
    size_t i;
    char *o = out;

    for (i = 0; i + 2 < n; i += 3) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
        *o++ = b64chars[(v >> 18) & 63];
        *o++ = b64chars[(v >> 12) & 63];
        *o++ = b64chars[(v >> 6) & 63];
        *o++ = b64chars[v & 63];
    }
    if (i < n) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < n)
            v |= (uint32_t)in[i + 1] << 8;
        *o++ = b64chars[(v >> 18) & 63];
        *o++ = b64chars[(v >> 12) & 63];
        *o++ = (i + 1 < n) ? b64chars[(v >> 6) & 63] : '=';
        *o++ = '=';
    }
    *o = '\0';
}

/* Value of one base64 character, or -1. */
static int b64val(int c)
{
    const char *p;

    if (c == 0)
        return -1;
    p = strchr(b64chars, c);
    return p ? (int)(p - b64chars) : -1;
}

long ca_b64_decode(const char *in, uint8_t *out, size_t outmax)
{
    size_t len = strlen(in), i, o = 0;

    if (len % 4 != 0)
        return -1;
    for (i = 0; i < len; i += 4) {
        int a = b64val(in[i]), b = b64val(in[i + 1]);
        int c = (in[i + 2] == '=') ? 0 : b64val(in[i + 2]);
        int d = (in[i + 3] == '=') ? 0 : b64val(in[i + 3]);
        uint32_t v;

        if (a < 0 || b < 0 || c < 0 || d < 0)
            return -1;
        v = ((uint32_t)a << 18) | ((uint32_t)b << 12) | ((uint32_t)c << 6) | (uint32_t)d;
        if (o >= outmax)
            return -1;
        out[o++] = (uint8_t)(v >> 16);
        if (in[i + 2] != '=') {
            if (o >= outmax)
                return -1;
            out[o++] = (uint8_t)(v >> 8);
        }
        if (in[i + 3] != '=') {
            if (o >= outmax)
                return -1;
            out[o++] = (uint8_t)v;
        }
    }
    return (long)o;
}

/* ---- Base32 -------------------------------------------------------- */

void ca_b32_encode(const uint8_t *in, size_t n, char *out, size_t outmax)
{
    static const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    uint32_t buf = 0;
    int bits = 0;
    size_t i, o = 0;

    for (i = 0; i < n; i++) {
        buf = (buf << 8) | in[i];
        bits += 8;
        while (bits >= 5 && o + 1 < outmax) {
            out[o++] = alpha[(buf >> (bits - 5)) & 31];
            bits -= 5;
        }
    }
    if (bits > 0 && o + 1 < outmax)
        out[o++] = alpha[(buf << (5 - bits)) & 31];
    out[o] = '\0';
}

/* ---- Debug log ----------------------------------------------------- */

void ca_debug_open(void)
{
    char path[CA_PATH_MAX];
    int fd;

    ca_state_path(path, sizeof path, "debug.log");
    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd >= 0)
        g_debug_fp = fdopen(fd, "a");
    if (g_debug_fp) {
        setvbuf(g_debug_fp, NULL, _IOLBF, 0);
        ca_debug("---- %s %s started ----", CA_NAME, PACKAGE_VERSION);
    }
}

void ca_debug(const char *fmt, ...)
{
    va_list ap;
    char stamp[32];
    time_t now;

    if (!g_debug_fp)
        return;
    now = time(NULL);
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(g_debug_fp, "%s ", stamp);
    va_start(ap, fmt);
    vfprintf(g_debug_fp, fmt, ap);
    va_end(ap);
    fputc('\n', g_debug_fp);
}

void ca_debug_close(void)
{
    if (g_debug_fp) {
        ca_debug("---- exit ----");
        fclose(g_debug_fp);
        g_debug_fp = NULL;
    }
}
