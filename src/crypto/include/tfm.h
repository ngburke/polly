/* 
    TomsFastMath, a fast ISO C bignum library.
 
    This project is meant to fill in where LibTomMath
    falls short.  That is speed ;-)
 
    This project is public domain and free for all purposes.
  
    Tom St Denis, tomstdenis@gmail.com
*/

#ifndef TFM_H_
#define TFM_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef MIN
   #define MIN(x,y) ((x)<(y)?(x):(y))
#endif

#ifndef MAX
   #define MAX(x,y) ((x)>(y)?(x):(y))
#endif

//
// ARM specific assembly speedups
//

//#define TFM_ARM


//
// Do we want the large set of small multiplications? 
// Enable these if you are going to be doing a lot of small (<= 16 digit) multiplications say in ECC
// Or if you're on a 64-bit machine doing RSA as a 1024-bit integer == 16 digits ;-)
//
//#define TFM_SMALL_SET

//
// Do we want huge code 
// Enable these if you are doing 20, 24, 28, 32, 48, 64 digit multiplications (useful for RSA)
// Less important on 64-bit machines as 32 digits == 2048 bits
//

//#define TFM_MUL3
//#define TFM_MUL4
//#define TFM_MUL6
//#define TFM_MUL7
#define TFM_MUL8
//#define TFM_MUL9
//#define TFM_MUL12
//#define TFM_MUL17
//#define TFM_MUL20
//#define TFM_MUL24
//#define TFM_MUL28
//#define TFM_MUL32
//#define TFM_MUL48
//#define TFM_MUL64

//#define TFM_SQR3
//#define TFM_SQR4
//#define TFM_SQR6
//#define TFM_SQR7
#define TFM_SQR8
//#define TFM_SQR9
//#define TFM_SQR12
//#define TFM_SQR17
//#define TFM_SQR20
//#define TFM_SQR24
//#define TFM_SQR28
//#define TFM_SQR32
//#define TFM_SQR48
//#define TFM_SQR64

//
// Do we want some overflow checks?
// Not required if you make sure your numbers are within range (e.g. by default a modulus for fp_exptmod() can only be upto 2048 bits long)
//
//#define TFM_CHECK

//
// Do we want timing resistant fp_exptmod()?
// This makes it slower but also timing invariant with respect to the exponent 
//
//#define TFM_TIMING_RESISTANT

//
// Max size of any number in bits.  Basically the largest size you will be multiplying
// should be half [or smaller] of FP_MAX_SIZE-four_digit
//
// You can externally define this or it defaults to 4096-bits [allowing multiplications upto 2048x2048 bits ]
//
#define FP_MAX_SIZE 1024

typedef uint32_t  fp_digit;
typedef uint64_t  fp_word;

// Digits
#define DIGIT_BIT  (int)((CHAR_BIT) * sizeof(fp_digit))
#define FP_MASK    (fp_digit)(-1)
#define FP_SIZE    (FP_MAX_SIZE/DIGIT_BIT)

// Signs
#define FP_ZPOS     0
#define FP_NEG      1

// Return codes
#define FP_OKAY     0
#define FP_VAL      1
#define FP_MEM      2

// Equalities
#define FP_LT        -1   // less than
#define FP_EQ         0   // equal to
#define FP_GT         1   // greater than

// Replies
#define FP_YES        1   // yes response
#define FP_NO         0   // no response

// FP type
typedef struct {
    fp_digit dp[FP_SIZE];
    int      used, 
             sign;
} fp_int;


// Initialization
void fp_init      (fp_int *a);
void fp_set       (fp_int *a, const fp_digit b);
void fp_copy      (const fp_int *a, fp_int *b);
void fp_init_copy (fp_int *a, const fp_int *b);
void fp_zero      (fp_int *a);

// Zero/even/odd
int  fp_iszero    (const fp_int *a);
int  fp_iseven    (const fp_int *a);
int  fp_isodd     (const fp_int *a);

// Clamp digits
void fp_clamp     (fp_int *a);

// Negate and absolute
void fp_neg       (const fp_int *a, fp_int *b);
void fp_abs       (const fp_int *a, fp_int *b);

// Right shift x bits
void fp_rshb      (fp_int *a, const int x);

// Right shift x digits
void fp_rshd      (fp_int *a, const int x);

// Left shift x digits
void fp_lshd      (fp_int *a, const int x);

// Is bit set?
int  fp_isbitset  (const fp_int *a, const int x);

// Signed comparison a ? b
int  fp_cmp       (const fp_int *a, const fp_int *b);

// Unsigned comparison
int  fp_cmp_mag   (const fp_int *a, const fp_int *b);

// Power of 2 operations
void fp_div_2d    (const fp_int *a, const int b, fp_int *c, fp_int *d);
void fp_mod_2d    (const fp_int *a, const int b, fp_int *c);
void fp_mul_2d    (const fp_int *a, const int b, fp_int *c);
void fp_2expt     (fp_int *a, const int b);
void fp_mul_2     (const fp_int *a, fp_int *c);
void fp_div_2     (const fp_int *a, fp_int *c);

// Counts the number of lsbs which are zero before the first zero bit
int  fp_cnt_lsb   (const fp_int *a);

// c = a + b 
void fp_add       (const fp_int *a, const fp_int *b, fp_int *c);

// c = a - b
void fp_sub       (const fp_int *a, const fp_int *b, fp_int *c);

// c = a * b
void fp_mul       (const fp_int *a, const fp_int *b, fp_int *c);

// b = a * a
void fp_sqr       (const fp_int *a, fp_int *b);

// a / b => cb + d == a
int  fp_div       (const fp_int *a, const fp_int *b, fp_int *c, fp_int *d);

// c = a mod b, 0 <= c < b
int  fp_mod       (const fp_int *a, const fp_int *b, fp_int *c);

// Compare against a single digit
int  fp_cmp_d     (const fp_int *a, const fp_digit b);

// c = a + b
void fp_add_d     (const fp_int *a, const fp_digit b, fp_int *c);

// c = a - b
void fp_sub_d     (const fp_int *a, const fp_digit b, fp_int *c);

// c = a * b
void fp_mul_d     (const fp_int *a, const fp_digit b, fp_int *c);

// a / b => cb + d == a
int  fp_div_d     (const fp_int *a, const fp_digit b, fp_int *c, fp_digit *d);

// c = a mod b, 0 <= c < b
int  fp_mod_d     (const fp_int *a, const fp_digit b, fp_digit *c);

//
// Number theory
//

// d = a + b (mod c)
int  fp_addmod    (const fp_int *a, const fp_int *b, const fp_int *c, fp_int *d);

// d = a - b (mod c)
int  fp_submod    (const fp_int *a, const fp_int *b, const fp_int *c, fp_int *d);

// d = a * b (mod c)
int  fp_mulmod    (const fp_int *a, const fp_int *b, const fp_int *c, fp_int *d);

// c = a * a (mod b)
int  fp_sqrmod    (const fp_int *a, const fp_int *b, fp_int *c);

// c = 1/a (mod b)
int  fp_invmod    (const fp_int *a, const fp_int *b, fp_int *c);

// c = (a, b)
void fp_gcd       (const fp_int *a, const fp_int *b, fp_int *c);

// c = [a, b]
void fp_lcm       (const fp_int *a, const fp_int *b, fp_int *c);

// d = a**b (mod c)
int  fp_exptmod   (const fp_int *a, fp_int *b, const fp_int *c, fp_int *d);

// Sets up the Montgomery reduction
int  fp_montgomery_setup (const fp_int *a, fp_digit *mp);

// Computes a = B**n mod b without division or multiplication useful for normalizing numbers in a Montgomery system.
void fp_montgomery_calc_normalization (fp_int *a, const fp_int *b);

// computes a/R == a (mod N) via Montgomery Reduction
void fp_montgomery_reduce (fp_int *a, const fp_int *m, const fp_digit mp);

//
// Primality
//

// Perform a Miller-Rabin test of a to the base b and store result in "result"
void fp_prime_miller_rabin (const fp_int * a, const fp_int * b, int *result);

// 256 trial divisions + 8 Miller-Rabins, returns FP_YES if probable prime
int  fp_isprime (const fp_int *a);

// Primality generation flags
#define TFM_PRIME_BBS      0x0001  // BBS style prime
#define TFM_PRIME_SAFE     0x0002  // Safe prime (p-1)/2 == prime
#define TFM_PRIME_2MSB_OFF 0x0004  // force 2nd MSB to 0
#define TFM_PRIME_2MSB_ON  0x0008  // force 2nd MSB to 1

// callback for fp_prime_random, should fill dst with random bytes and return how many read [upto len]
typedef int tfm_prime_callback(unsigned char *dst, int len, void *dat);

#define fp_prime_random(a, t, size, bbs, cb, dat) fp_prime_random_ex(a, t, ((size) * 8) + 1, (bbs==1)?TFM_PRIME_BBS:0, cb, dat)

int  fp_prime_random_ex(fp_int *a, int t, int size, int flags, tfm_prime_callback cb, void *dat);

//
// Radix conversions
//

int  fp_count_bits           (const fp_int *a);
int  fp_count_bytes          (const fp_int *a); 

int  fp_unsigned_bin_size    (const fp_int *a);
void fp_read_unsigned_bin    (fp_int *a, const unsigned char *b, int c);
void fp_to_unsigned_bin      (const fp_int *a, unsigned char *b);
void fp_to_unsigned_bin_full (const fp_int *a, unsigned char *b);

int  fp_signed_bin_size      (const fp_int *a);
void fp_read_signed_bin      (fp_int *a, const unsigned char *b, int c);
void fp_to_signed_bin        (const fp_int *a, unsigned char *b);

int  fp_read_radix           (fp_int *a, const char *str, const int radix);
int fp_toradix               (const fp_int *a, char *str, const int radix);
int fp_toradix_n             (const fp_int * a, char *str, const int radix, const int maxlen);

//
// Low level APIs
//

void s_fp_add   (const fp_int *a, const fp_int *b, fp_int *c);
void s_fp_sub   (const fp_int *a, const fp_int *b, fp_int *c);
void fp_reverse (unsigned char *s, const int len);

void fp_mul_comba (const fp_int *A, const fp_int *B, fp_int *C);

#ifdef TFM_SMALL_SET
void fp_mul_comba_small (const fp_int *A, const fp_int *B, fp_int *C);
#endif

#ifdef TFM_MUL3
void fp_mul_comba3(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL4
void fp_mul_comba4(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL6
void fp_mul_comba6(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL7
void fp_mul_comba7(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL8
void fp_mul_comba8(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL9
void fp_mul_comba9(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL12
void fp_mul_comba12(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL17
void fp_mul_comba17(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL20
void fp_mul_comba20(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL24
void fp_mul_comba24(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL28
void fp_mul_comba28(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL32
void fp_mul_comba32(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL48
void fp_mul_comba48(const fp_int *A, const fp_int *B, fp_int *C);
#endif
#ifdef TFM_MUL64
void fp_mul_comba64(const fp_int *A, const fp_int *B, fp_int *C);
#endif

void fp_sqr_comba(const fp_int *A, fp_int *B);

#ifdef TFM_SMALL_SET
void fp_sqr_comba_small(const fp_int *A, fp_int *B);
#endif

#ifdef TFM_SQR3
void fp_sqr_comba3(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR4
void fp_sqr_comba4(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR6
void fp_sqr_comba6(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR7
void fp_sqr_comba7(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR8
void fp_sqr_comba8(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR9
void fp_sqr_comba9(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR12
void fp_sqr_comba12(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR17
void fp_sqr_comba17(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR20
void fp_sqr_comba20(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR24
void fp_sqr_comba24(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR28
void fp_sqr_comba28(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR32
void fp_sqr_comba32(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR48
void fp_sqr_comba48(const fp_int *A, fp_int *B);
#endif
#ifdef TFM_SQR64
void fp_sqr_comba64(const fp_int *A, fp_int *B);
#endif
extern const char fp_s_rmap[];

#endif
