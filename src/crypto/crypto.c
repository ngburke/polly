#include <crypto.h>
#include <sha.h>
#include <ripemd160.h>
#include <ecdsa.h>
#include <string.h>
#include <assert.h>

#if HW_CC2538
#include <sha256.h>
#include <aes.h>
#include <gptimer.h>
#include <hw_memmap.h>
#endif

#define SHA256_BLOCK_BYTES 64

struct sha256ctx
{
#if HW_CC2538
    tSHA256State ctx;
#else
	SHA256Context ctx;
#endif
};

sha256ctx_t streamctx;

void crypto_init(void)
{
	ecdsa_init();
}

void crypto_sha256_reset(sha256ctx_t **ctx)
{
	*ctx = &streamctx;

#if HW_CC2538
    SHA256Init(&streamctx.ctx);
#else
	SHA256Reset(&streamctx.ctx);
#endif
}

void crypto_sha256_input(sha256ctx_t *ctx, const uint8_t *msg, const uint32_t bytes)
{
#if HW_CC2538
    SHA256Process(&ctx->ctx, (uint8_t*)msg, bytes);
#else
	SHA256Input(&ctx->ctx, msg, bytes);
#endif
}

void crypto_sha256_result(sha256ctx_t *ctx, uint8_t *hash)
{
#if HW_CC2538
    SHA256Done(&ctx->ctx, hash);
#else
	SHA256Result(&ctx->ctx, hash);
#endif
}


void crypto_sha256(const uint8_t *msg, const uint32_t bytes, uint8_t *hash)
{
#if HW_CC2538

    tSHA256State ctx;

    SHA256Init(&ctx);
    SHA256Process(&ctx, (uint8_t*)msg, bytes);
    SHA256Done(&ctx, hash);

#else

	SHA256Context ctx;

	SHA256Reset(&ctx);
	SHA256Input(&ctx, msg, bytes);
	SHA256Result(&ctx, hash);

#endif
}

void crypto_hmac256(const uint8_t *key, const uint32_t key_bytes, const uint8_t *msg, const uint32_t msg_bytes, uint8_t *hash)
{
#if HW_CC2538
    unsigned i;
    tSHA256State ctx;

    uint8_t key_xor_ipad[SHA256_BLOCK_BYTES];
    uint8_t key_xor_opad[SHA256_BLOCK_BYTES];
    uint8_t tmp_hash[SHA256_BYTES];
    uint8_t *actual_key;
    unsigned actual_key_bytes;

    if (key_bytes > SHA256_BLOCK_BYTES)
    {
        // Oops the key is too long, hash it first
        crypto_sha256(key, key_bytes, tmp_hash);
        actual_key       = tmp_hash;
        actual_key_bytes = SHA256_BYTES;
    }
    else
    {
        // Use the key as-is
        actual_key       = (uint8_t*)key;
        actual_key_bytes = key_bytes;
    }

    memset(key_xor_ipad, 0x36, SHA256_BLOCK_BYTES);
    memset(key_xor_opad, 0x5c, SHA256_BLOCK_BYTES);

    for (i = 0; i < actual_key_bytes; i++)
    {
        key_xor_ipad[i] ^= actual_key[i];
        key_xor_opad[i] ^= actual_key[i];
    }

    SHA256Init(&ctx);
    SHA256Process(&ctx, key_xor_ipad, SHA256_BLOCK_BYTES);
    SHA256Process(&ctx, (uint8_t*)msg, msg_bytes);
    SHA256Done(&ctx, tmp_hash);

    SHA256Init(&ctx);
    SHA256Process(&ctx, key_xor_opad, SHA256_BLOCK_BYTES);
    SHA256Process(&ctx, tmp_hash, SHA256_BYTES);
    SHA256Done(&ctx, hash);

#else

    HMACContext ctx;

	hmacReset(&ctx, SHA256, key, key_bytes);
	hmacInput(&ctx, msg, msg_bytes);
	hmacResult(&ctx, hash);

#endif
}

void crypto_hmac512(const uint8_t *key, const uint32_t key_bytes, const uint8_t *msg, const uint32_t msg_bytes, uint8_t *hash)
{
    HMACContext ctx;

	hmacReset(&ctx, SHA512, key, key_bytes);
	hmacInput(&ctx, msg, msg_bytes);
	hmacResult(&ctx, hash);
}

void crypto_ripemd160(const uint8_t *msg, const uint32_t bytes, uint8_t *hash)
{
    ripemd160(msg, bytes, hash);
}

void crypto_ecdsa_genpubkey(const uint8_t *exp, uint8_t *x, uint8_t *y)
{
    ecdsa_genPublicKey(x, y, exp);
}

unsigned crypto_ecdsa_sign(const uint8_t *exp, const uint8_t *sha256_hash, uint8_t *sig, uint8_t *sig_bytes)
{
	return ecdsa_sign(exp, sha256_hash, sig, sig_bytes);
}

void crypto_ecdsa_add256(const uint8_t *a, const uint8_t *b, uint8_t *c)
{
	ecdsa_addMod(a, b, c);
}

void crypto_pbkdf2_hmac512(const uint8_t *password, const unsigned pass_bytes, const uint8_t *salt, const unsigned salt_bytes, const unsigned rounds, uint8_t *out)
{
    unsigned i, j;
    uint8_t  hash[SHA512_BYTES];
    uint8_t  msg [SHA512_BYTES];

    assert(salt_bytes <= SHA512_BYTES);

    // The 'message' for the first round is the salt || '00000001'
    memcpy(msg, salt, salt_bytes);
    msg[salt_bytes    ] = 0;
    msg[salt_bytes + 1] = 0;
    msg[salt_bytes + 2] = 0;
    msg[salt_bytes + 3] = 1;

    // Round 1 is a special case
    crypto_hmac512(password, pass_bytes, msg, salt_bytes + 4, hash);
    memcpy(out, hash, SHA512_BYTES);

    for (i = 0; i < rounds - 1; i++)
    {
        // Just calculated hash is the next calculation's message
        crypto_hmac512(password, pass_bytes, hash, SHA512_BYTES, hash);

        // XOR the last hash with the running output
        for (j = 0; j < SHA512_BYTES; j++)
        {
            out[j] ^= hash[j];
        }
    }
}

void crypto_pbkdf2_hmac256(const uint8_t *password, const unsigned pass_bytes, const uint8_t *salt, const unsigned salt_bytes, const unsigned rounds, uint8_t *out, const unsigned out_bytes)
{
    unsigned passes, i, j, offset;
    uint8_t  hash[SHA256_BYTES];
    uint8_t  msg [SHA256_BYTES];

    assert(out_bytes % 32 == 0);
    assert(salt_bytes <= (SHA256_BYTES - 4)); // -4 for the || '00000001' (see below)

    for (passes = 0; passes < out_bytes / SHA256_BYTES; passes++)
    {
        offset = passes * SHA256_BYTES;

        // The 'message' for the first round is the salt || '00000001'
        memcpy(msg, salt, salt_bytes);
        msg[salt_bytes    ] = 0;
        msg[salt_bytes + 1] = 0;
        msg[salt_bytes + 2] = 0;
        msg[salt_bytes + 3] = passes + 1;

        // Round 1 is a special case
        crypto_hmac256(password, pass_bytes, msg, salt_bytes + 4, hash);
        memcpy(&out[offset], hash, SHA256_BYTES);

        for (i = 0; i < rounds - 1; i++)
        {
            // Just calculated hash is the next calculation's message
            crypto_hmac256(password, pass_bytes, hash, SHA256_BYTES, hash);

            // XOR the last hash with the running output
            for (j = 0; j < SHA256_BYTES; j++)
            {
                out[offset + j] ^= hash[j];
            }
        }
    }
}

void crypto_aes128_loadkey(uint8_t key[16], const unsigned keyslot)
{
#if HW_CC2538

    AESLoadKey((uint8_t*)key, keyslot);

    memset(key, 0, sizeof(key));

#else

    BROKEN;

#endif
}

void crypto_aes128_process(uint8_t *input, uint8_t *output, const unsigned bytes, const op_e operation, const unsigned keyslot)
{
#if HW_CC2538

    AESECBStart(input, output, bytes, keyslot, operation, false);

    while (!AESECBCheckResult());

    AESECBGetResult();

#else

    BROKEN;

#endif
}

void crypto_entropy_timer_reset(void)
{
#if HW_CC2538

    TimerConfigure(GPTIMER0_BASE, GPTIMER_CFG_A_PERIODIC);
    TimerEnable(GPTIMER0_BASE, GPTIMER_A);

#else

    BROKEN

#endif
}

uint8_t crypto_entropy_timer_get(void)
{
#if HW_CC2538

    return TimerValueGet(GPTIMER0_BASE, GPTIMER_A) & 0xFF;

#else
    BROKEN
#endif
}
