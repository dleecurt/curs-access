/*
 * security.h - Passwords, the recovery code and the usage log key.
 *
 * Stored in <config dir>/security.conf (mode 0600):
 *   - Exit and Menu Maintenance passwords, as slow one-way hashes.
 *   - The recovery code, also as a one-way hash.  The code itself is
 *     shown to the user once and never stored.
 *   - The usage log key pair.  The public key is stored as is, so the
 *     launcher can always append encrypted log entries.  The secret key
 *     is stored twice, encrypted ("wrapped") once with a key derived from
 *     the Maintenance password and once with a key derived from the
 *     recovery code.  Reading the log therefore needs one of the two.
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

#ifndef CA_SECURITY_H
#define CA_SECURITY_H

#include <stddef.h>
#include <stdint.h>

/* Length of a recovery code as shown: 8 groups of 4 plus 7 dashes. */
#define CA_RECOVERY_LEN 40

/* Results of sec_load(). */
enum {
    SEC_OK = 0,        /* loaded */
    SEC_NEW = 1,       /* no file yet: call sec_setup() */
    SEC_MISMATCH = 2,  /* written by a build with the other crypto back end */
    SEC_ERROR = -1     /* unreadable or damaged */
};

int sec_load(void);

/* Back end named in the file (valid after SEC_MISMATCH). */
const char *sec_file_backend(void);

/*
 * First run (or reset): new key pair, new recovery code, no passwords.
 * The formatted code (XXXX-XXXX-...) is written to code_out.
 */
int sec_setup(char *code_out, size_t outlen);

int sec_has_exit_password(void);
int sec_has_maint_password(void);

/* 1 = correct, 0 = wrong. */
int sec_check_exit(const char *pw);
int sec_check_maint(const char *pw);
int sec_check_recovery(const char *code);

/*
 * Unlock the usage log secret key with the Maintenance password
 * (an empty string when none is set).  Returns 0 on success.
 */
int sec_unlock(const char *maint_pw);

/*
 * Recovery: check the code, unlock the log key with it and clear the
 * Exit and Maintenance passwords.  Returns 0 on success.
 */
int sec_recover(const char *code);

/* Forget the unlocked secret key. */
void sec_lock(void);
int sec_is_unlocked(void);

/* Change passwords.  An empty string removes the password. */
int sec_set_exit_password(const char *pw);
int sec_set_maint_password(const char *pw);    /* needs sec_is_unlocked() */

/* Replace the recovery code (needs sec_is_unlocked()). */
int sec_new_recovery(char *code_out, size_t outlen);

/* Usage log keys. sec_log_sk() is NULL unless unlocked. */
const uint8_t *sec_log_pk(void);
const uint8_t *sec_log_sk(void);

#endif /* CA_SECURITY_H */
