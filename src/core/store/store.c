#include <store.h>
#include <crypto.h>
#include <flash.h>
#include <wallet.h>
#include <assert.h>
#include <string.h>
#include <linker.h>

#define STORE_KEY_BYTES 16

const char     MODEL[]         = "polly";
const uint32_t REVISION        = 0x00010000; // 1.0
const char     VERIFICATION[]  = "polly context 1";

typedef struct
{
    char     model[8];
    uint32_t revision;
    uint32_t rsvd;
} store_header_t;

typedef struct
{
    char     verification[16];
    uint8_t  master_seed[MASTER_SEED_BYTES];  // HD wallet seed
} store_t;


bool store_present(void)
{
    store_t store;
    store_header_t *hdr = (store_header_t*)STORAGE_BASE;

    // Copy the context from flash to memory - cannot automate crypto directly form flash
    memcpy(&store, (void*)(STORAGE_BASE + 16), sizeof(store));

    if (0 == strncmp(hdr->model, MODEL, sizeof(MODEL)) &&
        REVISION == hdr->revision)
    {
        return true;
    }
    else
    {
        return false;
    }
}

store_status_e store_load(const unsigned keyslot)
{
    store_t store;

    assert(store_present());

    // Copy the context from flash to memory - cannot automate crypto directly from flash
    memcpy(&store, (void*)(STORAGE_BASE + 16), sizeof(store));

    // Found the context header, try to decrypt it, assumes keyslot was loaded previously
    crypto_aes128_process((uint8_t*)&store, (uint8_t*)&store, sizeof(store), AES_DECRYPT, keyslot);

    if (0 == strncmp(store.verification, VERIFICATION, sizeof(VERIFICATION)))
    {
        return STORE_OK;
    }
    else
    {
        return STORE_ERR_INVALID_PIN;
    }
}


void store_create(const unsigned keyslot)
{
    unsigned seed_bytes;
    store_header_t header;
    store_t        store;

    // Create a new context
    memset(&header, 0, sizeof(header));
    memset(&store,    0, sizeof(store));

    strncpy(header.model, MODEL, sizeof(MODEL));
    header.revision = REVISION;

    strncpy(store.verification, VERIFICATION, sizeof(VERIFICATION));

    // Assumes that the master seed has already been set
    wallet_master_seed_get(store.master_seed, &seed_bytes);

    assert(seed_bytes == MASTER_SEED_BYTES);

    // Erase the context flash page
    FlashMainPageErase(STORAGE_BASE);

    // Write the header in plaintext
    FlashMainPageProgram((uint32_t*)&header, STORAGE_BASE, sizeof(header));

    // Encrypt the context, assumes keyslot was loaded previously
    crypto_aes128_process((uint8_t*)&store, (uint8_t*)&store, sizeof(store), AES_ENCRYPT, keyslot);

    // Write the encrypted context to flash
    FlashMainPageProgram((uint32_t*)&store, STORAGE_BASE + sizeof(header), sizeof(store));
}


void store_wipe()
{
    FlashMainPageErase(STORAGE_BASE);
    FlashMainPageErase(STORAGE_BASE);
}
