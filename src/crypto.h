#pragma once

// Vault file crypto.
//
// On-disk format of vault.fkm (little effort, documented so the host
// tool and the firmware stay in sync — see tools/fkm_vault.py):
//
//     offset  size  field
//     0       4     magic  "FKM1"
//     4       16    salt   (random, used by the KDF)
//     20      16    iv     (random, AES-CTR initial counter block)
//     36      ..    ciphertext = AES-256-CTR(plaintext)
//
// Key = KDF(passphrase, salt). The KDF is iterated SHA-256 (see
// crypto.c). It is intentionally simple so the two implementations
// match byte-for-byte; upgrading to PBKDF2-HMAC-SHA256 is a TODO and
// must be changed in BOTH places at once.
//
// NOTE: there is no MAC yet, so this gives confidentiality but not
// integrity. Adding HMAC-SHA256 over (iv || ciphertext) is a TODO.

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FKM_MAGIC "FKM1"
#define FKM_MAGIC_LEN 4
#define FKM_SALT_LEN 16
#define FKM_IV_LEN 16
#define FKM_HEADER_LEN (FKM_MAGIC_LEN + FKM_SALT_LEN + FKM_IV_LEN) // 36
#define FKM_KEY_LEN 32 // AES-256
#define FKM_KDF_ITERATIONS 100000

// Derive a 32-byte key from passphrase + salt. `out_key` must be 32 bytes.
void fkm_kdf(const char* passphrase, const uint8_t salt[FKM_SALT_LEN], uint8_t out_key[FKM_KEY_LEN]);

// Decrypt a whole vault file blob in place-ish: reads `file`/`file_len`
// (including the 36-byte header), writes the plaintext into a freshly
// malloc'd buffer returned via *out_plain / *out_len. Caller frees and
// should wipe it. Returns false on bad magic / short file / OOM.
bool fkm_decrypt_file(
    const char* passphrase,
    const uint8_t* file,
    size_t file_len,
    uint8_t** out_plain,
    size_t* out_len);
