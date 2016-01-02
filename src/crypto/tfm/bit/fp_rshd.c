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

void fp_rshd(fp_int *a, const int x)
{
  int y;

  /* too many digits just zero and return */
  if (x >= a->used) {
     fp_zero(a);
     return;
  }

   /* shift */
   for (y = 0; y < a->used - x; y++) {
      a->dp[y] = a->dp[y+x];
   }

   /* zero rest */
   for (; y < a->used; y++) {
      a->dp[y] = 0;
   }
   
   /* decrement count */
   a->used -= x;
   fp_clamp(a);
}

/* right shift by bit count */
void fp_rshb(fp_int *c, const int x)
{
    register fp_digit *tmpc, mask, shift;
    fp_digit r, rr;
    fp_digit D = x;
    int i = x;

    /* mask */
    mask = (((fp_digit)1) << D) - 1;

    /* shift for lsb */
    shift = DIGIT_BIT - D;

    /* alias */
    tmpc = c->dp + (c->used - 1);

    /* carry */
    r = 0;
    for (i = c->used - 1; i >= 0; i--) {
        /* get the lower  bits of this word in a temp */
        rr = *tmpc & mask;

        /* shift the current word and mix in the carry bits from previous word */
        *tmpc = (*tmpc >> D) | (r << shift);
        --tmpc;

        /* set the carry to the carry bits of the current word found above */
        r = rr;
    }
}


/* $Source: /cvs/libtom/tomsfastmath/src/bit/fp_rshd.c,v $ */
/* $Revision: 1.1 $ */
/* $Date: 2006/12/31 21:25:53 $ */
