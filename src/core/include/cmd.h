#ifndef CMD_H_
#define CMD_H_

#include <words.h>
#include <wallet.h>

// Defines
#define SMALL_BYTES               1
#define GET_PUBLIC_KEY_BYTES      (1 + sizeof(getPublicKey_t))
#define SET_MASTER_SEED_MAX_BYTES (1 + sizeof(setMasterSeed_t))

#define SMALL_RESP_BYTES          1
#define ID_RESP_BYTES             (1 + sizeof(identifyResp_t))
#define GET_PUBLIC_KEY_RESP_BYTES (1 + sizeof(getPublicKeyResp_t))

#define TX_HASH_BYTES             32
#define HASH_ADDR_BYTES           20

// Types
typedef enum
{
    CMD_NONE = 0,

    // Public commands
    CMD_RESET           = 1,
    CMD_IDENTIFY        = 2,
    CMD_GET_PUBLIC_KEY  = 3,
    CMD_SIGN_TX         = 4,
    CMD_PREV_TX         = 5,
    CMD_GET_SIGNED_TX   = 6,
    CMD_ENTROPY         = 7,
    CMD_FW_DOWNLOAD     = 8,

    // Audit commands
    CMD_AUDIT_MODE      = 10,
    CMD_SET_MASTER_SEED = 11,

    // Validation commands
    CMD_VAL_MODE        = 20,

    // Simple responses
    CMD_ACK_SUCCESS     = 32,
    CMD_ACK_INVALID     = 33,
    CMD_ACK_DENIED      = 34,
    CMD_ACK_USER        = 35,
    CMD_ACK_BUSY        = 36,

} cmd_e;

#define MODEL_NAME_BYTES        16

//
// We need to set an arbitrary cap on the signed tx due to
// memory constraints. This will constrain how many inputs
// can be used in a single spend. The math is as follows:
//
// Various constant sized stuff for the tx is:
//
//      version  4
//  input count  1
// output count  1
//    lock time  4
// -----------------
//               10B
//
// Input and output count can  be > 1B, but we're going to limit
// counts to < 252 total.
//
// Now, outputs. To make it very clear to the user we're only
// allowing a single output address so it can be displayed and
// verified. One more output is allowed to gather back change
// to an address within the wallet. This address can be one of
// the inputs or a different address within the wallet. Sizes:
//
//  satoshis          8
//  pk_script length  1
//  pk_script         25
// ----------------------
//                    34B
//
// Finally, inputs. Each individual signed input contains these
// fields:
//
// prev output sha256  32
//  prev output index  4
//  sig script length  1
//         sig length  1
//    ecdsa signature  73 (72 and 71 are possible, less is also possible but highly unlikely)
//     pub key length  1
// compressed pub key  33
//           sequence  4
// ----------------------
//                     148B
//
// A maximum of 32 inputs sounds good, mainly to limit the
// time it takes to sign a tx, but also to limit the tx size.
// Solve for this equation using our previous data to set the
// cap on the transaction size:
//
// 10 + (34 * 2) + (148 * 32) = 4814 (round up to next dword)
//

#define MAX_INPUTS           32
#define MAX_OUTPUTS          2
#define SIGNED_TX_MAX_BYTES  4816

#define PK_SCRIPT_BYTES      25
#define ECDSA_SIG_BYTES_MAX  73

#define KEY_BYTES            32
#define COMPRESS_KEY_BYTES   33

//
// A sign tx request consists of the following fields:
//
//         command   1
//     input count   1
//        input id   (1 + 1 + 4) * MAX_INPUTS
//    input pub key  COMPRESS_KEY_BYTES * MAX_INPUTS
//  output address   20
// output satoshis   8
//       change id   (1 + 1 + 4)
// change satoshis   8
// --------------------

#define SIGN_TX_MAX_BYTES   (1 + 1 + ((1 + 1 + 4) * MAX_INPUTS) + (COMPRESS_KEY_BYTES * MAX_INPUTS) + 20 + 8 + (1 + 1 + 4) + 8)
#define STREAM_CHUNK_BYTES  256  // Get prev tx streamed in 256 bytes at a time

#pragma pack(1)

typedef struct
{
    uint8_t model[MODEL_NAME_BYTES];
} identifyResp_t;

typedef struct
{
    char wordlist[WORDLIST_MAX_CHARS];
} setMasterSeed_t;

typedef struct
{
    keytype_e type : 8;
    uint8_t   account;
    uint8_t   chain;
    uint32_t  address;
} getPublicKey_t;

typedef struct
{
    uint8_t pubkey_x[KEY_BYTES];
    uint8_t pubkey_y[KEY_BYTES];
    uint8_t chaincode[KEY_BYTES];
} getPublicKeyResp_t;

typedef struct
{
    cmd_e cmd;

    union
    {
        uint8_t         signtx[SIGN_TX_MAX_BYTES];
        uint8_t         prevtx[STREAM_CHUNK_BYTES];
        getPublicKey_t  pk;
        setMasterSeed_t seed;
    };
} cmd_t;

typedef struct
{
    cmd_e cmd;

    union
    {
        identifyResp_t     id;
        getPublicKeyResp_t pk;
        uint8_t            signedtx[SIGNED_TX_MAX_BYTES];
    };
} cmdResp_t;

#pragma pack()

typedef enum
{
	RESP_NONE,
	RESP_NEW,
	RESP_CONT,
} resp_e;

// Functions
resp_e cmd_handler       (const cmd_t *cmd, const int chunk_bytes, const int total_bytes, const bool streaming, const bool start, cmdResp_t *resp, int *resp_bytes);
bool   cmd_handler_quick (      cmd_t *cmd, const unsigned cmd_bytes, cmdResp_t *resp, unsigned *resp_bytes);


#endif // CMD_H_
