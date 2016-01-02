#ifndef CRYPTO_H_
#define CRYPTO_H_

#include <stdint.h>
#include <stdbool.h>

// HW accelerationof crypto ops
#define HW_CC2538      1

#define KEY_BYTES           32
#define COMPRESS_KEY_BYTES  33

#define SHA256_BYTES        32
#define SHA512_BYTES        64

#define SALT_MAX_BYTES      60

typedef enum
{
    AES_DECRYPT = 0,
    AES_ENCRYPT,
} op_e;

typedef struct sha256ctx sha256ctx_t;

void     crypto_init            (void);

void     crypto_sha256_reset    (sha256ctx_t **ctx);
void     crypto_sha256_input    (sha256ctx_t *ctx, const uint8_t *msg, const uint32_t bytes);
void     crypto_sha256_result   (sha256ctx_t *ctx, uint8_t *hash);
void     crypto_sha256          (const uint8_t *msg, const uint32_t bytes, uint8_t *hash);

void     crypto_hmac512         (const uint8_t *key, const uint32_t key_bytes, const uint8_t *msg, const uint32_t msg_bytes, uint8_t *hash);

void     crypto_ripemd160       (const uint8_t *msg, const uint32_t bytes, uint8_t *hash);

void     crypto_ecdsa_genpubkey (const uint8_t *exp, uint8_t *x, uint8_t *y);
unsigned crypto_ecdsa_sign      (const uint8_t *exp, const uint8_t *sha256_hash, uint8_t *sig, uint8_t *sig_bytes);
void     crypto_ecdsa_add256    (const uint8_t *a, const uint8_t *b, uint8_t *c);

void     crypto_pbkdf2_hmac256  (const uint8_t *password, const unsigned pass_bytes, const uint8_t *salt, const unsigned salt_bytes, const unsigned rounds, uint8_t *out, const unsigned out_bytes);
void     crypto_pbkdf2_hmac512  (const uint8_t *password, const unsigned pass_bytes, const uint8_t *salt, const unsigned salt_bytes, const unsigned rounds, uint8_t *out);

void     crypto_aes128_loadkey  (uint8_t key[16], const unsigned keyslot);
void     crypto_aes128_process  (uint8_t *input, uint8_t *output, const unsigned bytes, const op_e operation, const unsigned keyslot);

void     crypto_entropy_timer_reset (void);
uint8_t  crypto_entropy_timer_get   (void);

#endif
