#include <screen.h>
#include <pixel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <touch.h>
#include <words.h>
#include <wallet.h>
#include <assert.h>
#include <ctype.h>
#include <crypto.h>
#include <store.h>
#include <stdbool.h>
#include <cmd.h>
#include <util.h>

#define DEBUG_CHARSET         0
#define DEBUG_PIN             0
#define SKIP_CONFIRMATION     0

#define BANNER_LINE_START     0
#define BANNER_LINES          16
#define BANNER_TXT_LINE       2
#define BANNER_COL_START      0
#define BANNER_COLS           SCREEN_WIDTH_BITS

#define BUTTON_LINE_START     115
#define BUTTON_LINES          13
#define BUTTON_TXT_LINE       117

#define TX_OFFSET_BITS        118
#define RX_OFFSET_BITS        123

#define PROGRESS_COL_START    9
#define PROGRESS_COLS         108

#define MAX_PIN_WORDS         3


typedef struct
{
    char     word       [WORD_LEN_MAX + 1];
    uint8_t  press_hist [WORD_LEN_MAX];
    unsigned press_idx;
} word_data_t;

static void screen_clear   (void);
static void satoshi_to_str (char str[14], uint64_t satoshis, unsigned max_chars);
static void button_slider  (const char *left, const char *right, const uint8_t bits);

static light_e rx;
static light_e tx;
static light_e rx_screen;
static light_e tx_screen;


void satoshi_to_str(char str[14], uint64_t satoshis, unsigned max_chars)
{
    unsigned sig_decimal, i;
    char tmp[14];

    // BTC outputs
    sprintf(str, "$%lld", satoshis / 100000000);
    sprintf(tmp, ".%08lld", satoshis % 100000000);

    // Calculate how many significant decimals there are
    sig_decimal = 2;

    for(i = 1; i < strlen(tmp); i++)
    {
        if (tmp[i] != '0')
        {
            sig_decimal = i + 1;
        }
    }

    if (sig_decimal < max_chars - strlen(str))
    {
        strncat(&str[strlen(str)], tmp, sig_decimal);
    }
    else
    {
        strncat(&str[strlen(str)], tmp, max_chars - strlen(str));
    }

}

void screen_init(void)
{
    rx        = LIGHT_OFF;
    tx        = LIGHT_OFF;
    rx_screen = LIGHT_OFF;
    tx_screen = LIGHT_OFF;
}

void screen_clear(void)
{
    pixel_clear();
}

void screen_banner(char str[])
{
    rx_screen = LIGHT_OFF;
    tx_screen = LIGHT_OFF;

    pixel_rect(BANNER_LINE_START, BANNER_LINES, BANNER_COL_START, BANNER_COLS);
    pixel_str(str, BANNER_TXT_LINE, CENTER);

    screen_rx(rx);
    screen_tx(tx);
}


void screen_rx(light_e state)
{
    rx = state;

    if      (LIGHT_ON  == rx && LIGHT_OFF == rx_screen)  goto write;
    else if (LIGHT_OFF == rx && LIGHT_ON  == rx_screen)  goto write;
    else    return;

write:
    rx_screen = state;
    pixel_str("~", 0, RX_OFFSET_BITS);
}


void screen_tx(light_e state)
{
    tx = state;

    if      (LIGHT_ON  == tx && LIGHT_OFF == tx_screen)  goto write;
    else if (LIGHT_OFF == tx && LIGHT_ON  == tx_screen)  goto write;
    else    return;

write:
    tx_screen = state;
    pixel_str("@", 0, TX_OFFSET_BITS);
}


void screen_idle()
{
    screen_clear();
    screen_banner("Polly");

#if DEBUG_CHARSET

    pixel_str("$0123456789(+-", 22, CENTER);
    pixel_str("AaBbCcDdEeFf" , 34, CENTER);
    pixel_str("GgHhIiJjKkLl" , 46, CENTER);
    pixel_str("MmNnOoPpQqRr" , 58, CENTER);
    pixel_str("SsTtUuVvWwXx" , 70, CENTER);
    pixel_str("YyZz" , 82, CENTER);

#endif
}


void button_slider(const char *left, const char *right, const uint8_t bits)
{
    unsigned bit_start;

    pixel_font(5);

    bit_start = (SCREEN_WIDTH_BITS - bits) / 2;

    pixel_rect(BUTTON_LINE_START, BUTTON_LINES, bit_start, bits);
    pixel_str(left,  BUTTON_LINE_START + 3, bit_start + 4);
    pixel_str(right, BUTTON_LINE_START + 3, bits + bit_start - pixel_str_width(right) - 2);

    pixel_font(8);
}


void button(const char *str, const uint8_t bit, const uint8_t bits)
{
    unsigned str_bits, str_lines, str_bit_off, str_line_off, box_bit;

    pixel_font(5);

    box_bit = bit;
    str_bits = pixel_str_width(str);

    // Adjustments

    // Take away the trailing space to center properly
    str_bits -= 2;

    // Take away the descender, using all caps
    str_lines = 9 - 2;

    assert(str_bits < bits);
    assert(str_lines < BUTTON_LINES);

    str_bit_off  = (bits - str_bits) / 2;
    str_line_off = (BUTTON_LINES - str_lines) / 2;

    if (CENTER_LEFT == bit)
    {
        box_bit = 0;
    }
    else if (CENTER_RIGHT == bit)
    {
        box_bit = SCREEN_WIDTH_BITS - bits;
    }
    else if (CENTER == bit)
    {
        box_bit = (SCREEN_WIDTH_BITS - bits) / 2;
    }

    pixel_rect(BUTTON_LINE_START, BUTTON_LINES, box_bit, bits);
    pixel_str(str, BUTTON_LINE_START + str_line_off, box_bit + str_bit_off);

    pixel_font(8);
}


void screen_generate_seed(void)
{
    unsigned  i, j, down, up, wordlist_bytes;
    uint8_t   entropy[ENTROPY_BYTES];
    char      wordlist[WORDLIST_MAX_CHARS];
    char     *word;

    // Restore or new?
    screen_clear();
    screen_banner("New Wallet");

    pixel_str("Restore or", 40, 19);
    pixel_str("generate a", 54, 19);
    pixel_str("new wallet?", 68, 19);

    button("RESTORE", CENTER_LEFT, 55);
    button("NEW",     CENTER_RIGHT, 55);

    up = 0;

    while (up < 3)
    {
        touch_get(&down, &up);
    }

    // Warning

    pixel_clear_lines(BANNER_LINES + 1, SCREEN_HEIGHT_BITS - BANNER_LINES);

    pixel_str("Your seed is", 22, CENTER);
    pixel_str("the only way", 36, CENTER);
    pixel_str("to recover a", 50, CENTER);
    pixel_str("lost wallet.", 64, CENTER);

    pixel_str("Keep it safe", 82, CENTER);
    pixel_str("and secret." , 96, CENTER);

    button("OK (SLIDE) >>>", CENTER, 120);

    while (TOUCH_SLIDE_RIGHT != touch_get_slide());

    // Generate

    pixel_clear_lines(BANNER_LINES + 1, SCREEN_HEIGHT_BITS - BANNER_LINES);

    pixel_str("Keep pressing" , 40, CENTER);
    pixel_str("to generate a" , 54, CENTER);
    pixel_str("random seed."   , 68, CENTER);

    pixel_str("____________" , 80, CENTER);

    button("PRESS BELOW", CENTER, 120);

    // Generate the raw entropy for the mnemonic
    crypto_entropy_timer_reset();

    for (i = 0; i < ENTROPY_BYTES; i++)
    {
        // Any touch will do
        touch_get_any();

        entropy[i] = crypto_entropy_timer_get();

        // Show the progress
        pixel_rect(87, 2, PROGRESS_COL_START, (PROGRESS_COLS * (i + 1)) / ENTROPY_BYTES);
    }

    wordlist_bytes = words_create_mnemonic_wordlist(entropy, ENTROPY_BYTES, wordlist);

    // Display
    screen_clear();
    screen_banner("Wallet Seed");

    pixel_font(5);

    word = wordlist;

    // Display the new wordlist, column one
    for (i = 0; i < 9; i++)
    {
        for (j = 0; j < strlen(word); j++)
        {
            word[j] = toupper(word[j]);
        }
        pixel_str(word, 22 + (i * 10) , 5);
        word += strlen(word) + 1;
    }

    // Display the new wordlist, column two
    for (i = 0; i < 9; i++)
    {
        for (j = 0; j < strlen(word); j++)
        {
            word[j] = toupper(word[j]);
        }
        pixel_str(word, 22 + (i * 10) , 69);
        word += strlen(word) + 1;
    }

    pixel_font(8);

    button("SEED SAVED? >>>", CENTER, 120);
    while (TOUCH_SLIDE_RIGHT != touch_get_slide());

    button("ARE YOU SURE? >>>", CENTER, 120);
    while (TOUCH_SLIDE_RIGHT != touch_get_slide());

    button("LAST CHANCE >>>", CENTER, 120);
    while (TOUCH_SLIDE_RIGHT != touch_get_slide());

    button("SAVING", CENTER, 120);

    wallet_master_seed_set(wordlist, wordlist_bytes);
}


#define PIN_ENTROPY_BYTES 5

void screen_generate_pin(void)
{
    const unsigned word_line[MAX_PIN_WORDS] = {86, 72, 58};

    unsigned  i;
    uint16_t  entropy[MAX_PIN_WORDS];
    char      pass_str[MAX_PIN_WORDS * (WORD_LEN_MAX + 1)];
    unsigned  pass_str_pos, start_pos;
    char     *word;
    unsigned  wordlen;
    bool      accepted = false;

    assert((MAX_PIN_WORDS * WORDS_MAX_BITS) < (PIN_ENTROPY_BYTES * 8));

    // Warning
    screen_clear();
    screen_banner("New Passphrase");

    pixel_str("Your wallet"   , 22, CENTER);
    pixel_str("can only be"   , 36, CENTER);
    pixel_str("unlocked by a" , 50, CENTER);
    pixel_str("passphrase."   , 64, CENTER);

    pixel_str("Memorize and"    , 82, CENTER);
    pixel_str("keep it secret." , 96, CENTER);

    button("OK >>>", CENTER, 120);

    while (TOUCH_SLIDE_RIGHT != touch_get_slide());

    while (false == accepted)
    {
        pixel_font(8);

        // Generate
        screen_clear();
        screen_banner("New Passphrase");

        pixel_str("Keep pressing" , 40, CENTER);
        pixel_str("to generate a" , 54, CENTER);
        pixel_str("passphrase."   , 68, CENTER);

        pixel_str("____________" , 80, CENTER);

        button("PRESS BELOW", CENTER, 120);


        // Generate the raw entropy for the passphrase
        crypto_entropy_timer_reset();

        for (i = 0; i < MAX_PIN_WORDS * 2; i++)
        {
            // Any touch will do
            touch_get_any();

            if (0 == i % 2)
            {
                entropy[i / 2] = 0;
            }

            entropy[i / 2] |= crypto_entropy_timer_get() << ((i % 2) * 8);

            // Show the progress
            pixel_rect(87, 2, PROGRESS_COL_START, (PROGRESS_COLS * (i + 1)) / (MAX_PIN_WORDS * 2));
        }

        // Confirm

        pixel_clear_lines(BANNER_LINES + 1, SCREEN_HEIGHT_BITS - BANNER_LINES);
        pass_str_pos = 0;

        pixel_str("Your wallet"   , 22, CENTER);
        pixel_str("passphrase:"   , 36, CENTER);

        for (i = 0; i < MAX_PIN_WORDS; i++)
        {
            start_pos = pass_str_pos;

            word = word_get(entropy[i] & (WORDS_MAX - 1), &wordlen);
            strncpy(&pass_str[pass_str_pos], word, wordlen);
        
            pass_str_pos += wordlen;
            pass_str[pass_str_pos] = 0;
            pass_str_pos++;

            pixel_str(&pass_str[start_pos], word_line[i], CENTER);
        }

        pixel_str("________" , 94, CENTER);

        button_slider("<<< NEW", "OK? >>>", 120);
        if (TOUCH_SLIDE_RIGHT != touch_get_slide())  continue;

        button_slider("<<< NEW", "SURE? >>>", 120);
        if (TOUCH_SLIDE_RIGHT != touch_get_slide())  continue;

        accepted = true;
    }

    // Load in the newly minted store unlocking key based on the passphrase
    uint8_t store_key[SHA256_BYTES];

    crypto_pbkdf2_hmac256((uint8_t*)pass_str_pos, pass_str_pos, "salt TODO", sizeof("salt TODO") - 1, SEED_ROUNDS, store_key, sizeof(store_key));
    crypto_aes128_loadkey(store_key, STORE_KEYSLOT);

    memset(store_key, 0, sizeof(store_key));
}


// output_addr is in RIPEMD-160 with a leading 'version' byte
bool screen_send(const uint8_t *output_addr, uint64_t btc, uint64_t fee)
{
    unsigned i, span, outline;
    char str[14], str_fee[14], output_base58[34];

    screen_clear();
    screen_banner("Send BTC?");

    // BTC outputs
    satoshi_to_str(str, btc, 14);
    pixel_str(str, 25, CENTER);

    satoshi_to_str(str, fee, 11);
    sprintf(str_fee, "Fee %s", str);

    pixel_str(str_fee, 39, CENTER);

    // Address
    pixel_str("____________"  , 48, CENTER);

    wallet_base58_encode(output_addr, HASH_ADDR_BYTES + 1, output_base58, sizeof(output_base58));

    for (i = 0, outline = 66; i < 34; i += span, outline += 14)
    {
        span = min(34 - i, 12);

        strncpy(str, &output_base58[i], span);
        str[span] = 0;
        pixel_str(str, outline, CENTER);
    }

    // Confirmation buttons
#if !SKIP_CONFIRMATION
    button_slider("<<< NO", "SEND >>>", 120);
    if (TOUCH_SLIDE_RIGHT != touch_get_slide())  goto abort;
#endif

    pixel_clear_lines(BUTTON_LINE_START, BUTTON_LINES);
    screen_banner("Signing");
    return true;

#if !SKIP_CONFIRMATION
abort:
    pixel_clear_lines(BUTTON_LINE_START, BUTTON_LINES);
    screen_banner("Cancelled");
    return false;
#endif
}


void screen_signing_progress(unsigned done, unsigned total)
{
    unsigned bits = (PROGRESS_COLS * done) / total;

    pixel_rect(55, 2, PROGRESS_COL_START, bits);

    if (done == total)
    {
        screen_banner("Signed");
    }
}


void screen_signing_done(void)
{
}


void screen_enter_pin(bool failed)
{
    const unsigned word_line[MAX_PIN_WORDS] = {30, 44, 58};

    // Add one to the strings to preserve the null terminator
    word_data_t  word_list[MAX_PIN_WORDS];
    word_data_t *w;
    uint32_t     down;
    uint32_t     up;
    touch_type_e type;
    bool         word_list_changed;
    uint32_t     word;
    char        *match;
    unsigned     i;
    unsigned     matchlen;
    unsigned     pass_str_pos;
    char         pass_str[(WORD_LEN_MAX + 1) * MAX_PIN_WORDS];

    memset(word_list, 0, sizeof(word_list));

    //
    // Display the password entry screen
    //
    screen_clear();

    if (!failed)
    {
        screen_banner("Passphrase?");
    }
    else
    {
        screen_banner("Incorrect");
    }

    pixel_font(5);

    pixel_rect(87, 13, 0, 128);
    pixel_str("<<< DEL", 90, 4);
    pixel_str("OK >>>",  90, 92);

    pixel_rect(101, 27, 0, 128);

    pixel_str("ABC",  106, 5);
    pixel_str("GHI",  106, 35);
    pixel_str("MNO",  106, 65);
    pixel_str("STUV", 106, 95);

    pixel_str("DEF",  118, 5);
    pixel_str("JKL",  118, 35);
    pixel_str("PQR",  118, 65);
    pixel_str("WXYZ", 118, 95);

    pixel_font(8);

    word = 0;
    word_list_changed = false;

    while (word < MAX_PIN_WORDS)
    {
        w = &word_list[word];

        // Check for matches
        matchlen = words_match(w->press_hist, w->press_idx, &match);

        memset(w->word, 0, sizeof(w->word));

        if (matchlen > 0)
        {
            strncpy(w->word, match, matchlen);
        }
        else
        {
            strncat(w->word, "********", w->press_idx);
        }

        // Update the word list
        if (word_list_changed)
        {
            pixel_clear_lines(word_line[0], (14 * 3) + 6);

            // Print existing words
            for (i = 0; i < word; i++)
            {
                pixel_str(word_list[i].word, word_line[i], CENTER);
            }

            word_list_changed = false;
        }

        // Current word
        pixel_clear_lines(word_line[word], 14 + 6);
        pixel_str(w->word, word_line[word], CENTER);
        pixel_str("________",  word_line[word] + 8, CENTER);

        // Process touches
        type = touch_get(&down, &up);

        switch (type)
        {
            // Select key
            case TOUCH_POINT:
                if (w->press_idx >= WORD_LEN_MAX)
                {
                    // Hit the max word length, no more input
                    break;
                }
                w->press_hist[w->press_idx] = up - 1;
                w->press_idx += 1;

            break;

            // Delete letter
            case TOUCH_SLIDE_LEFT:
                if (w->press_idx > 0)
                {
                    // Delete a letter
                    w->press_idx -= 1;
                    w->press_hist[w->press_idx] = NO_PRESS;
                }
                else if (word > 0)
                {
                    // Delete a word
                    word--;
                    word_list_changed = true;
                }
            break;

            // Enter word
            case TOUCH_SLIDE_RIGHT:
                if (matchlen > 0)
                {
                    if (word < MAX_PIN_WORDS)
                    {
                        word++;
                        word_list_changed = true;
                    }
                }
            break;
        }

    }

    // Create the password string from the words entered by the user
    pixel_clear_lines(87, 128 - 87);
    button("LOADING", CENTER, 120);

    pass_str_pos = 0;

    for (i = 0; i < MAX_PIN_WORDS; i++)
    {
        strncpy(&pass_str[pass_str_pos], word_list[i].word, strlen(word_list[i].word));
        pass_str_pos += strlen(word_list[i].word);
        pass_str[pass_str_pos] = 0;
        pass_str_pos++;
    }

    uint8_t store_key[SHA256_BYTES];

    crypto_pbkdf2_hmac256((uint8_t*)pass_str_pos, pass_str_pos, "salt TODO", sizeof("salt TODO") - 1, SEED_ROUNDS, store_key, sizeof(store_key));
    crypto_aes128_loadkey(store_key, STORE_KEYSLOT);

    memset(store_key, 0, sizeof(store_key));
}

#if TOUCH_CAPACITIVE

void screen_captouch_debug(void)
{
    char str[16];
    uint32_t *ticks;
    uint32_t ticknum;
    uint32_t press;
    unsigned i;

    press = touch_read(false);
    ticks = touch_raw_ticks(&ticknum);

    pixel_clear();

    for (i = 0; i < ticknum; i++)
    {
        ltoa(ticks[i], str);
        pixel_str(str, i * 15 + 10, CENTER);
    }

    ltoa(press, str);
    pixel_str(str, 70, CENTER);
}

#endif
