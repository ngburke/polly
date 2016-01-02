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

void fp_to_unsigned_bin( const fp_int *a, unsigned char *b )
{
  int     x;
  fp_int  t;

  fp_init_copy(&t, a);

  x = 0;
  while (fp_iszero (&t) == FP_NO) {
      b[x++] = (unsigned char) (t.dp[0] & 255);
      fp_div_2d (&t, 8, &t, NULL);
  }
  fp_reverse (b, x);
}

void fp_to_unsigned_bin_full( const fp_int *a, unsigned char *b )
{
    int x;
    char *data = (char*)a->dp;
    int bytes = a->used * sizeof(fp_digit);

    for(x = 0; x < bytes; x++)
    {
        b[x] = data[bytes - 1 - x];
    }   
}

/* $Source: /cvs/libtom/tomsfastmath/src/bin/fp_to_unsigned_bin.c,v $ */
/* $Revision: 1.2 $ */
/* $Date: 2007/02/27 02:38:44 $ */
