/*
 * security.c - Passwords, the recovery code and the usage log key.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "security.h"
#include "util.h"

/* Wrapped secret key: salt || aead(nonce || sk || tag), as base64. */
#define WRAP_MAX 256

static struct {
    char backend[16];
    char exit_pw[CA_HASH_MAX];
    char maint_pw[CA_HASH_MAX];
    char recovery[CA_HASH_MAX];
    char pk64[64];
    char wrap_pw[WRAP_MAX];
    char wrap_rc[WRAP_MAX];
} S;

static uint8_t g_pk[CA_KEY_BYTES];
static uint8_t g_sk[CA_KEY_BYTES];
static int g_unlocked;

/* Full path of security.conf. */
static void sec_path(char *out, size_t len)
{
    ca_config_path(out, len, "security.conf");
}

/* Write security.conf from S. */
static int sec_save(void)
{
    char path[CA_PATH_MAX];
    char buf[4096];
    int n;

    n = snprintf(buf, sizeof buf,
                 "# Curses Access security data - written by the program.\n"
                 "# Passwords and the recovery code are stored only as one-way\n"
                 "# hashes.  Deleting this file (or running curs-access\n"
                 "# --reset-security) removes all passwords and makes the\n"
                 "# existing usage log unreadable.\n"
                 "backend = %s\n"
                 "exit_password = %s\n"
                 "maint_password = %s\n"
                 "recovery = %s\n"
                 "log_public_key = %s\n"
                 "log_key_password = %s\n"
                 "log_key_recovery = %s\n",
                 S.backend, S.exit_pw, S.maint_pw, S.recovery, S.pk64,
                 S.wrap_pw, S.wrap_rc);
    if (n < 0 || (size_t)n >= sizeof buf)
        return -1;
    sec_path(path, sizeof path);
    return ca_write_file_atomic(path, buf, (size_t)n);
}

int sec_load(void)
{
    char path[CA_PATH_MAX], line[1024];
    FILE *fp;

    memset(&S, 0, sizeof S);
    sec_path(path, sizeof path);
    fp = fopen(path, "r");
    if (!fp)
        return SEC_NEW;
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
        if (!strcmp(key, "backend"))
            ca_strlcpy(S.backend, val, sizeof S.backend);
        else if (!strcmp(key, "exit_password"))
            ca_strlcpy(S.exit_pw, val, sizeof S.exit_pw);
        else if (!strcmp(key, "maint_password"))
            ca_strlcpy(S.maint_pw, val, sizeof S.maint_pw);
        else if (!strcmp(key, "recovery"))
            ca_strlcpy(S.recovery, val, sizeof S.recovery);
        else if (!strcmp(key, "log_public_key"))
            ca_strlcpy(S.pk64, val, sizeof S.pk64);
        else if (!strcmp(key, "log_key_password"))
            ca_strlcpy(S.wrap_pw, val, sizeof S.wrap_pw);
        else if (!strcmp(key, "log_key_recovery"))
            ca_strlcpy(S.wrap_rc, val, sizeof S.wrap_rc);
    }
    fclose(fp);

    if (strcmp(S.backend, ca_crypto_backend()) != 0)
        return S.backend[0] ? SEC_MISMATCH : SEC_ERROR;
    if (ca_b64_decode(S.pk64, g_pk, sizeof g_pk) != CA_KEY_BYTES ||
        !S.recovery[0] || !S.wrap_pw[0] || !S.wrap_rc[0])
        return SEC_ERROR;
    ca_debug("security data loaded (%s)", S.backend);
    return SEC_OK;
}

const char *sec_file_backend(void) { return S.backend; }

/* Encrypt the secret key under a key derived from secret; base64 out. */
static int wrap_sk(const char *secret, char *out, size_t outlen)
{
    uint8_t salt[CA_SALT_BYTES], kek[CA_KEY_BYTES];
    uint8_t blob[CA_SALT_BYTES + CA_KEY_BYTES + 64];
    size_t clen = 0;
    int rc = -1;

    ca_random(salt, sizeof salt);
    if (ca_kdf(secret, salt, kek) != 0)
        return -1;
    memcpy(blob, salt, sizeof salt);
    if (ca_aead_seal(kek, g_sk, CA_KEY_BYTES, blob + CA_SALT_BYTES, &clen) == 0 &&
        ca_b64_len(CA_SALT_BYTES + clen) < outlen) {
        ca_b64_encode(blob, CA_SALT_BYTES + clen, out);
        rc = 0;
    }
    ca_wipe(kek, sizeof kek);
    return rc;
}

/* Decrypt a wrapped secret key into g_sk. */
static int unwrap_sk(const char *secret, const char *wrapped)
{
    uint8_t blob[WRAP_MAX], kek[CA_KEY_BYTES], sk[CA_KEY_BYTES + 64];
    long bl = ca_b64_decode(wrapped, blob, sizeof blob);
    size_t sl = 0;
    int rc = -1;

    if (bl <= CA_SALT_BYTES)
        return -1;
    if (ca_kdf(secret, blob, kek) != 0)
        return -1;
    if (ca_aead_open(kek, blob + CA_SALT_BYTES, (size_t)bl - CA_SALT_BYTES, sk, &sl) == 0 &&
        sl == CA_KEY_BYTES) {
        memcpy(g_sk, sk, CA_KEY_BYTES);
        g_unlocked = 1;
        rc = 0;
    }
    ca_wipe(kek, sizeof kek);
    ca_wipe(sk, sizeof sk);
    return rc;
}

/*
 * Recovery codes are typed by people: ignore dashes, spaces and case.
 * The normalised form (32 base32 characters) is what gets hashed.
 */
static void normalise_code(const char *in, char *out, size_t outlen)
{
    size_t o = 0;

    for (; *in && o + 1 < outlen; in++) {
        int c = toupper((unsigned char)*in);

        if (!isalnum(c))
            continue;
        /* Base32 has no 0, 1 or 8: read them as the letters they look like. */
        if (c == '0') c = 'O';
        if (c == '1') c = 'I';
        if (c == '8') c = 'B';
        out[o++] = (char)c;
    }
    out[o] = '\0';
}

/* Make a new recovery code; store its hash and re-wrap the key with it. */
static int make_recovery(char *code_out, size_t outlen)
{
    uint8_t raw[20];
    char b32[40];
    size_t i, o = 0;

    ca_random(raw, sizeof raw);
    ca_b32_encode(raw, sizeof raw, b32, sizeof b32);   /* 32 characters */
    ca_wipe(raw, sizeof raw);
    for (i = 0; b32[i] && o + 2 < outlen; i++) {
        if (i > 0 && i % 4 == 0)
            code_out[o++] = '-';
        code_out[o++] = b32[i];
    }
    code_out[o] = '\0';
    if (ca_pw_hash(b32, S.recovery, sizeof S.recovery) != 0 ||
        wrap_sk(b32, S.wrap_rc, sizeof S.wrap_rc) != 0) {
        ca_wipe(b32, sizeof b32);
        return -1;
    }
    ca_wipe(b32, sizeof b32);
    return 0;
}

int sec_setup(char *code_out, size_t outlen)
{
    memset(&S, 0, sizeof S);
    ca_strlcpy(S.backend, ca_crypto_backend(), sizeof S.backend);
    if (ca_box_keypair(g_pk, g_sk) != 0)
        return -1;
    g_unlocked = 1;
    ca_b64_encode(g_pk, CA_KEY_BYTES, S.pk64);
    if (wrap_sk("", S.wrap_pw, sizeof S.wrap_pw) != 0 ||
        make_recovery(code_out, outlen) != 0)
        return -1;
    ca_debug("new security data created");
    return sec_save();
}

int sec_has_exit_password(void) { return S.exit_pw[0] != '\0'; }
int sec_has_maint_password(void) { return S.maint_pw[0] != '\0'; }

int sec_check_exit(const char *pw)
{
    return !S.exit_pw[0] || ca_pw_verify(S.exit_pw, pw) == 1;
}

int sec_check_maint(const char *pw)
{
    return !S.maint_pw[0] || ca_pw_verify(S.maint_pw, pw) == 1;
}

int sec_check_recovery(const char *code)
{
    char n[64];
    int ok;

    normalise_code(code, n, sizeof n);
    ok = ca_pw_verify(S.recovery, n) == 1;
    ca_wipe(n, sizeof n);
    return ok;
}

int sec_unlock(const char *maint_pw)
{
    if (!sec_check_maint(maint_pw))
        return -1;
    if (unwrap_sk(maint_pw, S.wrap_pw) != 0) {
        ca_debug("log key could not be unlocked with the password");
        return -1;
    }
    return 0;
}

int sec_recover(const char *code)
{
    char n[64];
    int rc = -1;

    normalise_code(code, n, sizeof n);
    if (ca_pw_verify(S.recovery, n) == 1 && unwrap_sk(n, S.wrap_rc) == 0) {
        S.exit_pw[0] = '\0';
        S.maint_pw[0] = '\0';
        if (wrap_sk("", S.wrap_pw, sizeof S.wrap_pw) == 0 && sec_save() == 0)
            rc = 0;
        ca_debug("recovery code used: passwords cleared");
    }
    ca_wipe(n, sizeof n);
    return rc;
}

void sec_lock(void)
{
    ca_wipe(g_sk, sizeof g_sk);
    g_unlocked = 0;
}

int sec_is_unlocked(void) { return g_unlocked; }

int sec_set_exit_password(const char *pw)
{
    if (!pw[0])
        S.exit_pw[0] = '\0';
    else if (ca_pw_hash(pw, S.exit_pw, sizeof S.exit_pw) != 0)
        return -1;
    return sec_save();
}

int sec_set_maint_password(const char *pw)
{
    if (!g_unlocked)
        return -1;
    if (!pw[0])
        S.maint_pw[0] = '\0';
    else if (ca_pw_hash(pw, S.maint_pw, sizeof S.maint_pw) != 0)
        return -1;
    if (wrap_sk(pw, S.wrap_pw, sizeof S.wrap_pw) != 0)
        return -1;
    return sec_save();
}

int sec_new_recovery(char *code_out, size_t outlen)
{
    if (!g_unlocked)
        return -1;
    if (make_recovery(code_out, outlen) != 0)
        return -1;
    ca_debug("recovery code replaced");
    return sec_save();
}

const uint8_t *sec_log_pk(void) { return g_pk; }
const uint8_t *sec_log_sk(void) { return g_unlocked ? g_sk : NULL; }
