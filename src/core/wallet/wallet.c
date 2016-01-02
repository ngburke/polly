#include <wallet.h>
#include <cmd.h>
#include <assert.h>
#include <crypto.h>
#include <string.h>
#include <words.h>

#define CHAINS 2

static void seed_updated(void);

static const uint8_t  masterSeedKey[] = "Bitcoin seed";
static const uint8_t  seed_default[]  = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

static unsigned master_seed_bytes;
static uint8_t  master_seed              [MASTER_SEED_BYTES];

static uint8_t  master_priv_key          [KEY_BYTES];
static uint8_t  master_chain_code        [KEY_BYTES];

static uint8_t  account_priv_key         [KEY_BYTES];  // Hardened
static uint8_t  account_chain_code       [KEY_BYTES];

static uint8_t  chain_priv_key           [CHAINS][KEY_BYTES];
static uint8_t  chain_chain_code         [CHAINS][KEY_BYTES];
static uint8_t  chain_pub_key_compress   [CHAINS][COMPRESS_KEY_BYTES];

/*
static uint8_t  master_pub_key_x         [KEY_BYTES];
static uint8_t  master_pub_key_y         [KEY_BYTES];
static uint8_t  master_pub_key_compress  [COMPRESS_KEY_BYTES];
*/

static uint32_t swap_uint32(uint32_t val);


uint32_t swap_uint32(uint32_t val)
{
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF );
    return (val << 16) | (val >> 16);
}


void wallet_init(void)
{
	// Default seed
    memcpy(master_seed, seed_default, sizeof(seed_default));
    master_seed_bytes = sizeof(seed_default);

    seed_updated();
}


void wallet_address_private_key_get(const uint32_t account, const uint32_t chain, const uint32_t index, uint8_t *priv_key, uint8_t *chain_code)
{
    wallet_private_key_get(&chain_priv_key[chain][0], &chain_pub_key_compress[chain][0], &chain_chain_code[chain][0], index, priv_key, chain_code);
}

#define DATA_BYTES (COMPRESS_KEY_BYTES + 4)
uint8_t data[DATA_BYTES];

void wallet_private_key_get(const uint8_t *parent_priv_key, const uint8_t *parent_pub_key_compress, const uint8_t *parent_chain_code, const uint32_t index, uint8_t *priv_key, uint8_t *chain_code)
{
	uint32_t    index_swap;
    uint8_t     hash[SHA512_BYTES];

	index_swap = swap_uint32(index);

	// Calculate the private key for keyNum
	if (index & HARDENED_KEY)
	{
		// Hardened derivation
	    data[0] = 0;
		memcpy(&data[1], parent_priv_key, KEY_BYTES);
		memcpy(&data[COMPRESS_KEY_BYTES], &index_swap, sizeof(uint32_t));
	}
	else
	{
		// Normal derivation
	    assert(NULL != parent_pub_key_compress);

		memcpy(data, parent_pub_key_compress, COMPRESS_KEY_BYTES);
		memcpy(&data[COMPRESS_KEY_BYTES], &index_swap, sizeof(uint32_t));
	}
	
	// Calculate the private key (exponent) and chain code
	crypto_hmac512(parent_chain_code, KEY_BYTES, data, DATA_BYTES, hash);

	// First 32 bytes of hash is I[L], the private key - use to get the final child key
	crypto_ecdsa_add256(hash, parent_priv_key, priv_key);

    // Second 32 bytes of hash is I[R], the child's chain code
	if (NULL != chain_code)
	{
	    memcpy(chain_code, &hash[KEY_BYTES], KEY_BYTES);
	}
}

void wallet_public_key_get(const keytype_e type, const uint32_t account, const uint32_t chain, const uint32_t address, uint8_t *pubkey_x, uint8_t *pubkey_y, uint8_t *chaincode)
{
    uint8_t priv_key[KEY_BYTES];
    
    switch (type)
    {
        case KEY_MASTER:
            crypto_ecdsa_genpubkey(master_priv_key, pubkey_x, pubkey_y);

            // Not returning chain codes for master keys
            if (chaincode != NULL)  memset(chaincode, 0, KEY_BYTES);

            break;

        case KEY_ACCOUNT:
            crypto_ecdsa_genpubkey(account_priv_key, pubkey_x, pubkey_y);
            if (chaincode != NULL)  memcpy(chaincode, account_chain_code, KEY_BYTES);
            break;

        case KEY_CHAIN:
            crypto_ecdsa_genpubkey(chain_priv_key[chain], pubkey_x, pubkey_y);
            if (chaincode != NULL)  memcpy(chaincode, chain_chain_code[chain], KEY_BYTES);
            break;

        case KEY_ADDRESS:

            // Calculate the private key
            wallet_private_key_get(chain_priv_key[chain],
                                   chain_pub_key_compress[chain],
                                   chain_chain_code[chain],
                                   address,
                                   priv_key,
                                   NULL);

            // Calculate the public key
            crypto_ecdsa_genpubkey(priv_key, pubkey_x, pubkey_y);

            // Not returning chain codes for address keys
            if (chaincode != NULL)  memset(chaincode, 0, KEY_BYTES);

            break;

        default:
            assert(0);
    }
}


void wallet_public_key_to_hash160(const uint8_t *pubKeyX, const uint8_t *pubKeyY, uint8_t *hash160)
{
    uint8_t pubKeyCompress[COMPRESS_KEY_BYTES];
    uint8_t hash[SHA256_BYTES];
   
    wallet_public_key_compress(pubKeyX, pubKeyY, pubKeyCompress); 

    crypto_sha256(pubKeyCompress, sizeof(pubKeyCompress), hash);

    crypto_ripemd160(hash, SHA256_BYTES, hash160);
}    


void wallet_master_seed_set(const char *wordlist, const unsigned bytes)
{
    assert(bytes <= WORDLIST_MAX_CHARS);
    
    // Convert the 18 word mnemonic wordlist into 64 byte seed
    crypto_pbkdf2_hmac256((uint8_t*)wordlist, bytes, "polly", sizeof("polly") - 1, SEED_ROUNDS, master_seed, sizeof(master_seed));
    master_seed_bytes = MASTER_SEED_BYTES;

    // Regenerate the wallet based on the new seed
    seed_updated();
}


void wallet_master_seed_get(uint8_t seed[MASTER_SEED_BYTES], unsigned *bytes)
{
    memcpy(seed, master_seed, master_seed_bytes);
    *bytes = master_seed_bytes;
}


void wallet_public_key_compress(const uint8_t *pubKeyX, const uint8_t *pubKeyY, uint8_t *compressed)
{
    compressed[0] = pubKeyY[KEY_BYTES - 1] & 1 ? 0x03 : 0x02;
    memcpy(&compressed[1], pubKeyX, KEY_BYTES);
}

#define BASE58_INPUT_BYTES  21
#define BASE58_CHECK_BYTES  4

void wallet_base58_encode(const uint8_t *data, const unsigned data_bytes, char *str, const unsigned str_bytes)
{
    const char base58[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    uint8_t chk_data[BASE58_INPUT_BYTES + BASE58_CHECK_BYTES];
    uint8_t hash[SHA256_BYTES];
    unsigned i, j, tmp, remain;
    char c;

    // Only expecting to build up from a RIPEMD-160 (plus version byte) bitcoin address
    assert(BASE58_INPUT_BYTES == data_bytes);
    assert(BASE58_ADDR_BYTES == str_bytes);

    crypto_sha256(data, data_bytes, hash);
    crypto_sha256(hash, SHA256_BYTES, hash);

    memcpy(chk_data, data, data_bytes);

    // Add on the checksum
    memcpy(&chk_data[data_bytes], hash, 4);

    for (i = 0; i < BASE58_ADDR_BYTES; i++)
    {
        remain = chk_data[0] % 58;
        chk_data[0] /= 58;

        for (j = 1; j < data_bytes + 4; j++)
        {
            tmp = remain * 24 + chk_data[j];  // 2^8 == 4*58 + 24
            chk_data[j] = remain * 4 + (tmp / 58);
            remain = tmp % 58;
        }
        str[i] = base58[remain];
    }

    // Reverse string
    for (i = 0; i < str_bytes / 2; i++)
    {
        c = str[i];
        str[i] = str[str_bytes - 1 - i];
        str[str_bytes - 1 - i] = c;
    }
}

static void seed_updated(void)
{
    unsigned i;
    uint8_t hash[SHA512_BYTES];
    uint8_t pub_key_x[KEY_BYTES];
    uint8_t pub_key_y[KEY_BYTES];
    uint8_t account_pub_key_compress[COMPRESS_KEY_BYTES];

    // Assumes that the new seed is already copied into the masterSeed global

    // Calculate the master private key (exponent) and master chain code
    crypto_hmac512(masterSeedKey, sizeof(masterSeedKey) - 1, master_seed, master_seed_bytes, hash);
    
    memcpy(master_priv_key, hash, 32);
	memcpy(master_chain_code, &hash[32], 32);

	// Calculate the account keys
	wallet_private_key_get(master_priv_key, NULL, master_chain_code, HARDENED_KEY | 0, account_priv_key, account_chain_code);

    crypto_ecdsa_genpubkey(account_priv_key, pub_key_x, pub_key_y);
    wallet_public_key_compress(pub_key_x, pub_key_y, account_pub_key_compress);


	// Calculate the chain keys
    for (i = 0; i < CHAINS; i++)
    {
        wallet_private_key_get(account_priv_key, account_pub_key_compress, account_chain_code, i, chain_priv_key[i], chain_chain_code[i]);

        crypto_ecdsa_genpubkey(chain_priv_key[i], pub_key_x, pub_key_y);
        wallet_public_key_compress(pub_key_x, pub_key_y, chain_pub_key_compress[i]);
    }

	/*
    // Calculate the master public key, and calculate the compressed form
    crypto_ecdsa_genpubkey(master_priv_key, master_pub_key_x, master_pub_key_y);

    wallet_public_key_compress(master_pub_key_x, master_pub_key_y, master_pub_key_compress);
    */
}
