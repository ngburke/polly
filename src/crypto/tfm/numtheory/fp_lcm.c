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

/* c = [a, b] */
void fp_lcm( const fp_int *a, const fp_int *b, fp_int *c )
{
   fp_int  t1, t2;

   fp_init(&t1);
   fp_init(&t2);
   fp_gcd(a, b, &t1);
   if (fp_cmp_mag(a, b) == FP_GT) {
      fp_div(a, &t1, &t2, NULL);
      fp_mul(b, &t2, c);
   } else {
      fp_div(b, &t1, &t2, NULL);
      fp_mul(a, &t2, c);
   }   
}

/* $Source: /cvs/libtom/tomsfastmath/src/numtheory/fp_lcm.c,v $ */
/* $Revision: 1.1 $ */
/* $Date: 2007/01/24 21:25:19 $ */
