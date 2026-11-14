#include "crypto.h"

#include <furi.h>
#include <string.h>

#include "vendor/sha256.h" // B-Con/crypto-algorithms
#include "vendor/aes.h" // kokke/tiny-AES-c  (AES256=1, CTR=1)

// Iterated SHA-256 KDF.
//
//   h0 = SHA256(salt || passphrase)
//   hi = SHA256(h_{i-1})     for i in 1..ITER
//   key = h_ITER             (32 bytes)
//
// Keep this identical to kdf() in tools/fkm_vault.py.
void fkm_kdf(
    const char* passphrase,
    const uint8_t salt[FKM_SALT_LEN],
    uint8_t out_key[FKM_KEY_LEN]) {
    SHA256_CTX ctx;
    uint8_t digest[SHA256_BLOCK_SIZE]; // 32

    sha256_init(&ctx);
    sha256_update(&ctx, salt, FKM_SALT_LEN);
    sha256_update(&ctx, (const BYTE*)passphrase, strlen(passphrase));
    sha256_final(&ctx, digest);

    for(uint32_t i = 0; i < FKM_KDF_ITERATIONS; i++) {
        sha256_init(&ctx);
        sha256_update(&ctx, digest, SHA256_BLOCK_SIZE);
        sha256_final(&ctx, digest);
    }

    memcpy(out_key, digest, FKM_KEY_LEN);
    memset(digest, 0, sizeof(digest));
}

bool fkm_decrypt_file(
    const char* passphrase,
    const uint8_t* file,
    size_t file_len,
    uint8_t** out_plain,
    size_t* out_len) {
    if(file_len < FKM_HEADER_LEN) return false;
    if(memcmp(file, FKM_MAGIC, FKM_MAGIC_LEN) != 0) return false;

    const uint8_t* salt = file + FKM_MAGIC_LEN;
    const uint8_t* iv = file + FKM_MAGIC_LEN + FKM_SALT_LEN;
    const uint8_t* cipher = file + FKM_HEADER_LEN;
    size_t cipher_len = file_len - FKM_HEADER_LEN;

    uint8_t key[FKM_KEY_LEN];
    fkm_kdf(passphrase, salt, key);

    uint8_t* plain = malloc(cipher_len ? cipher_len : 1);
    if(!plain) {
        memset(key, 0, sizeof(key));
        return false;
    }
    memcpy(plain, cipher, cipher_len);

    // AES-256-CTR is symmetric: the same call decrypts.
    struct AES_ctx ctx;
    uint8_t iv_copy[FKM_IV_LEN];
    memcpy(iv_copy, iv, FKM_IV_LEN); // tiny-AES mutates the counter
    AES_init_ctx_iv(&ctx, key, iv_copy);
    AES_CTR_xcrypt_buffer(&ctx, plain, cipher_len);

    // Wipe key material.
    memset(key, 0, sizeof(key));
    memset(&ctx, 0, sizeof(ctx));
    memset(iv_copy, 0, sizeof(iv_copy));

    *out_plain = plain;
    *out_len = cipher_len;
    return true;
}
