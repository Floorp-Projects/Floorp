/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * RSA key generation, public key op, private key op.
 */
#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "secerr.h"

#include "prclist.h"
#include "nssilock.h"
#include "prinit.h"
#include "blapi.h"
#include "mpi.h"
#include "mpprime.h"
#include "mplogic.h"
#include "secmpi.h"
#include "secitem.h"
#include "blapii.h"

/* The minimal required randomness is 64 bits */
/* EXP_BLINDING_RANDOMNESS_LEN is the length of the randomness in mp_digits */
/* for 32 bits platforts it is 2 mp_digits (= 2 * 32 bits), for 64 bits it is equal to 128 bits */
#define EXP_BLINDING_RANDOMNESS_LEN ((128 + MP_DIGIT_BIT - 1) / MP_DIGIT_BIT)
#define EXP_BLINDING_RANDOMNESS_LEN_BYTES (EXP_BLINDING_RANDOMNESS_LEN * sizeof(mp_digit))

/*
** Number of times to attempt to generate a prime (p or q) from a random
** seed (the seed changes for each iteration).
*/
#define MAX_PRIME_GEN_ATTEMPTS 10
/*
** Number of times to attempt to generate a key.  The primes p and q change
** for each attempt.
*/
#define MAX_KEY_GEN_ATTEMPTS 10

/* Blinding Parameters max cache size  */
#define RSA_BLINDING_PARAMS_MAX_CACHE_SIZE 20

/* exponent should not be greater than modulus */
#define BAD_RSA_KEY_SIZE(modLen, expLen)                           \
    ((expLen) > (modLen) || (modLen) > RSA_MAX_MODULUS_BITS / 8 || \
     (expLen) > RSA_MAX_EXPONENT_BITS / 8)

struct blindingParamsStr;
typedef struct blindingParamsStr blindingParams;

struct blindingParamsStr {
    blindingParams *next;
    mp_int f, g; /* blinding parameter                 */
    int counter; /* number of remaining uses of (f, g) */
};

/*
** RSABlindingParamsStr
**
** For discussion of Paul Kocher's timing attack against an RSA private key
** operation, see http://www.cryptography.com/timingattack/paper.html.  The
** countermeasure to this attack, known as blinding, is also discussed in
** the Handbook of Applied Cryptography, 11.118-11.119.
*/
struct RSABlindingParamsStr {
    /* Blinding-specific parameters */
    PRCList link;              /* link to list of structs            */
    SECItem modulus;           /* list element "key"                 */
    blindingParams *free, *bp; /* Blinding parameters queue          */
    blindingParams array[RSA_BLINDING_PARAMS_MAX_CACHE_SIZE];
};
typedef struct RSABlindingParamsStr RSABlindingParams;

/*
** RSABlindingParamsListStr
**
** List of key-specific blinding params.  The arena holds the volatile pool
** of memory for each entry and the list itself.  The lock is for list
** operations, in this case insertions and iterations, as well as control
** of the counter for each set of blinding parameters.
*/
struct RSABlindingParamsListStr {
    PZLock *lock;    /* Lock for the list   */
    PRCondVar *cVar; /* Condidtion Variable */
    int waitCount;   /* Number of threads waiting on cVar */
    PRCList head;    /* Pointer to the list */
};

/*
** The master blinding params list.
*/
static struct RSABlindingParamsListStr blindingParamsList = { 0 };

/* Number of times to reuse (f, g).  Suggested by Paul Kocher */
#define RSA_BLINDING_PARAMS_MAX_REUSE 50

/* Global, allows optional use of blinding.  On by default. */
/* Cannot be changed at the moment, due to thread-safety issues. */
static PRBool nssRSAUseBlinding = PR_TRUE;

static SECStatus
rsa_build_from_primes(const mp_int *p, const mp_int *q,
                      mp_int *e, PRBool needPublicExponent,
                      mp_int *d, PRBool needPrivateExponent,
                      RSAPrivateKey *key, unsigned int keySizeInBits)
{
    mp_int n, phi;
    mp_int psub1, qsub1, tmp;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&phi) = 0;
    MP_DIGITS(&psub1) = 0;
    MP_DIGITS(&qsub1) = 0;
    MP_DIGITS(&tmp) = 0;
    CHECK_MPI_OK(mp_init(&n));
    CHECK_MPI_OK(mp_init(&phi));
    CHECK_MPI_OK(mp_init(&psub1));
    CHECK_MPI_OK(mp_init(&qsub1));
    CHECK_MPI_OK(mp_init(&tmp));
    /* p and q must be distinct. */
    if (mp_cmp(p, q) == 0) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        rv = SECFailure;
        goto cleanup;
    }
    /* 1.  Compute n = p*q */
    CHECK_MPI_OK(mp_mul(p, q, &n));
    /*     verify that the modulus has the desired number of bits */
    if ((unsigned)mpl_significant_bits(&n) != keySizeInBits) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        rv = SECFailure;
        goto cleanup;
    }

    /* at least one exponent must be given */
    PORT_Assert(!(needPublicExponent && needPrivateExponent));

    /* 2.  Compute phi = (p-1)*(q-1) */
    CHECK_MPI_OK(mp_sub_d(p, 1, &psub1));
    CHECK_MPI_OK(mp_sub_d(q, 1, &qsub1));
    if (needPublicExponent || needPrivateExponent) {
        CHECK_MPI_OK(mp_lcm(&psub1, &qsub1, &phi));
        /* 3.  Compute d = e**-1 mod(phi) */
        /*     or      e = d**-1 mod(phi) as necessary */
        if (needPublicExponent) {
            err = mp_invmod(d, &phi, e);
        } else {
            err = mp_invmod(e, &phi, d);
        }
    } else {
        err = MP_OKAY;
    }
    /*     Verify that phi(n) and e have no common divisors */
    if (err != MP_OKAY) {
        if (err == MP_UNDEF) {
            PORT_SetError(SEC_ERROR_NEED_RANDOM);
            err = MP_OKAY; /* to keep PORT_SetError from being called again */
            rv = SECFailure;
        }
        goto cleanup;
    }

    /* 4.  Compute exponent1 = d mod (p-1) */
    CHECK_MPI_OK(mp_mod(d, &psub1, &tmp));
    MPINT_TO_SECITEM(&tmp, &key->exponent1, key->arena);
    /* 5.  Compute exponent2 = d mod (q-1) */
    CHECK_MPI_OK(mp_mod(d, &qsub1, &tmp));
    MPINT_TO_SECITEM(&tmp, &key->exponent2, key->arena);
    /* 6.  Compute coefficient = q**-1 mod p */
    CHECK_MPI_OK(mp_invmod(q, p, &tmp));
    MPINT_TO_SECITEM(&tmp, &key->coefficient, key->arena);

    /* copy our calculated results, overwrite what is there */
    key->modulus.data = NULL;
    MPINT_TO_SECITEM(&n, &key->modulus, key->arena);
    key->privateExponent.data = NULL;
    MPINT_TO_SECITEM(d, &key->privateExponent, key->arena);
    key->publicExponent.data = NULL;
    MPINT_TO_SECITEM(e, &key->publicExponent, key->arena);
    key->prime1.data = NULL;
    MPINT_TO_SECITEM(p, &key->prime1, key->arena);
    key->prime2.data = NULL;
    MPINT_TO_SECITEM(q, &key->prime2, key->arena);
cleanup:
    mp_clear(&n);
    mp_clear(&phi);
    mp_clear(&psub1);
    mp_clear(&qsub1);
    mp_clear(&tmp);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

SECStatus
generate_prime(mp_int *prime, int primeLen)
{
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    int piter;
    unsigned char *pb = NULL;
    pb = PORT_Alloc(primeLen);
    if (!pb) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto cleanup;
    }
    for (piter = 0; piter < MAX_PRIME_GEN_ATTEMPTS; piter++) {
        CHECK_SEC_OK(RNG_GenerateGlobalRandomBytes(pb, primeLen));
        pb[0] |= 0xC0;            /* set two high-order bits */
        pb[primeLen - 1] |= 0x01; /* set low-order bit       */
        CHECK_MPI_OK(mp_read_unsigned_octets(prime, pb, primeLen));
        err = mpp_make_prime_secure(prime, primeLen * 8, PR_FALSE);
        if (err != MP_NO)
            goto cleanup;
        /* keep going while err == MP_NO */
    }
cleanup:
    if (pb)
        PORT_ZFree(pb, primeLen);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/*
 *  make sure the key components meet fips186 requirements.
 */
static PRBool
rsa_fips186_verify(mp_int *p, mp_int *q, mp_int *d, int keySizeInBits)
{
    mp_int pq_diff;
    mp_err err = MP_OKAY;
    PRBool ret = PR_FALSE;

    if (keySizeInBits < 250) {
        /* not a valid FIPS length, no point in our other tests */
        /* if you are here, and in FIPS mode, you are outside the security
         * policy */
        return PR_TRUE;
    }

    /* p & q are already known to be greater then sqrt(2)*2^(keySize/2-1) */
    /* we also know that gcd(p-1,e) = 1 and gcd(q-1,e) = 1 because the
     * mp_invmod() function will fail. */
    /* now check p-q > 2^(keysize/2-100) */
    MP_DIGITS(&pq_diff) = 0;
    CHECK_MPI_OK(mp_init(&pq_diff));
    /* NSS always has p > q, so we know pq_diff is positive */
    CHECK_MPI_OK(mp_sub(p, q, &pq_diff));
    if ((unsigned)mpl_significant_bits(&pq_diff) < (keySizeInBits / 2 - 100)) {
        goto cleanup;
    }
    /* now verify d is large enough*/
    if ((unsigned)mpl_significant_bits(d) < (keySizeInBits / 2)) {
        goto cleanup;
    }
    ret = PR_TRUE;

cleanup:
    mp_clear(&pq_diff);
    return ret;
}

/*
** Generate and return a new RSA public and private key.
**  Both keys are encoded in a single RSAPrivateKey structure.
**  "cx" is the random number generator context
**  "keySizeInBits" is the size of the key to be generated, in bits.
**     512, 1024, etc.
**  "publicExponent" when not NULL is a pointer to some data that
**     represents the public exponent to use. The data is a byte
**     encoded integer, in "big endian" order.
*/
RSAPrivateKey *
RSA_NewKey(int keySizeInBits, SECItem *publicExponent)
{
    unsigned int primeLen;
    mp_int p = { 0, 0, 0, NULL };
    mp_int q = { 0, 0, 0, NULL };
    mp_int e = { 0, 0, 0, NULL };
    mp_int d = { 0, 0, 0, NULL };
    int kiter;
    int max_attempts;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    int prerr = 0;
    RSAPrivateKey *key = NULL;
    PLArenaPool *arena = NULL;
    /* Require key size to be a multiple of 16 bits. */
    if (!publicExponent || keySizeInBits % 16 != 0 ||
        BAD_RSA_KEY_SIZE((unsigned int)keySizeInBits / 8, publicExponent->len)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    /* 1.  Set the public exponent and check if it's uneven and greater than 2.*/
    MP_DIGITS(&e) = 0;
    CHECK_MPI_OK(mp_init(&e));
    SECITEM_TO_MPINT(*publicExponent, &e);
    if (mp_iseven(&e) || !(mp_cmp_d(&e, 2) > 0)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto cleanup;
    }
#ifndef NSS_FIPS_DISABLED
    /* Check that the exponent is not smaller than 65537  */
    if (mp_cmp_d(&e, 0x10001) < 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto cleanup;
    }
#endif

    /* 2. Allocate arena & key */
    arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
    if (!arena) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto cleanup;
    }
    key = PORT_ArenaZNew(arena, RSAPrivateKey);
    if (!key) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto cleanup;
    }
    key->arena = arena;
    /* length of primes p and q (in bytes) */
    primeLen = keySizeInBits / (2 * PR_BITS_PER_BYTE);
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&d) = 0;
    CHECK_MPI_OK(mp_init(&p));
    CHECK_MPI_OK(mp_init(&q));
    CHECK_MPI_OK(mp_init(&d));
    /* 3.  Set the version number (PKCS1 v1.5 says it should be zero) */
    SECITEM_AllocItem(arena, &key->version, 1);
    key->version.data[0] = 0;

    kiter = 0;
    max_attempts = 5 * (keySizeInBits / 2); /* FIPS 186-4 B.3.3 steps 4.7 and 5.8 */
    do {
        PORT_SetError(0);
        CHECK_SEC_OK(generate_prime(&p, primeLen));
        CHECK_SEC_OK(generate_prime(&q, primeLen));
        /* Assure p > q */
        /* NOTE: PKCS #1 does not require p > q, and NSS doesn't use any
         * implementation optimization that requires p > q. We can remove
         * this code in the future.
         */
        if (mp_cmp(&p, &q) < 0)
            mp_exch(&p, &q);
        /* Attempt to use these primes to generate a key */
        rv = rsa_build_from_primes(&p, &q,
                                   &e, PR_FALSE, /* needPublicExponent=false */
                                   &d, PR_TRUE,  /* needPrivateExponent=true */
                                   key, keySizeInBits);
        if (rv == SECSuccess) {
            if (rsa_fips186_verify(&p, &q, &d, keySizeInBits)) {
                break;
            }
            prerr = SEC_ERROR_NEED_RANDOM; /* retry with different values */
        } else {
            prerr = PORT_GetError();
        }
        kiter++;
        /* loop until have primes */
    } while (prerr == SEC_ERROR_NEED_RANDOM && kiter < max_attempts);

cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&e);
    mp_clear(&d);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    if (rv && arena) {
        PORT_FreeArena(arena, PR_TRUE);
        key = NULL;
    }
    return key;
}

mp_err
rsa_is_prime(mp_int *p)
{
    int res;

    /* run a Fermat test */
    res = mpp_fermat(p, 2);
    if (res != MP_OKAY) {
        return res;
    }

    /* If that passed, run some Miller-Rabin tests */
    res = mpp_pprime_secure(p, 2);
    return res;
}

/*
 * Factorize a RSA modulus n into p and q by using the exponents e and d.
 *
 * In: e, d, n
 * Out: p, q
 *
 * See Handbook of Applied Cryptography, 8.2.2(i).
 *
 * The algorithm is probabilistic, it is run 64 times and each run has a 50%
 * chance of succeeding with a runtime of O(log(e*d)).
 *
 * The returned p might be smaller than q.
 */
static mp_err
rsa_factorize_n_from_exponents(mp_int *e, mp_int *d, mp_int *p, mp_int *q,
                               mp_int *n)
{
    /* lambda is the private modulus: e*d = 1 mod lambda */
    /* so: e*d - 1 = k*lambda = t*2^s where t is odd */
    mp_int klambda;
    mp_int t, onetwentyeight;
    unsigned long s = 0;
    unsigned long i;

    /* cand = a^(t * 2^i) mod n, next_cand = a^(t * 2^(i+1)) mod n */
    mp_int a;
    mp_int cand;
    mp_int next_cand;

    mp_int n_minus_one;
    mp_err err = MP_OKAY;

    MP_DIGITS(&klambda) = 0;
    MP_DIGITS(&t) = 0;
    MP_DIGITS(&a) = 0;
    MP_DIGITS(&cand) = 0;
    MP_DIGITS(&n_minus_one) = 0;
    MP_DIGITS(&next_cand) = 0;
    MP_DIGITS(&onetwentyeight) = 0;
    CHECK_MPI_OK(mp_init(&klambda));
    CHECK_MPI_OK(mp_init(&t));
    CHECK_MPI_OK(mp_init(&a));
    CHECK_MPI_OK(mp_init(&cand));
    CHECK_MPI_OK(mp_init(&n_minus_one));
    CHECK_MPI_OK(mp_init(&next_cand));
    CHECK_MPI_OK(mp_init(&onetwentyeight));

    mp_set_int(&onetwentyeight, 128);

    /* calculate k*lambda = e*d - 1 */
    CHECK_MPI_OK(mp_mul(e, d, &klambda));
    CHECK_MPI_OK(mp_sub_d(&klambda, 1, &klambda));

    /* factorize klambda into t*2^s */
    CHECK_MPI_OK(mp_copy(&klambda, &t));
    while (mpp_divis_d(&t, 2) == MP_YES) {
        CHECK_MPI_OK(mp_div_2(&t, &t));
        s += 1;
    }

    /* precompute n_minus_one = n - 1 */
    CHECK_MPI_OK(mp_copy(n, &n_minus_one));
    CHECK_MPI_OK(mp_sub_d(&n_minus_one, 1, &n_minus_one));

    /* pick random bases a, each one has a 50% leading to a factorization */
    CHECK_MPI_OK(mp_set_int(&a, 2));
    /* The following is equivalent to for (a=2, a <= 128, a+=2) */
    while (mp_cmp(&a, &onetwentyeight) <= 0) {
        /* compute the base cand = a^(t * 2^0) [i = 0] */
        CHECK_MPI_OK(mp_exptmod(&a, &t, n, &cand));

        for (i = 0; i < s; i++) {
            /* condition 1: skip the base if we hit a trivial factor of n */
            if (mp_cmp(&cand, &n_minus_one) == 0 || mp_cmp_d(&cand, 1) == 0) {
                break;
            }

            /* increase i in a^(t * 2^i) by squaring the number */
            CHECK_MPI_OK(mp_exptmod_d(&cand, 2, n, &next_cand));

            /* condition 2: a^(t * 2^(i+1)) = 1 mod n */
            if (mp_cmp_d(&next_cand, 1) == 0) {
                /* conditions verified, gcd(a^(t * 2^i) - 1, n) is a factor */
                CHECK_MPI_OK(mp_sub_d(&cand, 1, &cand));
                CHECK_MPI_OK(mp_gcd(&cand, n, p));
                if (mp_cmp_d(p, 1) == 0) {
                    CHECK_MPI_OK(mp_add_d(&cand, 1, &cand));
                    break;
                }
                CHECK_MPI_OK(mp_div(n, p, q, NULL));
                goto cleanup;
            }
            CHECK_MPI_OK(mp_copy(&next_cand, &cand));
        }

        CHECK_MPI_OK(mp_add_d(&a, 2, &a));
    }

    /* if we reach here it's likely (2^64 - 1 / 2^64) that d is wrong */
    err = MP_RANGE;

cleanup:
    mp_clear(&klambda);
    mp_clear(&t);
    mp_clear(&a);
    mp_clear(&cand);
    mp_clear(&n_minus_one);
    mp_clear(&next_cand);
    mp_clear(&onetwentyeight);
    return err;
}

/*
 * Try to find the two primes based on 2 exponents plus a prime.
 *
 * In: e, d and p.
 * Out: p,q.
 *
 * Step 1, Since d = e**-1 mod phi, we know that d*e == 1 mod phi, or
 *  d*e = 1+k*phi, or d*e-1 = k*phi. since d is less than phi and e is
 *  usually less than d, then k must be an integer between e-1 and 1
 *  (probably on the order of e).
 * Step 1a, We can divide k*phi by prime-1 and get k*(q-1). This will reduce
 *      the size of our division through the rest of the loop.
 * Step 2, Loop through the values k=e-1 to 1 looking for k. k should be on
 *  the order or e, and e is typically small. This may take a while for
 *  a large random e. We are looking for a k that divides kphi
 *  evenly. Once we find a k that divides kphi evenly, we assume it
 *  is the true k. It's possible this k is not the 'true' k but has
 *  swapped factors of p-1 and/or q-1. Because of this, we
 *  tentatively continue Steps 3-6 inside this loop, and may return looking
 *  for another k on failure.
 * Step 3, Calculate our tentative phi=kphi/k. Note: real phi is (p-1)*(q-1).
 * Step 4a, kphi is k*(q-1), so phi is our tenative q-1. q = phi+1.
 *      If k is correct, q should be the right length and prime.
 * Step 4b, It's possible q-1 and k could have swapped factors. We now have a
 *  possible solution that meets our criteria. It may not be the only
 *      solution, however, so we keep looking. If we find more than one,
 *      we will fail since we cannot determine which is the correct
 *      solution, and returning the wrong modulus will compromise both
 *      moduli. If no other solution is found, we return the unique solution.
 *
 * This will return p & q. q may be larger than p in the case that p was given
 * and it was the smaller prime.
 */
static mp_err
rsa_get_prime_from_exponents(mp_int *e, mp_int *d, mp_int *p, mp_int *q,
                             mp_int *n, unsigned int keySizeInBits)
{
    mp_int kphi; /* k*phi */
    mp_int k;    /* current guess at 'k' */
    mp_int phi;  /* (p-1)(q-1) */
    mp_int r;    /* remainder */
    mp_int tmp;  /* p-1 if p is given */
    mp_err err = MP_OKAY;
    unsigned int order_k;

    MP_DIGITS(&kphi) = 0;
    MP_DIGITS(&phi) = 0;
    MP_DIGITS(&k) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&tmp) = 0;
    CHECK_MPI_OK(mp_init(&kphi));
    CHECK_MPI_OK(mp_init(&phi));
    CHECK_MPI_OK(mp_init(&k));
    CHECK_MPI_OK(mp_init(&r));
    CHECK_MPI_OK(mp_init(&tmp));

    /* our algorithm looks for a factor k whose maximum size is dependent
     * on the size of our smallest exponent, which had better be the public
     * exponent (if it's the private, the key is vulnerable to a brute force
     * attack).
     *
     * since our factor search is linear, we need to limit the maximum
     * size of the public key. this should not be a problem normally, since
     * public keys are usually small.
     *
     * if we want to handle larger public key sizes, we should have
     * a version which tries to 'completely' factor k*phi (where completely
     * means 'factor into primes, or composites with which are products of
     * large primes). Once we have all the factors, we can sort them out and
     * try different combinations to form our phi. The risk is if (p-1)/2,
     * (q-1)/2, and k are all large primes. In any case if the public key
     * is small (order of 20 some bits), then a linear search for k is
     * manageable.
     */
    if (mpl_significant_bits(e) > 23) {
        err = MP_RANGE;
        goto cleanup;
    }

    /* calculate k*phi = e*d - 1 */
    CHECK_MPI_OK(mp_mul(e, d, &kphi));
    CHECK_MPI_OK(mp_sub_d(&kphi, 1, &kphi));

    /* kphi is (e*d)-1, which is the same as k*(p-1)(q-1)
     * d < (p-1)(q-1), therefor k must be less than e-1
     * We can narrow down k even more, though. Since p and q are odd and both
     * have their high bit set, then we know that phi must be on order of
     * keySizeBits.
     */
    order_k = (unsigned)mpl_significant_bits(&kphi) - keySizeInBits;

    /* for (k=kinit; order(k) >= order_k; k--) { */
    /* k=kinit: k can't be bigger than  kphi/2^(keySizeInBits -1) */
    CHECK_MPI_OK(mp_2expt(&k, keySizeInBits - 1));
    CHECK_MPI_OK(mp_div(&kphi, &k, &k, NULL));
    if (mp_cmp(&k, e) >= 0) {
        /* also can't be bigger then e-1 */
        CHECK_MPI_OK(mp_sub_d(e, 1, &k));
    }

    /* calculate our temp value */
    /* This saves recalculating this value when the k guess is wrong, which
     * is reasonably frequent. */
    /* tmp = p-1 (used to calculate q-1= phi/tmp) */
    CHECK_MPI_OK(mp_sub_d(p, 1, &tmp));
    CHECK_MPI_OK(mp_div(&kphi, &tmp, &kphi, &r));
    if (mp_cmp_z(&r) != 0) {
        /* p-1 doesn't divide kphi, some parameter wasn't correct */
        err = MP_RANGE;
        goto cleanup;
    }
    mp_zero(q);
    /* kphi is now k*(q-1) */

    /* rest of the for loop */
    for (; (err == MP_OKAY) && (mpl_significant_bits(&k) >= order_k);
         err = mp_sub_d(&k, 1, &k)) {
        CHECK_MPI_OK(err);
        /* looking for k as a factor of kphi */
        CHECK_MPI_OK(mp_div(&kphi, &k, &phi, &r));
        if (mp_cmp_z(&r) != 0) {
            /* not a factor, try the next one */
            continue;
        }
        /* we have a possible phi, see if it works */
        if ((unsigned)mpl_significant_bits(&phi) != keySizeInBits / 2) {
            /* phi is not the right size */
            continue;
        }
        /* phi should be divisible by 2, since
         * q is odd and phi=(q-1). */
        if (mpp_divis_d(&phi, 2) == MP_NO) {
            /* phi is not divisible by 4 */
            continue;
        }
        /* we now have a candidate for the second prime */
        CHECK_MPI_OK(mp_add_d(&phi, 1, &tmp));

        /* check to make sure it is prime */
        err = rsa_is_prime(&tmp);
        if (err != MP_OKAY) {
            if (err == MP_NO) {
                /* No, then we still have the wrong phi */
                continue;
            }
            goto cleanup;
        }
        /*
         * It is possible that we have the wrong phi if
         * k_guess*(q_guess-1) = k*(q-1) (k and q-1 have swapped factors).
         * since our q_quess is prime, however. We have found a valid
         * rsa key because:
         *   q is the correct order of magnitude.
         *   phi = (p-1)(q-1) where p and q are both primes.
         *   e*d mod phi = 1.
         * There is no way to know from the info given if this is the
         * original key. We never want to return the wrong key because if
         * two moduli with the same factor is known, then euclid's gcd
         * algorithm can be used to find that factor. Even though the
         * caller didn't pass the original modulus, it doesn't mean the
         * modulus wasn't known or isn't available somewhere. So to be safe
         * if we can't be sure we have the right q, we don't return any.
         *
         * So to make sure we continue looking for other valid q's. If none
         * are found, then we can safely return this one, otherwise we just
         * fail */
        if (mp_cmp_z(q) != 0) {
            /* this is the second valid q, don't return either,
             * just fail */
            err = MP_RANGE;
            break;
        }
        /* we only have one q so far, save it and if no others are found,
         * it's safe to return it */
        CHECK_MPI_OK(mp_copy(&tmp, q));
        continue;
    }
    if ((unsigned)mpl_significant_bits(&k) < order_k) {
        if (mp_cmp_z(q) == 0) {
            /* If we get here, something was wrong with the parameters we
             * were given */
            err = MP_RANGE;
        }
    }
cleanup:
    mp_clear(&kphi);
    mp_clear(&phi);
    mp_clear(&k);
    mp_clear(&r);
    mp_clear(&tmp);
    return err;
}

/*
 * take a private key with only a few elements and fill out the missing pieces.
 *
 * All the entries will be overwritten with data allocated out of the arena
 * If no arena is supplied, one will be created.
 *
 * The following fields must be supplied in order for this function
 * to succeed:
 *   one of either publicExponent or privateExponent
 *   two more of the following 5 parameters.
 *      modulus (n)
 *      prime1  (p)
 *      prime2  (q)
 *      publicExponent (e)
 *      privateExponent (d)
 *
 * NOTE: if only the publicExponent, privateExponent, and one prime is given,
 * then there may be more than one RSA key that matches that combination.
 *
 * All parameters will be replaced in the key structure with new parameters
 * Allocated out of the arena. There is no attempt to free the old structures.
 * Prime1 will always be greater than prime2 (even if the caller supplies the
 * smaller prime as prime1 or the larger prime as prime2). The parameters are
 * not overwritten on failure.
 *
 *  How it works:
 *     We can generate all the parameters from one of the exponents, plus the
 *        two primes. (rsa_build_key_from_primes)
 *     If we are given one of the exponents and both primes, we are done.
 *     If we are given one of the exponents, the modulus and one prime, we
 *        caclulate the second prime by dividing the modulus by the given
 *        prime, giving us an exponent and 2 primes.
 *     If we are given 2 exponents and one of the primes we calculate
 *        k*phi = d*e-1, where k is an integer less than d which
 *        divides d*e-1. We find factor k so we can isolate phi.
 *            phi = (p-1)(q-1)
 *        We can use phi to find the other prime as follows:
 *        q = (phi/(p-1)) + 1. We now have 2 primes and an exponent.
 *        (NOTE: if more then one prime meets this condition, the operation
 *        will fail. See comments elsewhere in this file about this).
 *        (rsa_get_prime_from_exponents)
 *     If we are given 2 exponents and the modulus we factor the modulus to
 *        get the 2 missing primes (rsa_factorize_n_from_exponents)
 *
 */
SECStatus
RSA_PopulatePrivateKey(RSAPrivateKey *key)
{
    PLArenaPool *arena = NULL;
    PRBool needPublicExponent = PR_TRUE;
    PRBool needPrivateExponent = PR_TRUE;
    PRBool hasModulus = PR_FALSE;
    unsigned int keySizeInBits = 0;
    int prime_count = 0;
    /* standard RSA nominclature */
    mp_int p, q, e, d, n;
    /* remainder */
    mp_int r;
    mp_err err = 0;
    SECStatus rv = SECFailure;

    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&e) = 0;
    MP_DIGITS(&d) = 0;
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&r) = 0;
    CHECK_MPI_OK(mp_init(&p));
    CHECK_MPI_OK(mp_init(&q));
    CHECK_MPI_OK(mp_init(&e));
    CHECK_MPI_OK(mp_init(&d));
    CHECK_MPI_OK(mp_init(&n));
    CHECK_MPI_OK(mp_init(&r));

    /* if the key didn't already have an arena, create one. */
    if (key->arena == NULL) {
        arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
        if (!arena) {
            goto cleanup;
        }
        key->arena = arena;
    }

    /* load up the known exponents */
    if (key->publicExponent.data) {
        SECITEM_TO_MPINT(key->publicExponent, &e);
        needPublicExponent = PR_FALSE;
    }
    if (key->privateExponent.data) {
        SECITEM_TO_MPINT(key->privateExponent, &d);
        needPrivateExponent = PR_FALSE;
    }
    if (needPrivateExponent && needPublicExponent) {
        /* Not enough information, we need at least one exponent */
        err = MP_BADARG;
        goto cleanup;
    }

    /* load up the known primes. If only one prime is given, it will be
     * assigned 'p'. Once we have both primes, well make sure p is the larger.
     * The value prime_count tells us howe many we have acquired.
     */
    if (key->prime1.data) {
        int primeLen = key->prime1.len;
        if (key->prime1.data[0] == 0) {
            primeLen--;
        }
        keySizeInBits = primeLen * 2 * PR_BITS_PER_BYTE;
        SECITEM_TO_MPINT(key->prime1, &p);
        prime_count++;
    }
    if (key->prime2.data) {
        int primeLen = key->prime2.len;
        if (key->prime2.data[0] == 0) {
            primeLen--;
        }
        keySizeInBits = primeLen * 2 * PR_BITS_PER_BYTE;
        SECITEM_TO_MPINT(key->prime2, prime_count ? &q : &p);
        prime_count++;
    }
    /* load up the modulus */
    if (key->modulus.data) {
        int modLen = key->modulus.len;
        if (key->modulus.data[0] == 0) {
            modLen--;
        }
        keySizeInBits = modLen * PR_BITS_PER_BYTE;
        SECITEM_TO_MPINT(key->modulus, &n);
        hasModulus = PR_TRUE;
    }
    /* if we have the modulus and one prime, calculate the second. */
    if ((prime_count == 1) && (hasModulus)) {
        if (mp_div(&n, &p, &q, &r) != MP_OKAY || mp_cmp_z(&r) != 0) {
            /* p is not a factor or n, fail */
            err = MP_BADARG;
            goto cleanup;
        }
        prime_count++;
    }

    /* If we didn't have enough primes try to calculate the primes from
     * the exponents */
    if (prime_count < 2) {
        /* if we don't have at least 2 primes at this point, then we need both
         * exponents and one prime or a modulus*/
        if (!needPublicExponent && !needPrivateExponent &&
            (prime_count > 0)) {
            CHECK_MPI_OK(rsa_get_prime_from_exponents(&e, &d, &p, &q, &n,
                                                      keySizeInBits));
        } else if (!needPublicExponent && !needPrivateExponent && hasModulus) {
            CHECK_MPI_OK(rsa_factorize_n_from_exponents(&e, &d, &p, &q, &n));
        } else {
            /* not enough given parameters to get both primes */
            err = MP_BADARG;
            goto cleanup;
        }
    }

    /* Assure p > q */
    /* NOTE: PKCS #1 does not require p > q, and NSS doesn't use any
      * implementation optimization that requires p > q. We can remove
      * this code in the future.
      */
    if (mp_cmp(&p, &q) < 0)
        mp_exch(&p, &q);

    /* we now have our 2 primes and at least one exponent, we can fill
      * in the key */
    rv = rsa_build_from_primes(&p, &q,
                               &e, needPublicExponent,
                               &d, needPrivateExponent,
                               key, keySizeInBits);
cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&e);
    mp_clear(&d);
    mp_clear(&n);
    mp_clear(&r);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    if (rv && arena) {
        PORT_FreeArena(arena, PR_TRUE);
        key->arena = NULL;
    }
    return rv;
}

static unsigned int
rsa_modulusLen(SECItem *modulus)
{
    unsigned char byteZero = modulus->data[0];
    unsigned int modLen = modulus->len - !byteZero;
    return modLen;
}

/*
** Perform a raw public-key operation
**  Length of input and output buffers are equal to key's modulus len.
*/
SECStatus
RSA_PublicKeyOp(RSAPublicKey *key,
                unsigned char *output,
                const unsigned char *input)
{
    unsigned int modLen, expLen, offset;
    mp_int n, e, m, c;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    if (!key || !output || !input) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&e) = 0;
    MP_DIGITS(&m) = 0;
    MP_DIGITS(&c) = 0;
    CHECK_MPI_OK(mp_init(&n));
    CHECK_MPI_OK(mp_init(&e));
    CHECK_MPI_OK(mp_init(&m));
    CHECK_MPI_OK(mp_init(&c));
    modLen = rsa_modulusLen(&key->modulus);
    expLen = rsa_modulusLen(&key->publicExponent);
    /* 1.  Obtain public key (n, e) */
    if (BAD_RSA_KEY_SIZE(modLen, expLen)) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        rv = SECFailure;
        goto cleanup;
    }
    SECITEM_TO_MPINT(key->modulus, &n);
    SECITEM_TO_MPINT(key->publicExponent, &e);
    if (e.used > n.used) {
        /* exponent should not be greater than modulus */
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        rv = SECFailure;
        goto cleanup;
    }
    /* 2. check input out of range (needs to be in range [0..n-1]) */
    offset = (key->modulus.data[0] == 0) ? 1 : 0; /* may be leading 0 */
    if (memcmp(input, key->modulus.data + offset, modLen) >= 0) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        rv = SECFailure;
        goto cleanup;
    }
    /* 2 bis.  Represent message as integer in range [0..n-1] */
    CHECK_MPI_OK(mp_read_unsigned_octets(&m, input, modLen));
/* 3.  Compute c = m**e mod n */
#ifdef USE_MPI_EXPT_D
    /* XXX see which is faster */
    if (MP_USED(&e) == 1) {
        CHECK_MPI_OK(mp_exptmod_d(&m, MP_DIGIT(&e, 0), &n, &c));
    } else
#endif
        CHECK_MPI_OK(mp_exptmod(&m, &e, &n, &c));
    /* 4.  result c is ciphertext */
    err = mp_to_fixlen_octets(&c, output, modLen);
    if (err >= 0)
        err = MP_OKAY;
cleanup:
    mp_clear(&n);
    mp_clear(&e);
    mp_clear(&m);
    mp_clear(&c);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/*
**  RSA Private key operation (no CRT).
*/
static SECStatus
rsa_PrivateKeyOpNoCRT(RSAPrivateKey *key, mp_int *m, mp_int *c, mp_int *n,
                      unsigned int modLen)
{
    mp_int d;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    MP_DIGITS(&d) = 0;
    CHECK_MPI_OK(mp_init(&d));
    SECITEM_TO_MPINT(key->privateExponent, &d);
    /* 1. m = c**d mod n */
    CHECK_MPI_OK(mp_exptmod(c, &d, n, m));
cleanup:
    mp_clear(&d);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/*
**  RSA Private key operation using CRT.
*/
static SECStatus
rsa_PrivateKeyOpCRTNoCheck(RSAPrivateKey *key, mp_int *m, mp_int *c)
{
    mp_int p, q, d_p, d_q, qInv;
    /*
            The length of the randomness comes from the papers: 
            https://link.springer.com/chapter/10.1007/978-3-642-29912-4_7
            https://link.springer.com/chapter/10.1007/978-3-642-21554-4_5. 
        */
    mp_int blinding_dp, blinding_dq, r1, r2;
    unsigned char random_block[EXP_BLINDING_RANDOMNESS_LEN_BYTES];
    mp_int m1, m2, h, ctmp;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&d_p) = 0;
    MP_DIGITS(&d_q) = 0;
    MP_DIGITS(&qInv) = 0;
    MP_DIGITS(&m1) = 0;
    MP_DIGITS(&m2) = 0;
    MP_DIGITS(&h) = 0;
    MP_DIGITS(&ctmp) = 0;
    MP_DIGITS(&blinding_dp) = 0;
    MP_DIGITS(&blinding_dq) = 0;
    MP_DIGITS(&r1) = 0;
    MP_DIGITS(&r2) = 0;

    CHECK_MPI_OK(mp_init(&p));
    CHECK_MPI_OK(mp_init(&q));
    CHECK_MPI_OK(mp_init(&d_p));
    CHECK_MPI_OK(mp_init(&d_q));
    CHECK_MPI_OK(mp_init(&qInv));
    CHECK_MPI_OK(mp_init(&m1));
    CHECK_MPI_OK(mp_init(&m2));
    CHECK_MPI_OK(mp_init(&h));
    CHECK_MPI_OK(mp_init(&ctmp));
    CHECK_MPI_OK(mp_init(&blinding_dp));
    CHECK_MPI_OK(mp_init(&blinding_dq));
    CHECK_MPI_OK(mp_init_size(&r1, EXP_BLINDING_RANDOMNESS_LEN));
    CHECK_MPI_OK(mp_init_size(&r2, EXP_BLINDING_RANDOMNESS_LEN));

    /* copy private key parameters into mp integers */
    SECITEM_TO_MPINT(key->prime1, &p);         /* p */
    SECITEM_TO_MPINT(key->prime2, &q);         /* q */
    SECITEM_TO_MPINT(key->exponent1, &d_p);    /* d_p  = d mod (p-1) */
    SECITEM_TO_MPINT(key->exponent2, &d_q);    /* d_q  = d mod (q-1) */
    SECITEM_TO_MPINT(key->coefficient, &qInv); /* qInv = q**-1 mod p */

    // blinding_dp = 1
    CHECK_MPI_OK(mp_set_int(&blinding_dp, 1));
    // blinding_dp = p - 1
    CHECK_MPI_OK(mp_sub(&p, &blinding_dp, &blinding_dp));
    // generating a random value
    RNG_GenerateGlobalRandomBytes(random_block, EXP_BLINDING_RANDOMNESS_LEN_BYTES);
    MP_USED(&r1) = EXP_BLINDING_RANDOMNESS_LEN;
    memcpy(MP_DIGITS(&r1), random_block, sizeof(random_block));
    // blinding_dp = random * (p - 1)
    CHECK_MPI_OK(mp_mul(&blinding_dp, &r1, &blinding_dp));
    //d_p = d_p + random * (p - 1)
    CHECK_MPI_OK(mp_add(&d_p, &blinding_dp, &d_p));

    // blinding_dq = 1
    CHECK_MPI_OK(mp_set_int(&blinding_dq, 1));
    // blinding_dq = q - 1
    CHECK_MPI_OK(mp_sub(&q, &blinding_dq, &blinding_dq));
    // generating a random value
    RNG_GenerateGlobalRandomBytes(random_block, EXP_BLINDING_RANDOMNESS_LEN_BYTES);
    memcpy(MP_DIGITS(&r2), random_block, sizeof(random_block));
    MP_USED(&r2) = EXP_BLINDING_RANDOMNESS_LEN;
    // blinding_dq = random * (q - 1)
    CHECK_MPI_OK(mp_mul(&blinding_dq, &r2, &blinding_dq));
    //d_q = d_q + random * (q-1)
    CHECK_MPI_OK(mp_add(&d_q, &blinding_dq, &d_q));

    /* 1. m1 = c**d_p mod p */
    CHECK_MPI_OK(mp_mod(c, &p, &ctmp));
    CHECK_MPI_OK(mp_exptmod(&ctmp, &d_p, &p, &m1));
    /* 2. m2 = c**d_q mod q */
    CHECK_MPI_OK(mp_mod(c, &q, &ctmp));
    CHECK_MPI_OK(mp_exptmod(&ctmp, &d_q, &q, &m2));
    /* 3.  h = (m1 - m2) * qInv mod p */
    CHECK_MPI_OK(mp_submod(&m1, &m2, &p, &h));
    CHECK_MPI_OK(mp_mulmod(&h, &qInv, &p, &h));
    /* 4.  m = m2 + h * q */
    CHECK_MPI_OK(mp_mul(&h, &q, m));
    CHECK_MPI_OK(mp_add(m, &m2, m));
cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&d_p);
    mp_clear(&d_q);
    mp_clear(&qInv);
    mp_clear(&m1);
    mp_clear(&m2);
    mp_clear(&h);
    mp_clear(&ctmp);
    mp_clear(&blinding_dp);
    mp_clear(&blinding_dq);
    mp_clear(&r1);
    mp_clear(&r2);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/*
** An attack against RSA CRT was described by Boneh, DeMillo, and Lipton in:
** "On the Importance of Eliminating Errors in Cryptographic Computations",
** http://theory.stanford.edu/~dabo/papers/faults.ps.gz
**
** As a defense against the attack, carry out the private key operation,
** followed up with a public key operation to invert the result.
** Verify that result against the input.
*/
static SECStatus
rsa_PrivateKeyOpCRTCheckedPubKey(RSAPrivateKey *key, mp_int *m, mp_int *c)
{
    mp_int n, e, v;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&e) = 0;
    MP_DIGITS(&v) = 0;
    CHECK_MPI_OK(mp_init(&n));
    CHECK_MPI_OK(mp_init(&e));
    CHECK_MPI_OK(mp_init(&v));
    CHECK_SEC_OK(rsa_PrivateKeyOpCRTNoCheck(key, m, c));
    SECITEM_TO_MPINT(key->modulus, &n);
    SECITEM_TO_MPINT(key->publicExponent, &e);
    /* Perform a public key operation v = m ** e mod n */
    CHECK_MPI_OK(mp_exptmod(m, &e, &n, &v));
    if (mp_cmp(&v, c) != 0) {
        rv = SECFailure;
    }
cleanup:
    mp_clear(&n);
    mp_clear(&e);
    mp_clear(&v);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

static PRCallOnceType coBPInit = { 0, 0, 0 };
static PRStatus
init_blinding_params_list(void)
{
    blindingParamsList.lock = PZ_NewLock(nssILockOther);
    if (!blindingParamsList.lock) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return PR_FAILURE;
    }
    blindingParamsList.cVar = PR_NewCondVar(blindingParamsList.lock);
    if (!blindingParamsList.cVar) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return PR_FAILURE;
    }
    blindingParamsList.waitCount = 0;
    PR_INIT_CLIST(&blindingParamsList.head);
    return PR_SUCCESS;
}

static SECStatus
generate_blinding_params(RSAPrivateKey *key, mp_int *f, mp_int *g, mp_int *n,
                         unsigned int modLen)
{
    SECStatus rv = SECSuccess;
    mp_int e, k;
    mp_err err = MP_OKAY;
    unsigned char *kb = NULL;

    MP_DIGITS(&e) = 0;
    MP_DIGITS(&k) = 0;
    CHECK_MPI_OK(mp_init(&e));
    CHECK_MPI_OK(mp_init(&k));
    SECITEM_TO_MPINT(key->publicExponent, &e);
    /* generate random k < n */
    kb = PORT_Alloc(modLen);
    if (!kb) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto cleanup;
    }
    CHECK_SEC_OK(RNG_GenerateGlobalRandomBytes(kb, modLen));
    CHECK_MPI_OK(mp_read_unsigned_octets(&k, kb, modLen));
    /* k < n */
    CHECK_MPI_OK(mp_mod(&k, n, &k));
    /* f = k**e mod n */
    CHECK_MPI_OK(mp_exptmod(&k, &e, n, f));
    /* g = k**-1 mod n */
    CHECK_MPI_OK(mp_invmod(&k, n, g));
cleanup:
    if (kb)
        PORT_ZFree(kb, modLen);
    mp_clear(&k);
    mp_clear(&e);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

static SECStatus
init_blinding_params(RSABlindingParams *rsabp, RSAPrivateKey *key,
                     mp_int *n, unsigned int modLen)
{
    blindingParams *bp = rsabp->array;
    int i = 0;

    /* Initialize the list pointer for the element */
    PR_INIT_CLIST(&rsabp->link);
    for (i = 0; i < RSA_BLINDING_PARAMS_MAX_CACHE_SIZE; ++i, ++bp) {
        bp->next = bp + 1;
        MP_DIGITS(&bp->f) = 0;
        MP_DIGITS(&bp->g) = 0;
        bp->counter = 0;
    }
    /* The last bp->next value was initialized with out
     * of rsabp->array pointer and must be set to NULL
     */
    rsabp->array[RSA_BLINDING_PARAMS_MAX_CACHE_SIZE - 1].next = NULL;

    bp = rsabp->array;
    rsabp->bp = NULL;
    rsabp->free = bp;

    /* List elements are keyed using the modulus */
    return SECITEM_CopyItem(NULL, &rsabp->modulus, &key->modulus);
}

static SECStatus
get_blinding_params(RSAPrivateKey *key, mp_int *n, unsigned int modLen,
                    mp_int *f, mp_int *g)
{
    RSABlindingParams *rsabp = NULL;
    blindingParams *bpUnlinked = NULL;
    blindingParams *bp;
    PRCList *el;
    SECStatus rv = SECSuccess;
    mp_err err = MP_OKAY;
    int cmp = -1;
    PRBool holdingLock = PR_FALSE;

    do {
        if (blindingParamsList.lock == NULL) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
        /* Acquire the list lock */
        PZ_Lock(blindingParamsList.lock);
        holdingLock = PR_TRUE;

        /* Walk the list looking for the private key */
        for (el = PR_NEXT_LINK(&blindingParamsList.head);
             el != &blindingParamsList.head;
             el = PR_NEXT_LINK(el)) {
            rsabp = (RSABlindingParams *)el;
            cmp = SECITEM_CompareItem(&rsabp->modulus, &key->modulus);
            if (cmp >= 0) {
                /* The key is found or not in the list. */
                break;
            }
        }

        if (cmp) {
            /* At this point, the key is not in the list.  el should point to
            ** the list element before which this key should be inserted.
            */
            rsabp = PORT_ZNew(RSABlindingParams);
            if (!rsabp) {
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                goto cleanup;
            }

            rv = init_blinding_params(rsabp, key, n, modLen);
            if (rv != SECSuccess) {
                PORT_ZFree(rsabp, sizeof(RSABlindingParams));
                goto cleanup;
            }

            /* Insert the new element into the list
            ** If inserting in the middle of the list, el points to the link
            ** to insert before.  Otherwise, the link needs to be appended to
            ** the end of the list, which is the same as inserting before the
            ** head (since el would have looped back to the head).
            */
            PR_INSERT_BEFORE(&rsabp->link, el);
        }

        /* We've found (or created) the RSAblindingParams struct for this key.
         * Now, search its list of ready blinding params for a usable one.
         */
        while (0 != (bp = rsabp->bp)) {
#ifndef UNSAFE_FUZZER_MODE
            if (--(bp->counter) > 0)
#endif
            {
                /* Found a match and there are still remaining uses left */
                /* Return the parameters */
                CHECK_MPI_OK(mp_copy(&bp->f, f));
                CHECK_MPI_OK(mp_copy(&bp->g, g));

                PZ_Unlock(blindingParamsList.lock);
                return SECSuccess;
            }
            /* exhausted this one, give its values to caller, and
             * then retire it.
             */
            mp_exch(&bp->f, f);
            mp_exch(&bp->g, g);
            mp_clear(&bp->f);
            mp_clear(&bp->g);
            bp->counter = 0;
            /* Move to free list */
            rsabp->bp = bp->next;
            bp->next = rsabp->free;
            rsabp->free = bp;
            /* In case there're threads waiting for new blinding
             * value - notify 1 thread the value is ready
             */
            if (blindingParamsList.waitCount > 0) {
                PR_NotifyCondVar(blindingParamsList.cVar);
                blindingParamsList.waitCount--;
            }
            PZ_Unlock(blindingParamsList.lock);
            return SECSuccess;
        }
        /* We did not find a usable set of blinding params.  Can we make one? */
        /* Find a free bp struct. */
        if ((bp = rsabp->free) != NULL) {
            /* unlink this bp */
            rsabp->free = bp->next;
            bp->next = NULL;
            bpUnlinked = bp; /* In case we fail */

            PZ_Unlock(blindingParamsList.lock);
            holdingLock = PR_FALSE;
            /* generate blinding parameter values for the current thread */
            CHECK_SEC_OK(generate_blinding_params(key, f, g, n, modLen));

            /* put the blinding parameter values into cache */
            CHECK_MPI_OK(mp_init(&bp->f));
            CHECK_MPI_OK(mp_init(&bp->g));
            CHECK_MPI_OK(mp_copy(f, &bp->f));
            CHECK_MPI_OK(mp_copy(g, &bp->g));

            /* Put this at head of queue of usable params. */
            PZ_Lock(blindingParamsList.lock);
            holdingLock = PR_TRUE;
            (void)holdingLock;
            /* initialize RSABlindingParamsStr */
            bp->counter = RSA_BLINDING_PARAMS_MAX_REUSE;
            bp->next = rsabp->bp;
            rsabp->bp = bp;
            bpUnlinked = NULL;
            /* In case there're threads waiting for new blinding value
             * just notify them the value is ready
             */
            if (blindingParamsList.waitCount > 0) {
                PR_NotifyAllCondVar(blindingParamsList.cVar);
                blindingParamsList.waitCount = 0;
            }
            PZ_Unlock(blindingParamsList.lock);
            return SECSuccess;
        }
        /* Here, there are no usable blinding parameters available,
         * and no free bp blocks, presumably because they're all
         * actively having parameters generated for them.
         * So, we need to wait here and not eat up CPU until some
         * change happens.
         */
        blindingParamsList.waitCount++;
        PR_WaitCondVar(blindingParamsList.cVar, PR_INTERVAL_NO_TIMEOUT);
        PZ_Unlock(blindingParamsList.lock);
        holdingLock = PR_FALSE;
        (void)holdingLock;
    } while (1);

cleanup:
    /* It is possible to reach this after the lock is already released.  */
    if (bpUnlinked) {
        if (!holdingLock) {
            PZ_Lock(blindingParamsList.lock);
            holdingLock = PR_TRUE;
        }
        bp = bpUnlinked;
        mp_clear(&bp->f);
        mp_clear(&bp->g);
        bp->counter = 0;
        /* Must put the unlinked bp back on the free list */
        bp->next = rsabp->free;
        rsabp->free = bp;
    }
    if (holdingLock) {
        PZ_Unlock(blindingParamsList.lock);
    }
    if (err) {
        MP_TO_SEC_ERROR(err);
    }
    return SECFailure;
}

/*
** Perform a raw private-key operation
**  Length of input and output buffers are equal to key's modulus len.
*/
static SECStatus
rsa_PrivateKeyOp(RSAPrivateKey *key,
                 unsigned char *output,
                 const unsigned char *input,
                 PRBool check)
{
    unsigned int modLen;
    unsigned int offset;
    SECStatus rv = SECSuccess;
    mp_err err;
    mp_int n, c, m;
    mp_int f, g;
    if (!key || !output || !input) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    /* check input out of range (needs to be in range [0..n-1]) */
    modLen = rsa_modulusLen(&key->modulus);
    offset = (key->modulus.data[0] == 0) ? 1 : 0; /* may be leading 0 */
    if (memcmp(input, key->modulus.data + offset, modLen) >= 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&c) = 0;
    MP_DIGITS(&m) = 0;
    MP_DIGITS(&f) = 0;
    MP_DIGITS(&g) = 0;
    CHECK_MPI_OK(mp_init(&n));
    CHECK_MPI_OK(mp_init(&c));
    CHECK_MPI_OK(mp_init(&m));
    CHECK_MPI_OK(mp_init(&f));
    CHECK_MPI_OK(mp_init(&g));
    SECITEM_TO_MPINT(key->modulus, &n);
    OCTETS_TO_MPINT(input, &c, modLen);
    /* If blinding, compute pre-image of ciphertext by multiplying by
    ** blinding factor
    */
    if (nssRSAUseBlinding) {
        CHECK_SEC_OK(get_blinding_params(key, &n, modLen, &f, &g));
        /* c' = c*f mod n */
        CHECK_MPI_OK(mp_mulmod(&c, &f, &n, &c));
    }
    /* Do the private key operation m = c**d mod n */
    if (key->prime1.len == 0 ||
        key->prime2.len == 0 ||
        key->exponent1.len == 0 ||
        key->exponent2.len == 0 ||
        key->coefficient.len == 0) {
        CHECK_SEC_OK(rsa_PrivateKeyOpNoCRT(key, &m, &c, &n, modLen));
    } else if (check) {
        CHECK_SEC_OK(rsa_PrivateKeyOpCRTCheckedPubKey(key, &m, &c));
    } else {
        CHECK_SEC_OK(rsa_PrivateKeyOpCRTNoCheck(key, &m, &c));
    }
    /* If blinding, compute post-image of plaintext by multiplying by
    ** blinding factor
    */
    if (nssRSAUseBlinding) {
        /* m = m'*g mod n */
        CHECK_MPI_OK(mp_mulmod(&m, &g, &n, &m));
    }
    err = mp_to_fixlen_octets(&m, output, modLen);
    if (err >= 0)
        err = MP_OKAY;
cleanup:
    mp_clear(&n);
    mp_clear(&c);
    mp_clear(&m);
    mp_clear(&f);
    mp_clear(&g);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

SECStatus
RSA_PrivateKeyOp(RSAPrivateKey *key,
                 unsigned char *output,
                 const unsigned char *input)
{
    return rsa_PrivateKeyOp(key, output, input, PR_FALSE);
}

SECStatus
RSA_PrivateKeyOpDoubleChecked(RSAPrivateKey *key,
                              unsigned char *output,
                              const unsigned char *input)
{
    return rsa_PrivateKeyOp(key, output, input, PR_TRUE);
}

SECStatus
RSA_PrivateKeyCheck(const RSAPrivateKey *key)
{
    mp_int p, q, n, psub1, qsub1, e, d, d_p, d_q, qInv, res;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&psub1) = 0;
    MP_DIGITS(&qsub1) = 0;
    MP_DIGITS(&e) = 0;
    MP_DIGITS(&d) = 0;
    MP_DIGITS(&d_p) = 0;
    MP_DIGITS(&d_q) = 0;
    MP_DIGITS(&qInv) = 0;
    MP_DIGITS(&res) = 0;
    CHECK_MPI_OK(mp_init(&p));
    CHECK_MPI_OK(mp_init(&q));
    CHECK_MPI_OK(mp_init(&n));
    CHECK_MPI_OK(mp_init(&psub1));
    CHECK_MPI_OK(mp_init(&qsub1));
    CHECK_MPI_OK(mp_init(&e));
    CHECK_MPI_OK(mp_init(&d));
    CHECK_MPI_OK(mp_init(&d_p));
    CHECK_MPI_OK(mp_init(&d_q));
    CHECK_MPI_OK(mp_init(&qInv));
    CHECK_MPI_OK(mp_init(&res));

    if (!key->modulus.data || !key->prime1.data || !key->prime2.data ||
        !key->publicExponent.data || !key->privateExponent.data ||
        !key->exponent1.data || !key->exponent2.data ||
        !key->coefficient.data) {
        /* call RSA_PopulatePrivateKey first, if the application wishes to
         * recover these parameters */
        err = MP_BADARG;
        goto cleanup;
    }

    SECITEM_TO_MPINT(key->modulus, &n);
    SECITEM_TO_MPINT(key->prime1, &p);
    SECITEM_TO_MPINT(key->prime2, &q);
    SECITEM_TO_MPINT(key->publicExponent, &e);
    SECITEM_TO_MPINT(key->privateExponent, &d);
    SECITEM_TO_MPINT(key->exponent1, &d_p);
    SECITEM_TO_MPINT(key->exponent2, &d_q);
    SECITEM_TO_MPINT(key->coefficient, &qInv);
    /* p and q must be distinct. */
    if (mp_cmp(&p, &q) == 0) {
        rv = SECFailure;
        goto cleanup;
    }
#define VERIFY_MPI_EQUAL(m1, m2) \
    if (mp_cmp(m1, m2) != 0) {   \
        rv = SECFailure;         \
        goto cleanup;            \
    }
#define VERIFY_MPI_EQUAL_1(m)  \
    if (mp_cmp_d(m, 1) != 0) { \
        rv = SECFailure;       \
        goto cleanup;          \
    }
    /* n == p * q */
    CHECK_MPI_OK(mp_mul(&p, &q, &res));
    VERIFY_MPI_EQUAL(&res, &n);
    /* gcd(e, p-1) == 1 */
    CHECK_MPI_OK(mp_sub_d(&p, 1, &psub1));
    CHECK_MPI_OK(mp_gcd(&e, &psub1, &res));
    VERIFY_MPI_EQUAL_1(&res);
    /* gcd(e, q-1) == 1 */
    CHECK_MPI_OK(mp_sub_d(&q, 1, &qsub1));
    CHECK_MPI_OK(mp_gcd(&e, &qsub1, &res));
    VERIFY_MPI_EQUAL_1(&res);
    /* d*e == 1 mod p-1 */
    CHECK_MPI_OK(mp_mulmod(&d, &e, &psub1, &res));
    VERIFY_MPI_EQUAL_1(&res);
    /* d*e == 1 mod q-1 */
    CHECK_MPI_OK(mp_mulmod(&d, &e, &qsub1, &res));
    VERIFY_MPI_EQUAL_1(&res);
    /* d_p == d mod p-1 */
    CHECK_MPI_OK(mp_mod(&d, &psub1, &res));
    VERIFY_MPI_EQUAL(&res, &d_p);
    /* d_q == d mod q-1 */
    CHECK_MPI_OK(mp_mod(&d, &qsub1, &res));
    VERIFY_MPI_EQUAL(&res, &d_q);
    /* q * q**-1 == 1 mod p */
    CHECK_MPI_OK(mp_mulmod(&q, &qInv, &p, &res));
    VERIFY_MPI_EQUAL_1(&res);

cleanup:
    mp_clear(&n);
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&psub1);
    mp_clear(&qsub1);
    mp_clear(&e);
    mp_clear(&d);
    mp_clear(&d_p);
    mp_clear(&d_q);
    mp_clear(&qInv);
    mp_clear(&res);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

SECStatus
RSA_Init(void)
{
    if (PR_CallOnce(&coBPInit, init_blinding_params_list) != PR_SUCCESS) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    return SECSuccess;
}

/* cleanup at shutdown */
void
RSA_Cleanup(void)
{
    blindingParams *bp = NULL;
    if (!coBPInit.initialized)
        return;

    while (!PR_CLIST_IS_EMPTY(&blindingParamsList.head)) {
        RSABlindingParams *rsabp =
            (RSABlindingParams *)PR_LIST_HEAD(&blindingParamsList.head);
        PR_REMOVE_LINK(&rsabp->link);
        /* clear parameters cache */
        while (rsabp->bp != NULL) {
            bp = rsabp->bp;
            rsabp->bp = rsabp->bp->next;
            mp_clear(&bp->f);
            mp_clear(&bp->g);
        }
        SECITEM_ZfreeItem(&rsabp->modulus, PR_FALSE);
        PORT_Free(rsabp);
    }

    if (blindingParamsList.cVar) {
        PR_DestroyCondVar(blindingParamsList.cVar);
        blindingParamsList.cVar = NULL;
    }

    if (blindingParamsList.lock) {
        SKIP_AFTER_FORK(PZ_DestroyLock(blindingParamsList.lock));
        blindingParamsList.lock = NULL;
    }

    coBPInit.initialized = 0;
    coBPInit.inProgress = 0;
    coBPInit.status = 0;
}

/*
 * need a central place for this function to free up all the memory that
 * free_bl may have allocated along the way. Currently only RSA does this,
 * so I've put it here for now.
 */
void
BL_Cleanup(void)
{
    RSA_Cleanup();
}

PRBool bl_parentForkedAfterC_Initialize;

/*
 * Set fork flag so it can be tested in SKIP_AFTER_FORK on relevant platforms.
 */
void
BL_SetForkState(PRBool forked)
{
    bl_parentForkedAfterC_Initialize = forked;
}
