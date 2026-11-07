/*
 * crypto.c - libsodium and OpenSSL implementations of crypto.h.
 *
 * Exactly one of the two halves of this file is compiled, chosen by
 * configure (HAVE_LIBSODIUM is defined when libsodium was found).
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

#include "crypto.h"
#include "util.h"

#ifdef HAVE_LIBSODIUM
/* ==================================================================== */
/*                           libsodium back end                         */
/* ==================================================================== */

#include <sodium.h>

#define TAG "sodium:"

/*
 * Argon2id cost.  "Interactive" limits (64 MiB, 2 passes) keep a
 * password check well under a second on modest machines while still
 * making guessing expensive.
 */
#define PW_OPS crypto_pwhash_OPSLIMIT_INTERACTIVE
#define PW_MEM crypto_pwhash_MEMLIMIT_INTERACTIVE

int ca_crypto_init(void)
{
    return sodium_init() < 0 ? -1 : 0;
}

const char *ca_crypto_backend(void) { return "sodium"; }

const char *ca_crypto_description(void)
{
    return "libsodium: Argon2id, XChaCha20-Poly1305, X25519 sealed boxes";
}

void ca_random(void *buf, size_t n) { randombytes_buf(buf, n); }

void ca_wipe(void *buf, size_t n) { sodium_memzero(buf, n); }

int ca_pw_hash(const char *pw, char *out, size_t outlen)
{
    char h[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(h, pw, strlen(pw), PW_OPS, PW_MEM) != 0)
        return -1;
    if (strlen(TAG) + strlen(h) + 1 > outlen)
        return -1;
    snprintf(out, outlen, "%s%s", TAG, h);
    return 0;
}

int ca_pw_verify(const char *encoded, const char *pw)
{
    size_t tl = strlen(TAG);

    if (strncmp(encoded, TAG, tl) != 0)
        return -1;
    return crypto_pwhash_str_verify(encoded + tl, pw, strlen(pw)) == 0 ? 1 : 0;
}

int ca_kdf(const char *secret, const uint8_t salt[CA_SALT_BYTES],
           uint8_t key[CA_KEY_BYTES])
{
    /* CA_SALT_BYTES (16) equals crypto_pwhash_SALTBYTES. */
    return crypto_pwhash(key, CA_KEY_BYTES, secret, strlen(secret), salt,
                         PW_OPS, PW_MEM, crypto_pwhash_ALG_ARGON2ID13) == 0 ? 0 : -1;
}

size_t ca_aead_overhead(void)
{
    return crypto_aead_xchacha20poly1305_ietf_NPUBBYTES +
           crypto_aead_xchacha20poly1305_ietf_ABYTES;
}

int ca_aead_seal(const uint8_t key[CA_KEY_BYTES], const uint8_t *pt,
                 size_t ptlen, uint8_t *out, size_t *outlen)
{
    unsigned long long clen = 0;
    uint8_t *nonce = out;

    randombytes_buf(nonce, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            out + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, &clen,
            pt, ptlen, NULL, 0, NULL, nonce, key) != 0)
        return -1;
    *outlen = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES + (size_t)clen;
    return 0;
}

int ca_aead_open(const uint8_t key[CA_KEY_BYTES], const uint8_t *in,
                 size_t inlen, uint8_t *out, size_t *outlen)
{
    unsigned long long mlen = 0;
    const size_t nb = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

    if (inlen < ca_aead_overhead())
        return -1;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            out, &mlen, NULL, in + nb, inlen - nb, NULL, 0, in, key) != 0)
        return -1;
    *outlen = (size_t)mlen;
    return 0;
}

size_t ca_box_overhead(void) { return crypto_box_SEALBYTES; }

int ca_box_keypair(uint8_t pk[CA_KEY_BYTES], uint8_t sk[CA_KEY_BYTES])
{
    return crypto_box_keypair(pk, sk) == 0 ? 0 : -1;
}

int ca_box_seal(const uint8_t pk[CA_KEY_BYTES], const uint8_t *msg,
                size_t len, uint8_t *out, size_t *outlen)
{
    if (crypto_box_seal(out, msg, len, pk) != 0)
        return -1;
    *outlen = len + crypto_box_SEALBYTES;
    return 0;
}

int ca_box_open(const uint8_t pk[CA_KEY_BYTES], const uint8_t sk[CA_KEY_BYTES],
                const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen)
{
    if (inlen < crypto_box_SEALBYTES)
        return -1;
    if (crypto_box_seal_open(out, in, inlen, pk, sk) != 0)
        return -1;
    *outlen = inlen - crypto_box_SEALBYTES;
    return 0;
}

#else
/* ==================================================================== */
/*                         OpenSSL 3 back end                           */
/* ==================================================================== */

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define TAG "openssl:"

/* scrypt cost: N = 2^16, r = 8, p = 1 (about 64 MiB of memory). */
#define SCRYPT_LOG2N 16
#define SCRYPT_R     8
#define SCRYPT_P     1
#define SCRYPT_MAXMEM (256UL * 1024 * 1024)

#define GCM_NONCE 12
#define GCM_TAG   16

int ca_crypto_init(void) { return 0; }

const char *ca_crypto_backend(void) { return "openssl"; }

const char *ca_crypto_description(void)
{
    return "OpenSSL: scrypt, AES-256-GCM, X25519 sealed boxes";
}

void ca_random(void *buf, size_t n)
{
    if (RAND_bytes(buf, (int)n) != 1) {
        /* Without good randomness nothing here is safe: stop. */
        fprintf(stderr, "%s: random number generator failed\n", CA_NAME);
        abort();
    }
}

void ca_wipe(void *buf, size_t n) { OPENSSL_cleanse(buf, n); }

/* Run scrypt with the given log2(N). */
static int scrypt_raw(const char *secret, const uint8_t *salt, size_t saltlen,
                      int log2n, uint8_t *out, size_t outlen)
{
    return EVP_PBE_scrypt(secret, strlen(secret), salt, saltlen,
                          (uint64_t)1 << log2n, SCRYPT_R, SCRYPT_P,
                          SCRYPT_MAXMEM, out, outlen) == 1 ? 0 : -1;
}

/* Encoded form: openssl:scrypt$<log2N>$<r>$<p>$<salt b64>$<hash b64> */
int ca_pw_hash(const char *pw, char *out, size_t outlen)
{
    uint8_t salt[CA_SALT_BYTES], hash[CA_KEY_BYTES];
    char s64[64], h64[64];

    ca_random(salt, sizeof salt);
    if (scrypt_raw(pw, salt, sizeof salt, SCRYPT_LOG2N, hash, sizeof hash) != 0)
        return -1;
    ca_b64_encode(salt, sizeof salt, s64);
    ca_b64_encode(hash, sizeof hash, h64);
    ca_wipe(hash, sizeof hash);
    if ((size_t)snprintf(out, outlen, "%sscrypt$%d$%d$%d$%s$%s", TAG,
                         SCRYPT_LOG2N, SCRYPT_R, SCRYPT_P, s64, h64) >= outlen)
        return -1;
    return 0;
}

int ca_pw_verify(const char *encoded, const char *pw)
{
    size_t tl = strlen(TAG);
    int log2n, r, p, ok;
    char s64[64], h64[64];
    uint8_t salt[64], want[64], got[64];
    long sl, hl;

    if (strncmp(encoded, TAG, tl) != 0)
        return -1;
    if (sscanf(encoded + tl, "scrypt$%d$%d$%d$%63[^$]$%63s",
               &log2n, &r, &p, s64, h64) != 5)
        return -1;
    /* Only the parameters this build writes are accepted. */
    if (log2n < 10 || log2n > 20 || r != SCRYPT_R || p != SCRYPT_P)
        return -1;
    sl = ca_b64_decode(s64, salt, sizeof salt);
    hl = ca_b64_decode(h64, want, sizeof want);
    if (sl <= 0 || hl <= 0)
        return -1;
    if (scrypt_raw(pw, salt, (size_t)sl, log2n, got, (size_t)hl) != 0)
        return -1;
    ok = CRYPTO_memcmp(got, want, (size_t)hl) == 0;
    ca_wipe(got, sizeof got);
    return ok ? 1 : 0;
}

int ca_kdf(const char *secret, const uint8_t salt[CA_SALT_BYTES],
           uint8_t key[CA_KEY_BYTES])
{
    return scrypt_raw(secret, salt, CA_SALT_BYTES, SCRYPT_LOG2N, key, CA_KEY_BYTES);
}

size_t ca_aead_overhead(void) { return GCM_NONCE + GCM_TAG; }

/* Layout: nonce(12) || ciphertext || tag(16) */
int ca_aead_seal(const uint8_t key[CA_KEY_BYTES], const uint8_t *pt,
                 size_t ptlen, uint8_t *out, size_t *outlen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0, rc = -1;

    if (!ctx)
        return -1;
    ca_random(out, GCM_NONCE);
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, out) != 1)
        goto done;
    if (ptlen > 0 &&
        EVP_EncryptUpdate(ctx, out + GCM_NONCE, &len, pt, (int)ptlen) != 1)
        goto done;
    total = len;
    if (EVP_EncryptFinal_ex(ctx, out + GCM_NONCE + total, &len) != 1)
        goto done;
    total += len;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG,
                            out + GCM_NONCE + total) != 1)
        goto done;
    *outlen = GCM_NONCE + (size_t)total + GCM_TAG;
    rc = 0;
done:
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

int ca_aead_open(const uint8_t key[CA_KEY_BYTES], const uint8_t *in,
                 size_t inlen, uint8_t *out, size_t *outlen)
{
    EVP_CIPHER_CTX *ctx;
    int len = 0, total = 0, rc = -1;
    size_t ctlen;

    if (inlen < GCM_NONCE + GCM_TAG)
        return -1;
    ctlen = inlen - GCM_NONCE - GCM_TAG;
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return -1;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, in) != 1)
        goto done;
    if (ctlen > 0 &&
        EVP_DecryptUpdate(ctx, out, &len, in + GCM_NONCE, (int)ctlen) != 1)
        goto done;
    total = len;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG,
                            (void *)(in + GCM_NONCE + ctlen)) != 1)
        goto done;
    /* Final fails if the tag does not match: wrong key or altered data. */
    if (EVP_DecryptFinal_ex(ctx, out + total, &len) != 1)
        goto done;
    total += len;
    *outlen = (size_t)total;
    rc = 0;
done:
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

/* X25519 shared secret between a private key and a peer public key. */
static int x25519(const uint8_t sk[CA_KEY_BYTES], const uint8_t peer[CA_KEY_BYTES],
                  uint8_t shared[CA_KEY_BYTES])
{
    EVP_PKEY *priv = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, sk, CA_KEY_BYTES);
    EVP_PKEY *pub = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, peer, CA_KEY_BYTES);
    EVP_PKEY_CTX *ctx = NULL;
    size_t len = CA_KEY_BYTES;
    int rc = -1;

    if (!priv || !pub)
        goto done;
    ctx = EVP_PKEY_CTX_new(priv, NULL);
    if (!ctx || EVP_PKEY_derive_init(ctx) != 1 ||
        EVP_PKEY_derive_set_peer(ctx, pub) != 1 ||
        EVP_PKEY_derive(ctx, shared, &len) != 1 || len != CA_KEY_BYTES)
        goto done;
    rc = 0;
done:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(priv);
    EVP_PKEY_free(pub);
    return rc;
}

/* Box key = SHA-256(shared || ephemeral public || recipient public). */
static int box_key(const uint8_t shared[CA_KEY_BYTES], const uint8_t epk[CA_KEY_BYTES],
                   const uint8_t pk[CA_KEY_BYTES], uint8_t key[CA_KEY_BYTES])
{
    uint8_t buf[3 * CA_KEY_BYTES];
    unsigned int len = 0;
    int rc;

    memcpy(buf, shared, CA_KEY_BYTES);
    memcpy(buf + CA_KEY_BYTES, epk, CA_KEY_BYTES);
    memcpy(buf + 2 * CA_KEY_BYTES, pk, CA_KEY_BYTES);
    rc = EVP_Digest(buf, sizeof buf, key, &len, EVP_sha256(), NULL) == 1 &&
         len == CA_KEY_BYTES ? 0 : -1;
    ca_wipe(buf, sizeof buf);
    return rc;
}

size_t ca_box_overhead(void) { return CA_KEY_BYTES + ca_aead_overhead(); }

int ca_box_keypair(uint8_t pk[CA_KEY_BYTES], uint8_t sk[CA_KEY_BYTES])
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    EVP_PKEY *key = NULL;
    size_t pl = CA_KEY_BYTES, sl = CA_KEY_BYTES;
    int rc = -1;

    if (!ctx || EVP_PKEY_keygen_init(ctx) != 1 || EVP_PKEY_keygen(ctx, &key) != 1)
        goto done;
    if (EVP_PKEY_get_raw_private_key(key, sk, &sl) != 1 ||
        EVP_PKEY_get_raw_public_key(key, pk, &pl) != 1 ||
        sl != CA_KEY_BYTES || pl != CA_KEY_BYTES)
        goto done;
    rc = 0;
done:
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(ctx);
    return rc;
}

/* Layout: ephemeral public key(32) || aead(nonce || ciphertext || tag) */
int ca_box_seal(const uint8_t pk[CA_KEY_BYTES], const uint8_t *msg,
                size_t len, uint8_t *out, size_t *outlen)
{
    uint8_t esk[CA_KEY_BYTES], shared[CA_KEY_BYTES], key[CA_KEY_BYTES];
    size_t clen = 0;
    int rc = -1;

    if (ca_box_keypair(out, esk) != 0)
        return -1;
    if (x25519(esk, pk, shared) == 0 && box_key(shared, out, pk, key) == 0 &&
        ca_aead_seal(key, msg, len, out + CA_KEY_BYTES, &clen) == 0) {
        *outlen = CA_KEY_BYTES + clen;
        rc = 0;
    }
    ca_wipe(esk, sizeof esk);
    ca_wipe(shared, sizeof shared);
    ca_wipe(key, sizeof key);
    return rc;
}

int ca_box_open(const uint8_t pk[CA_KEY_BYTES], const uint8_t sk[CA_KEY_BYTES],
                const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen)
{
    uint8_t shared[CA_KEY_BYTES], key[CA_KEY_BYTES];
    int rc = -1;

    if (inlen < ca_box_overhead())
        return -1;
    if (x25519(sk, in, shared) == 0 && box_key(shared, in, pk, key) == 0 &&
        ca_aead_open(key, in + CA_KEY_BYTES, inlen - CA_KEY_BYTES, out, outlen) == 0)
        rc = 0;
    ca_wipe(shared, sizeof shared);
    ca_wipe(key, sizeof key);
    return rc;
}

#endif /* HAVE_LIBSODIUM */
