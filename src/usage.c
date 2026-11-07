/*
 * usage.c - Encrypted usage log.
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crypto.h"
#include "security.h"
#include "usage.h"
#include "util.h"

#define ENTRY_MAX 1024

/* Full path of usage.log. */
static void log_path(char *out, size_t len)
{
    ca_state_path(out, len, "usage.log");
}

int usage_record(const char *path, time_t start, long seconds, int status)
{
    char plain[ENTRY_MAX], stamp[32], file[CA_PATH_MAX];
    uint8_t sealed[ENTRY_MAX + 128];
    char *b64;
    size_t slen = 0;
    int n, fd, rc = -1;

    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&start));
    n = snprintf(plain, sizeof plain, "%s  %5lds  exit %-3d  %s",
                 stamp, seconds, status, path);
    if (n < 0)
        return -1;
    if ((size_t)n >= sizeof plain)
        n = (int)sizeof plain - 1;
    if (ca_box_seal(sec_log_pk(), (const uint8_t *)plain, (size_t)n, sealed, &slen) != 0)
        return -1;
    ca_wipe(plain, sizeof plain);
    b64 = malloc(ca_b64_len(slen) + 2);
    if (!b64)
        return -1;
    ca_b64_encode(sealed, slen, b64);
    strcat(b64, "\n");
    log_path(file, sizeof file);
    fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd >= 0) {
        size_t len = strlen(b64);
        if (write(fd, b64, len) == (ssize_t)len)
            rc = 0;
        close(fd);
    }
    free(b64);
    return rc;
}

int usage_read(char ***lines)
{
    char file[CA_PATH_MAX];
    char *line = NULL;
    size_t cap = 0;
    ssize_t got;
    FILE *fp;
    char **out = NULL;
    int n = 0, max = 0;
    const uint8_t *sk = sec_log_sk();

    *lines = NULL;
    if (!sk)
        return -1;
    log_path(file, sizeof file);
    fp = fopen(file, "r");
    if (!fp)
        return 0;               /* no log yet */
    while ((got = getline(&line, &cap, fp)) > 0) {
        uint8_t *bin;
        char *plain;
        long bl;
        size_t pl = 0;

        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0])
            continue;
        if (n == max) {
            char **no;
            max = max ? max * 2 : 64;
            no = realloc(out, (size_t)max * sizeof *out);
            if (!no)
                break;
            out = no;
        }
        bin = malloc(strlen(line));
        plain = malloc(strlen(line) + 64);   /* room for the error text too */
        if (!bin || !plain) {
            free(bin);
            free(plain);
            break;
        }
        bl = ca_b64_decode(line, bin, strlen(line));
        if (bl > 0 && ca_box_open(sec_log_pk(), sk, bin, (size_t)bl,
                                  (uint8_t *)plain, &pl) == 0) {
            plain[pl] = '\0';
        } else {
            strcpy(plain, "(entry could not be decrypted)");
        }
        free(bin);
        out[n++] = plain;
    }
    free(line);
    fclose(fp);
    *lines = out;
    return n;
}

void usage_free(char **lines, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        ca_wipe(lines[i], strlen(lines[i]));
        free(lines[i]);
    }
    free(lines);
}

int usage_clear(void)
{
    char file[CA_PATH_MAX];

    log_path(file, sizeof file);
    return (unlink(file) == 0) ? 0 : -1;
}
