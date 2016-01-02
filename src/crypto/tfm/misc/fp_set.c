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

void fp_init(fp_int *a)
{
    memset(a, 0, sizeof(fp_int));
}

void fp_init_copy(fp_int *a, const fp_int *b)
{
    fp_init(a); 
    fp_copy(b, a);
}
    
void fp_zero(fp_int *a)
{
    fp_init(a);
}    

void fp_set(fp_int *a, const fp_digit b)
{
   fp_zero(a);
   a->dp[0] = b;
   a->used  = a->dp[0] ? 1 : 0;
}

void fp_copy(const fp_int *a, fp_int *b)
{
    int i;
    
    for (i = 0; i < a->used; i++)
    {
        b->dp[i] = a->dp[i];
    }
    
    for(; i < FP_SIZE; i++)
    {
        b->dp[i] = 0;
    }
    
    b->sign = a->sign;
    b->used = a->used;
}

int fp_iszero(const fp_int *a)
{
    return a->used == 0 ? FP_YES : FP_NO;
}

int fp_iseven(const fp_int *a)
{
    return (a->used >= 0 && ((a->dp[0] & 1) == 0)) ? FP_YES : FP_NO;
}

int fp_isodd(const fp_int *a)
{
    return (a->used > 0  && ((a->dp[0] & 1) == 1)) ? FP_YES : FP_NO;
}

void fp_clamp(fp_int *a)
{ 
    while (a->used && a->dp[a->used-1] == 0)
    { 
        --(a->used); 
    }
        
    a->sign = a->used ? a->sign : FP_ZPOS;
}

void fp_neg(const fp_int *a, fp_int *b)
{ 
    fp_copy(a, b); 
    b->sign ^= 1; 
    fp_clamp(b); 
}

void fp_abs(const fp_int *a, fp_int *b)
{ 
    fp_copy(a, b); 
    b->sign = 0;
}
