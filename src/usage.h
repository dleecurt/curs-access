/*
 * usage.h - Encrypted usage log (<state dir>/usage.log).
 *
 * Each line is one program run, encrypted to the usage log public key
 * (see security.h), so entries can be added without any password but
 * only read after Menu Maintenance has been unlocked.
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

#ifndef CA_USAGE_H
#define CA_USAGE_H

#include <time.h>

/* Append an entry: menu path, start time, run time and exit status. */
int usage_record(const char *path, time_t start, long seconds, int status);

/*
 * Decrypt the whole log (needs sec_is_unlocked()).  Returns the number
 * of lines and sets *lines to a malloc'd array of malloc'd strings,
 * oldest first; free with usage_free().  Returns -1 on error.
 */
int usage_read(char ***lines);
void usage_free(char **lines, int n);

/* Remove the log. */
int usage_clear(void);

#endif /* CA_USAGE_H */
