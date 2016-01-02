#ifndef WALLET_H_
#define WALLET_H_

#include <stdint.h>
#include <stdbool.h>

#define MASTER_SEED_BYTES  64
#define SEED_ROUNDS        2048
#define BASE58_ADDR_BYTES  34  // Base58 output length for a bitcoin address
#define HARDENED_KEY       0x80000000

typedef enum
{
    KEY_MASTER = 0,
    KEY_ACCOUNT,
    KEY_CHAIN,
    KEY_ADDRESS,
    KEY_TYPE_INVALID,
} keytype_e;


void wallet_init                    (void);
void wallet_address_private_key_get (const uint32_t account, const uint32_t chain, const uint32_t index, uint8_t *priv_key, uint8_t *chain_code);
void wallet_private_key_get         (const uint8_t *parent_priv_key, const uint8_t *parent_pub_key_compress, const uint8_t *parent_chain_code, const uint32_t index, uint8_t *priv_key, uint8_t *chain_code);
void wallet_public_key_get          (const keytype_e type, const uint32_t account, const uint32_t chain, const uint32_t address, uint8_t *pub_key_x, uint8_t *pub_key_y, uint8_t *chaincode);
void wallet_master_seed_set         (const char *wordlist, const unsigned bytes);
void wallet_master_seed_get         (uint8_t seed[MASTER_SEED_BYTES], unsigned *bytes);
void wallet_public_key_to_hash160   (const uint8_t *pub_key_x, const uint8_t *pub_key_y, uint8_t *hash160);
void wallet_public_key_compress     (const uint8_t *pub_key_x, const uint8_t *pub_key_y, uint8_t *compressed);
void wallet_base58_encode           (const uint8_t *data, const unsigned data_bytes, char *str, const unsigned str_bytes);

#endif // WALLET_H_
