#ifndef WORDS_H_
#define WORDS_H_

#include <stdint.h>
#include <stdbool.h>

#define WORDS_MAX        2048
#define WORDS_MAX_BITS   11
#define WORD_LEN_MIN     3
#define WORD_LEN_MAX     8
#define NO_PRESS         0x00
#define ENTROPY_BYTES    (192 / 8)

//
// Polly uses the BIP039 mnemonic for setting a seed.
// 18 words are required, words are <= 8 letters separated by spaces
//
#define WORDLIST_MAX_CHARS   (18 * (WORD_LEN_MAX + 1))


unsigned words_match                    (const uint8_t press[WORD_LEN_MAX], const unsigned presses, char **word);
unsigned words_create_mnemonic_wordlist (const uint8_t *entropy, const unsigned entropy_bytes, char wordlist[WORDLIST_MAX_CHARS]);
char*    word_get                       (const unsigned index, unsigned *wordlen);

#endif
