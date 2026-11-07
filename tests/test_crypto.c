/*
 * test_crypto.c - Tests for the encryption back end and the
 *                 base64 / base32 encoders (run by make check).
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
#include <string.h>

#include "crypto.h"
#include "util.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
    else printf("ok   %s\n", #cond); } while (0)

/* Run every check; the exit status is non-zero if any failed. */
int main(void)
{
    char hash[CA_HASH_MAX], b64[128], b32[64];
    uint8_t salt[CA_SALT_BYTES] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    uint8_t k1[CA_KEY_BYTES], k2[CA_KEY_BYTES], pk[CA_KEY_BYTES], sk[CA_KEY_BYTES];
    uint8_t ct[256], pt[256], bin[64];
    size_t cl = 0, pl = 0;
    const char *msg = "2026-11-06 12:00:00  Main Menu > Text Editor";

    CHECK(ca_crypto_init() == 0);
    printf("back end: %s\n", ca_crypto_description());

    /* Encoders: RFC 4648 test vectors. */
    ca_b64_encode((const uint8_t *)"foobar", 6, b64);
    CHECK(strcmp(b64, "Zm9vYmFy") == 0);
    ca_b64_encode((const uint8_t *)"fo", 2, b64);
    CHECK(strcmp(b64, "Zm8=") == 0);
    CHECK(ca_b64_decode("Zm9vYg==", bin, sizeof bin) == 4 && memcmp(bin, "foob", 4) == 0);
    CHECK(ca_b64_decode("Zm9*Yg==", bin, sizeof bin) == -1);
    ca_b32_encode((const uint8_t *)"foobar", 6, b32, sizeof b32);
    CHECK(strcmp(b32, "MZXW6YTBOI") == 0);

    /* Password hashing. */
    CHECK(ca_pw_hash("correct horse", hash, sizeof hash) == 0);
    CHECK(strncmp(hash, ca_crypto_backend(), strlen(ca_crypto_backend())) == 0);
    CHECK(ca_pw_verify(hash, "correct horse") == 1);
    CHECK(ca_pw_verify(hash, "correct horsE") == 0);
    CHECK(ca_pw_verify(hash, "") == 0);
    CHECK(ca_pw_verify("other:$x$", "correct horse") == -1);

    /* Key derivation is repeatable and depends on the secret. */
    CHECK(ca_kdf("secret", salt, k1) == 0);
    CHECK(ca_kdf("secret", salt, k2) == 0);
    CHECK(memcmp(k1, k2, sizeof k1) == 0);
    CHECK(ca_kdf("Secret", salt, k2) == 0);
    CHECK(memcmp(k1, k2, sizeof k1) != 0);

    /* Authenticated encryption. */
    CHECK(ca_aead_seal(k1, (const uint8_t *)msg, strlen(msg), ct, &cl) == 0);
    CHECK(cl == strlen(msg) + ca_aead_overhead());
    CHECK(ca_aead_open(k1, ct, cl, pt, &pl) == 0 && pl == strlen(msg) &&
          memcmp(pt, msg, pl) == 0);
    CHECK(ca_aead_open(k2, ct, cl, pt, &pl) != 0);       /* wrong key */
    ct[cl - 1] ^= 1;
    CHECK(ca_aead_open(k1, ct, cl, pt, &pl) != 0);       /* altered */

    /* Sealed boxes. */
    CHECK(ca_box_keypair(pk, sk) == 0);
    CHECK(ca_box_seal(pk, (const uint8_t *)msg, strlen(msg), ct, &cl) == 0);
    CHECK(cl == strlen(msg) + ca_box_overhead());
    CHECK(ca_box_open(pk, sk, ct, cl, pt, &pl) == 0 && pl == strlen(msg) &&
          memcmp(pt, msg, pl) == 0);
    sk[1] ^= 1;      /* (bits 0-2 of byte 0 are cleared by X25519 clamping) */
    CHECK(ca_box_open(pk, sk, ct, cl, pt, &pl) != 0);    /* wrong key */

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
