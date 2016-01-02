#include <ecdsa.h>
#include <tfm.h>
#include <assert.h>
#include <sha.h>
#include <crypto.h>

// Modulus reduction methods
#define MONTGOMERY     1

// Point multiplication methods
#define DOUBLE_ADD     0
#define SLIDING_WINDOW 1
#define PRECALC_M      1


#ifdef HW_CC2538

#include <pka.h>
#include <ecc_curveinfo.h>

const char     ci_name[] = "secp256k1";
const uint8_t  ci_dwords = 8;
const uint32_t ci_p[]    = {0xfffffc2f, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
const uint32_t ci_n[]    = {0xd0364141, 0xbfd25e8c, 0xaf48a03b, 0xbaaedce6, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff};
const uint32_t ci_a[]    = {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};
const uint32_t ci_b[]    = {0x00000007, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};
const uint32_t ci_gx[]   = {0x16f81798, 0x59f2815b, 0x2dce28d9, 0x029bfcdb, 0xce870b07, 0x55a06295, 0xf9dcbbac, 0x79be667e};
const uint32_t ci_gy[]   = {0xfb10d4b8, 0x9c47d08f, 0xa6855419, 0xfd17b448, 0x0e1108a8, 0x5da4fbfc, 0x26a3c465, 0x483ada77};

const tECCCurveInfo secp256k1_hw = {
    .name       = (char*) ci_name,
    .ui8Size    = 8,
    .pui32Prime = (uint32_t*) ci_p,
    .pui32N     = (uint32_t*) ci_n,
    .pui32A     = (uint32_t*) ci_a,
    .pui32B     = (uint32_t*) ci_b,
    .pui32Gx    = (uint32_t*) ci_gx,
    .pui32Gy    = (uint32_t*) ci_gy,
};

#endif

#if (1 == SLIDING_WINDOW)
#define WINSIZE 4
#endif

#if (1 == MONTGOMERY)
#define mod(x) fp_montgomery_reduce((x), &p, mp)
#else
#error "Pick a modular reduction method"
#endif

#define mod_sub(x)  if (fp_cmp_d((x), 0) == FP_LT) fp_add((x), &p, (x))
#define mod_add(x)  if (fp_cmp((x), &p) != FP_LT)  fp_sub((x), &p, (x))

typedef struct 
{
    uint32_t a;
    uint32_t b;
    char p[65];
    char gx[65];
    char gy[65];
    char n[65];
}  curve_t;

typedef struct
{
    fp_int x;
    fp_int y;
    fp_int z;
} point_t;

#if (1 == DOUBLE_ADD)
static int msbOne   (fp_int* x);
static int bitIsOne (fp_int* v, int j);
#endif

#if !HW_CC2538
static void jacobianToAffine (point_t *r);
static void pointAdd         (point_t *in1, point_t *in2, point_t *out);
static void pointDouble      (point_t *in, point_t *out);
#endif

static void pointGenMul      (fp_int* v, point_t *r);
static bool generateK        (const uint8_t *privKey, const uint8_t *s256hash, fp_int *secretK);

const curve_t secp256k1 = {
    .a  = 0x0,
    .b  = 0x7,
    .p  = "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f\0",
    .n  = "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141\0",
    .gx = "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798\0",
    .gy = "483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8\0",
};

// Curve parameters
fp_int  p;
fp_int  or;
fp_int  ordiv2;
point_t gen;

// Modular reduction parameters
fp_digit mp;
fp_int   mu;

#if !HW_CC2538
// Point double/add temp variables
fp_int a, b, c, d, e, f, g, h;
point_t tmpInMem;
    
 #if (1 == SLIDING_WINDOW)
point_t M[8];
 #endif

#endif // HW_CC2538

void ecdsa_init(void)
{
    // Use only the secp256k1 curve
    fp_read_radix(&p,     secp256k1.p,  16);
    fp_read_radix(&or,    secp256k1.n,  16);
    fp_read_radix(&gen.x, secp256k1.gx, 16);
    fp_read_radix(&gen.y, secp256k1.gy, 16);
    fp_set       (&gen.z, 1);

    fp_div_2     (&or, &ordiv2);
}    

#if (1 == DOUBLE_ADD)
// Calculate the bit position of the most significant 1b
int msbOne(fp_int* x)
{
    fp_digit mask = 1 << (DIGIT_BIT - 1);
    fp_digit msd = x->dp[x->used - 1];
    
    for (int i = DIGIT_BIT - 1; i >= 0; i--)
    {
        if (mask & msd)
        {
            return i + ((x->used - 1) * DIGIT_BIT);
        }
        
        mask >>= 1;
    }
    
    return -1;
}

int bitIsOne(fp_int* v, int j)
{
    int digit = j / DIGIT_BIT;
    int pos = j % DIGIT_BIT;
    fp_digit mask = 1 << pos;
    
    if (v->dp[digit] & mask)
    {
        return 1;
    }
    
    return 0;
}
#endif

#if !HW_CC2538
void pointAdd(point_t *in1, point_t *in2, point_t *out)
{    
    point_t *pIn1, *pIn2;

    // z = 0 is the point at 'infinity'
    if (fp_cmp_d(&in1->z, 0) == FP_EQ)
    {
        fp_copy(&in2->x, &out->x);
        fp_copy(&in2->y, &out->y);
        fp_copy(&in2->z, &out->z);       
        return;
    }
    else if (fp_cmp_d(&in2->z, 0) == FP_EQ)
    {
        fp_copy(&in1->x, &out->x);
        fp_copy(&in1->y, &out->y);
        fp_copy(&in1->z, &out->z);        
        return;        
    }

    // Handle in == out cases
    if (in1 == out)
    {
        fp_copy(&in1->x, &tmpInMem.x);
        fp_copy(&in1->y, &tmpInMem.y);
        fp_copy(&in1->z, &tmpInMem.z);

        pIn1 = &tmpInMem;
        pIn2 = in2;
    }
    else if (in2 == out)
    {
        fp_copy(&in2->x, &tmpInMem.x);
        fp_copy(&in2->y, &tmpInMem.y);
        fp_copy(&in2->z, &tmpInMem.z);

        pIn1 = in1;
        pIn2 = &tmpInMem;
    }
    else
    {
        pIn1 = in1;
        pIn2 = in2;
    }
    
    // Equation below for ECC secp256k1 (a==0) using jacobian coordinates at:
    // http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html#addition-add-2007-bl
   
    // A = Z1^2
    fp_sqr(&pIn1->z, &a);
    mod(&a);
    
    // B = Z2^2
    fp_sqr(&pIn2->z, &b);
    mod(&b);
    
    // C = X1*B
    fp_mul(&pIn1->x, &b, &c);
    mod(&c);
        
    // D = X2*A
    fp_mul(&pIn2->x, &a, &d);
    mod(&d);
    
    // E = Z2*B
    fp_mul(&pIn2->z, &b, &e);
    mod(&e);

    // E = Y1*E
    fp_mul(&pIn1->y, &e, &e);
    mod(&e);

    // F = Z1*A
    fp_mul(&pIn1->z, &a, &f);
    mod(&f);

    // F = Y2*F
    fp_mul(&pIn2->y, &f, &f);
    mod(&f);

    // D  = D-C
    fp_sub(&d, &c, &d);
    mod_sub(&d);

    // G  = 2*D
    fp_add(&d, &d, &g);
    mod_add(&g);
    
    // G  = G^2
    fp_sqr(&g, &g);
    mod(&g);

    // H  = D*G
    fp_mul(&d, &g, &h);
    mod(&h);

    // Z3 = Z1+Z2
    fp_add(&pIn1->z, &pIn2->z, &out->z);
    mod_add(&out->z);
    
    // Z3 = Z3^2
    fp_sqr(&out->z, &out->z);
    mod(&out->z);

    // Z3 = Z3-A
    fp_sub(&out->z, &a, &out->z);
    mod_sub(&out->z);

    // Z3 = Z3-B
    fp_sub(&out->z, &b, &out->z);
    mod_sub(&out->z);

    // Z3 = Z3*D
    fp_mul(&out->z, &d, &out->z);
    mod(&out->z);

    // A  = F-E
    fp_sub(&f, &e, &a);
    mod_sub(&a);    
    
    // A  = 2*A
    fp_add(&a, &a, &a);
    mod_add(&a);

    // B  = C*G
    fp_mul(&c, &g, &b);
    mod(&b);
    
    // X3 = A^2
    fp_sqr(&a, &out->x);
    mod(&out->x);

    // C  = 2*B
    fp_add(&b, &b, &c);
    mod_add(&c);
        
    // X3 = X3-H
    fp_sub(&out->x, &h, &out->x);
    mod_sub(&out->x);    
    
    // X3 = X3-C
    fp_sub(&out->x, &c, &out->x);
    mod_sub(&out->x);   
    
    // Y3 = B-X3
    fp_sub(&b, &out->x, &out->y);
    mod_sub(&out->y);    
    
    // E  = E*H
    fp_mul(&e, &h, &e);
    mod(&e);    
    
    // E  = 2*E
    fp_add(&e, &e, &e);
    mod_add(&e);    
    
    // Y3 = A*Y3
    fp_mul(&a, &out->y, &out->y);
    mod(&out->y);    
        
    // Y3 = Y3-E
    fp_sub(&out->y, &e, &out->y);
    mod_sub(&out->y);   
}


void pointDouble(point_t *in, point_t *out)
{
    point_t* pIn;
        
    // z = 0 is the point at 'infinity'
    if (fp_cmp_d(&in->z, 0) == FP_EQ)
    {
        fp_copy(&in->x, &out->x);
        fp_copy(&in->y, &out->y);
        fp_copy(&in->z, &out->z);
        return;
    }
    
    // Handle in == out case
    if (in == out)
    {
        fp_copy(&in->x, &tmpInMem.x);
        fp_copy(&in->y, &tmpInMem.y);
        fp_copy(&in->z, &tmpInMem.z);

        pIn = &tmpInMem;
    }
    else
    {
        pIn = in;
    }
       
    // Equation below for ECC secp256k1 (a==0) using jacobian coordinates at:
    // http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html#doubling-dbl-2009-l
    
    // A = X1^2
    fp_sqr(&pIn->x, &a);
    mod(&a);

    // B = Y1^2
    fp_sqr(&pIn->y, &b);
    mod(&b);
        
    // C = B^2
    fp_sqr(&b, &c);
    mod(&c);
    
    // D = X1+B
    fp_add(&pIn->x, &b, &d);
    mod_add(&d);
    
    // D = D^2
    fp_sqr(&d, &d);
    mod(&d);  

    // D = D-A
    fp_sub(&d, &a, &d);
    mod_sub(&d);
            
    // D = D-C
    fp_sub(&d, &c, &d);
    mod_sub(&d);
    
    // D = 2*D
    fp_add(&d, &d, &d);
    mod_add(&d);

    // E = 3*A
    // Note Montgomery reduction requires addition not single digit multiplication here!
    fp_add(&a, &a, &e);
    mod_add(&e);
    fp_add(&a, &e, &e);
    mod_add(&e);

    // A = E^2
    fp_sqr(&e, &a);
    mod(&a);

    // X3 = 2*D
    fp_add(&d, &d, &out->x);
    mod_add(&out->x);
    
    // X3 = A-X3
    fp_sub(&a, &out->x, &out->x);
    mod_sub(&out->x);
    
    // Y3 = D-X3
    fp_sub(&d, &out->x, &out->y);
    mod_sub(&out->y);

    // A = 8*C
    // Note Montgomery reduction requires addition not single digit multiplication here!
    fp_add(&c, &c, &a);
    mod_add(&a);
    fp_add(&c, &a, &a);
    mod_add(&a);
    fp_add(&c, &a, &a);
    mod_add(&a);
    fp_add(&c, &a, &a);
    mod_add(&a);
    fp_add(&c, &a, &a);
    mod_add(&a);
    fp_add(&c, &a, &a);
    mod_add(&a);
    fp_add(&c, &a, &a);
    mod_add(&a);
    
    // Y3 = E*Y3
    fp_mul(&e, &out->y, &out->y);
    mod(&out->y);
    
    // Y3 = Y3-A
    fp_sub(&out->y, &a, &out->y);
    mod_sub(&out->y);
    
    // Z3 = Y1*Z1
    fp_mul(&pIn->y, &pIn->z, &out->z);
    mod(&out->z);
    
    // Z3 = 2*Z3
    fp_add(&out->z, &out->z, &out->z);
    mod_add(&out->z);
}

void jacobianToAffine(point_t *inout)
{
    fp_int tmp0, tmp1;
    
    fp_zero(&tmp0);
    fp_zero(&tmp1);
    
    // first map z back to normal
#if (1 == MONTGOMERY)
    fp_montgomery_reduce(&inout->z, &p, mp);
#endif

    // get 1/z
    fp_invmod(&inout->z, &p, &tmp0);
    
    // get 1/z^2 and 1/z^3
    fp_sqr(&tmp0, &tmp1);
    fp_mod(&tmp1, &p, &tmp1);
    fp_mul(&tmp0, &tmp1, &tmp0);
    fp_mod(&tmp0, &p, &tmp0);

    // multiply against x/y
    fp_mul(&inout->x, &tmp1, &inout->x);
    mod(&inout->x);
    
    fp_mul(&inout->y, &tmp0, &inout->y);
    mod(&inout->y);
    
    fp_set(&inout->z, 1);
}
#endif // !HW_CC2538

void pointGenMul(fp_int* scalar, point_t *out)
{   
#if HW_CC2538

	tECPt      point;
	uint32_t   loc;

	point.pui32X = out->x.dp;
	point.pui32Y = out->y.dp;

	PKAECCMultGenPtStart(scalar->dp, (tECCCurveInfo*)&secp256k1_hw, &loc);

	while (PKA_STATUS_OPERATION_INPRG == PKAGetOpsStatus());

	PKAECCMultGenPtGetResult(&point, loc);

	out->x.used = KEY_BYTES / 4;
	out->y.used = KEY_BYTES / 4;

	out->x.sign = 0;
	out->y.sign = 0;

#else

    static point_t tU; 
    
    fp_init(&tU.x);
    fp_init(&tU.y);
    fp_init(&tU.z);
        
#if (1 == MONTGOMERY)

    fp_init(&mu);
        
    // Init Montgomery reduction
    fp_montgomery_setup(&p, &mp);
    fp_montgomery_calc_normalization(&mu, &p); 
    
    fp_mulmod(&gen.x, &mu, &p, &tU.x);
    fp_mulmod(&gen.y, &mu, &p, &tU.y);
    fp_mulmod(&gen.z, &mu, &p, &tU.z);
#endif

#if (1 == DOUBLE_ADD)

    static point_t qq;
    int j, maxBits;

    // Q starts at infinity
    fp_set(&qq.x, 0);
    fp_set(&qq.y, 0);
    fp_set(&qq.z, 0);

    maxBits = msbOne(scalar);
    
    for (j = 0; j <= maxBits; j++)
    {
        if (bitIsOne(scalar, j) > 0)
        {
            pointAdd(&qq, &tU, &qq);
        }            
   
        pointDouble(&tU, &tU);
    }

    fp_copy(&qq.x, &out->x);
    fp_copy(&qq.y, &out->y);
    fp_copy(&qq.z, &out->z);

#elif (1 == SLIDING_WINDOW)

    int i, j, first, bitbuf, bitcpy, bitcnt, mode, digidx;
    fp_digit buf;
    
    for (i = 0; i < 8; i++) 
    {
        fp_init(&M[i].x);
        fp_init(&M[i].y);
        fp_init(&M[i].z);
        
#if (1 == PRECALC_M)
        M[i].x.used = 8;         
        M[i].y.used = 8;         
        M[i].z.used = 8;
#endif
    }        

#if (1 == PRECALC_M)
    M[0].x.dp[0] = 0xc1c748d8;
    M[0].x.dp[1] = 0x229529c8;
    M[0].x.dp[2] = 0x383a056e;
    M[0].x.dp[3] = 0xf350270e;
    M[0].x.dp[4] = 0xd22461cd;
    M[0].x.dp[5] = 0xf390a938;
    M[0].x.dp[6] = 0x6a921689;
    M[0].x.dp[7] = 0xea765e5d;

    M[0].y.dp[0] = 0x8eb77651;
    M[0].y.dp[1] = 0x3e05b375;
    M[0].y.dp[2] = 0xc39339e6;
    M[0].y.dp[3] = 0xe1438a06;
    M[0].y.dp[4] = 0xaa67f35b;
    M[0].y.dp[5] = 0x67fbd1fd;
    M[0].y.dp[6] = 0x3319cee0;
    M[0].y.dp[7] = 0xa3e5721f;

    M[0].z.dp[0] = 0x99d87506;
    M[0].z.dp[1] = 0x0f98fa8b;
    M[0].z.dp[2] = 0x5fcf78d0;
    M[0].z.dp[3] = 0x897d606a;
    M[0].z.dp[4] = 0xc23b887b;
    M[0].z.dp[5] = 0x145cb157;
    M[0].z.dp[6] = 0x212ebf5d;
    M[0].z.dp[7] = 0x25e3331d;


    M[1].x.dp[0] = 0x40badaf2;
    M[1].x.dp[1] = 0x8cd95d80; 
    M[1].x.dp[2] = 0xcd155dc6; 
    M[1].x.dp[3] = 0xf56ee366; 
    M[1].x.dp[4] = 0xc79f400f; 
    M[1].x.dp[5] = 0xc9f7882b; 
    M[1].x.dp[6] = 0x9cbc2388; 
    M[1].x.dp[7] = 0xa44573b7; 

    M[1].y.dp[0] = 0x80c6dc2e; 
    M[1].y.dp[1] = 0x828db750; 
    M[1].y.dp[2] = 0xc7f002e0; 
    M[1].y.dp[3] = 0x351dc5bd; 
    M[1].y.dp[4] = 0xaf19c973; 
    M[1].y.dp[5] = 0x4ba22053; 
    M[1].y.dp[6] = 0x62a89577; 
    M[1].y.dp[7] = 0x6a921436; 

    M[1].z.dp[0] = 0x82e9156c; 
    M[1].z.dp[1] = 0x4bc50caa; 
    M[1].z.dp[2] = 0x02672334; 
    M[1].z.dp[3] = 0xf1415a58; 
    M[1].z.dp[4] = 0xce250724; 
    M[1].z.dp[5] = 0x112f1a18; 
    M[1].z.dp[6] = 0xd121dfa6; 
    M[1].z.dp[7] = 0x5329bb85; 


    M[2].x.dp[0] = 0x8ed46e08; 
    M[2].x.dp[1] = 0x908709f2; 
    M[2].x.dp[2] = 0x38dc0e6e; 
    M[2].x.dp[3] = 0xddc7ba22; 
    M[2].x.dp[4] = 0x1a8d2253; 
    M[2].x.dp[5] = 0x0d05cb55; 
    M[2].x.dp[6] = 0xa729a1eb; 
    M[2].x.dp[7] = 0x96a806b2; 

    M[2].y.dp[0] = 0x6d81fb76; 
    M[2].y.dp[1] = 0x351889c0; 
    M[2].y.dp[2] = 0x97126be2; 
    M[2].y.dp[3] = 0x31703c31; 
    M[2].y.dp[4] = 0xa8498cbf; 
    M[2].y.dp[5] = 0x55785ff8; 
    M[2].y.dp[6] = 0x737bcd02; 
    M[2].y.dp[7] = 0x953f4f73; 

    M[2].z.dp[0] = 0xa1bd5fd8; 
    M[2].z.dp[1] = 0x72ba29ae; 
    M[2].z.dp[2] = 0x99f537df; 
    M[2].z.dp[3] = 0x0841507e; 
    M[2].z.dp[4] = 0x1bed0a5d; 
    M[2].z.dp[5] = 0x89d0929b; 
    M[2].z.dp[6] = 0x749a134e; 
    M[2].z.dp[7] = 0x9f587585; 


    M[3].x.dp[0] = 0xf5712e5d; 
    M[3].x.dp[1] = 0x6c313e7e; 
    M[3].x.dp[2] = 0x21efb3ae; 
    M[3].x.dp[3] = 0x41bb6ce6; 
    M[3].x.dp[4] = 0x75caa192; 
    M[3].x.dp[5] = 0xb31ccd34; 
    M[3].x.dp[6] = 0xd456f442; 
    M[3].x.dp[7] = 0x6642d410; 

    M[3].y.dp[0] = 0x64f291de; 
    M[3].y.dp[1] = 0xc07cf881; 
    M[3].y.dp[2] = 0x6d26e748; 
    M[3].y.dp[3] = 0xe35b14e2; 
    M[3].y.dp[4] = 0xd0fb2188; 
    M[3].y.dp[5] = 0x9cbf2b24; 
    M[3].y.dp[6] = 0x57be04fe; 
    M[3].y.dp[7] = 0xe7067793; 

    M[3].z.dp[0] = 0xadcfd8a1; 
    M[3].z.dp[1] = 0x6136ebbc; 
    M[3].z.dp[2] = 0x06f8ab62; 
    M[3].z.dp[3] = 0x93a49a8a; 
    M[3].z.dp[4] = 0x7dc3b696; 
    M[3].z.dp[5] = 0x61e4d2de; 
    M[3].z.dp[6] = 0x5d532183; 
    M[3].z.dp[7] = 0x04e0d6aa; 


    M[4].x.dp[0] = 0x80548355; 
    M[4].x.dp[1] = 0x4a67b876; 
    M[4].x.dp[2] = 0x39dac7f8; 
    M[4].x.dp[3] = 0x00f40941; 
    M[4].x.dp[4] = 0x0c177d15; 
    M[4].x.dp[5] = 0x49ebd9f6; 
    M[4].x.dp[6] = 0x5efd7bc0; 
    M[4].x.dp[7] = 0xcc093d98; 

    M[4].y.dp[0] = 0xb56d7148; 
    M[4].y.dp[1] = 0xe7dfd01f; 
    M[4].y.dp[2] = 0x7576b78d; 
    M[4].y.dp[3] = 0x87b4fb05; 
    M[4].y.dp[4] = 0x62fdd229; 
    M[4].y.dp[5] = 0x1a2ca950; 
    M[4].y.dp[6] = 0x7d9f1822; 
    M[4].y.dp[7] = 0xaa222286; 

    M[4].z.dp[0] = 0x2c01fc19; 
    M[4].z.dp[1] = 0x2e97c7cf; 
    M[4].z.dp[2] = 0x901c627b; 
    M[4].z.dp[3] = 0x5670b065; 
    M[4].z.dp[4] = 0xe5ff3ba6; 
    M[4].z.dp[5] = 0x3f43f01d; 
    M[4].z.dp[6] = 0xc3211efb; 
    M[4].z.dp[7] = 0x54d09845; 


    M[5].x.dp[0] = 0xc08e9d2c; 
    M[5].x.dp[1] = 0x2f4d1a5b; 
    M[5].x.dp[2] = 0xe5c5bf32; 
    M[5].x.dp[3] = 0x5ea255cb; 
    M[5].x.dp[4] = 0xd0d19156; 
    M[5].x.dp[5] = 0x4ad34a09; 
    M[5].x.dp[6] = 0x69583386; 
    M[5].x.dp[7] = 0x40fd8070; 

    M[5].y.dp[0] = 0xdc03888f; 
    M[5].y.dp[1] = 0xee6c035c; 
    M[5].y.dp[2] = 0x675230e0; 
    M[5].y.dp[3] = 0x31c94e1e; 
    M[5].y.dp[4] = 0x28bacd6f; 
    M[5].y.dp[5] = 0xe79cc8a9; 
    M[5].y.dp[6] = 0xa6ee8b62; 
    M[5].y.dp[7] = 0x057094ff; 

    M[5].z.dp[0] = 0x0ff227e2; 
    M[5].z.dp[1] = 0x79acb09a; 
    M[5].z.dp[2] = 0x2f81dca3; 
    M[5].z.dp[3] = 0x7c519cf6; 
    M[5].z.dp[4] = 0xd8793c91; 
    M[5].z.dp[5] = 0x7ad7c4dd; 
    M[5].z.dp[6] = 0x07eece0e; 
    M[5].z.dp[7] = 0xc71ef7eb; 


    M[6].x.dp[0] = 0x8dc34b7d; 
    M[6].x.dp[1] = 0xaf79bc6b; 
    M[6].x.dp[2] = 0xb793291f; 
    M[6].x.dp[3] = 0x6e554f4a; 
    M[6].x.dp[4] = 0x37449736; 
    M[6].x.dp[5] = 0xe983d249; 
    M[6].x.dp[6] = 0x5b2eca30; 
    M[6].x.dp[7] = 0xe6b4af2a; 

    M[6].y.dp[0] = 0x36188262; 
    M[6].y.dp[1] = 0x1250b29a; 
    M[6].y.dp[2] = 0xedb44bbb; 
    M[6].y.dp[3] = 0xc67aa954; 
    M[6].y.dp[4] = 0xa0a9c641; 
    M[6].y.dp[5] = 0xeae8f28e; 
    M[6].y.dp[6] = 0xc629d0af; 
    M[6].y.dp[7] = 0xb6877175; 

    M[6].z.dp[0] = 0x6156c6ce; 
    M[6].z.dp[1] = 0x9ccc1e46; 
    M[6].z.dp[2] = 0xacbef203; 
    M[6].z.dp[3] = 0x5ce4a803; 
    M[6].z.dp[4] = 0xa236233a; 
    M[6].z.dp[5] = 0x2646c735; 
    M[6].z.dp[6] = 0x1f339bff; 
    M[6].z.dp[7] = 0x9be9af05; 


    M[7].x.dp[0] = 0x00fd775c; 
    M[7].x.dp[1] = 0x0122aace; 
    M[7].x.dp[2] = 0x89983529; 
    M[7].x.dp[3] = 0x5f611d87; 
    M[7].x.dp[4] = 0x61540647; 
    M[7].x.dp[5] = 0x4d6d1eb5; 
    M[7].x.dp[6] = 0xc465fd93; 
    M[7].x.dp[7] = 0xd95700f0; 

    M[7].y.dp[0] = 0xff04a5c4; 
    M[7].y.dp[1] = 0x7688fbcd; 
    M[7].y.dp[2] = 0x8a3b523b; 
    M[7].y.dp[3] = 0x8d8627e3; 
    M[7].y.dp[4] = 0x330e413b; 
    M[7].y.dp[5] = 0xac678c39; 
    M[7].y.dp[6] = 0x2397e6c8; 
    M[7].y.dp[7] = 0xd64ec6ba; 

    M[7].z.dp[0] = 0xc28a6800; 
    M[7].z.dp[1] = 0xb4443819; 
    M[7].z.dp[2] = 0xd9392d7d; 
    M[7].z.dp[3] = 0x8af848be; 
    M[7].z.dp[4] = 0x37833d20; 
    M[7].z.dp[5] = 0xb97d63be; 
    M[7].z.dp[6] = 0x96ff5500; 
    M[7].z.dp[7] = 0x2ff61ba4; 

#else

    // calc the M tab, which holds kG for k==8..15
    // M[0] == 8G
    pointDouble(&tU, &M[0]);
    pointDouble(&M[0], &M[0]);
    pointDouble(&M[0], &M[0]);
    
    // now find (8+k)G for k=1..7
    for (j = 9; j < 16; j++) 
    {
        pointAdd(&M[j-9], &tU, &M[j-8]);
    } 
       
#endif

    // setup sliding window
    mode   = 0;
    bitcnt = 1;
    buf    = 0;
    digidx = scalar->used - 1;
    bitcpy = bitbuf = 0;
    first  = 1;

    // perform ops
    for (;;) 
    {
        // grab next digit as required
        if (--bitcnt == 0) 
        {
            if (digidx == -1) 
            {
                break;
            }
            buf    = scalar->dp[digidx];
            bitcnt = DIGIT_BIT;
            --digidx;
        }
        
        // grab the next msb from the multiplicand
        i = (buf >> (DIGIT_BIT - 1)) & 1;
        buf <<= 1;
        
        // skip leading zero bits
        if (mode == 0 && i == 0) {
            continue;
        }

        // if the bit is zero and mode == 1 then we double
        if (mode == 1 && i == 0) 
        {
            pointDouble(out, out);
            continue;
        }

        // else we add it to the window
        bitbuf |= (i << (WINSIZE - ++bitcpy));
        mode = 2;

        if (bitcpy == WINSIZE) 
        {
            // if this is the first window we do a simple copy
            if (first == 1) 
            {
                // R = kG [k = first window]
                fp_copy(&M[bitbuf-8].x, &out->x);
                fp_copy(&M[bitbuf-8].y, &out->y);
                fp_copy(&M[bitbuf-8].z, &out->z);
                first = 0;
            } 
            else 
            {
                // normal window
                // ok window is filled so double as required and add
                // double first
                for (j = 0; j < WINSIZE; j++) 
                {
                    pointDouble(out, out);
                }
                
                // then add, bitbuf will be 8..15 [8..2^WINSIZE] guaranteed
                pointAdd(out, &M[bitbuf-8], out);
            }
            
            // empty window and reset
            bitcpy = bitbuf = 0;
            mode = 1;
        }
    }
    
    // if bits remain then double/add
    if (mode == 2 && bitcpy > 0) 
    {
        // double then add
        for (j = 0; j < bitcpy; j++) 
        {
            // only double if we have had at least one add first
            if (first == 0) 
            {
                pointDouble(out, out);
            }
             
            bitbuf <<= 1;
            if ((bitbuf & (1 << WINSIZE)) != 0) 
            {
                if (first == 1)
                {
                    // first add, so copy
                    fp_copy(&tU.x, &out->x);
                    fp_copy(&tU.y, &out->y);
                    fp_copy(&tU.z, &out->z);
                    first = 0;
                } 
                else 
                {
                    // then add
                    pointAdd(out, &tU, out);
                }
            }
        }
    }

#else
#error "Pick a point multiplication method"
#endif

    // Remap to affine coordinates
    jacobianToAffine(out);

#endif // HW_CC2538

}

//
// Using the RFC 6979 method for k generation 
// see https://tools.ietf.org/html/rfc6979#section-3.2
//
// Hard coded for secp256k1 curve and SHA 256 hashes as the message
//
bool generateK(const uint8_t *privKey, const uint8_t *s256hash, fp_int *secretK)
{
    const uint8_t zero = 0x00;
    const uint8_t one  = 0x01;
    
    unsigned    i;
    uint8_t     xh[KEY_BYTES + SHA256HashSize];
    uint8_t     v[SHA256HashSize];
    uint8_t     k[SHA256HashSize];
    HMACContext ctx;
    
    memcpy(xh, privKey, KEY_BYTES);
    memcpy(&xh[KEY_BYTES], s256hash, SHA256HashSize);

    // Step b
    memset(v, 0x01, sizeof(v)); 

    // Step c
    memset(k, 0x00, sizeof(k)); 

    // Step d
	hmacReset(&ctx, SHA256, k, sizeof(k));
	hmacInput(&ctx, v, sizeof(v));
	hmacInput(&ctx, &zero, 1);
	hmacInput(&ctx, xh, sizeof(xh));
	hmacResult(&ctx, k);

    // Step e
	hmacReset(&ctx, SHA256, k, sizeof(k));
	hmacInput(&ctx, v, sizeof(v));
	hmacResult(&ctx, v);

    // Step f
	hmacReset(&ctx, SHA256, k, sizeof(k));
	hmacInput(&ctx, v, sizeof(v));
	hmacInput(&ctx, &one, 1);
	hmacInput(&ctx, xh, sizeof(xh));
	hmacResult(&ctx, k);

    // Step g
	hmacReset(&ctx, SHA256, k, sizeof(k));
	hmacInput(&ctx, v, sizeof(v));
	hmacResult(&ctx, v);

    // Step h
    for (i = 0; i < 100; i++)
    {
        // Step h2 - only needs one round as the SHA256 has == order length
        hmacReset(&ctx, SHA256, k, sizeof(k));
        hmacInput(&ctx, v, sizeof(v));
        hmacResult(&ctx, v);

        // Step h3 - don't need to truncate the secret, order length is a multiple of 8

        fp_read_unsigned_bin(secretK, v, sizeof(v));

        if (!fp_iszero(secretK) && (FP_LT == fp_cmp(secretK, &or)))
        {
            return true;
        }

        // Our secretK did not meet the criteria, try again
        hmacReset(&ctx, SHA256, k, sizeof(k));
        hmacInput(&ctx, v, sizeof(v));
        hmacInput(&ctx, &zero, 1);
        hmacResult(&ctx, k);

        // Step g
        hmacReset(&ctx, SHA256, k, sizeof(k));
        hmacInput(&ctx, v, sizeof(v));
        hmacResult(&ctx, v);
    }

    //
    // 100 tries, and still no dice - bail out with an error 
    // This is extremely unlikely
    //
    return false;
}    

unsigned ecdsa_sign(const uint8_t *privKey, const uint8_t *s256hash, uint8_t *sig, uint8_t *sigBytes)
{
    point_t pt;
    fp_int  k, r, s, pk, hash;
    unsigned bytes;

    fp_read_unsigned_bin(&pk, privKey, KEY_BYTES);
    fp_read_unsigned_bin(&hash, s256hash, SHA256HashSize);

    // Use deterministic values of 'k' to prevent problems with RNG attacks - see RFC 6979
    if (generateK(privKey, s256hash, &k) == false)  return 1;

    // Calculate the curve point
    pointGenMul(&k, &pt);

    // Calculate r
    fp_mod(&pt.x, &or, &r);

    if (fp_iszero(&r))  return 1;
    
    // Calculate s
    fp_invmod(&k, &or, &k);
    fp_mulmod(&r, &pk, &or, &s); 
    fp_add(&hash, &s, &s);

    if (fp_cmp(&s, &or) != FP_LT)
    {
        fp_sub(&s, &or, &s);
    }

    fp_mulmod(&k, &s, &or, &s);
    
    if (fp_iszero(&s))  return 1;

    // Ensure that 's' is even - see https://bitcointalk.org/index.php?topic=285142.msg3295518#msg3295518
    if (fp_cmp(&s, &ordiv2) == FP_GT)
    {
        fp_sub(&or, &s, &s);
    }
            
    //
    // Finally, DER encode the signature
    //
    
    sig[0] = 0x30;
    sig[1] = 0x00; // The overall length, will fill in at the end
    sig[2] = 0x02; // First component, the r value

    *sigBytes = 3;
    
    // DER encoded numbers are 2's complement, must add 0x00 byte to r/s with leading byte >= 0x80 
    bytes = fp_count_bytes(&r);
    
    if (fp_isbitset(&r, (bytes * 8) - 1))
    {
        sig[(*sigBytes)++] = bytes + 1;
        sig[(*sigBytes)++] = 0x00;

        fp_to_unsigned_bin(&r, &sig[*sigBytes]);
    }
    else
    {
        sig[(*sigBytes)++] = bytes;

        fp_to_unsigned_bin(&r, &sig[*sigBytes]);
    }

    *sigBytes += bytes;

    // DER encoded numbers are 2's complement, must add 0x00 byte to r/s with leading byte >= 0x80 
    sig[(*sigBytes)++] = 0x02; // Second component, the s value

    bytes = fp_count_bytes(&s);

    if (fp_isbitset(&s, (bytes * 8) - 1))
    {
        sig[(*sigBytes)++] = bytes + 1;
        sig[(*sigBytes)++] = 0x00;

        fp_to_unsigned_bin(&s, &sig[*sigBytes]);
    }
    else
    {
        sig[(*sigBytes)++] = bytes;

        fp_to_unsigned_bin(&s, &sig[*sigBytes]);
    }

    *sigBytes += bytes;

    sig[(*sigBytes)++] = 0x01; // Signature type = SIGHASH_ALL

    // Enter in the final length of all the DER encoded stuff, 
    // minus the header, length byte, and signature type
    sig[1] = *sigBytes - 3;
    
    return 0; 
}

void reverse_stream(uint8_t *x, const uint32_t bytes)
{
    uint32_t i, j;
    uint8_t  tmp;

    for (i = 0, j = bytes - 1; i < j; i++, j--)
   {
       tmp  = x[i];
       x[i] = x[j];
       x[j] = tmp;
    }
}

void reverse_memcpy(void *dst, const void *src, const unsigned bytes)
{
   unsigned i;
   uint8_t *d = dst;
   const uint8_t *s = src;

   for (i = 0; i < bytes; i++)
   {
       d[i] = s[bytes - 1 - i];
   }
}

void ecdsa_genPublicKey(uint8_t *pubKeyX, uint8_t *pubKeyY, const uint8_t* exponent)
{  
	point_t out;
    fp_int  exp;
    
    fp_read_unsigned_bin(&exp, exponent, KEY_BYTES);

    // Calculate the public key 
    pointGenMul(&exp, &out);

    // Convert to byte stream    
    assert (fp_count_bits(&out.x) <= (KEY_BYTES * 8));
    assert (fp_count_bits(&out.y) <= (KEY_BYTES * 8));

    fp_to_unsigned_bin_full(&out.x, pubKeyX);
    fp_to_unsigned_bin_full(&out.y, pubKeyY);
}

void ecdsa_addMod(const uint8_t *inA, const uint8_t *inB, uint8_t *out)
{
    fp_int bigA, bigB;
    
    fp_read_unsigned_bin(&bigA, inA, KEY_BYTES);
    fp_read_unsigned_bin(&bigB, inB, KEY_BYTES);

	// a = a + b
    fp_add(&bigA, &bigB, &bigA);
	if (fp_cmp(&bigA, &or) != FP_LT)
	{
		fp_sub(&bigA, &or, &bigA);
	}
	
    fp_to_unsigned_bin_full(&bigA, out);
}
