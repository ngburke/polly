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

/* unsigned subtraction ||a|| >= ||b|| ALWAYS! */
void s_fp_sub( const fp_int *a, const fp_int *b, fp_int *c )
{
  int      x, oldbused, oldused;
  fp_word  t;

  oldused  = c->used;
  oldbused = b->used;
  c->used  = a->used;
  t       = 0;
  for (x = 0; x < oldbused; x++) {
     t         = ((fp_word)a->dp[x]) - (((fp_word)b->dp[x]) + t);
     c->dp[x]  = (fp_digit)t;
     t         = (t >> DIGIT_BIT)&1;
  }
  for (; x < a->used; x++) {
     t         = ((fp_word)a->dp[x]) - t;
     c->dp[x]  = (fp_digit)t;
     t         = (t >> DIGIT_BIT);
   }
  for (; x < oldused; x++) {
     c->dp[x] = 0;
  }
  fp_clamp(c);
}

/* $Source: /cvs/libtom/tomsfastmath/src/addsub/s_fp_sub.c,v $ */
/* $Revision: 1.1 $ */
/* $Date: 2006/12/31 21:25:53 $ */