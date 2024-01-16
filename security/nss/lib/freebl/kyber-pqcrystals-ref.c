/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file was generated from
 *   https://github.com/pq-crystals/kyber/commit/e0d1c6ff
 *
 * Files from that repository are listed here surrounded by
 * "* begin: [file] *" and "* end: [file] *" comments.
 *
 * The following changes have been made:
 *  - include guards have been removed,
 *  - include directives have been removed,
 *  - "#ifdef KYBER90S" blocks have been evaluated with "KYBER90S" undefined,
 *  - functions outside of kem.c have been made static.
*/

/** begin: ref/LICENSE **
Public Domain (https://creativecommons.org/share-your-work/public-domain/cc0/);
or Apache 2.0 License (https://www.apache.org/licenses/LICENSE-2.0.html).

For Keccak and AES we are using public-domain
code from sources and by authors listed in
comments on top of the respective files.
** end: ref/LICENSE **/

/** begin: ref/AUTHORS **
Joppe Bos,
Léo Ducas,
Eike Kiltz,
Tancrède Lepoint,
Vadim Lyubashevsky,
John Schanck,
Peter Schwabe,
Gregor Seiler,
Damien Stehlé
** end: ref/AUTHORS **/

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "secport.h"

// We need to provide an implementation of randombytes to avoid an unused
// function warning. We don't use the randomized API in freebl, so we'll make
// calling randombytes an error.
static void
randombytes(uint8_t *out, size_t outlen)
{
    // this memset is to avoid "maybe-uninitialized" warnings that gcc-11 issues
    // for the (unused) crypto_kem_keypair and crypto_kem_enc functions.
    memset(out, 0, outlen);
    assert(0);
}

/*************************************************
* Name:        verify
*
* Description: Compare two arrays for equality in constant time.
*
* Arguments:   const uint8_t *a: pointer to first byte array
*              const uint8_t *b: pointer to second byte array
*              size_t len:       length of the byte arrays
*
* Returns 0 if the byte arrays are equal, 1 otherwise
**************************************************/
static int
verify(const uint8_t *a, const uint8_t *b, size_t len)
{
    return NSS_SecureMemcmp(a, b, len);
}

/*************************************************
* Name:        cmov
*
* Description: Copy len bytes from x to r if b is 1;
*              don't modify x if b is 0. Requires b to be in {0,1};
*              assumes two's complement representation of negative integers.
*              Runs in constant time.
*
* Arguments:   uint8_t *r:       pointer to output byte array
*              const uint8_t *x: pointer to input byte array
*              size_t len:       Amount of bytes to be copied
*              uint8_t b:        Condition bit; has to be in {0,1}
**************************************************/
static void
cmov(uint8_t *r, const uint8_t *x, size_t len, uint8_t b)
{
    NSS_SecureSelect(r, r, x, len, b);
}

/** begin: ref/params.h **/
#ifndef KYBER_K
#define KYBER_K 3 /* Change this for different security strengths */
#endif

//#define KYBER_90S	/* Uncomment this if you want the 90S variant */

/* Don't change parameters below this line */
#if (KYBER_K == 2)
#define KYBER_NAMESPACE(s) pqcrystals_kyber512_ref_##s
#elif (KYBER_K == 3)
#define KYBER_NAMESPACE(s) pqcrystals_kyber768_ref_##s
#elif (KYBER_K == 4)
#define KYBER_NAMESPACE(s) pqcrystals_kyber1024_ref_##s
#else
#error "KYBER_K must be in {2,3,4}"
#endif

#define KYBER_N 256
#define KYBER_Q 3329

#define KYBER_SYMBYTES 32 /* size in bytes of hashes, and seeds */
#define KYBER_SSBYTES 32  /* size in bytes of shared key */

#define KYBER_POLYBYTES 384
#define KYBER_POLYVECBYTES (KYBER_K * KYBER_POLYBYTES)

#if KYBER_K == 2
#define KYBER_ETA1 3
#define KYBER_POLYCOMPRESSEDBYTES 128
#define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 320)
#elif KYBER_K == 3
#define KYBER_ETA1 2
#define KYBER_POLYCOMPRESSEDBYTES 128
#define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 320)
#elif KYBER_K == 4
#define KYBER_ETA1 2
#define KYBER_POLYCOMPRESSEDBYTES 160
#define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 352)
#endif

#define KYBER_ETA2 2

#define KYBER_INDCPA_MSGBYTES (KYBER_SYMBYTES)
#define KYBER_INDCPA_PUBLICKEYBYTES (KYBER_POLYVECBYTES + KYBER_SYMBYTES)
#define KYBER_INDCPA_SECRETKEYBYTES (KYBER_POLYVECBYTES)
#define KYBER_INDCPA_BYTES (KYBER_POLYVECCOMPRESSEDBYTES + KYBER_POLYCOMPRESSEDBYTES)

#define KYBER_PUBLICKEYBYTES (KYBER_INDCPA_PUBLICKEYBYTES)
/* 32 bytes of additional space to save H(pk) */
#define KYBER_SECRETKEYBYTES (KYBER_INDCPA_SECRETKEYBYTES + KYBER_INDCPA_PUBLICKEYBYTES + 2 * KYBER_SYMBYTES)
#define KYBER_CIPHERTEXTBYTES (KYBER_INDCPA_BYTES)
/** end: ref/params.h **/

/** begin: ref/reduce.h **/
#define MONT -1044 // 2^16 mod q
#define QINV -3327 // q^-1 mod 2^16

#define montgomery_reduce KYBER_NAMESPACE(montgomery_reduce)
static int16_t montgomery_reduce(int32_t a);

#define barrett_reduce KYBER_NAMESPACE(barrett_reduce)
static int16_t barrett_reduce(int16_t a);
/** end: ref/reduce.h **/

/** begin: ref/ntt.h **/
#define zetas KYBER_NAMESPACE(zetas)
extern const int16_t zetas[128];

#define ntt KYBER_NAMESPACE(ntt)
static void ntt(int16_t poly[256]);

#define invntt KYBER_NAMESPACE(invntt)
static void invntt(int16_t poly[256]);

#define basemul KYBER_NAMESPACE(basemul)
static void basemul(int16_t r[2], const int16_t a[2], const int16_t b[2], int16_t zeta);
/** end: ref/ntt.h **/

/** begin: ref/poly.h **/
/*
 * Elements of R_q = Z_q[X]/(X^n + 1). Represents polynomial
 * coeffs[0] + X*coeffs[1] + X^2*coeffs[2] + ... + X^{n-1}*coeffs[n-1]
 */
typedef struct {
    int16_t coeffs[KYBER_N];
} poly;

#define poly_compress KYBER_NAMESPACE(poly_compress)
static void poly_compress(uint8_t r[KYBER_POLYCOMPRESSEDBYTES], const poly *a);
#define poly_decompress KYBER_NAMESPACE(poly_decompress)
static void poly_decompress(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES]);

#define poly_tobytes KYBER_NAMESPACE(poly_tobytes)
static void poly_tobytes(uint8_t r[KYBER_POLYBYTES], const poly *a);
#define poly_frombytes KYBER_NAMESPACE(poly_frombytes)
static void poly_frombytes(poly *r, const uint8_t a[KYBER_POLYBYTES]);

#define poly_frommsg KYBER_NAMESPACE(poly_frommsg)
static void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES]);
#define poly_tomsg KYBER_NAMESPACE(poly_tomsg)
static void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *r);

#define poly_getnoise_eta1 KYBER_NAMESPACE(poly_getnoise_eta1)
static void poly_getnoise_eta1(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce);

#define poly_getnoise_eta2 KYBER_NAMESPACE(poly_getnoise_eta2)
static void poly_getnoise_eta2(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce);

#define poly_ntt KYBER_NAMESPACE(poly_ntt)
static void poly_ntt(poly *r);
#define poly_invntt_tomont KYBER_NAMESPACE(poly_invntt_tomont)
static void poly_invntt_tomont(poly *r);
#define poly_basemul_montgomery KYBER_NAMESPACE(poly_basemul_montgomery)
static void poly_basemul_montgomery(poly *r, const poly *a, const poly *b);
#define poly_tomont KYBER_NAMESPACE(poly_tomont)
static void poly_tomont(poly *r);

#define poly_reduce KYBER_NAMESPACE(poly_reduce)
static void poly_reduce(poly *r);

#define poly_add KYBER_NAMESPACE(poly_add)
static void poly_add(poly *r, const poly *a, const poly *b);
#define poly_sub KYBER_NAMESPACE(poly_sub)
static void poly_sub(poly *r, const poly *a, const poly *b);
/** end: ref/poly.h **/

/** begin: ref/cbd.h **/
#define poly_cbd_eta1 KYBER_NAMESPACE(poly_cbd_eta1)
static void poly_cbd_eta1(poly *r, const uint8_t buf[KYBER_ETA1 * KYBER_N / 4]);

#define poly_cbd_eta2 KYBER_NAMESPACE(poly_cbd_eta2)
static void poly_cbd_eta2(poly *r, const uint8_t buf[KYBER_ETA2 * KYBER_N / 4]);
/** end: ref/cbd.h **/

/** begin: ref/polyvec.h **/
typedef struct {
    poly vec[KYBER_K];
} polyvec;

#define polyvec_compress KYBER_NAMESPACE(polyvec_compress)
static void polyvec_compress(uint8_t r[KYBER_POLYVECCOMPRESSEDBYTES], const polyvec *a);
#define polyvec_decompress KYBER_NAMESPACE(polyvec_decompress)
static void polyvec_decompress(polyvec *r, const uint8_t a[KYBER_POLYVECCOMPRESSEDBYTES]);

#define polyvec_tobytes KYBER_NAMESPACE(polyvec_tobytes)
static void polyvec_tobytes(uint8_t r[KYBER_POLYVECBYTES], const polyvec *a);
#define polyvec_frombytes KYBER_NAMESPACE(polyvec_frombytes)
static void polyvec_frombytes(polyvec *r, const uint8_t a[KYBER_POLYVECBYTES]);

#define polyvec_ntt KYBER_NAMESPACE(polyvec_ntt)
static void polyvec_ntt(polyvec *r);
#define polyvec_invntt_tomont KYBER_NAMESPACE(polyvec_invntt_tomont)
static void polyvec_invntt_tomont(polyvec *r);

#define polyvec_basemul_acc_montgomery KYBER_NAMESPACE(polyvec_basemul_acc_montgomery)
static void polyvec_basemul_acc_montgomery(poly *r, const polyvec *a, const polyvec *b);

#define polyvec_reduce KYBER_NAMESPACE(polyvec_reduce)
static void polyvec_reduce(polyvec *r);

#define polyvec_add KYBER_NAMESPACE(polyvec_add)
static void polyvec_add(polyvec *r, const polyvec *a, const polyvec *b);
/** end: ref/polyvec.h **/

/** begin: ref/indcpa.h **/
#define gen_matrix KYBER_NAMESPACE(gen_matrix)
static void gen_matrix(polyvec *a, const uint8_t seed[KYBER_SYMBYTES], int transposed);

#define indcpa_keypair_derand KYBER_NAMESPACE(indcpa_keypair_derand)
static void indcpa_keypair_derand(uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                                  uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES],
                                  const uint8_t coins[KYBER_SYMBYTES]);

#define indcpa_enc KYBER_NAMESPACE(indcpa_enc)
static void indcpa_enc(uint8_t c[KYBER_INDCPA_BYTES],
                       const uint8_t m[KYBER_INDCPA_MSGBYTES],
                       const uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                       const uint8_t coins[KYBER_SYMBYTES]);

#define indcpa_dec KYBER_NAMESPACE(indcpa_dec)
static void indcpa_dec(uint8_t m[KYBER_INDCPA_MSGBYTES],
                       const uint8_t c[KYBER_INDCPA_BYTES],
                       const uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES]);
/** end: ref/indcpa.h **/

/** begin: ref/fips202.h **/
#define SHAKE128_RATE 168
#define SHAKE256_RATE 136
#define SHA3_256_RATE 136
#define SHA3_512_RATE 72

#define FIPS202_NAMESPACE(s) pqcrystals_kyber_fips202_ref_##s

typedef struct {
    uint64_t s[25];
    unsigned int pos;
} keccak_state;

#define shake128_init FIPS202_NAMESPACE(shake128_init)
void shake128_init(keccak_state *state);
#define shake128_absorb FIPS202_NAMESPACE(shake128_absorb)
void shake128_absorb(keccak_state *state, const uint8_t *in, size_t inlen);
#define shake128_finalize FIPS202_NAMESPACE(shake128_finalize)
void shake128_finalize(keccak_state *state);
#define shake128_squeeze FIPS202_NAMESPACE(shake128_squeeze)
void shake128_squeeze(uint8_t *out, size_t outlen, keccak_state *state);
#define shake128_absorb_once FIPS202_NAMESPACE(shake128_absorb_once)
void shake128_absorb_once(keccak_state *state, const uint8_t *in, size_t inlen);
#define shake128_squeezeblocks FIPS202_NAMESPACE(shake128_squeezeblocks)
void shake128_squeezeblocks(uint8_t *out, size_t nblocks, keccak_state *state);

#define shake256_init FIPS202_NAMESPACE(shake256_init)
void shake256_init(keccak_state *state);
#define shake256_absorb FIPS202_NAMESPACE(shake256_absorb)
void shake256_absorb(keccak_state *state, const uint8_t *in, size_t inlen);
#define shake256_finalize FIPS202_NAMESPACE(shake256_finalize)
void shake256_finalize(keccak_state *state);
#define shake256_squeeze FIPS202_NAMESPACE(shake256_squeeze)
void shake256_squeeze(uint8_t *out, size_t outlen, keccak_state *state);
#define shake256_absorb_once FIPS202_NAMESPACE(shake256_absorb_once)
void shake256_absorb_once(keccak_state *state, const uint8_t *in, size_t inlen);
#define shake256_squeezeblocks FIPS202_NAMESPACE(shake256_squeezeblocks)
void shake256_squeezeblocks(uint8_t *out, size_t nblocks, keccak_state *state);

#define shake128 FIPS202_NAMESPACE(shake128)
void shake128(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);
#define shake256 FIPS202_NAMESPACE(shake256)
void shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);
#define sha3_256 FIPS202_NAMESPACE(sha3_256)
void sha3_256(uint8_t h[32], const uint8_t *in, size_t inlen);
#define sha3_512 FIPS202_NAMESPACE(sha3_512)
void sha3_512(uint8_t h[64], const uint8_t *in, size_t inlen);
/** end: ref/fips202.h **/

/** begin: ref/symmetric.h **/
typedef keccak_state xof_state;

#define kyber_shake128_absorb KYBER_NAMESPACE(kyber_shake128_absorb)
static void kyber_shake128_absorb(keccak_state *s,
                                  const uint8_t seed[KYBER_SYMBYTES],
                                  uint8_t x,
                                  uint8_t y);

#define kyber_shake256_prf KYBER_NAMESPACE(kyber_shake256_prf)
static void kyber_shake256_prf(uint8_t *out, size_t outlen, const uint8_t key[KYBER_SYMBYTES], uint8_t nonce);

#define XOF_BLOCKBYTES SHAKE128_RATE

#define hash_h(OUT, IN, INBYTES) sha3_256(OUT, IN, INBYTES)
#define hash_g(OUT, IN, INBYTES) sha3_512(OUT, IN, INBYTES)
#define xof_absorb(STATE, SEED, X, Y) kyber_shake128_absorb(STATE, SEED, X, Y)
#define xof_squeezeblocks(OUT, OUTBLOCKS, STATE) shake128_squeezeblocks(OUT, OUTBLOCKS, STATE)
#define prf(OUT, OUTBYTES, KEY, NONCE) kyber_shake256_prf(OUT, OUTBYTES, KEY, NONCE)
#define kdf(OUT, IN, INBYTES) shake256(OUT, KYBER_SSBYTES, IN, INBYTES)
/** end: ref/symmetric.h **/

/** begin: ref/kem.h **/
#define CRYPTO_SECRETKEYBYTES KYBER_SECRETKEYBYTES
#define CRYPTO_PUBLICKEYBYTES KYBER_PUBLICKEYBYTES
#define CRYPTO_CIPHERTEXTBYTES KYBER_CIPHERTEXTBYTES
#define CRYPTO_BYTES KYBER_SSBYTES

#if (KYBER_K == 2)
#define CRYPTO_ALGNAME "Kyber512"
#elif (KYBER_K == 3)
#define CRYPTO_ALGNAME "Kyber768"
#elif (KYBER_K == 4)
#define CRYPTO_ALGNAME "Kyber1024"
#endif

#define crypto_kem_keypair_derand KYBER_NAMESPACE(keypair_derand)
int crypto_kem_keypair_derand(uint8_t *pk, uint8_t *sk, const uint8_t *coins);

#define crypto_kem_keypair KYBER_NAMESPACE(keypair)
int crypto_kem_keypair(uint8_t *pk, uint8_t *sk);

#define crypto_kem_enc_derand KYBER_NAMESPACE(enc_derand)
int crypto_kem_enc_derand(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);

#define crypto_kem_enc KYBER_NAMESPACE(enc)
int crypto_kem_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);

#define crypto_kem_dec KYBER_NAMESPACE(dec)
int crypto_kem_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);
/** end: ref/kem.h **/

/** begin: ref/reduce.c **/
/*************************************************
* Name:        montgomery_reduce
*
* Description: Montgomery reduction; given a 32-bit integer a, computes
*              16-bit integer congruent to a * R^-1 mod q, where R=2^16
*
* Arguments:   - int32_t a: input integer to be reduced;
*                           has to be in {-q2^15,...,q2^15-1}
*
* Returns:     integer in {-q+1,...,q-1} congruent to a * R^-1 modulo q.
**************************************************/
static int16_t
montgomery_reduce(int32_t a)
{
    int16_t t;

    t = (int16_t)a * QINV;
    t = (a - (int32_t)t * KYBER_Q) >> 16;
    return t;
}

/*************************************************
* Name:        barrett_reduce
*
* Description: Barrett reduction; given a 16-bit integer a, computes
*              centered representative congruent to a mod q in {-(q-1)/2,...,(q-1)/2}
*
* Arguments:   - int16_t a: input integer to be reduced
*
* Returns:     integer in {-(q-1)/2,...,(q-1)/2} congruent to a modulo q.
**************************************************/
static int16_t
barrett_reduce(int16_t a)
{
    int16_t t;
    const int16_t v = ((1 << 26) + KYBER_Q / 2) / KYBER_Q;

    t = ((int32_t)v * a + (1 << 25)) >> 26;
    t *= KYBER_Q;
    return a - t;
}
/** end: ref/reduce.c **/

/** begin: ref/cbd.c **/
/*************************************************
* Name:        load32_littleendian
*
* Description: load 4 bytes into a 32-bit integer
*              in little-endian order
*
* Arguments:   - const uint8_t *x: pointer to input byte array
*
* Returns 32-bit unsigned integer loaded from x
**************************************************/
static uint32_t
load32_littleendian(const uint8_t x[4])
{
    uint32_t r;
    r = (uint32_t)x[0];
    r |= (uint32_t)x[1] << 8;
    r |= (uint32_t)x[2] << 16;
    r |= (uint32_t)x[3] << 24;
    return r;
}

/*************************************************
* Name:        load24_littleendian
*
* Description: load 3 bytes into a 32-bit integer
*              in little-endian order.
*              This function is only needed for Kyber-512
*
* Arguments:   - const uint8_t *x: pointer to input byte array
*
* Returns 32-bit unsigned integer loaded from x (most significant byte is zero)
**************************************************/
#if KYBER_ETA1 == 3
static uint32_t
load24_littleendian(const uint8_t x[3])
{
    uint32_t r;
    r = (uint32_t)x[0];
    r |= (uint32_t)x[1] << 8;
    r |= (uint32_t)x[2] << 16;
    return r;
}
#endif

/*************************************************
* Name:        cbd2
*
* Description: Given an array of uniformly random bytes, compute
*              polynomial with coefficients distributed according to
*              a centered binomial distribution with parameter eta=2
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *buf: pointer to input byte array
**************************************************/
static void
cbd2(poly *r, const uint8_t buf[2 * KYBER_N / 4])
{
    unsigned int i, j;
    uint32_t t, d;
    int16_t a, b;

    for (i = 0; i < KYBER_N / 8; i++) {
        t = load32_littleendian(buf + 4 * i);
        d = t & 0x55555555;
        d += (t >> 1) & 0x55555555;

        for (j = 0; j < 8; j++) {
            a = (d >> (4 * j + 0)) & 0x3;
            b = (d >> (4 * j + 2)) & 0x3;
            r->coeffs[8 * i + j] = a - b;
        }
    }
}

/*************************************************
* Name:        cbd3
*
* Description: Given an array of uniformly random bytes, compute
*              polynomial with coefficients distributed according to
*              a centered binomial distribution with parameter eta=3.
*              This function is only needed for Kyber-512
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *buf: pointer to input byte array
**************************************************/
#if KYBER_ETA1 == 3
static void
cbd3(poly *r, const uint8_t buf[3 * KYBER_N / 4])
{
    unsigned int i, j;
    uint32_t t, d;
    int16_t a, b;

    for (i = 0; i < KYBER_N / 4; i++) {
        t = load24_littleendian(buf + 3 * i);
        d = t & 0x00249249;
        d += (t >> 1) & 0x00249249;
        d += (t >> 2) & 0x00249249;

        for (j = 0; j < 4; j++) {
            a = (d >> (6 * j + 0)) & 0x7;
            b = (d >> (6 * j + 3)) & 0x7;
            r->coeffs[4 * i + j] = a - b;
        }
    }
}
#endif

static void
poly_cbd_eta1(poly *r, const uint8_t buf[KYBER_ETA1 * KYBER_N / 4])
{
#if KYBER_ETA1 == 2
    cbd2(r, buf);
#elif KYBER_ETA1 == 3
    cbd3(r, buf);
#else
#error "This implementation requires eta1 in {2,3}"
#endif
}

static void
poly_cbd_eta2(poly *r, const uint8_t buf[KYBER_ETA2 * KYBER_N / 4])
{
#if KYBER_ETA2 == 2
    cbd2(r, buf);
#else
#error "This implementation requires eta2 = 2"
#endif
}
/** end: ref/cbd.c **/

/** begin: ref/ntt.c **/
/* Code to generate zetas and zetas_inv used in the number-theoretic transform:

#define KYBER_ROOT_OF_UNITY 17

static const uint8_t tree[128] = {
  0, 64, 32, 96, 16, 80, 48, 112, 8, 72, 40, 104, 24, 88, 56, 120,
  4, 68, 36, 100, 20, 84, 52, 116, 12, 76, 44, 108, 28, 92, 60, 124,
  2, 66, 34, 98, 18, 82, 50, 114, 10, 74, 42, 106, 26, 90, 58, 122,
  6, 70, 38, 102, 22, 86, 54, 118, 14, 78, 46, 110, 30, 94, 62, 126,
  1, 65, 33, 97, 17, 81, 49, 113, 9, 73, 41, 105, 25, 89, 57, 121,
  5, 69, 37, 101, 21, 85, 53, 117, 13, 77, 45, 109, 29, 93, 61, 125,
  3, 67, 35, 99, 19, 83, 51, 115, 11, 75, 43, 107, 27, 91, 59, 123,
  7, 71, 39, 103, 23, 87, 55, 119, 15, 79, 47, 111, 31, 95, 63, 127
};

static void init_ntt() {
  unsigned int i;
  int16_t tmp[128];

  tmp[0] = MONT;
  for(i=1;i<128;i++)
    tmp[i] = fqmul(tmp[i-1],MONT*KYBER_ROOT_OF_UNITY % KYBER_Q);

  for(i=0;i<128;i++) {
    zetas[i] = tmp[tree[i]];
    if(zetas[i] > KYBER_Q/2)
      zetas[i] -= KYBER_Q;
    if(zetas[i] < -KYBER_Q/2)
      zetas[i] += KYBER_Q;
  }
}
*/

const int16_t zetas[128] = {
    -1044, -758, -359, -1517, 1493, 1422, 287, 202,
    -171, 622, 1577, 182, 962, -1202, -1474, 1468,
    573, -1325, 264, 383, -829, 1458, -1602, -130,
    -681, 1017, 732, 608, -1542, 411, -205, -1571,
    1223, 652, -552, 1015, -1293, 1491, -282, -1544,
    516, -8, -320, -666, -1618, -1162, 126, 1469,
    -853, -90, -271, 830, 107, -1421, -247, -951,
    -398, 961, -1508, -725, 448, -1065, 677, -1275,
    -1103, 430, 555, 843, -1251, 871, 1550, 105,
    422, 587, 177, -235, -291, -460, 1574, 1653,
    -246, 778, 1159, -147, -777, 1483, -602, 1119,
    -1590, 644, -872, 349, 418, 329, -156, -75,
    817, 1097, 603, 610, 1322, -1285, -1465, 384,
    -1215, -136, 1218, -1335, -874, 220, -1187, -1659,
    -1185, -1530, -1278, 794, -1510, -854, -870, 478,
    -108, -308, 996, 991, 958, -1460, 1522, 1628
};

/*************************************************
* Name:        fqmul
*
* Description: Multiplication followed by Montgomery reduction
*
* Arguments:   - int16_t a: first factor
*              - int16_t b: second factor
*
* Returns 16-bit integer congruent to a*b*R^{-1} mod q
**************************************************/
static int16_t
fqmul(int16_t a, int16_t b)
{
    return montgomery_reduce((int32_t)a * b);
}

/*************************************************
* Name:        ntt
*
* Description: Inplace number-theoretic transform (NTT) in Rq.
*              input is in standard order, output is in bitreversed order
*
* Arguments:   - int16_t r[256]: pointer to input/output vector of elements of Zq
**************************************************/
static void
ntt(int16_t r[256])
{
    unsigned int len, start, j, k;
    int16_t t, zeta;

    k = 1;
    for (len = 128; len >= 2; len >>= 1) {
        for (start = 0; start < 256; start = j + len) {
            zeta = zetas[k++];
            for (j = start; j < start + len; j++) {
                t = fqmul(zeta, r[j + len]);
                r[j + len] = r[j] - t;
                r[j] = r[j] + t;
            }
        }
    }
}

/*************************************************
* Name:        invntt_tomont
*
* Description: Inplace inverse number-theoretic transform in Rq and
*              multiplication by Montgomery factor 2^16.
*              Input is in bitreversed order, output is in standard order
*
* Arguments:   - int16_t r[256]: pointer to input/output vector of elements of Zq
**************************************************/
static void
invntt(int16_t r[256])
{
    unsigned int start, len, j, k;
    int16_t t, zeta;
    const int16_t f = 1441; // mont^2/128

    k = 127;
    for (len = 2; len <= 128; len <<= 1) {
        for (start = 0; start < 256; start = j + len) {
            zeta = zetas[k--];
            for (j = start; j < start + len; j++) {
                t = r[j];
                r[j] = barrett_reduce(t + r[j + len]);
                r[j + len] = r[j + len] - t;
                r[j + len] = fqmul(zeta, r[j + len]);
            }
        }
    }

    for (j = 0; j < 256; j++)
        r[j] = fqmul(r[j], f);
}

/*************************************************
* Name:        basemul
*
* Description: Multiplication of polynomials in Zq[X]/(X^2-zeta)
*              used for multiplication of elements in Rq in NTT domain
*
* Arguments:   - int16_t r[2]: pointer to the output polynomial
*              - const int16_t a[2]: pointer to the first factor
*              - const int16_t b[2]: pointer to the second factor
*              - int16_t zeta: integer defining the reduction polynomial
**************************************************/
static void
basemul(int16_t r[2], const int16_t a[2], const int16_t b[2], int16_t zeta)
{
    r[0] = fqmul(a[1], b[1]);
    r[0] = fqmul(r[0], zeta);
    r[0] += fqmul(a[0], b[0]);
    r[1] = fqmul(a[0], b[1]);
    r[1] += fqmul(a[1], b[0]);
}
/** end: ref/ntt.c **/

/** begin: ref/poly.c **/
/*************************************************
* Name:        poly_compress
*
* Description: Compression and subsequent serialization of a polynomial
*
* Arguments:   - uint8_t *r: pointer to output byte array
*                            (of length KYBER_POLYCOMPRESSEDBYTES)
*              - const poly *a: pointer to input polynomial
**************************************************/
static void
poly_compress(uint8_t r[KYBER_POLYCOMPRESSEDBYTES], const poly *a)
{
    unsigned int i, j;
    int16_t u;
    uint32_t d0;
    uint8_t t[8];

#if (KYBER_POLYCOMPRESSEDBYTES == 128)
    for (i = 0; i < KYBER_N / 8; i++) {
        for (j = 0; j < 8; j++) {
            // map to positive standard representatives
            u = a->coeffs[8 * i + j];
            u += (u >> 15) & KYBER_Q;
            /*    t[j] = ((((uint16_t)u << 4) + KYBER_Q/2)/KYBER_Q) & 15; */
            d0 = u << 4;
            d0 += 1665;
            d0 *= 80635;
            d0 >>= 28;
            t[j] = d0 & 0xf;
        }

        r[0] = t[0] | (t[1] << 4);
        r[1] = t[2] | (t[3] << 4);
        r[2] = t[4] | (t[5] << 4);
        r[3] = t[6] | (t[7] << 4);
        r += 4;
    }
#elif (KYBER_POLYCOMPRESSEDBYTES == 160)
    for (i = 0; i < KYBER_N / 8; i++) {
        for (j = 0; j < 8; j++) {
            // map to positive standard representatives
            u = a->coeffs[8 * i + j];
            u += (u >> 15) & KYBER_Q;
            /*      t[j] = ((((uint32_t)u << 5) + KYBER_Q/2)/KYBER_Q) & 31; */
            d0 = u << 5;
            d0 += 1664;
            d0 *= 40318;
            d0 >>= 27;
            t[j] = d0 & 0x1f;
        }

        r[0] = (t[0] >> 0) | (t[1] << 5);
        r[1] = (t[1] >> 3) | (t[2] << 2) | (t[3] << 7);
        r[2] = (t[3] >> 1) | (t[4] << 4);
        r[3] = (t[4] >> 4) | (t[5] << 1) | (t[6] << 6);
        r[4] = (t[6] >> 2) | (t[7] << 3);
        r += 5;
    }
#else
#error "KYBER_POLYCOMPRESSEDBYTES needs to be in {128, 160}"
#endif
}

/*************************************************
* Name:        poly_decompress
*
* Description: De-serialization and subsequent decompression of a polynomial;
*              approximate inverse of poly_compress
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *a: pointer to input byte array
*                                  (of length KYBER_POLYCOMPRESSEDBYTES bytes)
**************************************************/
static void
poly_decompress(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES])
{
    unsigned int i;

#if (KYBER_POLYCOMPRESSEDBYTES == 128)
    for (i = 0; i < KYBER_N / 2; i++) {
        r->coeffs[2 * i + 0] = (((uint16_t)(a[0] & 15) * KYBER_Q) + 8) >> 4;
        r->coeffs[2 * i + 1] = (((uint16_t)(a[0] >> 4) * KYBER_Q) + 8) >> 4;
        a += 1;
    }
#elif (KYBER_POLYCOMPRESSEDBYTES == 160)
    unsigned int j;
    uint8_t t[8];
    for (i = 0; i < KYBER_N / 8; i++) {
        t[0] = (a[0] >> 0);
        t[1] = (a[0] >> 5) | (a[1] << 3);
        t[2] = (a[1] >> 2);
        t[3] = (a[1] >> 7) | (a[2] << 1);
        t[4] = (a[2] >> 4) | (a[3] << 4);
        t[5] = (a[3] >> 1);
        t[6] = (a[3] >> 6) | (a[4] << 2);
        t[7] = (a[4] >> 3);
        a += 5;

        for (j = 0; j < 8; j++)
            r->coeffs[8 * i + j] = ((uint32_t)(t[j] & 31) * KYBER_Q + 16) >> 5;
    }
#else
#error "KYBER_POLYCOMPRESSEDBYTES needs to be in {128, 160}"
#endif
}

/*************************************************
* Name:        poly_tobytes
*
* Description: Serialization of a polynomial
*
* Arguments:   - uint8_t *r: pointer to output byte array
*                            (needs space for KYBER_POLYBYTES bytes)
*              - const poly *a: pointer to input polynomial
**************************************************/
static void
poly_tobytes(uint8_t r[KYBER_POLYBYTES], const poly *a)
{
    unsigned int i;
    uint16_t t0, t1;

    for (i = 0; i < KYBER_N / 2; i++) {
        // map to positive standard representatives
        t0 = a->coeffs[2 * i];
        t0 += ((int16_t)t0 >> 15) & KYBER_Q;
        t1 = a->coeffs[2 * i + 1];
        t1 += ((int16_t)t1 >> 15) & KYBER_Q;
        r[3 * i + 0] = (t0 >> 0);
        r[3 * i + 1] = (t0 >> 8) | (t1 << 4);
        r[3 * i + 2] = (t1 >> 4);
    }
}

/*************************************************
* Name:        poly_frombytes
*
* Description: De-serialization of a polynomial;
*              inverse of poly_tobytes
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *a: pointer to input byte array
*                                  (of KYBER_POLYBYTES bytes)
**************************************************/
static void
poly_frombytes(poly *r, const uint8_t a[KYBER_POLYBYTES])
{
    unsigned int i;
    for (i = 0; i < KYBER_N / 2; i++) {
        r->coeffs[2 * i] = ((a[3 * i + 0] >> 0) | ((uint16_t)a[3 * i + 1] << 8)) & 0xFFF;
        r->coeffs[2 * i + 1] = ((a[3 * i + 1] >> 4) | ((uint16_t)a[3 * i + 2] << 4)) & 0xFFF;
    }
}

/*************************************************
* Name:        poly_frommsg
*
* Description: Convert 32-byte message to polynomial
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *msg: pointer to input message
**************************************************/
static void
poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
{
    unsigned int i, j;
    int16_t mask;

#if (KYBER_INDCPA_MSGBYTES != KYBER_N / 8)
#error "KYBER_INDCPA_MSGBYTES must be equal to KYBER_N/8 bytes!"
#endif

    for (i = 0; i < KYBER_N / 8; i++) {
        for (j = 0; j < 8; j++) {
            mask = -(int16_t)((msg[i] >> j) & 1);
            r->coeffs[8 * i + j] = mask & ((KYBER_Q + 1) / 2);
        }
    }
}

/*************************************************
* Name:        poly_tomsg
*
* Description: Convert polynomial to 32-byte message
*
* Arguments:   - uint8_t *msg: pointer to output message
*              - const poly *a: pointer to input polynomial
**************************************************/
static void
poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *a)
{
    unsigned int i, j;
    uint32_t t;

    for (i = 0; i < KYBER_N / 8; i++) {
        msg[i] = 0;
        for (j = 0; j < 8; j++) {
            t = a->coeffs[8 * i + j];
            // t += ((int16_t)t >> 15) & KYBER_Q;
            // t  = (((t << 1) + KYBER_Q/2)/KYBER_Q) & 1;
            t <<= 1;
            t += 1665;
            t *= 80635;
            t >>= 28;
            t &= 1;
            msg[i] |= t << j;
        }
    }
}

/*************************************************
* Name:        poly_getnoise_eta1
*
* Description: Sample a polynomial deterministically from a seed and a nonce,
*              with output polynomial close to centered binomial distribution
*              with parameter KYBER_ETA1
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *seed: pointer to input seed
*                                     (of length KYBER_SYMBYTES bytes)
*              - uint8_t nonce: one-byte input nonce
**************************************************/
static void
poly_getnoise_eta1(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce)
{
    uint8_t buf[KYBER_ETA1 * KYBER_N / 4];
    prf(buf, sizeof(buf), seed, nonce);
    poly_cbd_eta1(r, buf);
}

/*************************************************
* Name:        poly_getnoise_eta2
*
* Description: Sample a polynomial deterministically from a seed and a nonce,
*              with output polynomial close to centered binomial distribution
*              with parameter KYBER_ETA2
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *seed: pointer to input seed
*                                     (of length KYBER_SYMBYTES bytes)
*              - uint8_t nonce: one-byte input nonce
**************************************************/
static void
poly_getnoise_eta2(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce)
{
    uint8_t buf[KYBER_ETA2 * KYBER_N / 4];
    prf(buf, sizeof(buf), seed, nonce);
    poly_cbd_eta2(r, buf);
}

/*************************************************
* Name:        poly_ntt
*
* Description: Computes negacyclic number-theoretic transform (NTT) of
*              a polynomial in place;
*              inputs assumed to be in normal order, output in bitreversed order
*
* Arguments:   - uint16_t *r: pointer to in/output polynomial
**************************************************/
static void
poly_ntt(poly *r)
{
    ntt(r->coeffs);
    poly_reduce(r);
}

/*************************************************
* Name:        poly_invntt_tomont
*
* Description: Computes inverse of negacyclic number-theoretic transform (NTT)
*              of a polynomial in place;
*              inputs assumed to be in bitreversed order, output in normal order
*
* Arguments:   - uint16_t *a: pointer to in/output polynomial
**************************************************/
static void
poly_invntt_tomont(poly *r)
{
    invntt(r->coeffs);
}

/*************************************************
* Name:        poly_basemul_montgomery
*
* Description: Multiplication of two polynomials in NTT domain
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const poly *a: pointer to first input polynomial
*              - const poly *b: pointer to second input polynomial
**************************************************/
static void
poly_basemul_montgomery(poly *r, const poly *a, const poly *b)
{
    unsigned int i;
    for (i = 0; i < KYBER_N / 4; i++) {
        basemul(&r->coeffs[4 * i], &a->coeffs[4 * i], &b->coeffs[4 * i], zetas[64 + i]);
        basemul(&r->coeffs[4 * i + 2], &a->coeffs[4 * i + 2], &b->coeffs[4 * i + 2], -zetas[64 + i]);
    }
}

/*************************************************
* Name:        poly_tomont
*
* Description: Inplace conversion of all coefficients of a polynomial
*              from normal domain to Montgomery domain
*
* Arguments:   - poly *r: pointer to input/output polynomial
**************************************************/
static void
poly_tomont(poly *r)
{
    unsigned int i;
    const int16_t f = (1ULL << 32) % KYBER_Q;
    for (i = 0; i < KYBER_N; i++)
        r->coeffs[i] = montgomery_reduce((int32_t)r->coeffs[i] * f);
}

/*************************************************
* Name:        poly_reduce
*
* Description: Applies Barrett reduction to all coefficients of a polynomial
*              for details of the Barrett reduction see comments in reduce.c
*
* Arguments:   - poly *r: pointer to input/output polynomial
**************************************************/
static void
poly_reduce(poly *r)
{
    unsigned int i;
    for (i = 0; i < KYBER_N; i++)
        r->coeffs[i] = barrett_reduce(r->coeffs[i]);
}

/*************************************************
* Name:        poly_add
*
* Description: Add two polynomials; no modular reduction is performed
*
* Arguments: - poly *r: pointer to output polynomial
*            - const poly *a: pointer to first input polynomial
*            - const poly *b: pointer to second input polynomial
**************************************************/
static void
poly_add(poly *r, const poly *a, const poly *b)
{
    unsigned int i;
    for (i = 0; i < KYBER_N; i++)
        r->coeffs[i] = a->coeffs[i] + b->coeffs[i];
}

/*************************************************
* Name:        poly_sub
*
* Description: Subtract two polynomials; no modular reduction is performed
*
* Arguments: - poly *r:       pointer to output polynomial
*            - const poly *a: pointer to first input polynomial
*            - const poly *b: pointer to second input polynomial
**************************************************/
static void
poly_sub(poly *r, const poly *a, const poly *b)
{
    unsigned int i;
    for (i = 0; i < KYBER_N; i++)
        r->coeffs[i] = a->coeffs[i] - b->coeffs[i];
}
/** end: ref/poly.c **/

/** begin: ref/polyvec.c **/
/*************************************************
* Name:        polyvec_compress
*
* Description: Compress and serialize vector of polynomials
*
* Arguments:   - uint8_t *r: pointer to output byte array
*                            (needs space for KYBER_POLYVECCOMPRESSEDBYTES)
*              - const polyvec *a: pointer to input vector of polynomials
**************************************************/
static void
polyvec_compress(uint8_t r[KYBER_POLYVECCOMPRESSEDBYTES], const polyvec *a)
{
    unsigned int i, j, k;
    uint64_t d0;

#if (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * 352))
    uint16_t t[8];
    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_N / 8; j++) {
            for (k = 0; k < 8; k++) {
                t[k] = a->vec[i].coeffs[8 * j + k];
                t[k] += ((int16_t)t[k] >> 15) & KYBER_Q;
                /*      t[k]  = ((((uint32_t)t[k] << 11) + KYBER_Q/2)/KYBER_Q) & 0x7ff; */
                d0 = t[k];
                d0 <<= 11;
                d0 += 1664;
                d0 *= 645084;
                d0 >>= 31;
                t[k] = d0 & 0x7ff;
            }

            r[0] = (t[0] >> 0);
            r[1] = (t[0] >> 8) | (t[1] << 3);
            r[2] = (t[1] >> 5) | (t[2] << 6);
            r[3] = (t[2] >> 2);
            r[4] = (t[2] >> 10) | (t[3] << 1);
            r[5] = (t[3] >> 7) | (t[4] << 4);
            r[6] = (t[4] >> 4) | (t[5] << 7);
            r[7] = (t[5] >> 1);
            r[8] = (t[5] >> 9) | (t[6] << 2);
            r[9] = (t[6] >> 6) | (t[7] << 5);
            r[10] = (t[7] >> 3);
            r += 11;
        }
    }
#elif (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * 320))
    uint16_t t[4];
    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_N / 4; j++) {
            for (k = 0; k < 4; k++) {
                t[k] = a->vec[i].coeffs[4 * j + k];
                t[k] += ((int16_t)t[k] >> 15) & KYBER_Q;
                /*      t[k]  = ((((uint32_t)t[k] << 10) + KYBER_Q/2)/ KYBER_Q) & 0x3ff; */
                d0 = t[k];
                d0 <<= 10;
                d0 += 1665;
                d0 *= 1290167;
                d0 >>= 32;
                t[k] = d0 & 0x3ff;
            }

            r[0] = (t[0] >> 0);
            r[1] = (t[0] >> 8) | (t[1] << 2);
            r[2] = (t[1] >> 6) | (t[2] << 4);
            r[3] = (t[2] >> 4) | (t[3] << 6);
            r[4] = (t[3] >> 2);
            r += 5;
        }
    }
#else
#error "KYBER_POLYVECCOMPRESSEDBYTES needs to be in {320*KYBER_K, 352*KYBER_K}"
#endif
}

/*************************************************
* Name:        polyvec_decompress
*
* Description: De-serialize and decompress vector of polynomials;
*              approximate inverse of polyvec_compress
*
* Arguments:   - polyvec *r:       pointer to output vector of polynomials
*              - const uint8_t *a: pointer to input byte array
*                                  (of length KYBER_POLYVECCOMPRESSEDBYTES)
**************************************************/
static void
polyvec_decompress(polyvec *r, const uint8_t a[KYBER_POLYVECCOMPRESSEDBYTES])
{
    unsigned int i, j, k;

#if (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * 352))
    uint16_t t[8];
    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_N / 8; j++) {
            t[0] = (a[0] >> 0) | ((uint16_t)a[1] << 8);
            t[1] = (a[1] >> 3) | ((uint16_t)a[2] << 5);
            t[2] = (a[2] >> 6) | ((uint16_t)a[3] << 2) | ((uint16_t)a[4] << 10);
            t[3] = (a[4] >> 1) | ((uint16_t)a[5] << 7);
            t[4] = (a[5] >> 4) | ((uint16_t)a[6] << 4);
            t[5] = (a[6] >> 7) | ((uint16_t)a[7] << 1) | ((uint16_t)a[8] << 9);
            t[6] = (a[8] >> 2) | ((uint16_t)a[9] << 6);
            t[7] = (a[9] >> 5) | ((uint16_t)a[10] << 3);
            a += 11;

            for (k = 0; k < 8; k++)
                r->vec[i].coeffs[8 * j + k] = ((uint32_t)(t[k] & 0x7FF) * KYBER_Q + 1024) >> 11;
        }
    }
#elif (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * 320))
    uint16_t t[4];
    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_N / 4; j++) {
            t[0] = (a[0] >> 0) | ((uint16_t)a[1] << 8);
            t[1] = (a[1] >> 2) | ((uint16_t)a[2] << 6);
            t[2] = (a[2] >> 4) | ((uint16_t)a[3] << 4);
            t[3] = (a[3] >> 6) | ((uint16_t)a[4] << 2);
            a += 5;

            for (k = 0; k < 4; k++)
                r->vec[i].coeffs[4 * j + k] = ((uint32_t)(t[k] & 0x3FF) * KYBER_Q + 512) >> 10;
        }
    }
#else
#error "KYBER_POLYVECCOMPRESSEDBYTES needs to be in {320*KYBER_K, 352*KYBER_K}"
#endif
}

/*************************************************
* Name:        polyvec_tobytes
*
* Description: Serialize vector of polynomials
*
* Arguments:   - uint8_t *r: pointer to output byte array
*                            (needs space for KYBER_POLYVECBYTES)
*              - const polyvec *a: pointer to input vector of polynomials
**************************************************/
static void
polyvec_tobytes(uint8_t r[KYBER_POLYVECBYTES], const polyvec *a)
{
    unsigned int i;
    for (i = 0; i < KYBER_K; i++)
        poly_tobytes(r + i * KYBER_POLYBYTES, &a->vec[i]);
}

/*************************************************
* Name:        polyvec_frombytes
*
* Description: De-serialize vector of polynomials;
*              inverse of polyvec_tobytes
*
* Arguments:   - uint8_t *r:       pointer to output byte array
*              - const polyvec *a: pointer to input vector of polynomials
*                                  (of length KYBER_POLYVECBYTES)
**************************************************/
static void
polyvec_frombytes(polyvec *r, const uint8_t a[KYBER_POLYVECBYTES])
{
    unsigned int i;
    for (i = 0; i < KYBER_K; i++)
        poly_frombytes(&r->vec[i], a + i * KYBER_POLYBYTES);
}

/*************************************************
* Name:        polyvec_ntt
*
* Description: Apply forward NTT to all elements of a vector of polynomials
*
* Arguments:   - polyvec *r: pointer to in/output vector of polynomials
**************************************************/
static void
polyvec_ntt(polyvec *r)
{
    unsigned int i;
    for (i = 0; i < KYBER_K; i++)
        poly_ntt(&r->vec[i]);
}

/*************************************************
* Name:        polyvec_invntt_tomont
*
* Description: Apply inverse NTT to all elements of a vector of polynomials
*              and multiply by Montgomery factor 2^16
*
* Arguments:   - polyvec *r: pointer to in/output vector of polynomials
**************************************************/
static void
polyvec_invntt_tomont(polyvec *r)
{
    unsigned int i;
    for (i = 0; i < KYBER_K; i++)
        poly_invntt_tomont(&r->vec[i]);
}

/*************************************************
* Name:        polyvec_basemul_acc_montgomery
*
* Description: Multiply elements of a and b in NTT domain, accumulate into r,
*              and multiply by 2^-16.
*
* Arguments: - poly *r: pointer to output polynomial
*            - const polyvec *a: pointer to first input vector of polynomials
*            - const polyvec *b: pointer to second input vector of polynomials
**************************************************/
static void
polyvec_basemul_acc_montgomery(poly *r, const polyvec *a, const polyvec *b)
{
    unsigned int i;
    poly t;

    poly_basemul_montgomery(r, &a->vec[0], &b->vec[0]);
    for (i = 1; i < KYBER_K; i++) {
        poly_basemul_montgomery(&t, &a->vec[i], &b->vec[i]);
        poly_add(r, r, &t);
    }

    poly_reduce(r);
}

/*************************************************
* Name:        polyvec_reduce
*
* Description: Applies Barrett reduction to each coefficient
*              of each element of a vector of polynomials;
*              for details of the Barrett reduction see comments in reduce.c
*
* Arguments:   - polyvec *r: pointer to input/output polynomial
**************************************************/
static void
polyvec_reduce(polyvec *r)
{
    unsigned int i;
    for (i = 0; i < KYBER_K; i++)
        poly_reduce(&r->vec[i]);
}

/*************************************************
* Name:        polyvec_add
*
* Description: Add vectors of polynomials
*
* Arguments: - polyvec *r: pointer to output vector of polynomials
*            - const polyvec *a: pointer to first input vector of polynomials
*            - const polyvec *b: pointer to second input vector of polynomials
**************************************************/
static void
polyvec_add(polyvec *r, const polyvec *a, const polyvec *b)
{
    unsigned int i;
    for (i = 0; i < KYBER_K; i++)
        poly_add(&r->vec[i], &a->vec[i], &b->vec[i]);
}
/** end: ref/polyvec.c **/

/** begin: ref/indcpa.c **/
/*************************************************
* Name:        pack_pk
*
* Description: Serialize the public key as concatenation of the
*              serialized vector of polynomials pk
*              and the public seed used to generate the matrix A.
*
* Arguments:   uint8_t *r: pointer to the output serialized public key
*              polyvec *pk: pointer to the input public-key polyvec
*              const uint8_t *seed: pointer to the input public seed
**************************************************/
static void
pack_pk(uint8_t r[KYBER_INDCPA_PUBLICKEYBYTES],
        polyvec *pk,
        const uint8_t seed[KYBER_SYMBYTES])
{
    size_t i;
    polyvec_tobytes(r, pk);
    for (i = 0; i < KYBER_SYMBYTES; i++)
        r[i + KYBER_POLYVECBYTES] = seed[i];
}

/*************************************************
* Name:        unpack_pk
*
* Description: De-serialize public key from a byte array;
*              approximate inverse of pack_pk
*
* Arguments:   - polyvec *pk: pointer to output public-key polynomial vector
*              - uint8_t *seed: pointer to output seed to generate matrix A
*              - const uint8_t *packedpk: pointer to input serialized public key
**************************************************/
static void
unpack_pk(polyvec *pk,
          uint8_t seed[KYBER_SYMBYTES],
          const uint8_t packedpk[KYBER_INDCPA_PUBLICKEYBYTES])
{
    size_t i;
    polyvec_frombytes(pk, packedpk);
    for (i = 0; i < KYBER_SYMBYTES; i++)
        seed[i] = packedpk[i + KYBER_POLYVECBYTES];
}

/*************************************************
* Name:        pack_sk
*
* Description: Serialize the secret key
*
* Arguments:   - uint8_t *r: pointer to output serialized secret key
*              - polyvec *sk: pointer to input vector of polynomials (secret key)
**************************************************/
static void
pack_sk(uint8_t r[KYBER_INDCPA_SECRETKEYBYTES], polyvec *sk)
{
    polyvec_tobytes(r, sk);
}

/*************************************************
* Name:        unpack_sk
*
* Description: De-serialize the secret key; inverse of pack_sk
*
* Arguments:   - polyvec *sk: pointer to output vector of polynomials (secret key)
*              - const uint8_t *packedsk: pointer to input serialized secret key
**************************************************/
static void
unpack_sk(polyvec *sk, const uint8_t packedsk[KYBER_INDCPA_SECRETKEYBYTES])
{
    polyvec_frombytes(sk, packedsk);
}

/*************************************************
* Name:        pack_ciphertext
*
* Description: Serialize the ciphertext as concatenation of the
*              compressed and serialized vector of polynomials b
*              and the compressed and serialized polynomial v
*
* Arguments:   uint8_t *r: pointer to the output serialized ciphertext
*              poly *pk: pointer to the input vector of polynomials b
*              poly *v: pointer to the input polynomial v
**************************************************/
static void
pack_ciphertext(uint8_t r[KYBER_INDCPA_BYTES], polyvec *b, poly *v)
{
    polyvec_compress(r, b);
    poly_compress(r + KYBER_POLYVECCOMPRESSEDBYTES, v);
}

/*************************************************
* Name:        unpack_ciphertext
*
* Description: De-serialize and decompress ciphertext from a byte array;
*              approximate inverse of pack_ciphertext
*
* Arguments:   - polyvec *b: pointer to the output vector of polynomials b
*              - poly *v: pointer to the output polynomial v
*              - const uint8_t *c: pointer to the input serialized ciphertext
**************************************************/
static void
unpack_ciphertext(polyvec *b, poly *v, const uint8_t c[KYBER_INDCPA_BYTES])
{
    polyvec_decompress(b, c);
    poly_decompress(v, c + KYBER_POLYVECCOMPRESSEDBYTES);
}

/*************************************************
* Name:        rej_uniform
*
* Description: Run rejection sampling on uniform random bytes to generate
*              uniform random integers mod q
*
* Arguments:   - int16_t *r: pointer to output buffer
*              - unsigned int len: requested number of 16-bit integers (uniform mod q)
*              - const uint8_t *buf: pointer to input buffer (assumed to be uniformly random bytes)
*              - unsigned int buflen: length of input buffer in bytes
*
* Returns number of sampled 16-bit integers (at most len)
**************************************************/
static unsigned int
rej_uniform(int16_t *r,
            unsigned int len,
            const uint8_t *buf,
            unsigned int buflen)
{
    unsigned int ctr, pos;
    uint16_t val0, val1;

    ctr = pos = 0;
    while (ctr < len && pos + 3 <= buflen) {
        val0 = ((buf[pos + 0] >> 0) | ((uint16_t)buf[pos + 1] << 8)) & 0xFFF;
        val1 = ((buf[pos + 1] >> 4) | ((uint16_t)buf[pos + 2] << 4)) & 0xFFF;
        pos += 3;

        if (val0 < KYBER_Q)
            r[ctr++] = val0;
        if (ctr < len && val1 < KYBER_Q)
            r[ctr++] = val1;
    }

    return ctr;
}

#define gen_a(A, B) gen_matrix(A, B, 0)
#define gen_at(A, B) gen_matrix(A, B, 1)

/*************************************************
* Name:        gen_matrix
*
* Description: Deterministically generate matrix A (or the transpose of A)
*              from a seed. Entries of the matrix are polynomials that look
*              uniformly random. Performs rejection sampling on output of
*              a XOF
*
* Arguments:   - polyvec *a: pointer to ouptput matrix A
*              - const uint8_t *seed: pointer to input seed
*              - int transposed: boolean deciding whether A or A^T is generated
**************************************************/
#define GEN_MATRIX_NBLOCKS ((12 * KYBER_N / 8 * (1 << 12) / KYBER_Q + XOF_BLOCKBYTES) / XOF_BLOCKBYTES)
// Not static for benchmarking
static void
gen_matrix(polyvec *a, const uint8_t seed[KYBER_SYMBYTES], int transposed)
{
    unsigned int ctr, i, j, k;
    unsigned int buflen, off;
    uint8_t buf[GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES + 2];
    xof_state state;

    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_K; j++) {
            if (transposed)
                xof_absorb(&state, seed, i, j);
            else
                xof_absorb(&state, seed, j, i);

            xof_squeezeblocks(buf, GEN_MATRIX_NBLOCKS, &state);
            buflen = GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES;
            ctr = rej_uniform(a[i].vec[j].coeffs, KYBER_N, buf, buflen);

            while (ctr < KYBER_N) {
                off = buflen % 3;
                for (k = 0; k < off; k++)
                    buf[k] = buf[buflen - off + k];
                xof_squeezeblocks(buf + off, 1, &state);
                buflen = off + XOF_BLOCKBYTES;
                ctr += rej_uniform(a[i].vec[j].coeffs + ctr, KYBER_N - ctr, buf, buflen);
            }
        }
    }
}

/*************************************************
* Name:        indcpa_keypair_derand
*
* Description: Generates public and private key for the CPA-secure
*              public-key encryption scheme underlying Kyber
*
* Arguments:   - uint8_t *pk: pointer to output public key
*                             (of length KYBER_INDCPA_PUBLICKEYBYTES bytes)
*              - uint8_t *sk: pointer to output private key
*                             (of length KYBER_INDCPA_SECRETKEYBYTES bytes)
*              - const uint8_t *coins: pointer to input randomness
*                             (of length KYBER_SYMBYTES bytes)
**************************************************/
static void
indcpa_keypair_derand(uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                      uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES],
                      const uint8_t coins[KYBER_SYMBYTES])
{
    unsigned int i;
    uint8_t buf[2 * KYBER_SYMBYTES];
    const uint8_t *publicseed = buf;
    const uint8_t *noiseseed = buf + KYBER_SYMBYTES;
    uint8_t nonce = 0;
    polyvec a[KYBER_K], e, pkpv, skpv;

    hash_g(buf, coins, KYBER_SYMBYTES);

    gen_a(a, publicseed);

    for (i = 0; i < KYBER_K; i++)
        poly_getnoise_eta1(&skpv.vec[i], noiseseed, nonce++);
    for (i = 0; i < KYBER_K; i++)
        poly_getnoise_eta1(&e.vec[i], noiseseed, nonce++);

    polyvec_ntt(&skpv);
    polyvec_ntt(&e);

    // matrix-vector multiplication
    for (i = 0; i < KYBER_K; i++) {
        polyvec_basemul_acc_montgomery(&pkpv.vec[i], &a[i], &skpv);
        poly_tomont(&pkpv.vec[i]);
    }

    polyvec_add(&pkpv, &pkpv, &e);
    polyvec_reduce(&pkpv);

    pack_sk(sk, &skpv);
    pack_pk(pk, &pkpv, publicseed);
}

/*************************************************
* Name:        indcpa_enc
*
* Description: Encryption function of the CPA-secure
*              public-key encryption scheme underlying Kyber.
*
* Arguments:   - uint8_t *c: pointer to output ciphertext
*                            (of length KYBER_INDCPA_BYTES bytes)
*              - const uint8_t *m: pointer to input message
*                                  (of length KYBER_INDCPA_MSGBYTES bytes)
*              - const uint8_t *pk: pointer to input public key
*                                   (of length KYBER_INDCPA_PUBLICKEYBYTES)
*              - const uint8_t *coins: pointer to input random coins used as seed
*                                      (of length KYBER_SYMBYTES) to deterministically
*                                      generate all randomness
**************************************************/
static void
indcpa_enc(uint8_t c[KYBER_INDCPA_BYTES],
           const uint8_t m[KYBER_INDCPA_MSGBYTES],
           const uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
           const uint8_t coins[KYBER_SYMBYTES])
{
    unsigned int i;
    uint8_t seed[KYBER_SYMBYTES];
    uint8_t nonce = 0;
    polyvec sp, pkpv, ep, at[KYBER_K], b;
    poly v, k, epp;

    unpack_pk(&pkpv, seed, pk);
    poly_frommsg(&k, m);
    gen_at(at, seed);

    for (i = 0; i < KYBER_K; i++)
        poly_getnoise_eta1(sp.vec + i, coins, nonce++);
    for (i = 0; i < KYBER_K; i++)
        poly_getnoise_eta2(ep.vec + i, coins, nonce++);
    poly_getnoise_eta2(&epp, coins, nonce++);

    polyvec_ntt(&sp);

    // matrix-vector multiplication
    for (i = 0; i < KYBER_K; i++)
        polyvec_basemul_acc_montgomery(&b.vec[i], &at[i], &sp);

    polyvec_basemul_acc_montgomery(&v, &pkpv, &sp);

    polyvec_invntt_tomont(&b);
    poly_invntt_tomont(&v);

    polyvec_add(&b, &b, &ep);
    poly_add(&v, &v, &epp);
    poly_add(&v, &v, &k);
    polyvec_reduce(&b);
    poly_reduce(&v);

    pack_ciphertext(c, &b, &v);
}

/*************************************************
* Name:        indcpa_dec
*
* Description: Decryption function of the CPA-secure
*              public-key encryption scheme underlying Kyber.
*
* Arguments:   - uint8_t *m: pointer to output decrypted message
*                            (of length KYBER_INDCPA_MSGBYTES)
*              - const uint8_t *c: pointer to input ciphertext
*                                  (of length KYBER_INDCPA_BYTES)
*              - const uint8_t *sk: pointer to input secret key
*                                   (of length KYBER_INDCPA_SECRETKEYBYTES)
**************************************************/
static void
indcpa_dec(uint8_t m[KYBER_INDCPA_MSGBYTES],
           const uint8_t c[KYBER_INDCPA_BYTES],
           const uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES])
{
    polyvec b, skpv;
    poly v, mp;

    unpack_ciphertext(&b, &v, c);
    unpack_sk(&skpv, sk);

    polyvec_ntt(&b);
    polyvec_basemul_acc_montgomery(&mp, &skpv, &b);
    poly_invntt_tomont(&mp);

    poly_sub(&mp, &v, &mp);
    poly_reduce(&mp);

    poly_tomsg(m, &mp);
}
/** end: ref/indcpa.c **/

/** begin: ref/fips202.c **/
/* Based on the public domain implementation in crypto_hash/keccakc512/simple/ from
 * http://bench.cr.yp.to/supercop.html by Ronny Van Keer and the public domain "TweetFips202"
 * implementation from https://twitter.com/tweetfips202 by Gilles Van Assche, Daniel J. Bernstein,
 * and Peter Schwabe */

#define NROUNDS 24
#define ROL(a, offset) ((a << offset) ^ (a >> (64 - offset)))

/*************************************************
* Name:        load64
*
* Description: Load 8 bytes into uint64_t in little-endian order
*
* Arguments:   - const uint8_t *x: pointer to input byte array
*
* Returns the loaded 64-bit unsigned integer
**************************************************/
static uint64_t
load64(const uint8_t x[8])
{
    unsigned int i;
    uint64_t r = 0;

    for (i = 0; i < 8; i++)
        r |= (uint64_t)x[i] << 8 * i;

    return r;
}

/*************************************************
* Name:        store64
*
* Description: Store a 64-bit integer to array of 8 bytes in little-endian order
*
* Arguments:   - uint8_t *x: pointer to the output byte array (allocated)
*              - uint64_t u: input 64-bit unsigned integer
**************************************************/
static void
store64(uint8_t x[8], uint64_t u)
{
    unsigned int i;

    for (i = 0; i < 8; i++)
        x[i] = u >> 8 * i;
}

/* Keccak round constants */
static const uint64_t KeccakF_RoundConstants[NROUNDS] = {
    (uint64_t)0x0000000000000001ULL,
    (uint64_t)0x0000000000008082ULL,
    (uint64_t)0x800000000000808aULL,
    (uint64_t)0x8000000080008000ULL,
    (uint64_t)0x000000000000808bULL,
    (uint64_t)0x0000000080000001ULL,
    (uint64_t)0x8000000080008081ULL,
    (uint64_t)0x8000000000008009ULL,
    (uint64_t)0x000000000000008aULL,
    (uint64_t)0x0000000000000088ULL,
    (uint64_t)0x0000000080008009ULL,
    (uint64_t)0x000000008000000aULL,
    (uint64_t)0x000000008000808bULL,
    (uint64_t)0x800000000000008bULL,
    (uint64_t)0x8000000000008089ULL,
    (uint64_t)0x8000000000008003ULL,
    (uint64_t)0x8000000000008002ULL,
    (uint64_t)0x8000000000000080ULL,
    (uint64_t)0x000000000000800aULL,
    (uint64_t)0x800000008000000aULL,
    (uint64_t)0x8000000080008081ULL,
    (uint64_t)0x8000000000008080ULL,
    (uint64_t)0x0000000080000001ULL,
    (uint64_t)0x8000000080008008ULL
};

/*************************************************
* Name:        KeccakF1600_StatePermute
*
* Description: The Keccak F1600 Permutation
*
* Arguments:   - uint64_t *state: pointer to input/output Keccak state
**************************************************/
static void
KeccakF1600_StatePermute(uint64_t state[25])
{
    int round;

    uint64_t Aba, Abe, Abi, Abo, Abu;
    uint64_t Aga, Age, Agi, Ago, Agu;
    uint64_t Aka, Ake, Aki, Ako, Aku;
    uint64_t Ama, Ame, Ami, Amo, Amu;
    uint64_t Asa, Ase, Asi, Aso, Asu;
    uint64_t BCa, BCe, BCi, BCo, BCu;
    uint64_t Da, De, Di, Do, Du;
    uint64_t Eba, Ebe, Ebi, Ebo, Ebu;
    uint64_t Ega, Ege, Egi, Ego, Egu;
    uint64_t Eka, Eke, Eki, Eko, Eku;
    uint64_t Ema, Eme, Emi, Emo, Emu;
    uint64_t Esa, Ese, Esi, Eso, Esu;

    //copyFromState(A, state)
    Aba = state[0];
    Abe = state[1];
    Abi = state[2];
    Abo = state[3];
    Abu = state[4];
    Aga = state[5];
    Age = state[6];
    Agi = state[7];
    Ago = state[8];
    Agu = state[9];
    Aka = state[10];
    Ake = state[11];
    Aki = state[12];
    Ako = state[13];
    Aku = state[14];
    Ama = state[15];
    Ame = state[16];
    Ami = state[17];
    Amo = state[18];
    Amu = state[19];
    Asa = state[20];
    Ase = state[21];
    Asi = state[22];
    Aso = state[23];
    Asu = state[24];

    for (round = 0; round < NROUNDS; round += 2) {
        //    prepareTheta
        BCa = Aba ^ Aga ^ Aka ^ Ama ^ Asa;
        BCe = Abe ^ Age ^ Ake ^ Ame ^ Ase;
        BCi = Abi ^ Agi ^ Aki ^ Ami ^ Asi;
        BCo = Abo ^ Ago ^ Ako ^ Amo ^ Aso;
        BCu = Abu ^ Agu ^ Aku ^ Amu ^ Asu;

        //thetaRhoPiChiIotaPrepareTheta(round, A, E)
        Da = BCu ^ ROL(BCe, 1);
        De = BCa ^ ROL(BCi, 1);
        Di = BCe ^ ROL(BCo, 1);
        Do = BCi ^ ROL(BCu, 1);
        Du = BCo ^ ROL(BCa, 1);

        Aba ^= Da;
        BCa = Aba;
        Age ^= De;
        BCe = ROL(Age, 44);
        Aki ^= Di;
        BCi = ROL(Aki, 43);
        Amo ^= Do;
        BCo = ROL(Amo, 21);
        Asu ^= Du;
        BCu = ROL(Asu, 14);
        Eba = BCa ^ ((~BCe) & BCi);
        Eba ^= (uint64_t)KeccakF_RoundConstants[round];
        Ebe = BCe ^ ((~BCi) & BCo);
        Ebi = BCi ^ ((~BCo) & BCu);
        Ebo = BCo ^ ((~BCu) & BCa);
        Ebu = BCu ^ ((~BCa) & BCe);

        Abo ^= Do;
        BCa = ROL(Abo, 28);
        Agu ^= Du;
        BCe = ROL(Agu, 20);
        Aka ^= Da;
        BCi = ROL(Aka, 3);
        Ame ^= De;
        BCo = ROL(Ame, 45);
        Asi ^= Di;
        BCu = ROL(Asi, 61);
        Ega = BCa ^ ((~BCe) & BCi);
        Ege = BCe ^ ((~BCi) & BCo);
        Egi = BCi ^ ((~BCo) & BCu);
        Ego = BCo ^ ((~BCu) & BCa);
        Egu = BCu ^ ((~BCa) & BCe);

        Abe ^= De;
        BCa = ROL(Abe, 1);
        Agi ^= Di;
        BCe = ROL(Agi, 6);
        Ako ^= Do;
        BCi = ROL(Ako, 25);
        Amu ^= Du;
        BCo = ROL(Amu, 8);
        Asa ^= Da;
        BCu = ROL(Asa, 18);
        Eka = BCa ^ ((~BCe) & BCi);
        Eke = BCe ^ ((~BCi) & BCo);
        Eki = BCi ^ ((~BCo) & BCu);
        Eko = BCo ^ ((~BCu) & BCa);
        Eku = BCu ^ ((~BCa) & BCe);

        Abu ^= Du;
        BCa = ROL(Abu, 27);
        Aga ^= Da;
        BCe = ROL(Aga, 36);
        Ake ^= De;
        BCi = ROL(Ake, 10);
        Ami ^= Di;
        BCo = ROL(Ami, 15);
        Aso ^= Do;
        BCu = ROL(Aso, 56);
        Ema = BCa ^ ((~BCe) & BCi);
        Eme = BCe ^ ((~BCi) & BCo);
        Emi = BCi ^ ((~BCo) & BCu);
        Emo = BCo ^ ((~BCu) & BCa);
        Emu = BCu ^ ((~BCa) & BCe);

        Abi ^= Di;
        BCa = ROL(Abi, 62);
        Ago ^= Do;
        BCe = ROL(Ago, 55);
        Aku ^= Du;
        BCi = ROL(Aku, 39);
        Ama ^= Da;
        BCo = ROL(Ama, 41);
        Ase ^= De;
        BCu = ROL(Ase, 2);
        Esa = BCa ^ ((~BCe) & BCi);
        Ese = BCe ^ ((~BCi) & BCo);
        Esi = BCi ^ ((~BCo) & BCu);
        Eso = BCo ^ ((~BCu) & BCa);
        Esu = BCu ^ ((~BCa) & BCe);

        //    prepareTheta
        BCa = Eba ^ Ega ^ Eka ^ Ema ^ Esa;
        BCe = Ebe ^ Ege ^ Eke ^ Eme ^ Ese;
        BCi = Ebi ^ Egi ^ Eki ^ Emi ^ Esi;
        BCo = Ebo ^ Ego ^ Eko ^ Emo ^ Eso;
        BCu = Ebu ^ Egu ^ Eku ^ Emu ^ Esu;

        //thetaRhoPiChiIotaPrepareTheta(round+1, E, A)
        Da = BCu ^ ROL(BCe, 1);
        De = BCa ^ ROL(BCi, 1);
        Di = BCe ^ ROL(BCo, 1);
        Do = BCi ^ ROL(BCu, 1);
        Du = BCo ^ ROL(BCa, 1);

        Eba ^= Da;
        BCa = Eba;
        Ege ^= De;
        BCe = ROL(Ege, 44);
        Eki ^= Di;
        BCi = ROL(Eki, 43);
        Emo ^= Do;
        BCo = ROL(Emo, 21);
        Esu ^= Du;
        BCu = ROL(Esu, 14);
        Aba = BCa ^ ((~BCe) & BCi);
        Aba ^= (uint64_t)KeccakF_RoundConstants[round + 1];
        Abe = BCe ^ ((~BCi) & BCo);
        Abi = BCi ^ ((~BCo) & BCu);
        Abo = BCo ^ ((~BCu) & BCa);
        Abu = BCu ^ ((~BCa) & BCe);

        Ebo ^= Do;
        BCa = ROL(Ebo, 28);
        Egu ^= Du;
        BCe = ROL(Egu, 20);
        Eka ^= Da;
        BCi = ROL(Eka, 3);
        Eme ^= De;
        BCo = ROL(Eme, 45);
        Esi ^= Di;
        BCu = ROL(Esi, 61);
        Aga = BCa ^ ((~BCe) & BCi);
        Age = BCe ^ ((~BCi) & BCo);
        Agi = BCi ^ ((~BCo) & BCu);
        Ago = BCo ^ ((~BCu) & BCa);
        Agu = BCu ^ ((~BCa) & BCe);

        Ebe ^= De;
        BCa = ROL(Ebe, 1);
        Egi ^= Di;
        BCe = ROL(Egi, 6);
        Eko ^= Do;
        BCi = ROL(Eko, 25);
        Emu ^= Du;
        BCo = ROL(Emu, 8);
        Esa ^= Da;
        BCu = ROL(Esa, 18);
        Aka = BCa ^ ((~BCe) & BCi);
        Ake = BCe ^ ((~BCi) & BCo);
        Aki = BCi ^ ((~BCo) & BCu);
        Ako = BCo ^ ((~BCu) & BCa);
        Aku = BCu ^ ((~BCa) & BCe);

        Ebu ^= Du;
        BCa = ROL(Ebu, 27);
        Ega ^= Da;
        BCe = ROL(Ega, 36);
        Eke ^= De;
        BCi = ROL(Eke, 10);
        Emi ^= Di;
        BCo = ROL(Emi, 15);
        Eso ^= Do;
        BCu = ROL(Eso, 56);
        Ama = BCa ^ ((~BCe) & BCi);
        Ame = BCe ^ ((~BCi) & BCo);
        Ami = BCi ^ ((~BCo) & BCu);
        Amo = BCo ^ ((~BCu) & BCa);
        Amu = BCu ^ ((~BCa) & BCe);

        Ebi ^= Di;
        BCa = ROL(Ebi, 62);
        Ego ^= Do;
        BCe = ROL(Ego, 55);
        Eku ^= Du;
        BCi = ROL(Eku, 39);
        Ema ^= Da;
        BCo = ROL(Ema, 41);
        Ese ^= De;
        BCu = ROL(Ese, 2);
        Asa = BCa ^ ((~BCe) & BCi);
        Ase = BCe ^ ((~BCi) & BCo);
        Asi = BCi ^ ((~BCo) & BCu);
        Aso = BCo ^ ((~BCu) & BCa);
        Asu = BCu ^ ((~BCa) & BCe);
    }

    //copyToState(state, A)
    state[0] = Aba;
    state[1] = Abe;
    state[2] = Abi;
    state[3] = Abo;
    state[4] = Abu;
    state[5] = Aga;
    state[6] = Age;
    state[7] = Agi;
    state[8] = Ago;
    state[9] = Agu;
    state[10] = Aka;
    state[11] = Ake;
    state[12] = Aki;
    state[13] = Ako;
    state[14] = Aku;
    state[15] = Ama;
    state[16] = Ame;
    state[17] = Ami;
    state[18] = Amo;
    state[19] = Amu;
    state[20] = Asa;
    state[21] = Ase;
    state[22] = Asi;
    state[23] = Aso;
    state[24] = Asu;
}

/*************************************************
* Name:        keccak_init
*
* Description: Initializes the Keccak state.
*
* Arguments:   - uint64_t *s: pointer to Keccak state
**************************************************/
static void
keccak_init(uint64_t s[25])
{
    unsigned int i;
    for (i = 0; i < 25; i++)
        s[i] = 0;
}

/*************************************************
* Name:        keccak_absorb
*
* Description: Absorb step of Keccak; incremental.
*
* Arguments:   - uint64_t *s: pointer to Keccak state
*              - unsigned int pos: position in current block to be absorbed
*              - unsigned int r: rate in bytes (e.g., 168 for SHAKE128)
*              - const uint8_t *in: pointer to input to be absorbed into s
*              - size_t inlen: length of input in bytes
*
* Returns new position pos in current block
**************************************************/
static unsigned int
keccak_absorb(uint64_t s[25],
              unsigned int pos,
              unsigned int r,
              const uint8_t *in,
              size_t inlen)
{
    unsigned int i;

    while (pos + inlen >= r) {
        for (i = pos; i < r; i++)
            s[i / 8] ^= (uint64_t)*in++ << 8 * (i % 8);
        inlen -= r - pos;
        KeccakF1600_StatePermute(s);
        pos = 0;
    }

    for (i = pos; i < pos + inlen; i++)
        s[i / 8] ^= (uint64_t)*in++ << 8 * (i % 8);

    return i;
}

/*************************************************
* Name:        keccak_finalize
*
* Description: Finalize absorb step.
*
* Arguments:   - uint64_t *s: pointer to Keccak state
*              - unsigned int pos: position in current block to be absorbed
*              - unsigned int r: rate in bytes (e.g., 168 for SHAKE128)
*              - uint8_t p: domain separation byte
**************************************************/
static void
keccak_finalize(uint64_t s[25], unsigned int pos, unsigned int r, uint8_t p)
{
    s[pos / 8] ^= (uint64_t)p << 8 * (pos % 8);
    s[r / 8 - 1] ^= 1ULL << 63;
}

/*************************************************
* Name:        keccak_squeeze
*
* Description: Squeeze step of Keccak. Squeezes arbitratrily many bytes.
*              Modifies the state. Can be called multiple times to keep
*              squeezing, i.e., is incremental.
*
* Arguments:   - uint8_t *out: pointer to output
*              - size_t outlen: number of bytes to be squeezed (written to out)
*              - uint64_t *s: pointer to input/output Keccak state
*              - unsigned int pos: number of bytes in current block already squeezed
*              - unsigned int r: rate in bytes (e.g., 168 for SHAKE128)
*
* Returns new position pos in current block
**************************************************/
static unsigned int
keccak_squeeze(uint8_t *out,
               size_t outlen,
               uint64_t s[25],
               unsigned int pos,
               unsigned int r)
{
    unsigned int i;

    while (outlen) {
        if (pos == r) {
            KeccakF1600_StatePermute(s);
            pos = 0;
        }
        for (i = pos; i < r && i < pos + outlen; i++)
            *out++ = s[i / 8] >> 8 * (i % 8);
        outlen -= i - pos;
        pos = i;
    }

    return pos;
}

/*************************************************
* Name:        keccak_absorb_once
*
* Description: Absorb step of Keccak;
*              non-incremental, starts by zeroeing the state.
*
* Arguments:   - uint64_t *s: pointer to (uninitialized) output Keccak state
*              - unsigned int r: rate in bytes (e.g., 168 for SHAKE128)
*              - const uint8_t *in: pointer to input to be absorbed into s
*              - size_t inlen: length of input in bytes
*              - uint8_t p: domain-separation byte for different Keccak-derived functions
**************************************************/
static void
keccak_absorb_once(uint64_t s[25],
                   unsigned int r,
                   const uint8_t *in,
                   size_t inlen,
                   uint8_t p)
{
    unsigned int i;

    for (i = 0; i < 25; i++)
        s[i] = 0;

    while (inlen >= r) {
        for (i = 0; i < r / 8; i++)
            s[i] ^= load64(in + 8 * i);
        in += r;
        inlen -= r;
        KeccakF1600_StatePermute(s);
    }

    for (i = 0; i < inlen; i++)
        s[i / 8] ^= (uint64_t)in[i] << 8 * (i % 8);

    s[i / 8] ^= (uint64_t)p << 8 * (i % 8);
    s[(r - 1) / 8] ^= 1ULL << 63;
}

/*************************************************
* Name:        keccak_squeezeblocks
*
* Description: Squeeze step of Keccak. Squeezes full blocks of r bytes each.
*              Modifies the state. Can be called multiple times to keep
*              squeezing, i.e., is incremental. Assumes zero bytes of current
*              block have already been squeezed.
*
* Arguments:   - uint8_t *out: pointer to output blocks
*              - size_t nblocks: number of blocks to be squeezed (written to out)
*              - uint64_t *s: pointer to input/output Keccak state
*              - unsigned int r: rate in bytes (e.g., 168 for SHAKE128)
**************************************************/
static void
keccak_squeezeblocks(uint8_t *out,
                     size_t nblocks,
                     uint64_t s[25],
                     unsigned int r)
{
    unsigned int i;

    while (nblocks) {
        KeccakF1600_StatePermute(s);
        for (i = 0; i < r / 8; i++)
            store64(out + 8 * i, s[i]);
        out += r;
        nblocks -= 1;
    }
}

/*************************************************
* Name:        shake128_init
*
* Description: Initilizes Keccak state for use as SHAKE128 XOF
*
* Arguments:   - keccak_state *state: pointer to (uninitialized) Keccak state
**************************************************/
void
shake128_init(keccak_state *state)
{
    keccak_init(state->s);
    state->pos = 0;
}

/*************************************************
* Name:        shake128_absorb
*
* Description: Absorb step of the SHAKE128 XOF; incremental.
*
* Arguments:   - keccak_state *state: pointer to (initialized) output Keccak state
*              - const uint8_t *in: pointer to input to be absorbed into s
*              - size_t inlen: length of input in bytes
**************************************************/
void
shake128_absorb(keccak_state *state, const uint8_t *in, size_t inlen)
{
    state->pos = keccak_absorb(state->s, state->pos, SHAKE128_RATE, in, inlen);
}

/*************************************************
* Name:        shake128_finalize
*
* Description: Finalize absorb step of the SHAKE128 XOF.
*
* Arguments:   - keccak_state *state: pointer to Keccak state
**************************************************/
void
shake128_finalize(keccak_state *state)
{
    keccak_finalize(state->s, state->pos, SHAKE128_RATE, 0x1F);
    state->pos = SHAKE128_RATE;
}

/*************************************************
* Name:        shake128_squeeze
*
* Description: Squeeze step of SHAKE128 XOF. Squeezes arbitraily many
*              bytes. Can be called multiple times to keep squeezing.
*
* Arguments:   - uint8_t *out: pointer to output blocks
*              - size_t outlen : number of bytes to be squeezed (written to output)
*              - keccak_state *s: pointer to input/output Keccak state
**************************************************/
void
shake128_squeeze(uint8_t *out, size_t outlen, keccak_state *state)
{
    state->pos = keccak_squeeze(out, outlen, state->s, state->pos, SHAKE128_RATE);
}

/*************************************************
* Name:        shake128_absorb_once
*
* Description: Initialize, absorb into and finalize SHAKE128 XOF; non-incremental.
*
* Arguments:   - keccak_state *state: pointer to (uninitialized) output Keccak state
*              - const uint8_t *in: pointer to input to be absorbed into s
*              - size_t inlen: length of input in bytes
**************************************************/
void
shake128_absorb_once(keccak_state *state, const uint8_t *in, size_t inlen)
{
    keccak_absorb_once(state->s, SHAKE128_RATE, in, inlen, 0x1F);
    state->pos = SHAKE128_RATE;
}

/*************************************************
* Name:        shake128_squeezeblocks
*
* Description: Squeeze step of SHAKE128 XOF. Squeezes full blocks of
*              SHAKE128_RATE bytes each. Can be called multiple times
*              to keep squeezing. Assumes new block has not yet been
*              started (state->pos = SHAKE128_RATE).
*
* Arguments:   - uint8_t *out: pointer to output blocks
*              - size_t nblocks: number of blocks to be squeezed (written to output)
*              - keccak_state *s: pointer to input/output Keccak state
**************************************************/
void
shake128_squeezeblocks(uint8_t *out, size_t nblocks, keccak_state *state)
{
    keccak_squeezeblocks(out, nblocks, state->s, SHAKE128_RATE);
}

/*************************************************
* Name:        shake256_init
*
* Description: Initilizes Keccak state for use as SHAKE256 XOF
*
* Arguments:   - keccak_state *state: pointer to (uninitialized) Keccak state
**************************************************/
void
shake256_init(keccak_state *state)
{
    keccak_init(state->s);
    state->pos = 0;
}

/*************************************************
* Name:        shake256_absorb
*
* Description: Absorb step of the SHAKE256 XOF; incremental.
*
* Arguments:   - keccak_state *state: pointer to (initialized) output Keccak state
*              - const uint8_t *in: pointer to input to be absorbed into s
*              - size_t inlen: length of input in bytes
**************************************************/
void
shake256_absorb(keccak_state *state, const uint8_t *in, size_t inlen)
{
    state->pos = keccak_absorb(state->s, state->pos, SHAKE256_RATE, in, inlen);
}

/*************************************************
* Name:        shake256_finalize
*
* Description: Finalize absorb step of the SHAKE256 XOF.
*
* Arguments:   - keccak_state *state: pointer to Keccak state
**************************************************/
void
shake256_finalize(keccak_state *state)
{
    keccak_finalize(state->s, state->pos, SHAKE256_RATE, 0x1F);
    state->pos = SHAKE256_RATE;
}

/*************************************************
* Name:        shake256_squeeze
*
* Description: Squeeze step of SHAKE256 XOF. Squeezes arbitraily many
*              bytes. Can be called multiple times to keep squeezing.
*
* Arguments:   - uint8_t *out: pointer to output blocks
*              - size_t outlen : number of bytes to be squeezed (written to output)
*              - keccak_state *s: pointer to input/output Keccak state
**************************************************/
void
shake256_squeeze(uint8_t *out, size_t outlen, keccak_state *state)
{
    state->pos = keccak_squeeze(out, outlen, state->s, state->pos, SHAKE256_RATE);
}

/*************************************************
* Name:        shake256_absorb_once
*
* Description: Initialize, absorb into and finalize SHAKE256 XOF; non-incremental.
*
* Arguments:   - keccak_state *state: pointer to (uninitialized) output Keccak state
*              - const uint8_t *in: pointer to input to be absorbed into s
*              - size_t inlen: length of input in bytes
**************************************************/
void
shake256_absorb_once(keccak_state *state, const uint8_t *in, size_t inlen)
{
    keccak_absorb_once(state->s, SHAKE256_RATE, in, inlen, 0x1F);
    state->pos = SHAKE256_RATE;
}

/*************************************************
* Name:        shake256_squeezeblocks
*
* Description: Squeeze step of SHAKE256 XOF. Squeezes full blocks of
*              SHAKE256_RATE bytes each. Can be called multiple times
*              to keep squeezing. Assumes next block has not yet been
*              started (state->pos = SHAKE256_RATE).
*
* Arguments:   - uint8_t *out: pointer to output blocks
*              - size_t nblocks: number of blocks to be squeezed (written to output)
*              - keccak_state *s: pointer to input/output Keccak state
**************************************************/
void
shake256_squeezeblocks(uint8_t *out, size_t nblocks, keccak_state *state)
{
    keccak_squeezeblocks(out, nblocks, state->s, SHAKE256_RATE);
}

/*************************************************
* Name:        shake128
*
* Description: SHAKE128 XOF with non-incremental API
*
* Arguments:   - uint8_t *out: pointer to output
*              - size_t outlen: requested output length in bytes
*              - const uint8_t *in: pointer to input
*              - size_t inlen: length of input in bytes
**************************************************/
void
shake128(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen)
{
    size_t nblocks;
    keccak_state state;

    shake128_absorb_once(&state, in, inlen);
    nblocks = outlen / SHAKE128_RATE;
    shake128_squeezeblocks(out, nblocks, &state);
    outlen -= nblocks * SHAKE128_RATE;
    out += nblocks * SHAKE128_RATE;
    shake128_squeeze(out, outlen, &state);
}

/*************************************************
* Name:        shake256
*
* Description: SHAKE256 XOF with non-incremental API
*
* Arguments:   - uint8_t *out: pointer to output
*              - size_t outlen: requested output length in bytes
*              - const uint8_t *in: pointer to input
*              - size_t inlen: length of input in bytes
**************************************************/
void
shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen)
{
    size_t nblocks;
    keccak_state state;

    shake256_absorb_once(&state, in, inlen);
    nblocks = outlen / SHAKE256_RATE;
    shake256_squeezeblocks(out, nblocks, &state);
    outlen -= nblocks * SHAKE256_RATE;
    out += nblocks * SHAKE256_RATE;
    shake256_squeeze(out, outlen, &state);
}

/*************************************************
* Name:        sha3_256
*
* Description: SHA3-256 with non-incremental API
*
* Arguments:   - uint8_t *h: pointer to output (32 bytes)
*              - const uint8_t *in: pointer to input
*              - size_t inlen: length of input in bytes
**************************************************/
void
sha3_256(uint8_t h[32], const uint8_t *in, size_t inlen)
{
    unsigned int i;
    uint64_t s[25];

    keccak_absorb_once(s, SHA3_256_RATE, in, inlen, 0x06);
    KeccakF1600_StatePermute(s);
    for (i = 0; i < 4; i++)
        store64(h + 8 * i, s[i]);
}

/*************************************************
* Name:        sha3_512
*
* Description: SHA3-512 with non-incremental API
*
* Arguments:   - uint8_t *h: pointer to output (64 bytes)
*              - const uint8_t *in: pointer to input
*              - size_t inlen: length of input in bytes
**************************************************/
void
sha3_512(uint8_t h[64], const uint8_t *in, size_t inlen)
{
    unsigned int i;
    uint64_t s[25];

    keccak_absorb_once(s, SHA3_512_RATE, in, inlen, 0x06);
    KeccakF1600_StatePermute(s);
    for (i = 0; i < 8; i++)
        store64(h + 8 * i, s[i]);
}
/** end: ref/fips202.c **/

/** begin: ref/symmetric-shake.c **/
/*************************************************
* Name:        kyber_shake128_absorb
*
* Description: Absorb step of the SHAKE128 specialized for the Kyber context.
*
* Arguments:   - keccak_state *state: pointer to (uninitialized) output Keccak state
*              - const uint8_t *seed: pointer to KYBER_SYMBYTES input to be absorbed into state
*              - uint8_t i: additional byte of input
*              - uint8_t j: additional byte of input
**************************************************/
static void
kyber_shake128_absorb(keccak_state *state,
                      const uint8_t seed[KYBER_SYMBYTES],
                      uint8_t x,
                      uint8_t y)
{
    uint8_t extseed[KYBER_SYMBYTES + 2];

    memcpy(extseed, seed, KYBER_SYMBYTES);
    extseed[KYBER_SYMBYTES + 0] = x;
    extseed[KYBER_SYMBYTES + 1] = y;

    shake128_absorb_once(state, extseed, sizeof(extseed));
}

/*************************************************
* Name:        kyber_shake256_prf
*
* Description: Usage of SHAKE256 as a PRF, concatenates secret and public input
*              and then generates outlen bytes of SHAKE256 output
*
* Arguments:   - uint8_t *out: pointer to output
*              - size_t outlen: number of requested output bytes
*              - const uint8_t *key: pointer to the key (of length KYBER_SYMBYTES)
*              - uint8_t nonce: single-byte nonce (public PRF input)
**************************************************/
static void
kyber_shake256_prf(uint8_t *out, size_t outlen, const uint8_t key[KYBER_SYMBYTES], uint8_t nonce)
{
    uint8_t extkey[KYBER_SYMBYTES + 1];

    memcpy(extkey, key, KYBER_SYMBYTES);
    extkey[KYBER_SYMBYTES] = nonce;

    shake256(out, outlen, extkey, sizeof(extkey));
}
/** end: ref/symmetric-shake.c **/

/** begin: ref/kem.c **/
/*************************************************
* Name:        crypto_kem_keypair_derand
*
* Description: Generates public and private key
*              for CCA-secure Kyber key encapsulation mechanism
*
* Arguments:   - uint8_t *pk: pointer to output public key
*                (an already allocated array of KYBER_PUBLICKEYBYTES bytes)
*              - uint8_t *sk: pointer to output private key
*                (an already allocated array of KYBER_SECRETKEYBYTES bytes)
*              - uint8_t *coins: pointer to input randomness
*                (an already allocated array filled with 2*KYBER_SYMBYTES random bytes)
**
* Returns 0 (success)
**************************************************/
int
crypto_kem_keypair_derand(uint8_t *pk,
                          uint8_t *sk,
                          const uint8_t *coins)
{
    size_t i;
    indcpa_keypair_derand(pk, sk, coins);
    for (i = 0; i < KYBER_INDCPA_PUBLICKEYBYTES; i++)
        sk[i + KYBER_INDCPA_SECRETKEYBYTES] = pk[i];
    hash_h(sk + KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES, pk, KYBER_PUBLICKEYBYTES);
    /* Value z for pseudo-random output on reject */
    for (i = 0; i < KYBER_SYMBYTES; i++)
        sk[KYBER_SECRETKEYBYTES - KYBER_SYMBYTES + i] = coins[KYBER_SYMBYTES + i];
    return 0;
}

/*************************************************
* Name:        crypto_kem_keypair
*
* Description: Generates public and private key
*              for CCA-secure Kyber key encapsulation mechanism
*
* Arguments:   - uint8_t *pk: pointer to output public key
*                (an already allocated array of KYBER_PUBLICKEYBYTES bytes)
*              - uint8_t *sk: pointer to output private key
*                (an already allocated array of KYBER_SECRETKEYBYTES bytes)
*
* Returns 0 (success)
**************************************************/
int
crypto_kem_keypair(uint8_t *pk,
                   uint8_t *sk)
{
    uint8_t coins[2 * KYBER_SYMBYTES];
    randombytes(coins, KYBER_SYMBYTES);
    randombytes(coins + KYBER_SYMBYTES, KYBER_SYMBYTES);
    crypto_kem_keypair_derand(pk, sk, coins);
    return 0;
}

/*************************************************
* Name:        crypto_kem_enc_derand
*
* Description: Generates cipher text and shared
*              secret for given public key
*
* Arguments:   - uint8_t *ct: pointer to output cipher text
*                (an already allocated array of KYBER_CIPHERTEXTBYTES bytes)
*              - uint8_t *ss: pointer to output shared secret
*                (an already allocated array of KYBER_SSBYTES bytes)
*              - const uint8_t *pk: pointer to input public key
*                (an already allocated array of KYBER_PUBLICKEYBYTES bytes)
*              - const uint8_t *coins: pointer to input randomness
*                (an already allocated array filled with KYBER_SYMBYTES random bytes)
**
* Returns 0 (success)
**************************************************/
int
crypto_kem_enc_derand(uint8_t *ct,
                      uint8_t *ss,
                      const uint8_t *pk,
                      const uint8_t *coins)
{
    uint8_t buf[2 * KYBER_SYMBYTES];
    /* Will contain key, coins */
    uint8_t kr[2 * KYBER_SYMBYTES];

    /* Don't release system RNG output */
    hash_h(buf, coins, KYBER_SYMBYTES);

    /* Multitarget countermeasure for coins + contributory KEM */
    hash_h(buf + KYBER_SYMBYTES, pk, KYBER_PUBLICKEYBYTES);
    hash_g(kr, buf, 2 * KYBER_SYMBYTES);

    /* coins are in kr+KYBER_SYMBYTES */
    indcpa_enc(ct, buf, pk, kr + KYBER_SYMBYTES);

    /* overwrite coins in kr with H(c) */
    hash_h(kr + KYBER_SYMBYTES, ct, KYBER_CIPHERTEXTBYTES);
    /* hash concatenation of pre-k and H(c) to k */
    kdf(ss, kr, 2 * KYBER_SYMBYTES);
    return 0;
}

/*************************************************
* Name:        crypto_kem_enc
*
* Description: Generates cipher text and shared
*              secret for given public key
*
* Arguments:   - uint8_t *ct: pointer to output cipher text
*                (an already allocated array of KYBER_CIPHERTEXTBYTES bytes)
*              - uint8_t *ss: pointer to output shared secret
*                (an already allocated array of KYBER_SSBYTES bytes)
*              - const uint8_t *pk: pointer to input public key
*                (an already allocated array of KYBER_PUBLICKEYBYTES bytes)
*
* Returns 0 (success)
**************************************************/
int
crypto_kem_enc(uint8_t *ct,
               uint8_t *ss,
               const uint8_t *pk)
{
    uint8_t coins[KYBER_SYMBYTES];
    randombytes(coins, KYBER_SYMBYTES);
    crypto_kem_enc_derand(ct, ss, pk, coins);
    return 0;
}

/*************************************************
* Name:        crypto_kem_dec
*
* Description: Generates shared secret for given
*              cipher text and private key
*
* Arguments:   - uint8_t *ss: pointer to output shared secret
*                (an already allocated array of KYBER_SSBYTES bytes)
*              - const uint8_t *ct: pointer to input cipher text
*                (an already allocated array of KYBER_CIPHERTEXTBYTES bytes)
*              - const uint8_t *sk: pointer to input private key
*                (an already allocated array of KYBER_SECRETKEYBYTES bytes)
*
* Returns 0.
*
* On failure, ss will contain a pseudo-random value.
**************************************************/
int
crypto_kem_dec(uint8_t *ss,
               const uint8_t *ct,
               const uint8_t *sk)
{
    size_t i;
    int fail;
    uint8_t buf[2 * KYBER_SYMBYTES];
    /* Will contain key, coins */
    uint8_t kr[2 * KYBER_SYMBYTES];
    uint8_t cmp[KYBER_CIPHERTEXTBYTES];
    const uint8_t *pk = sk + KYBER_INDCPA_SECRETKEYBYTES;

    indcpa_dec(buf, ct, sk);

    /* Multitarget countermeasure for coins + contributory KEM */
    for (i = 0; i < KYBER_SYMBYTES; i++)
        buf[KYBER_SYMBYTES + i] = sk[KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES + i];
    hash_g(kr, buf, 2 * KYBER_SYMBYTES);

    /* coins are in kr+KYBER_SYMBYTES */
    indcpa_enc(cmp, buf, pk, kr + KYBER_SYMBYTES);

    fail = verify(ct, cmp, KYBER_CIPHERTEXTBYTES);

    /* overwrite coins in kr with H(c) */
    hash_h(kr + KYBER_SYMBYTES, ct, KYBER_CIPHERTEXTBYTES);

    /* Overwrite pre-k with z on re-encryption failure */
    cmov(kr, sk + KYBER_SECRETKEYBYTES - KYBER_SYMBYTES, KYBER_SYMBYTES, fail);

    /* hash concatenation of pre-k and H(c) to k */
    kdf(ss, kr, 2 * KYBER_SYMBYTES);
    return 0;
}
/** end: ref/kem.c **/
