#include <assert.h>
#include <string.h>
#include <core.h>
#include <cmdparser.h>
#include <crypto.h>
#include <wallet.h>
#include <store.h>
#include <screen.h>

// States
typedef enum
{
    STATE_START,
    STATE_LOAD_CTX,
    STATE_GENERATE_SEED,
    STATE_GENERATE_PIN,
    STATE_ENTER_PIN,

    STATE_READY,

    STATE_GATHER_PREV_TX,
    STATE_READY_TO_SIGN,
    STATE_SIGN,

    STATE_NULL,
    STATE_MAX,
} core_state_e;

// This transition table validates legal state transitions
const core_state_e transitions[STATE_MAX][2] = {

/* STATE_START          */ { STATE_LOAD_CTX       , STATE_NULL,          },
/* STATE_LOAD_CTX       */ { STATE_ENTER_PIN      , STATE_GENERATE_SEED, },
/* STATE_GENERATE_SEED  */ { STATE_GENERATE_PIN   , STATE_NULL,          },
/* STATE_GENERATE_PIN   */ { STATE_ENTER_PIN      , STATE_NULL,          },
/* STATE_ENTER_PIN      */ { STATE_READY          , STATE_ENTER_PIN,     },
/* STATE_READY          */ { STATE_GATHER_PREV_TX , STATE_NULL,          },
/* STATE_GATHER_PREV_TX */ { STATE_READY_TO_SIGN  , STATE_READY,         },
/* STATE_READY_TO_SIGN  */ { STATE_SIGN           , STATE_READY,         },
/* STATE_SIGN           */ { STATE_READY          , STATE_NULL,          },
/* STATE_NULL           */ { STATE_NULL           , STATE_NULL,          },

};


static core_state_e cs;

inline newstate(core_state_e state)
{
    assert(transitions[cs][0] == state ||
           transitions[cs][1] == state);

    cs = state;
}

void core_init(void)
{
    cs = STATE_START;

	cmdparser_init();
    wallet_init();
}

bool core_unlocked(void)
{
    return cs >= STATE_READY;
}

void core_load_ctx(void)
{
    store_status_e sts;
    bool failed = false;

    newstate(STATE_LOAD_CTX);

    //store_wipe();

    if (store_present())
    {
        do
        {
            core_enter_pin(failed);
            sts = store_load(STORE_KEYSLOT);

            // TODO pin entry delay
            failed = true;

        } while (STORE_OK != sts);
    }
    else
    {
        core_generate_seed();
        core_generate_pin();

        store_create(STORE_KEYSLOT);

        do
        {
            core_enter_pin(failed);
            sts = store_load(STORE_KEYSLOT);

            // TODO pin entry delay
            failed = true;

        } while (STORE_OK != sts);
    }
}

void core_enter_pin(bool failed)
{
    newstate(STATE_ENTER_PIN);

    screen_enter_pin(failed);
}

void core_generate_pin(void)
{
    newstate(STATE_GENERATE_PIN);

    screen_generate_pin();
}

void core_generate_seed(void)
{
    newstate(STATE_GENERATE_SEED);

    screen_generate_seed();
}

void core_ready(void)
{
    newstate(STATE_READY);

    screen_idle();
}

void     core_gather_prevtx (void);
void     core_ready_to_sign (void);
bool     core_sign          (void);
void     core_sign_progress (void);
