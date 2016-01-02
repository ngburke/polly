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

int fp_count_bits( const fp_int *a )
{
  int     r;
  fp_digit q;

  /* shortcut */
  if (a->used == 0) {
    return 0;
  }

  /* get number of digits and add that */
  r = (a->used - 1) * DIGIT_BIT;

  /* take the last digit and count the bits in it */
  q = a->dp[a->used - 1];
  while (q > ((fp_digit) 0)) {
    ++r;
    q >>= ((fp_digit) 1);
  }
  return r;
}

int fp_count_bytes(const fp_int *a)
{
    return (fp_count_bits(a) + 7) >> 3;
}

int fp_isbitset(const fp_int* a, const int x)
{
    int digit = x / DIGIT_BIT;
    int pos = x % DIGIT_BIT;
    fp_digit mask = 1 << pos;
    
    if (a->dp[digit] & mask)
    {
        return 1;
    }
    
    return 0;
}

/* $Source: /cvs/libtom/tomsfastmath/src/bit/fp_count_bits.c,v $ */
/* $Revision: 1.1 $ */
/* $Date: 2006/12/31 21:25:53 $ */
