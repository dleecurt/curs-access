/*
 * crypto.h - The small set of cryptographic operations Curses Access needs.
 *
 * Two back ends implement this interface; configure picks one:
 *   libsodium (preferred): Argon2id password hashing and key derivation,
 *                          XChaCha20-Poly1305 encryption, X25519 sealed boxes.
 *   OpenSSL 3 (fallback):  scrypt password hashing and key derivation,
 *                          AES-256-GCM encryption, X25519 + SHA-256 + AES-GCM
 *                          sealed boxes.
 *
 * Every stored string starts with the back end's tag ("sodium:" or
 * "openssl:") so a file written by one build is recognised, not
 * misread, by a build using the other back end.
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

#ifndef CA_CRYPTO_H
#define CA_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#define CA_KEY_BYTES   32   /* symmetric keys and X25519 keys */
#define CA_SALT_BYTES  16   /* salt for key derivation */
#define CA_HASH_MAX    256  /* room for an encoded password hash */

/* Start the library.  Returns 0 on success. */
int ca_crypto_init(void);

/* "sodium" or "openssl"; also the tag prefix of stored strings. */
const char *ca_crypto_backend(void);

/* Long, human readable description of the algorithms in use. */
const char *ca_crypto_description(void);

/* Fill buf with cryptographically secure random bytes. */
void ca_random(void *buf, size_t n);

/* Wipe memory that held secrets (not optimised away). */
void ca_wipe(void *buf, size_t n);

/*
 * Password hashing.  ca_pw_hash writes a self-describing string such as
 * "sodium:$argon2id$..." into out.  ca_pw_verify returns 1 on a match,
 * 0 on a mismatch and -1 when the hash was written by the other back end
 * or is damaged.
 */
int ca_pw_hash(const char *pw, char *out, size_t outlen);
int ca_pw_verify(const char *encoded, const char *pw);

/* Derive a 32-byte key from a secret and salt with the slow KDF. */
int ca_kdf(const char *secret, const uint8_t salt[CA_SALT_BYTES],
           uint8_t key[CA_KEY_BYTES]);

/*
 * Authenticated symmetric encryption.  The random nonce is generated
 * internally and stored in front of the ciphertext.
 * ca_aead_overhead() is the number of extra bytes added.
 * Both return 0 on success, -1 on failure (for open: wrong key or the
 * data was changed).
 */
size_t ca_aead_overhead(void);
int ca_aead_seal(const uint8_t key[CA_KEY_BYTES], const uint8_t *pt,
                 size_t ptlen, uint8_t *out, size_t *outlen);
int ca_aead_open(const uint8_t key[CA_KEY_BYTES], const uint8_t *in,
                 size_t inlen, uint8_t *out, size_t *outlen);

/*
 * Public key "sealed box": anyone with the public key can encrypt;
 * only the holder of the secret key can decrypt.  Used for the usage
 * log so the launcher can append entries without knowing a password.
 */
size_t ca_box_overhead(void);
int ca_box_keypair(uint8_t pk[CA_KEY_BYTES], uint8_t sk[CA_KEY_BYTES]);
int ca_box_seal(const uint8_t pk[CA_KEY_BYTES], const uint8_t *msg,
                size_t len, uint8_t *out, size_t *outlen);
int ca_box_open(const uint8_t pk[CA_KEY_BYTES], const uint8_t sk[CA_KEY_BYTES],
                const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen);

#endif /* CA_CRYPTO_H */
