#include <string.h>
#include <core.h>
#include <assert.h>
#include <cmd.h>
#include <util.h>
#include <screen.h>

// Defines

#define CTRL_READY        0x00
#define CTRL_START        0x80
#define CTRL_CONT         0x88
#define CTRL_START_STREAM 0xC0
#define CTRL_CONT_STREAM  0xC8

#define POS_CTRL  0
#define POS_BYTES 1

#define START_HEADER_BYTES 5  // CTRL_START + 4 bytes length
#define CONT_HEADER_BYTES  1  // CTRL_CONT

#define CLEAR_OUT_PACKET    memset(outpacket,  0, PACKET_BYTES)
#define CLEAR_IN_PACKET     memset(inpacket, 0, PACKET_BYTES)

#define SEND_CHUNK_THRESHOLD  PACKET_BYTES

// Types
typedef struct
{
    // Inbound
    cmd_t       incmd;
    int         inBytesTotal;
    int         inBytesLeft;
    int         inBytePos;
    uint8_t     inExpectedCtrl;
    bool        inStreaming;

    // Outbound
    cmdResp_t   outcmd;
    int         outBytesTotal;
    int         outBytesLeft;
    int         outRespPos;
    resp_e      outPending;
} parseInfo_t;

// Prototypes
static void reset_incmd (void);
static void fill_incmd  (uint8_t* src, int bytes);

// Globals
parseInfo_t     pi;
static uint8_t  outpacket[PACKET_BYTES];


void cmdparser_init(void)
{
	memset(&pi, 0x00, sizeof(pi));
}


bool core_inpacket(uint8_t *inpacket)
{
    int inbytes;
    int ctrl = inpacket[POS_CTRL];

    // Only allow new full commands if a response is not pending
    if (pi.outPending == RESP_NONE)
    {
        switch (ctrl)
        {
            case CTRL_START:
            case CTRL_START_STREAM:
                screen_rx(LIGHT_ON);

                pi.inBytesTotal = inpacket[POS_BYTES] +
                                  (inpacket[POS_BYTES + 1] << 8) +
                                  (inpacket[POS_BYTES + 2] << 16) +
                                  (inpacket[POS_BYTES + 3] << 24);
                pi.inBytesLeft  = pi.inBytesTotal;
                pi.inBytePos    = 0;

                // Figure out how many bytes to copy
                inbytes = min(pi.inBytesLeft, PACKET_BYTES - START_HEADER_BYTES);

                pi.inExpectedCtrl = ctrl == CTRL_START ? CTRL_CONT : CTRL_CONT_STREAM;
                pi.inStreaming    = ctrl == CTRL_START_STREAM ? true : false;

                // Copy from packet, skipping POS_CTRL and POS_BYTES
                fill_incmd(&inpacket[START_HEADER_BYTES], inbytes);

                break;

            case CTRL_CONT:
            case CTRL_CONT_STREAM:
                if (pi.inExpectedCtrl != CTRL_CONT &&
                    pi.inExpectedCtrl != CTRL_CONT_STREAM)
                {
                    goto error;
                }

                inbytes = min(PACKET_BYTES - CONT_HEADER_BYTES, pi.inBytesLeft);

                // Copy from packet, skipping POS_CTRL
                fill_incmd(&inpacket[CONT_HEADER_BYTES], inbytes);

                break;

            // Unknown control character or generally bad situation
            error:
            default:
                reset_incmd();
                break;
        }
    }

    CLEAR_IN_PACKET;

    if (pi.inBytesLeft == 0)
    {
        screen_rx(LIGHT_OFF);
    }

    return false;
}


void reset_incmd(void)
{
    //
    // Typically the command handler will wipe away command contents,
    // however when this function is call the command handler is not
    // invoked, so it must be cleared here.
    //
    memset(&pi.incmd, 0, sizeof(pi.incmd));

    pi.inBytesTotal   = 0;
    pi.inBytesLeft    = 0;
    pi.inBytePos      = 0;
    pi.inExpectedCtrl = 0;

    screen_rx(LIGHT_OFF);
}


void fill_incmd(uint8_t* src, int bytes)
{
    bool start;
    uint8_t *in = (uint8_t *) &pi.incmd;

    if (bytes + pi.inBytePos > (int)sizeof(cmd_t))
    {
        // Overflowing buffer, do not copy and reset command state
        reset_incmd();
    }
    else if (pi.inBytesLeft - bytes < 0)
    {
        // Byte count mismatch, do not copy and reset command state
        reset_incmd();
    }
    else
    {
        // Everything checks out, copy over
        memcpy(&in[pi.inBytePos], src, bytes);

        start = pi.inBytesLeft == pi.inBytesTotal;

        pi.inBytesLeft -= bytes;

        if (pi.inStreaming)
        {
            pi.outPending = cmd_handler(&pi.incmd, bytes, pi.inBytesTotal, pi.inStreaming, start, &pi.outcmd, &pi.outBytesTotal);
        }
        else
        {
            // Regular commands wait to collect everything before invoking
            // the command handler.
            if (pi.inBytesLeft == 0)
            {
                // Everything is collected, call the command handler - we always expect a response
                pi.outPending = cmd_handler(&pi.incmd, pi.inBytesTotal, pi.inBytesTotal, pi.inStreaming, start, &pi.outcmd, &pi.outBytesTotal);
            }
            else
            {
                assert(pi.inBytesLeft > 0);
                pi.inBytePos += bytes;
            }
        }
    }
}


bool core_outpacket_ready(void)
{
    return pi.outPending != RESP_NONE;
}


uint8_t* core_outpacket(void)
{
    uint8_t *outbytes = (uint8_t *)&pi.outcmd;
    uint8_t *ret      = NULL;
    int      bytes    = 0;

    CLEAR_OUT_PACKET;

    if (RESP_NEW == pi.outPending)
    {
        screen_tx(LIGHT_ON);

    	pi.outBytesLeft = pi.outBytesTotal;
    	pi.outRespPos   = 0;

    	// Set up the first response
    	outpacket[POS_CTRL]      = CTRL_START;
    	outpacket[POS_BYTES]     = pi.outBytesTotal & 0xFF;
    	outpacket[POS_BYTES + 1] = pi.outBytesTotal >> 8;
        outpacket[POS_BYTES + 2] = pi.outBytesTotal >> 16;
        outpacket[POS_BYTES + 3] = pi.outBytesTotal >> 24;

    	bytes = min(pi.outBytesLeft, PACKET_BYTES - START_HEADER_BYTES);

    	memcpy(&outpacket[START_HEADER_BYTES], &outbytes[pi.outRespPos], bytes);

    	pi.outBytesLeft -= bytes;
    	pi.outRespPos   += bytes;

    	ret = outpacket;
    }
    else if (RESP_CONT == pi.outPending)
    {
        outpacket[POS_CTRL] = CTRL_CONT;

        bytes = min(pi.outBytesLeft, PACKET_BYTES - CONT_HEADER_BYTES);

        memcpy(&outpacket[CONT_HEADER_BYTES], &outbytes[pi.outRespPos], bytes);

        pi.outBytesLeft -= bytes;
        pi.outRespPos   += bytes;

        ret = outpacket;
    }

    if (pi.outBytesLeft > 0)
    {
    	pi.outPending = RESP_CONT;
    }
    else
    {
        screen_tx(LIGHT_OFF);
    	pi.outPending = RESP_NONE;
    }

    return ret;
}
