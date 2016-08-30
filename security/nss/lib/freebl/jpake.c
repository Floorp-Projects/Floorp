/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "blapi.h"
#include "secerr.h"
#include "secitem.h"
#include "secmpi.h"

/* Hash an item's length and then its value. Only items smaller than 2^16 bytes
 * are allowed. Lengths are hashed in network byte order. This is designed
 * to match the OpenSSL J-PAKE implementation.
 */
static mp_err
hashSECItem(HASHContext *hash, const SECItem *it)
{
    unsigned char length[2];

    if (it->len > 0xffff)
        return MP_BADARG;

    length[0] = (unsigned char)(it->len >> 8);
    length[1] = (unsigned char)(it->len);
    hash->hashobj->update(hash->hash_context, length, 2);
    hash->hashobj->update(hash->hash_context, it->data, it->len);
    return MP_OKAY;
}

/* Hash all public components of the signature, each prefixed with its
   length, and then convert the hash to an mp_int. */
static mp_err
hashPublicParams(HASH_HashType hashType, const SECItem *g,
                 const SECItem *gv, const SECItem *gx,
                 const SECItem *signerID, mp_int *h)
{
    mp_err err;
    unsigned char hBuf[HASH_LENGTH_MAX];
    SECItem hItem;
    HASHContext hash;

    hash.hashobj = HASH_GetRawHashObject(hashType);
    if (hash.hashobj == NULL || hash.hashobj->length > sizeof hBuf) {
        return MP_BADARG;
    }
    hash.hash_context = hash.hashobj->create();
    if (hash.hash_context == NULL) {
        return MP_MEM;
    }

    hItem.data = hBuf;
    hItem.len = hash.hashobj->length;

    hash.hashobj->begin(hash.hash_context);
    CHECK_MPI_OK(hashSECItem(&hash, g));
    CHECK_MPI_OK(hashSECItem(&hash, gv));
    CHECK_MPI_OK(hashSECItem(&hash, gx));
    CHECK_MPI_OK(hashSECItem(&hash, signerID));
    hash.hashobj->end(hash.hash_context, hItem.data, &hItem.len,
                      sizeof hBuf);
    SECITEM_TO_MPINT(hItem, h);

cleanup:
    if (hash.hash_context != NULL) {
        hash.hashobj->destroy(hash.hash_context, PR_TRUE);
    }

    return err;
}

/* Generate a Schnorr signature for round 1 or round 2 */
SECStatus
JPAKE_Sign(PLArenaPool *arena, const PQGParams *pqg, HASH_HashType hashType,
           const SECItem *signerID, const SECItem *x,
           const SECItem *testRandom, const SECItem *gxIn, SECItem *gxOut,
           SECItem *gv, SECItem *r)
{
    SECStatus rv = SECSuccess;
    mp_err err;
    mp_int p;
    mp_int q;
    mp_int g;
    mp_int X;
    mp_int GX;
    mp_int V;
    mp_int GV;
    mp_int h;
    mp_int tmp;
    mp_int R;
    SECItem v;

    if (!arena ||
        !pqg || !pqg->prime.data || pqg->prime.len == 0 ||
        !pqg->subPrime.data || pqg->subPrime.len == 0 ||
        !pqg->base.data || pqg->base.len == 0 ||
        !signerID || !signerID->data || signerID->len == 0 ||
        !x || !x->data || x->len == 0 ||
        (testRandom && (!testRandom->data || testRandom->len == 0)) ||
        (gxIn == NULL && (!gxOut || gxOut->data != NULL)) ||
        (gxIn != NULL && (!gxIn->data || gxIn->len == 0 || gxOut != NULL)) ||
        !gv || gv->data != NULL ||
        !r || r->data != NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&g) = 0;
    MP_DIGITS(&X) = 0;
    MP_DIGITS(&GX) = 0;
    MP_DIGITS(&V) = 0;
    MP_DIGITS(&GV) = 0;
    MP_DIGITS(&h) = 0;
    MP_DIGITS(&tmp) = 0;
    MP_DIGITS(&R) = 0;

    CHECK_MPI_OK(mp_init(&p));
    CHECK_MPI_OK(mp_init(&q));
    CHECK_MPI_OK(mp_init(&g));
    CHECK_MPI_OK(mp_init(&X));
    CHECK_MPI_OK(mp_init(&GX));
    CHECK_MPI_OK(mp_init(&V));
    CHECK_MPI_OK(mp_init(&GV));
    CHECK_MPI_OK(mp_init(&h));
    CHECK_MPI_OK(mp_init(&tmp));
    CHECK_MPI_OK(mp_init(&R));

    SECITEM_TO_MPINT(pqg->prime, &p);
    SECITEM_TO_MPINT(pqg->subPrime, &q);
    SECITEM_TO_MPINT(pqg->base, &g);
    SECITEM_TO_MPINT(*x, &X);

    /* gx = g^x */
    if (gxIn == NULL) {
        CHECK_MPI_OK(mp_exptmod(&g, &X, &p, &GX));
        MPINT_TO_SECITEM(&GX, gxOut, arena);
        gxIn = gxOut;
    } else {
        SECITEM_TO_MPINT(*gxIn, &GX);
    }

    /* v is a random value in the q subgroup */
    if (testRandom == NULL) {
        v.data = NULL;
        rv = DSA_NewRandom(arena, &pqg->subPrime, &v);
        if (rv != SECSuccess) {
            goto cleanup;
        }
    } else {
        v.data = testRandom->data;
        v.len = testRandom->len;
    }
    SECITEM_TO_MPINT(v, &V);

    /* gv = g^v (mod q), random v, 1 <= v < q */
    CHECK_MPI_OK(mp_exptmod(&g, &V, &p, &GV));
    MPINT_TO_SECITEM(&GV, gv, arena);

    /* h = H(g, gv, gx, signerID) */
    CHECK_MPI_OK(hashPublicParams(hashType, &pqg->base, gv, gxIn, signerID,
                                  &h));

    /* r = v - x*h (mod q) */
    CHECK_MPI_OK(mp_mulmod(&X, &h, &q, &tmp));
    CHECK_MPI_OK(mp_submod(&V, &tmp, &q, &R));
    MPINT_TO_SECITEM(&R, r, arena);

cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&g);
    mp_clear(&X);
    mp_clear(&GX);
    mp_clear(&V);
    mp_clear(&GV);
    mp_clear(&h);
    mp_clear(&tmp);
    mp_clear(&R);

    if (rv == SECSuccess && err != MP_OKAY) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/* Verify a Schnorr signature generated by the peer in round 1 or round 2. */
SECStatus
JPAKE_Verify(PLArenaPool *arena, const PQGParams *pqg, HASH_HashType hashType,
             const SECItem *signerID, const SECItem *peerID,
             const SECItem *gx, const SECItem *gv, const SECItem *r)
{
    SECStatus rv = SECSuccess;
    mp_err err;
    mp_int p;
    mp_int q;
    mp_int g;
    mp_int p_minus_1;
    mp_int GX;
    mp_int h;
    mp_int one;
    mp_int R;
    mp_int gr;
    mp_int gxh;
    mp_int gr_gxh;
    SECItem calculated;

    if (!arena ||
        !pqg || !pqg->prime.data || pqg->prime.len == 0 ||
        !pqg->subPrime.data || pqg->subPrime.len == 0 ||
        !pqg->base.data || pqg->base.len == 0 ||
        !signerID || !signerID->data || signerID->len == 0 ||
        !peerID || !peerID->data || peerID->len == 0 ||
        !gx || !gx->data || gx->len == 0 ||
        !gv || !gv->data || gv->len == 0 ||
        !r || !r->data || r->len == 0 ||
        SECITEM_CompareItem(signerID, peerID) == SECEqual) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&g) = 0;
    MP_DIGITS(&p_minus_1) = 0;
    MP_DIGITS(&GX) = 0;
    MP_DIGITS(&h) = 0;
    MP_DIGITS(&one) = 0;
    MP_DIGITS(&R) = 0;
    MP_DIGITS(&gr) = 0;
    MP_DIGITS(&gxh) = 0;
    MP_DIGITS(&gr_gxh) = 0;
    calculated.data = NULL;

    CHECK_MPI_OK(mp_init(&p));
    CHECK_MPI_OK(mp_init(&q));
    CHECK_MPI_OK(mp_init(&g));
    CHECK_MPI_OK(mp_init(&p_minus_1));
    CHECK_MPI_OK(mp_init(&GX));
    CHECK_MPI_OK(mp_init(&h));
    CHECK_MPI_OK(mp_init(&one));
    CHECK_MPI_OK(mp_init(&R));
    CHECK_MPI_OK(mp_init(&gr));
    CHECK_MPI_OK(mp_init(&gxh));
    CHECK_MPI_OK(mp_init(&gr_gxh));

    SECITEM_TO_MPINT(pqg->prime, &p);
    SECITEM_TO_MPINT(pqg->subPrime, &q);
    SECITEM_TO_MPINT(pqg->base, &g);
    SECITEM_TO_MPINT(*gx, &GX);
    SECITEM_TO_MPINT(*r, &R);

    CHECK_MPI_OK(mp_sub_d(&p, 1, &p_minus_1));
    CHECK_MPI_OK(mp_exptmod(&GX, &q, &p, &one));
    /* Check g^x is in [1, p-2], R is in [0, q-1], and (g^x)^q mod p == 1 */
    if (!(mp_cmp_z(&GX) > 0 &&
          mp_cmp(&GX, &p_minus_1) < 0 &&
          mp_cmp(&R, &q) < 0 &&
          mp_cmp_d(&one, 1) == 0)) {
        goto badSig;
    }

    CHECK_MPI_OK(hashPublicParams(hashType, &pqg->base, gv, gx, peerID,
                                  &h));

    /* Calculate g^v = g^r * g^x^h */
    CHECK_MPI_OK(mp_exptmod(&g, &R, &p, &gr));
    CHECK_MPI_OK(mp_exptmod(&GX, &h, &p, &gxh));
    CHECK_MPI_OK(mp_mulmod(&gr, &gxh, &p, &gr_gxh));

    /* Compare calculated g^v to given g^v */
    MPINT_TO_SECITEM(&gr_gxh, &calculated, arena);
    if (calculated.len == gv->len &&
        NSS_SecureMemcmp(calculated.data, gv->data, calculated.len) == 0) {
        rv = SECSuccess;
    } else {
    badSig:
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        rv = SECFailure;
    }

cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&g);
    mp_clear(&p_minus_1);
    mp_clear(&GX);
    mp_clear(&h);
    mp_clear(&one);
    mp_clear(&R);
    mp_clear(&gr);
    mp_clear(&gxh);
    mp_clear(&gr_gxh);

    if (rv == SECSuccess && err != MP_OKAY) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/* Calculate base = gx1*gx3*gx4 (mod p), i.e. g^(x1+x3+x4) (mod p) */
static mp_err
jpake_Round2Base(const SECItem *gx1, const SECItem *gx3,
                 const SECItem *gx4, const mp_int *p, mp_int *base)
{
    mp_err err;
    mp_int GX1;
    mp_int GX3;
    mp_int GX4;
    mp_int tmp;

    MP_DIGITS(&GX1) = 0;
    MP_DIGITS(&GX3) = 0;
    MP_DIGITS(&GX4) = 0;
    MP_DIGITS(&tmp) = 0;

    CHECK_MPI_OK(mp_init(&GX1));
    CHECK_MPI_OK(mp_init(&GX3));
    CHECK_MPI_OK(mp_init(&GX4));
    CHECK_MPI_OK(mp_init(&tmp));

    SECITEM_TO_MPINT(*gx1, &GX1);
    SECITEM_TO_MPINT(*gx3, &GX3);
    SECITEM_TO_MPINT(*gx4, &GX4);

    /* In round 2, the peer/attacker sends us g^x3 and g^x4 and the protocol
       requires that these values are distinct. */
    if (mp_cmp(&GX3, &GX4) == 0) {
        return MP_BADARG;
    }

    CHECK_MPI_OK(mp_mul(&GX1, &GX3, &tmp));
    CHECK_MPI_OK(mp_mul(&tmp, &GX4, &tmp));
    CHECK_MPI_OK(mp_mod(&tmp, p, base));

cleanup:
    mp_clear(&GX1);
    mp_clear(&GX3);
    mp_clear(&GX4);
    mp_clear(&tmp);
    return err;
}

SECStatus
JPAKE_Round2(PLArenaPool *arena,
             const SECItem *p, const SECItem *q, const SECItem *gx1,
             const SECItem *gx3, const SECItem *gx4, SECItem *base,
             const SECItem *x2, const SECItem *s, SECItem *x2s)
{
    mp_err err;
    mp_int P;
    mp_int Q;
    mp_int X2;
    mp_int S;
    mp_int result;

    if (!arena ||
        !p || !p->data || p->len == 0 ||
        !q || !q->data || q->len == 0 ||
        !gx1 || !gx1->data || gx1->len == 0 ||
        !gx3 || !gx3->data || gx3->len == 0 ||
        !gx4 || !gx4->data || gx4->len == 0 ||
        !base || base->data != NULL ||
        (x2s != NULL && (x2s->data != NULL ||
                         !x2 || !x2->data || x2->len == 0 ||
                         !s || !s->data || s->len == 0))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    MP_DIGITS(&P) = 0;
    MP_DIGITS(&Q) = 0;
    MP_DIGITS(&X2) = 0;
    MP_DIGITS(&S) = 0;
    MP_DIGITS(&result) = 0;

    CHECK_MPI_OK(mp_init(&P));
    CHECK_MPI_OK(mp_init(&Q));
    CHECK_MPI_OK(mp_init(&result));

    if (x2s != NULL) {
        CHECK_MPI_OK(mp_init(&X2));
        CHECK_MPI_OK(mp_init(&S));

        SECITEM_TO_MPINT(*q, &Q);
        SECITEM_TO_MPINT(*x2, &X2);

        SECITEM_TO_MPINT(*s, &S);
        /* S must be in [1, Q-1] */
        if (mp_cmp_z(&S) <= 0 || mp_cmp(&S, &Q) >= 0) {
            err = MP_BADARG;
            goto cleanup;
        }

        CHECK_MPI_OK(mp_mulmod(&X2, &S, &Q, &result));
        MPINT_TO_SECITEM(&result, x2s, arena);
    }

    SECITEM_TO_MPINT(*p, &P);
    CHECK_MPI_OK(jpake_Round2Base(gx1, gx3, gx4, &P, &result));
    MPINT_TO_SECITEM(&result, base, arena);

cleanup:
    mp_clear(&P);
    mp_clear(&Q);
    mp_clear(&X2);
    mp_clear(&S);
    mp_clear(&result);

    if (err != MP_OKAY) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus
JPAKE_Final(PLArenaPool *arena, const SECItem *p, const SECItem *q,
            const SECItem *x2, const SECItem *gx4, const SECItem *x2s,
            const SECItem *B, SECItem *K)
{
    mp_err err;
    mp_int P;
    mp_int Q;
    mp_int tmp;
    mp_int exponent;
    mp_int divisor;
    mp_int base;

    if (!arena ||
        !p || !p->data || p->len == 0 ||
        !q || !q->data || q->len == 0 ||
        !x2 || !x2->data || x2->len == 0 ||
        !gx4 || !gx4->data || gx4->len == 0 ||
        !x2s || !x2s->data || x2s->len == 0 ||
        !B || !B->data || B->len == 0 ||
        !K || K->data != NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    MP_DIGITS(&P) = 0;
    MP_DIGITS(&Q) = 0;
    MP_DIGITS(&tmp) = 0;
    MP_DIGITS(&exponent) = 0;
    MP_DIGITS(&divisor) = 0;
    MP_DIGITS(&base) = 0;

    CHECK_MPI_OK(mp_init(&P));
    CHECK_MPI_OK(mp_init(&Q));
    CHECK_MPI_OK(mp_init(&tmp));
    CHECK_MPI_OK(mp_init(&exponent));
    CHECK_MPI_OK(mp_init(&divisor));
    CHECK_MPI_OK(mp_init(&base));

    /* exponent = -x2s (mod q) */
    SECITEM_TO_MPINT(*q, &Q);
    SECITEM_TO_MPINT(*x2s, &tmp);
    /*  q == 0 (mod q), so q - x2s == -x2s (mod q) */
    CHECK_MPI_OK(mp_sub(&Q, &tmp, &exponent));

    /* divisor = gx4^-x2s = 1/(gx4^x2s) (mod p) */
    SECITEM_TO_MPINT(*p, &P);
    SECITEM_TO_MPINT(*gx4, &tmp);
    CHECK_MPI_OK(mp_exptmod(&tmp, &exponent, &P, &divisor));

    /* base = B*divisor = B/(gx4^x2s) (mod p) */
    SECITEM_TO_MPINT(*B, &tmp);
    CHECK_MPI_OK(mp_mulmod(&divisor, &tmp, &P, &base));

    /* tmp = base^x2 (mod p) */
    SECITEM_TO_MPINT(*x2, &exponent);
    CHECK_MPI_OK(mp_exptmod(&base, &exponent, &P, &tmp));

    MPINT_TO_SECITEM(&tmp, K, arena);

cleanup:
    mp_clear(&P);
    mp_clear(&Q);
    mp_clear(&tmp);
    mp_clear(&exponent);
    mp_clear(&divisor);
    mp_clear(&base);

    if (err != MP_OKAY) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }
    return SECSuccess;
}
