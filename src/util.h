/*
 * util.h - Small helpers shared by every part of Curses Access:
 *          safe string handling, paths in the user's home directory,
 *          base64/base32 encoding and the optional debug log.
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

#ifndef CA_UTIL_H
#define CA_UTIL_H

#include <stddef.h>
#include <stdint.h>

/* Program name used in file and directory names. */
#define CA_NAME "curs-access"

/* Size used for every path buffer in the program. */
#define CA_PATH_MAX 1024

/* Copy at most dstsize-1 bytes and always NUL terminate (like strlcpy). */
size_t ca_strlcpy(char *dst, const char *src, size_t dstsize);

/* Remove leading and trailing white space in place; returns s. */
char *ca_trim(char *s);

/* Case-insensitive string compare (ASCII only). */
int ca_strcaseeq(const char *a, const char *b);

/*
 * Directories.  Everything lives in the user's home directory:
 *   config:  $XDG_CONFIG_HOME/curs-access  (default ~/.config/curs-access)
 *   state:   $XDG_STATE_HOME/curs-access   (default ~/.local/state/curs-access)
 * The directories are created (mode 0700) on first use.
 * Return 0 on success, -1 on failure.
 */
int ca_paths_init(void);
const char *ca_config_dir(void);
const char *ca_state_dir(void);

/* Build "<config dir>/<name>" or "<state dir>/<name>" into out. */
void ca_config_path(char *out, size_t outlen, const char *name);
void ca_state_path(char *out, size_t outlen, const char *name);

/* Expand a leading "~" or "~/" to the user's home directory. */
void ca_expand_home(char *out, size_t outlen, const char *in);

/* Write a whole file atomically (temp file + rename) with mode 0600. */
int ca_write_file_atomic(const char *path, const char *data, size_t len);

/* Base64 (standard alphabet, padded).  out must hold ca_b64_len(n)+1. */
size_t ca_b64_len(size_t n);
void ca_b64_encode(const uint8_t *in, size_t n, char *out);
/* Decode; returns decoded length or -1 on bad input. */
long ca_b64_decode(const char *in, uint8_t *out, size_t outmax);

/* Base32 (RFC 4648 alphabet, no padding) used for the recovery code. */
void ca_b32_encode(const uint8_t *in, size_t n, char *out, size_t outmax);

/*
 * Debug log.  Only written when the program is started with --debug.
 * The log never contains passwords, the recovery code or key material.
 */
void ca_debug_open(void);
void ca_debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ca_debug_close(void);

#endif /* CA_UTIL_H */
