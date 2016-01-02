#include <util.h>
#include <core.h>
#include <wallet.h>
#include <assert.h>
#include <string.h>
#include <crypto.h>
#include <cmd.h>
#include <cmdparser.h>
#include <screen.h>
#include <linker.h>
#include <fw_header.h>
#include <flash.h>

typedef enum
{
    SIGN_FLOW_SETUP,
    SIGN_FLOW_PREV_TX,
    SIGN_FLOW_SIGN_TX_READY,
    SIGN_FLOW_SIGN_TX_DONE,
    SIGN_FLOW_SIGN_TX_DENIED,
    SIGN_FLOW_SIGN_TX_ERROR,
} signFlow_e;

typedef enum
{
    PTX_HEADER,
    PTX_VERSION,
    PTX_IN_COUNT,
    PTX_IN_PREV_HASH,
    PTX_IN_PREV_INDEX,
    PTX_IN_SCRIPT_LEN,
    PTX_IN_SCRIPT,
    PTX_IN_SEQUENCE,
    PTX_OUT_COUNT,
    PTX_OUT_VALUE,
    PTX_OUT_SCRIPT_LEN,
    PTX_OUT_SCRIPT,
    PTX_LOCK_TIME,
    PTX_DONE,
} prevTxChunk_e;

#pragma pack(1)

typedef struct
{
    uint8_t  inputIdx;   // Index into the 'input' array of signInfo_t
    uint32_t outputIdx;  // Output index in the current prev tx
} prevTxHeader_t;


typedef struct
{
    uint8_t   account;
    uint8_t   chain;
    uint32_t  keyid;
    uint8_t   pubKeyCompress[COMPRESS_KEY_BYTES];
    uint8_t   prevTxHash[TX_HASH_BYTES];
    uint32_t  prevTxOutputIndex;
    uint8_t   prevTxPkScriptBytes;
    uint8_t   prevTxPkScript[PK_SCRIPT_BYTES];
    uint64_t  valueSatoshis;
    uint8_t   sig[ECDSA_SIG_BYTES_MAX];
    uint8_t   sigBytes;
} signInputs_t;

typedef struct
{
    uint16_t        totalBytes;          // Total bytes
    bool            hashCalc;            // When true, do a running SHA 256 operation over the prev tx
    sha256ctx_t    *hashCtx;             // Context for hash operations
    prevTxChunk_e   field;               // What chunk are we in
    uint32_t        fieldByte;           // Current chunk byte
    uint32_t        fieldTotalBytes;     // Current chunk total bytes
    uint8_t         headerEntries;       // How many entries in the header are valid
    prevTxHeader_t  header[MAX_INPUTS];  // The current prev tx header
    uint64_t        inputsLeft;          // Inputs left to process
    uint64_t        inputScriptBytes;    // Signature length of the current input being processed
    uint64_t        outputsTotal;        // Outputs left to process
    uint64_t        outputIndex;         // Current output index being processed
    bool            outputSave;          // Should we be saving the current output?
    uint8_t         outputSaveToIndex;   // If outputSave is true, this is the index into cmdi.signi.input[]
    uint64_t        outputScriptBytes;   // Signature length of the current output being processed
} prevTxInfo_t;

typedef struct
{
    //
    // Overall state of the signing flow, includes the 
    // setup (SIGN_TX cmd), previous tx gathering (PREV_TX cmds)
    // and the final signed transaction (GET_SIGN_TX cmd)
    //
    signFlow_e    sign_flow;
    uint8_t      *txbuff;

    //
    // Information from the SIGN_TX command
    //
    bool           giveChange;
    uint8_t        totalInputs;
    int            inputsLeftToVerify;
    signInputs_t   input[MAX_INPUTS];
    
    uint8_t        output_version;
    uint8_t        output_addr[HASH_ADDR_BYTES];
    uint64_t       output_satoshis;
    
    uint8_t        change_account;
    uint8_t        change_chain;
    uint32_t       change_keyid;
    uint8_t        change_addr[HASH_ADDR_BYTES];
    uint64_t       change_satoshis;

    //
    // Tracking info for PREV_TX commands
    //
    prevTxInfo_t   ptx;

} sign_info_t;

#define FW_WRITE_CHUNK  256

typedef struct
{
    unsigned total_bytes;
    unsigned rx_bytes;
    unsigned write_offset;
    unsigned chunk_offset;

    uint8_t  chunk[FW_WRITE_CHUNK];
} fwdl_info_t;

typedef struct
{
    // Response
    cmdResp_t   *resp;
    unsigned     respBytes;
    
    // Current command
    cmd_e        currCmd;
    
    // Signature flow info
    sign_info_t  signi;

    // Firmware download info
    fwdl_info_t  fwdl;
    
} cmdInfo_t;

typedef enum
{
    CP_IDLE,
    CP_BUSY,
    CP_USER,
} cp_state_e;

typedef enum
{
    CP_CMD_NONE,
    CP_CMD_SIGN,
} cp_cmd_e;

typedef struct
{
    cp_cmd_e   cmd;
    cp_state_e state;
} cp_t;

#pragma pack()

// Prototypes
static void        cp_sign           (void);
static cp_state_e  cp_state          (void);
static void        cp_set_state      (cp_state_e state);
static void        cp_set_cmd        (cp_cmd_e cmd);
static void        cp_end_cmd        (void);

static void        identify          (const cmd_t *cmd, const unsigned total_bytes);
static void        sign_tx           (const cmd_t *cmd, const unsigned cmdBytes);
static void        prev_tx           (const cmd_t *cmd, unsigned chunk_bytes, unsigned total_bytes, bool start);
static void        fw_download       (const cmd_t *cmd, unsigned chunk_bytes, unsigned total_bytes, bool start);
static void        get_signed_tx     (const cmd_t *cmd, const unsigned total_bytes);
static void        get_public_key    (const cmd_t *cmd, const unsigned total_bytes);
static void        set_master_seed   (const cmd_t *cmd, const unsigned total_bytes);
static void        small_resp        (const cmd_e cmd);
static void        reset_sign_state  (void);
static void        reset_ptx_state   (void);
static void        reset_fwdl_state  (void);

static bool        fw_write_chunk    (const uint8_t *d, unsigned bytes);
static bool        fw_verify         (void);

static unsigned    process_header    (const uint8_t *cd, const unsigned bytes, unsigned * const ateBytes);
static unsigned    process_skip      (const uint8_t *cd, const unsigned bytes, unsigned * const ateBytes, const unsigned totalBytes);
static unsigned    process_var_int   (const uint8_t *cd, const unsigned bytes, unsigned * const ateBytes, void *dest);
static unsigned    process_field     (const uint8_t *cd, const unsigned bytes, unsigned * const ateBytes, void *dest, const unsigned totalBytes);

static uint64_t    calc_fee          (void);
static unsigned    build_tx          (const bool forSigning, const unsigned inputIdx, uint8_t *dest);
static bool        save_curr_output  (const uint64_t outputIdx, uint8_t * const inputIdx);
static unsigned    output_script     (const uint8_t *hash160, uint8_t *script);
static unsigned    sign_tx_for_input (const uint8_t inputIdx, uint8_t *tx);

static void        memcpy_r          (void *dst, const void *src, const unsigned bytes);

// Constants
static const char     MODEL[MODEL_NAME_BYTES] = "Polly v0.3";
static const uint64_t MAX_SATOSHIS = 2100000000000000ULL;
static const unsigned ACCT_MAX  = 0;
static const unsigned CHAIN_MAX = 1;


// Globals
#pragma DATA_SECTION(cmdi, ".nonretenvar")
static cmdInfo_t     cmdi;
static prevTxInfo_t *ptx;
static cp_t          cp;

static const unsigned PTX_VERSION_B       = 4;
static const unsigned PTX_IN_PREV_HASH_B  = 32;
static const unsigned PTX_IN_PREV_INDEX_B = 4;
static const unsigned PTX_IN_SEQUENCE_B   = 4;
static const unsigned PTX_OUT_VALUE_B     = 8;
static const unsigned PTX_LOCK_TIME_B     = 4;


void cp_sign(void)
{
    unsigned    i;
    uint64_t    fee;
    signFlow_e  error;

    assert(cmdi.signi.sign_flow == SIGN_FLOW_SIGN_TX_READY);

    fee = calc_fee();
    if (fee > MAX_SATOSHIS)
    {
        error = SIGN_FLOW_SIGN_TX_DENIED;
        goto error;
    }

    cp_set_state(CP_USER);

    // signi.output_version is the starting byte of the output_address.
    if (!screen_send(&cmdi.signi.output_version, cmdi.signi.output_satoshis, fee))
    {
        error = SIGN_FLOW_SIGN_TX_DENIED;
        goto error;
    }

    cp_set_state(CP_BUSY);

    // Have each input sign the tx
    for (i = 0; i < cmdi.signi.totalInputs; i++)
    {
        if (sign_tx_for_input(i, cmdi.signi.txbuff) > 0)
        {
            error = SIGN_FLOW_SIGN_TX_ERROR;
            goto error;
        }
        screen_signing_progress(i + 1, cmdi.signi.totalInputs);
    }

    cmdi.signi.sign_flow = SIGN_FLOW_SIGN_TX_DONE;

    return;

error:
    reset_sign_state();

    // Preserve the sign flow state for error reporting
    cmdi.signi.sign_flow = error;
}

static cp_state_e cp_state(void)
{
    return cp.state;
}

static void cp_set_state(cp_state_e state)
{
    cp.state = state;
}

static void cp_set_cmd(cp_cmd_e cmd)
{
    assert(cp.cmd == CP_CMD_NONE);
    cp.cmd   = cmd;
    cp.state = CP_BUSY;
}

static void cp_end_cmd(void)
{
    cp.cmd   = CP_CMD_NONE;
    cp.state = CP_IDLE;
}

void core_process(void)
{
    if (cp.cmd == CP_CMD_NONE)  return;

    switch (cp.cmd)
    {
        case CP_CMD_SIGN:
            cp_sign();
            break;
    }

    cp_end_cmd();
}


void memcpy_r(void *dst, const void *src, const unsigned bytes)
{
   unsigned i;
   uint8_t *d = dst;
   const uint8_t *s = src;
   
   for (i = 0; i < bytes; i++)
   {
       d[i] = s[bytes - 1 - i];
   } 
}


void reset_sign_state(void)
{
    memset(&cmdi.signi, 0, sizeof(sign_info_t));

    cmdi.signi.sign_flow  = SIGN_FLOW_SETUP;
    reset_ptx_state();
}


void reset_ptx_state(void)
{
    ptx = &cmdi.signi.ptx;

    memset(ptx, 0, sizeof(prevTxInfo_t));

    ptx->field = PTX_HEADER;
    crypto_sha256_reset(&ptx->hashCtx);
}

void reset_fwdl_state(void)
{
    cmdi.fwdl.total_bytes  = 0;
    cmdi.fwdl.rx_bytes     = 0;
    cmdi.fwdl.write_offset = 0;
    cmdi.fwdl.chunk_offset = 0;
}

/*
bool cmd_handler_quick(cmd_t *cmd, const unsigned cmd_bytes, cmdResp_t *resp, unsigned *resp_bytes)
{
    bool handled = true;

    switch (cmd->cmd)
    {
        case CMD_IDENTIFY:
            identify(cmd, cmd_bytes, resp, resp_bytes);
            break;

        default:
            handled = false;
            break;
    }

    return handled;
}
*/

resp_e cmd_handler(const cmd_t *cmd, const int chunk_bytes, const int total_bytes, const bool streaming, const bool start, cmdResp_t *resp, int *resp_bytes)
{
    // Response is constructed by the code/functions below and must be cleared first.
    cmdi.resp = resp;

    if (!streaming || (cmdi.currCmd == CMD_NONE))
    {
        cmdi.currCmd = cmd->cmd;
    }
    
    if (!core_unlocked() && cmdi.currCmd != CMD_IDENTIFY)
    {
        // If we're locked, only identify is allowed
        cmdi.currCmd = CMD_NONE;
    }

    switch (cmdi.currCmd)
    {
        case CMD_RESET:
            small_resp(CMD_ACK_SUCCESS);
            break;
            
        case CMD_IDENTIFY:
            identify(cmd, total_bytes);
            break;
        
        case CMD_GET_PUBLIC_KEY:
            get_public_key(cmd, total_bytes);
            break;
            
        case CMD_SIGN_TX:
            sign_tx(cmd, total_bytes);
            break;

        case CMD_PREV_TX:
            prev_tx(cmd, chunk_bytes, total_bytes, start);
            break;
        
        case CMD_GET_SIGNED_TX:
            get_signed_tx(cmd, total_bytes);
            break;
        
        case CMD_FW_DOWNLOAD:
            fw_download(cmd, chunk_bytes, total_bytes, start);
            break;

        case CMD_SET_MASTER_SEED:
            set_master_seed(cmd, total_bytes);
            break;
            
        default:
            small_resp(CMD_ACK_INVALID);
            break;
    }
    
    if (cmdi.respBytes > 0)
    {
        *resp_bytes = cmdi.respBytes;
        
        cmdi.respBytes = 0;
        cmdi.resp      = NULL;
        cmdi.currCmd   = CMD_NONE;
        return RESP_NEW;
    }
    else
    {
        return RESP_NONE;
    }
}

static void small_resp(const cmd_e cmd)
{
    cmdi.resp->cmd = cmd;
    cmdi.respBytes = SMALL_RESP_BYTES;
}

static void identify(const cmd_t *cmd, const unsigned total_bytes)
{
    // Parameter checking
    if (cmd->cmd != CMD_IDENTIFY)   goto error;
    if (total_bytes != SMALL_BYTES) goto error;
    
    memcpy(cmdi.resp->id.model, MODEL, MODEL_NAME_BYTES);
    
    cmdi.resp->cmd = CMD_ACK_SUCCESS;
    cmdi.respBytes = ID_RESP_BYTES;
    return;

error:
    small_resp(CMD_ACK_INVALID);
}

static void sign_tx(const cmd_t *cmd, const unsigned cmdBytes)
{
    unsigned pos = 0, i;
    uint8_t *cmdData = (uint8_t*)cmd;
    uint8_t pubKeyX[KEY_BYTES], pubKeyY[KEY_BYTES];
    
    // Parameter checking
    if (cmd->cmd != CMD_SIGN_TX)      goto error;
    if (cmdBytes > SIGN_TX_MAX_BYTES) goto error;

    // No matter the previous state, start fresh when this command is received
    reset_sign_state();

    // Skip over the command byte
    pos++;
    
    // Grab the number of input key ids
    cmdi.signi.totalInputs = (unsigned)cmdData[pos];
    cmdi.signi.inputsLeftToVerify = cmdi.signi.totalInputs;
    
    if (cmdi.signi.totalInputs > MAX_INPUTS) goto error;

    // Input key ids;
    pos++;
    
    for (i = 0; i < cmdi.signi.totalInputs; i++)
    {
        // Only support a limited number of accounts
        cmdi.signi.input[i].account = cmdData[pos];
        if (cmdi.signi.input[i].account > ACCT_MAX) goto error;
        pos += 1;

        // Only support a limited number of chains
        cmdi.signi.input[i].chain = cmdData[pos];
        if (cmdi.signi.input[i].chain > CHAIN_MAX) goto error;
        pos += 1;

        // Upper key ID bit is reserved for derivation type
        cmdi.signi.input[i].keyid = *((uint32_t *)&cmdData[pos]);
        if (cmdi.signi.input[i].keyid & HARDENED_KEY) goto error;
        pos += 4; 

        memcpy(cmdi.signi.input[i].pubKeyCompress, &cmdData[pos], COMPRESS_KEY_BYTES);
        pos += COMPRESS_KEY_BYTES;
    }
    
    // Output
    cmdi.signi.output_version = 0;
    memcpy(cmdi.signi.output_addr, &cmdData[pos], HASH_ADDR_BYTES);
    pos += HASH_ADDR_BYTES;
    
    memcpy(&cmdi.signi.output_satoshis, &cmdData[pos], sizeof(uint64_t));

    if (cmdi.signi.output_satoshis > MAX_SATOSHIS)  goto error;

    pos += sizeof(uint64_t);
    
    // See if we have any change (a key id + satoshis)
    if (cmdBytes - pos == 1 + 1 + 4 + 8)
    {
        cmdi.signi.giveChange = true;
        
        // Only support a limited number of accounts
        cmdi.signi.change_account = cmdData[pos];
        if (cmdi.signi.change_account > ACCT_MAX) goto error;
        pos += 1;

        // Only support a limited number of chains
        cmdi.signi.change_chain = cmdData[pos];
        if (cmdi.signi.change_chain > CHAIN_MAX) goto error;
        pos += 1;

        cmdi.signi.change_keyid = *((uint32_t*)&cmdData[pos]);
        if (cmdi.signi.change_keyid & HARDENED_KEY) goto error;
        pos += 4;

        memcpy(&cmdi.signi.change_satoshis, &cmdData[pos], 8);
        if (cmdi.signi.change_satoshis > MAX_SATOSHIS)  goto error;

        pos += 8;

        // Get the public key for this id, and calculate its hash 160
        wallet_public_key_get(KEY_ADDRESS,
                              cmdi.signi.change_account,
                              cmdi.signi.change_chain,
                              cmdi.signi.change_keyid,
                              pubKeyX,
                              pubKeyY,
                              NULL);

        wallet_public_key_to_hash160(pubKeyX, pubKeyY, cmdi.signi.change_addr);
    }
    
    if (pos != cmdBytes) goto error;
    
    // Next state should be all the previous transactions
    cmdi.signi.sign_flow = SIGN_FLOW_PREV_TX;
    small_resp(CMD_ACK_SUCCESS);
    return;
    
error:
    reset_sign_state();
    small_resp(CMD_ACK_INVALID);
}

static unsigned process_header(const uint8_t *cd, const unsigned bytes, unsigned * const ateBytes)
{
    unsigned proc_bytes  = 0;
    unsigned copy_bytes = 0;
    

    if (ptx->fieldByte == 0)
    {
        // The first 2 bytes of the header must come together to know how long the rest is
        assert(bytes >= 2);

        assert(0 != cmdi.signi.totalInputs);
        assert(0 == ptx->headerEntries);
        assert(0 == ptx->fieldByte);
        assert(0 == ptx->fieldTotalBytes);
        assert(0 == ptx->inputsLeft);
        assert(0 == ptx->outputsTotal);

        // Checking the command bytes
        assert(cd[0] == CMD_PREV_TX);
        proc_bytes += 1;
        
        // Get the number of entries in the header
        ptx->headerEntries = cd[1];
        proc_bytes += 1;

        // For the header, fieldTotalBytes represents header bytes only
        ptx->fieldTotalBytes = ptx->headerEntries * sizeof(prevTxHeader_t);

        copy_bytes = min(bytes - 2, ptx->fieldTotalBytes);
    }
    else
    {
        copy_bytes = min(bytes, ptx->fieldTotalBytes - ptx->fieldByte);
    }
        
    memcpy(&ptx->header[ptx->fieldByte], &cd[proc_bytes], copy_bytes);

    ptx->fieldByte += copy_bytes;
    proc_bytes     += copy_bytes;

    *ateBytes = proc_bytes;

    if (ptx->fieldByte == ptx->fieldTotalBytes)
    {
        // Done
        ptx->fieldByte = 0;
        return true;
    }

    return false;
}

static unsigned process_skip(const uint8_t *cd, const unsigned bytes, unsigned * const ateBytes, const unsigned totalBytes)
{
    unsigned procBytes = 0;

    if (0 == ptx->fieldByte)
    {
        // Start
        ptx->fieldTotalBytes = totalBytes;
    }
        
    procBytes = min(bytes, ptx->fieldTotalBytes - ptx->fieldByte);
    ptx->fieldByte += procBytes;
    
    *ateBytes = procBytes;
    
    if (ptx->fieldByte == ptx->fieldTotalBytes)
    {
        // Done
        ptx->fieldByte = 0;        
        return true;
    }
    
    return false;
}    

static unsigned getVarIntBytes(uint8_t firstByte)
{
    if      (firstByte <  0xFD)  return 1;
    else if (firstByte == 0xFD)  return 3;
    else if (firstByte == 0xFE)  return 5;
    else                         return 9;

}

static unsigned process_var_int(const uint8_t *cd, const unsigned bytes, unsigned * const ateBytes, void *dest)
{
    unsigned procBytes = 0, cdPos = 0;
    uint8_t *d = dest;

    if (0 == ptx->fieldByte)
    {
        ptx->fieldTotalBytes = getVarIntBytes(cd[0]);
        
        if (ptx->fieldTotalBytes == 1)
        {
            d[0] = cd[cdPos];
        }
        
        // We've processed the first byte at least (and likely will be the last)
        ptx->fieldByte += 1;
        procBytes      += 1;
        cdPos          += 1;
    }
    
    procBytes += min(bytes, ptx->fieldTotalBytes - ptx->fieldByte);

    if (procBytes > 1)
    {
        //
        // This case will always be when the varint is > 1 byte
        // Therefore, we want to ignore the first field byte when copying
        // in data.
        //
        assert(ptx->fieldByte >= 1);
        
        memcpy(&d[ptx->fieldByte - 1], &cd[cdPos], procBytes);
        ptx->fieldByte += procBytes;    
    }
    
    *ateBytes = procBytes;
    
    if (ptx->fieldByte == ptx->fieldTotalBytes)
    {
        ptx->fieldByte = 0;
        return true;
    }
    
    return false;
}


static unsigned process_field(const uint8_t *cd, const unsigned bytes, unsigned * const ateBytes, void *dest, const unsigned totalBytes)
{
    unsigned procBytes = 0;
    uint8_t *d = dest;

    if (0 == ptx->fieldByte)
    {
        // Start
        ptx->fieldTotalBytes = totalBytes;
    }
        
    procBytes += min(bytes, ptx->fieldTotalBytes - ptx->fieldByte);
      
    memcpy(&d[ptx->fieldByte], cd, procBytes);

    ptx->fieldByte += procBytes;
    *ateBytes = procBytes;
    
    if (ptx->fieldByte == ptx->fieldTotalBytes)
    {
        // Done
        ptx->fieldByte = 0;
        return true;
    }
    
    return false;
}


static bool save_curr_output(const uint64_t outputIdx, uint8_t * const inputIdx)
{
    unsigned i;
    
    for (i = 0; i < ptx->headerEntries; i++)
    {
        if (ptx->header[i].outputIdx == outputIdx)
        {
            *inputIdx = ptx->header[i].inputIdx;
            return true;
        }
    }
    
    *inputIdx = 0;
    return false;
}


static void prev_tx(const cmd_t *cmd, unsigned chunk_bytes, unsigned total_bytes, bool start)
{
    uint8_t   *cd       = (uint8_t*) cmd;
    unsigned   ateBytes = 0;
    unsigned   i; 
    bool       done     = false;
    
    // Parameter checking
    if (chunk_bytes > STREAM_CHUNK_BYTES)                          goto error;
    if (cmdi.signi.sign_flow != SIGN_FLOW_PREV_TX)                 goto error;
    if (ptx->field == PTX_HEADER && ptx->fieldByte == 0 && !start) goto error;

    //
    // This function is called many times when processing the prev tx chunks
    // The state can change mid function, therefore the if statements below
    // are all checked individually (no if else)
    //

    while (chunk_bytes > 0)
    {
        switch (ptx->field)
        {
            case PTX_HEADER:
                if (process_header(cd, chunk_bytes, &ateBytes))
                {
                    ptx->field = PTX_VERSION;
                }                    
                break;
                
            case PTX_VERSION:
                if (process_skip(cd, chunk_bytes, &ateBytes, PTX_VERSION_B))
                {
                    ptx->field = PTX_IN_COUNT;
                }
                
                //
                // Turn on hash calculations at the version - may get set a couple times
                // if the version is split across chunks, but thats not as big deal.
                //
                ptx->hashCalc = true;
                break;
                     
            case PTX_IN_COUNT:
                if (process_var_int(cd, chunk_bytes, &ateBytes, &ptx->inputsLeft))
                {
                    ptx->field = PTX_IN_PREV_HASH;
                }
                break;
                
            case PTX_IN_PREV_HASH:
                if (process_skip(cd, chunk_bytes, &ateBytes, PTX_IN_PREV_HASH_B))
                {
                    ptx->field = PTX_IN_PREV_INDEX;
                }
                break;
                
            case PTX_IN_PREV_INDEX:
                if (process_skip(cd, chunk_bytes, &ateBytes, PTX_IN_PREV_INDEX_B))
                {
                    ptx->field = PTX_IN_SCRIPT_LEN;
                }                    
                break;
                
            case PTX_IN_SCRIPT_LEN:
                if (process_var_int(cd, chunk_bytes, &ateBytes, &ptx->inputScriptBytes))
                {
                    ptx->field = PTX_IN_SCRIPT;
                }
                break;
        
            case PTX_IN_SCRIPT:
                if (process_skip(cd, chunk_bytes, &ateBytes, ptx->inputScriptBytes))
                {
                    ptx->field = PTX_IN_SEQUENCE;
                }
                break;
                
            case PTX_IN_SEQUENCE:
                if (process_skip(cd, chunk_bytes, &ateBytes, PTX_IN_SEQUENCE_B))
                {
                    ptx->inputsLeft--; 
                    ptx->field = ptx->inputsLeft > 0 ? PTX_IN_PREV_HASH : PTX_OUT_COUNT;
                }                    
                break;

            case PTX_OUT_COUNT:
                if (process_var_int(cd, chunk_bytes, &ateBytes, &ptx->outputsTotal))
                {
                    ptx->outputIndex = 0;
                    ptx->outputSave = save_curr_output(ptx->outputIndex, &ptx->outputSaveToIndex);
                    ptx->field = PTX_OUT_VALUE;
                }
                break;

            case PTX_OUT_VALUE:
                if (ptx->outputSave)
                {
                    if (ptx->outputSaveToIndex > cmdi.signi.totalInputs) goto error;
                    
                    if (process_field(cd,
                                     chunk_bytes,
                                     &ateBytes, 
                                     &cmdi.signi.input[ptx->outputSaveToIndex].valueSatoshis, 
                                     PTX_OUT_VALUE_B))
                    {
                        ptx->field = PTX_OUT_SCRIPT_LEN;
                    }

                    if (cmdi.signi.input[ptx->outputSaveToIndex].valueSatoshis > MAX_SATOSHIS)  goto error;
                }
                else
                {
                    if (process_skip(cd, chunk_bytes, &ateBytes, PTX_OUT_VALUE_B))
                    {
                        ptx->field = PTX_OUT_SCRIPT_LEN;
                    }
                    
                }                    
                break;

            case PTX_OUT_SCRIPT_LEN:
                if (process_var_int(cd, chunk_bytes, &ateBytes, &ptx->outputScriptBytes))
                {
                    ptx->field = PTX_OUT_SCRIPT;
                }
                break;

            case PTX_OUT_SCRIPT:
                if (ptx->outputSave)
                {
                    if (ptx->outputSaveToIndex > cmdi.signi.totalInputs) goto error;
                    if (ptx->outputScriptBytes != PK_SCRIPT_BYTES)       goto error;

                    if (process_field(cd,
                                     chunk_bytes,
                                     &ateBytes,
                                     &cmdi.signi.input[ptx->outputSaveToIndex].prevTxPkScript,
                                     ptx->outputScriptBytes))
                    {
                        // Also save off additional details on this transaction
                        cmdi.signi.input[ptx->outputSaveToIndex].prevTxOutputIndex   = ptx->outputIndex;
                        cmdi.signi.input[ptx->outputSaveToIndex].prevTxPkScriptBytes = ptx->outputScriptBytes;

                        ptx->outputIndex++;
                        
                        if (ptx->outputIndex == ptx->outputsTotal)
                        {
                            ptx->field = PTX_LOCK_TIME;
                        }
                        else
                        {
                            ptx->outputSave = save_curr_output(ptx->outputIndex, &ptx->outputSaveToIndex);
                            ptx->field = PTX_OUT_VALUE;
                        }                        
                    }
                }
                else
                {
                    if (process_skip(cd, chunk_bytes, &ateBytes, ptx->outputScriptBytes))
                    {
                        ptx->outputIndex++;
                        
                        if (ptx->outputIndex == ptx->outputsTotal)
                        {
                            ptx->field = PTX_LOCK_TIME;
                        }
                        else
                        {
                            ptx->outputSave = save_curr_output(ptx->outputIndex, &ptx->outputSaveToIndex);
                            ptx->field = PTX_OUT_VALUE;
                        }
                    }
                
                }
                break;

            case PTX_LOCK_TIME:
                if (process_skip(cd, chunk_bytes, &ateBytes, PTX_LOCK_TIME_B))
                {                   
                    if (0 != ateBytes)
                    {
                        done = true;
                        small_resp(CMD_ACK_SUCCESS);
                    }
                    
                }
                break;

            default:
                ateBytes = 0;
                break;
        }
        
        //
        // Either a processing function encountered an error, or there 
        // was more data to process than we expected
        //
        if (0 == ateBytes) goto error;
        
        // Calculate hash on the chunk eaten?
        if (true == ptx->hashCalc)
        {
            crypto_sha256_input(ptx->hashCtx, cd, ateBytes);
        }
        
        // See if we're done processing this prev tx
        if (done)
        {
            // Finish out the hash and copy the results to the various inputs
            crypto_sha256_result (ptx->hashCtx, cmdi.signi.input[ptx->header[0].inputIdx].prevTxHash);
            crypto_sha256_reset  (&ptx->hashCtx);
            crypto_sha256_input  (ptx->hashCtx, cmdi.signi.input[ptx->header[0].inputIdx].prevTxHash, TX_HASH_BYTES);
            crypto_sha256_result (ptx->hashCtx, cmdi.signi.input[ptx->header[0].inputIdx].prevTxHash);

            cmdi.signi.inputsLeftToVerify--;
            
            for (i = 1; i < ptx->headerEntries; i++)
            {
                memcpy(cmdi.signi.input[ptx->header[i].inputIdx].prevTxHash, 
                       cmdi.signi.input[ptx->header[0].inputIdx].prevTxHash,
                       TX_HASH_BYTES);
                
                cmdi.signi.inputsLeftToVerify--;
            }

            if (cmdi.signi.inputsLeftToVerify < 0)  goto error;

            if (cmdi.signi.inputsLeftToVerify == 0)
            {
                // This is the last one, transition the state
                cmdi.signi.sign_flow = SIGN_FLOW_SIGN_TX_READY;
            }
            
            reset_ptx_state();        
        }
        
        cd         += ateBytes;
        chunk_bytes -= ateBytes;
    }
    
    return;
    
error:
    reset_sign_state();
    small_resp(CMD_ACK_INVALID);
}


static unsigned output_script(const uint8_t *hash160, uint8_t *script)
{
    // Set the script length and stack push ops
    *script++ = 0x19;
    *script++ = 0x76;
    *script++ = 0xa9;
    *script++ = 0x14;
   
    // The output address in hash 160 form
    memcpy(script, hash160, HASH_ADDR_BYTES);
    script += HASH_ADDR_BYTES;
    
    // Set the script calculation bytes
    *script++ = 0x88;
    *script++ = 0xac;
    
    return 4 + HASH_ADDR_BYTES + 2;
}

uint64_t calc_fee()
{
    unsigned i;
    uint64_t input_satoshis = 0;

    for (i = 0; i < cmdi.signi.totalInputs; i++)
    {
        input_satoshis += cmdi.signi.input[i].valueSatoshis;

        if (input_satoshis > MAX_SATOSHIS)  return MAX_SATOSHIS + 1;
    }

    if (input_satoshis < cmdi.signi.output_satoshis + cmdi.signi.change_satoshis)  return MAX_SATOSHIS + 1;

    return input_satoshis - cmdi.signi.output_satoshis - cmdi.signi.change_satoshis;
}

unsigned build_tx(const bool forSigning, const unsigned inputIdx, uint8_t *dest)
{
    unsigned i;
    uint8_t *tx = dest;

    // Version
    *tx++ = 0x01;
    *tx++ = 0x00;
    *tx++ = 0x00;
    *tx++ = 0x00;
    
    // Input count - no need to do varint as the supported inputs < 0xFD  
    assert(cmdi.signi.totalInputs < 0xFD);
    *tx++ = cmdi.signi.totalInputs;
    
    // Inputs
    for (i = 0; i < cmdi.signi.totalInputs; i++)
    {
        // Prev tx hash
        memcpy_r(tx, &cmdi.signi.input[i].prevTxHash, TX_HASH_BYTES);
        tx += TX_HASH_BYTES;
        
        // Prev tx output index
        memcpy(tx, &cmdi.signi.input[i].prevTxOutputIndex, 4);
        tx += 4;
       
        if (forSigning)
        {
            // For signing, use the previous output script or 0x00 for the signature slot            
            if (i == inputIdx)
            {
                // Prev tx output script
                *tx++ = cmdi.signi.input[i].prevTxPkScriptBytes;
                memcpy(tx, &cmdi.signi.input[i].prevTxPkScript, PK_SCRIPT_BYTES);
                tx += PK_SCRIPT_BYTES;
            }
            else
            {
                // One byte of zeroes 
                *tx++ = 0x00;
            }
        }
        else
        {
            // For full tx, use the signature and compressed public key
            
            // Total signature script bytes
            *tx++ = cmdi.signi.input[i].sigBytes + COMPRESS_KEY_BYTES + 2;
            
            // Signature bytes
            *tx++ = cmdi.signi.input[i].sigBytes;
            
            // Signature
            memcpy(tx, cmdi.signi.input[i].sig, cmdi.signi.input[i].sigBytes);
            tx += cmdi.signi.input[i].sigBytes;
            
            // Compressed public key bytes
            *tx++ = COMPRESS_KEY_BYTES;
            
            // Compressed public key
            memcpy(tx, cmdi.signi.input[i].pubKeyCompress, COMPRESS_KEY_BYTES);
            tx += COMPRESS_KEY_BYTES;
        }
        
        // Sequence
        memset(tx, 0xff, 4);
        tx += 4;
    }

    // Output count - one real output, and potentially one change
    if (cmdi.signi.giveChange)  *tx++ = 0x02;
    else                        *tx++ = 0x01;
    
    // Output - to destination
    memcpy(tx, &cmdi.signi.output_satoshis, 8);
    tx += 8;
    
    tx += output_script(cmdi.signi.output_addr, tx);
    
    // Output - change
    if (cmdi.signi.giveChange)
    {
        memcpy(tx, &cmdi.signi.change_satoshis, 8);
        tx += 8;
        
        tx += output_script(cmdi.signi.change_addr, tx);
    }

    // Lock time
    memset(tx, 0x00, 4);
    tx += 4;

    if (forSigning)
    {
        // Hash type field add-on (SIGHASH_ALL)
        *tx++ = 0x01;
        *tx++ = 0x00;
        *tx++ = 0x00;
        *tx++ = 0x00;
    }
    
    assert(tx - dest <= SIGNED_TX_MAX_BYTES);
    return tx - dest;
}    
        
static unsigned sign_tx_for_input(const uint8_t inputIdx, uint8_t *tx)
{
    unsigned txBytes;
    uint8_t  hash[SHA256_BYTES];
    uint8_t  privKey[KEY_BYTES];

    // Build up the tx for this input to sign
    txBytes = build_tx(true, inputIdx, tx);
     
    // Calculate the hash, a double SHA 256
    crypto_sha256(tx, txBytes, hash);
    crypto_sha256(hash, SHA256_BYTES, hash);

    // Sign the tx
    wallet_address_private_key_get(cmdi.signi.input[inputIdx].account,
                                   cmdi.signi.input[inputIdx].chain,
                                   cmdi.signi.input[inputIdx].keyid,
                                   privKey,
                                   NULL);

    return crypto_ecdsa_sign(privKey, hash, cmdi.signi.input[inputIdx].sig, &cmdi.signi.input[inputIdx].sigBytes);
}


void fw_download(const cmd_t *cmd, const unsigned chunk_bytes, const unsigned total_bytes, bool start)
{
    // The command is actually a data stream
    uint8_t *d = (uint8_t*) cmd;
    bool done;

    if (start)
    {
        assert(d[0] == CMD_FW_DOWNLOAD);

        if (total_bytes > MAIN_HEADER_BYTES + MAIN_BYTES)  goto error;

        reset_fwdl_state();
        cmdi.fwdl.total_bytes = total_bytes - 1; // Do not include the command byte

        done = fw_write_chunk(&d[1], chunk_bytes - 1);
    }
    else
    {
        done = fw_write_chunk(d, chunk_bytes);
    }

    if (done)
    {
        // Verify the downloaded FW image
        if (!fw_verify())  goto error;

        // Erase the main FW header to force an update next reboot
        FlashMainPageErase(MAIN_HEADER_BASE);

        small_resp(CMD_ACK_SUCCESS);
    }

    return;

error:
    reset_fwdl_state();
    small_resp(CMD_ACK_INVALID);
}

bool fw_write_chunk(const uint8_t *d, unsigned bytes)
{
    unsigned copy_bytes;
    unsigned copy_offset = 0;

    while (bytes > 0)
    {
        copy_bytes = min(bytes, FW_WRITE_CHUNK - cmdi.fwdl.chunk_offset);

        memcpy(&cmdi.fwdl.chunk[cmdi.fwdl.chunk_offset], &d[copy_offset], copy_bytes);

        bytes                  -= copy_bytes;
        copy_offset            += copy_bytes;
        cmdi.fwdl.chunk_offset += copy_bytes;
        cmdi.fwdl.rx_bytes     += copy_bytes;

        assert(cmdi.fwdl.chunk_offset <= FW_WRITE_CHUNK);

        if (cmdi.fwdl.chunk_offset == FW_WRITE_CHUNK ||
            cmdi.fwdl.rx_bytes  == cmdi.fwdl.total_bytes)
        {
            // We've filled up a chunk, push it down
            if (cmdi.fwdl.write_offset % 0x800 == 0)
            {
                FlashMainPageErase(DOWNLOAD_HEADER_BASE + cmdi.fwdl.write_offset);
            }

            FlashMainPageProgram((uint32_t*)cmdi.fwdl.chunk, DOWNLOAD_HEADER_BASE + cmdi.fwdl.write_offset, FW_WRITE_CHUNK);

            cmdi.fwdl.write_offset += FW_WRITE_CHUNK;
            cmdi.fwdl.chunk_offset = 0;
        }
    }

    if (cmdi.fwdl.write_offset >= cmdi.fwdl.total_bytes)
    {
        return true;
    }

    return false;
}

bool fw_verify(void)
{
    // TODO flesh this out completely
    fw_header_t *hdr = (fw_header_t *) DOWNLOAD_HEADER_BASE;

    if (hdr->f.id == STANDARD_ID)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static void get_signed_tx(const cmd_t *cmd, const unsigned total_bytes)
{
    // Parameter checking
    if (cmd->cmd != CMD_GET_SIGNED_TX)                   goto error;
    if (total_bytes != SMALL_BYTES)                      goto error;
    if (cmdi.signi.sign_flow < SIGN_FLOW_SIGN_TX_READY)  goto error;


    switch (cmdi.signi.sign_flow)
    {
        case SIGN_FLOW_SIGN_TX_READY:

            if (cp_state() == CP_IDLE)
            {
                // Latch the buffer to use for signing. I don't really like this.
                cmdi.signi.txbuff = (uint8_t*)&cmdi.resp->signedtx;

                cp_set_cmd(CP_CMD_SIGN);
                small_resp(CMD_ACK_BUSY);
            }
            else if (cp_state() == CP_BUSY)  small_resp(CMD_ACK_BUSY);
            else if (cp_state() == CP_USER)  small_resp(CMD_ACK_USER);
            else    assert(0);
            break;

        case SIGN_FLOW_SIGN_TX_DENIED:

            small_resp(CMD_ACK_DENIED);
            break;

        case SIGN_FLOW_SIGN_TX_ERROR:

            small_resp(CMD_ACK_INVALID);
            break;

        case SIGN_FLOW_SIGN_TX_DONE:

            // Build the final fully signed tx
            cmdi.respBytes = build_tx(false, 0, cmdi.resp->signedtx);
            cmdi.respBytes++; // Account for the command byte
            cmdi.resp->cmd = CMD_ACK_SUCCESS;
            break;

        default:
            assert(0);
            break;
    }

    return;

error:
    reset_sign_state();
    small_resp(CMD_ACK_INVALID);
}

static void get_public_key(const cmd_t *cmd, const unsigned total_bytes)
{
    // Parameter checking
    if (cmd->cmd != CMD_GET_PUBLIC_KEY)      goto error;
    if (total_bytes != GET_PUBLIC_KEY_BYTES) goto error;
    if (cmd->pk.type >= KEY_TYPE_INVALID)    goto error;
    if (cmd->pk.account > ACCT_MAX)          goto error;
    if (cmd->pk.chain > CHAIN_MAX)           goto error;
    
    wallet_public_key_get(cmd->pk.type,
                          cmd->pk.account,
                          cmd->pk.chain,
                          cmd->pk.address,
                          cmdi.resp->pk.pubkey_x,
                          cmdi.resp->pk.pubkey_y,
                          cmdi.resp->pk.chaincode);
    
    cmdi.resp->cmd = CMD_ACK_SUCCESS;
    cmdi.respBytes = GET_PUBLIC_KEY_RESP_BYTES;

    return;

error:
    small_resp(CMD_ACK_INVALID);
}

static void set_master_seed(const cmd_t *cmd, const unsigned total_bytes)
{
    // Parameter checking
    if (cmd->cmd != CMD_SET_MASTER_SEED)         goto error;
    if (total_bytes > SET_MASTER_SEED_MAX_BYTES) goto error;
    
    // Strip off the command itself from the command bytes field
    wallet_master_seed_set(cmd->seed.wordlist, total_bytes - 1);

    small_resp(CMD_ACK_SUCCESS);
    return;
    
error:
    small_resp(CMD_ACK_INVALID);
}
