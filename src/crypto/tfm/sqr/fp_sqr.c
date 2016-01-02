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

/* b = a*a  */
void fp_sqr( const fp_int *a, fp_int *b )
{
    int     y;

    /* call generic if we're out of range */
    if (a->used + a->used > FP_SIZE) {
       fp_sqr_comba(a, b);
       return ;
    }

    y = a->used;
#if defined(TFM_SQR3)
        if (y <= 3) {
           fp_sqr_comba3(a,b);
           return;
        }
#endif
#if defined(TFM_SQR4)
        if (y == 4) {
           fp_sqr_comba4(a,b);
           return;
        }
#endif
#if defined(TFM_SQR6)
        if (y <= 6) {
           fp_sqr_comba6(a,b);
           return;
        }
#endif
#if defined(TFM_SQR7)
        if (y == 7) {
           fp_sqr_comba7(a,b);
           return;
        }
#endif
#if defined(TFM_SQR8)
        if (y == 8) {
           fp_sqr_comba8(a,b);
           return;
        }
#endif
#if defined(TFM_SQR9)
        if (y == 9) {
           fp_sqr_comba9(a,b);
           return;
        }
#endif
#if defined(TFM_SQR12)
        if (y <= 12) {
           fp_sqr_comba12(a,b);
           return;
        }
#endif
#if defined(TFM_SQR17)
        if (y <= 17) {
           fp_sqr_comba17(a,b);
           return;
        }
#endif
#if defined(TFM_SMALL_SET)
        if (y <= 16) {
           fp_sqr_comba_small(a,b);
           return;
        }
#endif
#if defined(TFM_SQR20)
        if (y <= 20) {
           fp_sqr_comba20(a,b);
           return;
        }
#endif
#if defined(TFM_SQR24)
        if (y <= 24) {
           fp_sqr_comba24(a,b);
           return;
        }
#endif
#if defined(TFM_SQR28)
        if (y <= 28) {
           fp_sqr_comba28(a,b);
           return;
        }
#endif
#if defined(TFM_SQR32)
        if (y <= 32) {
           fp_sqr_comba32(a,b);
           return;
        }
#endif
#if defined(TFM_SQR48)
        if (y <= 48) {
           fp_sqr_comba48(a,b);
           return;
        }
#endif
#if defined(TFM_SQR64)
        if (y <= 64) {
           fp_sqr_comba64(a,b);
           return;
        }
#endif
       fp_sqr_comba(a, b);
}


/* $Source: /cvs/libtom/tomsfastmath/src/sqr/fp_sqr.c,v $ */
/* $Revision: 1.1 $ */
/* $Date: 2006/12/31 21:25:53 $ */
