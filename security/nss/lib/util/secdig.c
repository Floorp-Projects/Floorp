/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "secdig.h"

#include "secoid.h"
#include "secasn1.h"
#include "secerr.h"

/*
 * XXX Want to have a SGN_DecodeDigestInfo, like:
 *  SGNDigestInfo *SGN_DecodeDigestInfo(SECItem *didata);
 * that creates a pool and allocates from it and decodes didata into
 * the newly allocated DigestInfo structure.  Then fix secvfy.c (it
 * will no longer need an arena itself) to call this and then call
 * DestroyDigestInfo when it is done, then can remove the old template
 * above and keep our new template static and "hidden".
 */

/*
 * XXX It might be nice to combine the following two functions (create
 * and encode).  I think that is all anybody ever wants to do anyway.
 */

SECItem *
SGN_EncodeDigestInfo(PLArenaPool *poolp, SECItem *dest, SGNDigestInfo *diginfo)
{
    return SEC_ASN1EncodeItem(poolp, dest, diginfo, sgn_DigestInfoTemplate);
}

SGNDigestInfo *
SGN_CreateDigestInfo(SECOidTag algorithm, const unsigned char *sig,
                     unsigned len)
{
    SGNDigestInfo *di;
    SECStatus rv;
    PLArenaPool *arena;
    SECItem *null_param;
    SECItem dummy_value;

    switch (algorithm) {
        case SEC_OID_MD2:
        case SEC_OID_MD5:
        case SEC_OID_SHA1:
        case SEC_OID_SHA224:
        case SEC_OID_SHA256:
        case SEC_OID_SHA384:
        case SEC_OID_SHA512:
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return NULL;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        return NULL;
    }

    di = (SGNDigestInfo *)PORT_ArenaZAlloc(arena, sizeof(SGNDigestInfo));
    if (di == NULL) {
        PORT_FreeArena(arena, PR_FALSE);
        return NULL;
    }

    di->arena = arena;

    /*
     * PKCS #1 specifies that the AlgorithmID must have a NULL parameter
     * (as opposed to no parameter at all).
     */
    dummy_value.data = NULL;
    dummy_value.len = 0;
    null_param = SEC_ASN1EncodeItem(NULL, NULL, &dummy_value, SEC_NullTemplate);
    if (null_param == NULL) {
        goto loser;
    }

    rv = SECOID_SetAlgorithmID(arena, &di->digestAlgorithm, algorithm,
                               null_param);

    SECITEM_FreeItem(null_param, PR_TRUE);

    if (rv != SECSuccess) {
        goto loser;
    }

    di->digest.data = (unsigned char *)PORT_ArenaAlloc(arena, len);
    if (di->digest.data == NULL) {
        goto loser;
    }

    di->digest.len = len;
    PORT_Memcpy(di->digest.data, sig, len);
    return di;

loser:
    SGN_DestroyDigestInfo(di);
    return NULL;
}

SGNDigestInfo *
SGN_DecodeDigestInfo(SECItem *didata)
{
    PLArenaPool *arena;
    SGNDigestInfo *di;
    SECStatus rv = SECFailure;
    SECItem diCopy = { siBuffer, NULL, 0 };

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (arena == NULL)
        return NULL;

    rv = SECITEM_CopyItem(arena, &diCopy, didata);
    if (rv != SECSuccess) {
        PORT_FreeArena(arena, PR_FALSE);
        return NULL;
    }

    di = (SGNDigestInfo *)PORT_ArenaZAlloc(arena, sizeof(SGNDigestInfo));
    if (di != NULL) {
        di->arena = arena;
        rv = SEC_QuickDERDecodeItem(arena, di, sgn_DigestInfoTemplate, &diCopy);
    }

    if ((di == NULL) || (rv != SECSuccess)) {
        PORT_FreeArena(arena, PR_FALSE);
        di = NULL;
    }

    return di;
}

void
SGN_DestroyDigestInfo(SGNDigestInfo *di)
{
    if (di && di->arena) {
        PORT_FreeArena(di->arena, PR_TRUE);
    }

    return;
}

SECStatus
SGN_CopyDigestInfo(PLArenaPool *poolp, SGNDigestInfo *a, SGNDigestInfo *b)
{
    SECStatus rv;
    void *mark;

    if ((poolp == NULL) || (a == NULL) || (b == NULL))
        return SECFailure;

    mark = PORT_ArenaMark(poolp);
    a->arena = poolp;
    rv = SECOID_CopyAlgorithmID(poolp, &a->digestAlgorithm,
                                &b->digestAlgorithm);
    if (rv == SECSuccess)
        rv = SECITEM_CopyItem(poolp, &a->digest, &b->digest);

    if (rv != SECSuccess) {
        PORT_ArenaRelease(poolp, mark);
    } else {
        PORT_ArenaUnmark(poolp, mark);
    }

    return rv;
}

SECComparison
SGN_CompareDigestInfo(SGNDigestInfo *a, SGNDigestInfo *b)
{
    SECComparison rv;

    /* Check signature algorithm's */
    rv = SECOID_CompareAlgorithmID(&a->digestAlgorithm, &b->digestAlgorithm);
    if (rv)
        return rv;

    /* Compare signature block length's */
    rv = SECITEM_CompareItem(&a->digest, &b->digest);
    return rv;
}
