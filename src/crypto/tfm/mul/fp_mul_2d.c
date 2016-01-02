/* TomsFastMath, a fast ISO C bignum library.
 * 
 * This project is meant to fill in where LibTomMath
 * falls short.  That is speed ;-)
 *
 * This project is public domain and free for all purposes.
 * 
 * Tom St Denis, tomstdenis@gmail.com
 */
#include <tfm.h>

/* c = a * 2**d */
void fp_mul_2d(const fp_int *a, const int b, fp_int *c)
{
   fp_digit carry, carrytmp, shift;
   int x, bmod;

   /* copy it */
   fp_copy(a, c);

   /* handle whole digits */
   if (b >= DIGIT_BIT) {
      fp_lshd(c, b/DIGIT_BIT);
   }
   
   bmod = b % DIGIT_BIT;

   /* shift the digits */
   if (bmod != 0) {
      carry = 0;   
      shift = DIGIT_BIT - bmod;
      for (x = 0; x < c->used; x++) {
          carrytmp = c->dp[x] >> shift;
          c->dp[x] = (c->dp[x] << bmod) + carry;
          carry = carrytmp;
      }
      /* store last carry if room */
      if (carry && x < FP_SIZE) {
         c->dp[c->used++] = carry;
      }
   }
   fp_clamp(c);
}


/* $Source: /cvs/libtom/tomsfastmath/src/mul/fp_mul_2d.c,v $ */
/* $Revision: 1.1 $ */
/* $Date: 2006/12/31 21:25:53 $ */
