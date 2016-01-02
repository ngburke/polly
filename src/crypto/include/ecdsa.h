#ifndef ECDSA_H_
#define ECDSA_H_

#include <stdint.h>
#include <stdbool.h>

void     ecdsa_init         (void);
void     ecdsa_genPublicKey (uint8_t *pubKeyX, uint8_t *pubKeyY, const uint8_t* exponent);
unsigned ecdsa_sign         (const uint8_t *privKey, const uint8_t *s256hash, uint8_t *sig, uint8_t *sigBytes);
void     ecdsa_addMod       (const uint8_t *inA, const uint8_t *inB, uint8_t *out);

#endif
