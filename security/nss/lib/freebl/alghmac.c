/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "secport.h"
#include "hasht.h"
#include "blapit.h"
#include "alghmac.h"
#include "secerr.h"

#define HMAC_PAD_SIZE HASH_BLOCK_LENGTH_MAX

struct HMACContextStr {
    void *hash;
    const SECHashObject *hashobj;
    PRBool wasAllocated;
    unsigned char ipad[HMAC_PAD_SIZE];
    unsigned char opad[HMAC_PAD_SIZE];
};

void
HMAC_Destroy(HMACContext *cx, PRBool freeit)
{
    if (cx == NULL)
        return;

    PORT_Assert(!freeit == !cx->wasAllocated);
    if (cx->hash != NULL) {
        cx->hashobj->destroy(cx->hash, PR_TRUE);
        PORT_Memset(cx, 0, sizeof *cx);
    }
    if (freeit)
        PORT_Free(cx);
}

static SECStatus
hmac_initKey(HMACContext *cx, const unsigned char *secret,
             unsigned int secret_len, PRBool isFIPS)
{
    unsigned int i;
    unsigned char hashed_secret[HASH_LENGTH_MAX];

    /* required by FIPS 198 Section 3 */
    if (isFIPS && secret_len < cx->hashobj->length / 2) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (secret_len > cx->hashobj->blocklength) {
        cx->hashobj->begin(cx->hash);
        cx->hashobj->update(cx->hash, secret, secret_len);
        PORT_Assert(cx->hashobj->length <= sizeof hashed_secret);
        cx->hashobj->end(cx->hash, hashed_secret, &secret_len,
                         sizeof hashed_secret);
        if (secret_len != cx->hashobj->length) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            goto loser;
        }
        secret = (const unsigned char *)&hashed_secret[0];
    }

    PORT_Memset(cx->ipad, 0x36, cx->hashobj->blocklength);
    PORT_Memset(cx->opad, 0x5c, cx->hashobj->blocklength);

    /* fold secret into padding */
    for (i = 0; i < secret_len; i++) {
        cx->ipad[i] ^= secret[i];
        cx->opad[i] ^= secret[i];
    }
    PORT_Memset(hashed_secret, 0, sizeof hashed_secret);
    return SECSuccess;

loser:
    PORT_Memset(hashed_secret, 0, sizeof hashed_secret);
    return SECFailure;
}

SECStatus
HMAC_Init(HMACContext *cx, const SECHashObject *hash_obj,
          const unsigned char *secret, unsigned int secret_len, PRBool isFIPS)
{
    SECStatus rv;

    if (cx == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    cx->wasAllocated = PR_FALSE;
    cx->hashobj = hash_obj;
    cx->hash = cx->hashobj->create();
    if (cx->hash == NULL)
        goto loser;

    rv = hmac_initKey(cx, secret, secret_len, isFIPS);
    if (rv != SECSuccess)
        goto loser;

    return rv;
loser:
    if (cx->hash != NULL)
        cx->hashobj->destroy(cx->hash, PR_TRUE);
    return SECFailure;
}

HMACContext *
HMAC_Create(const SECHashObject *hash_obj, const unsigned char *secret,
            unsigned int secret_len, PRBool isFIPS)
{
    SECStatus rv;
    HMACContext *cx = PORT_ZNew(HMACContext);
    if (cx == NULL)
        return NULL;
    rv = HMAC_Init(cx, hash_obj, secret, secret_len, isFIPS);
    cx->wasAllocated = PR_TRUE;
    if (rv != SECSuccess) {
        PORT_Free(cx); /* contains no secret info */
        cx = NULL;
    }
    return cx;
}

/* this allows us to reuse an existing HMACContext with a new key and
 * Hash function */
SECStatus
HMAC_ReInit(HMACContext *cx, const SECHashObject *hash_obj,
            const unsigned char *secret, unsigned int secret_len, PRBool isFIPS)
{
    PRBool wasAllocated;
    SECStatus rv;

    /* if we are using the same hash, keep the hash contexts and only
     * init the key */
    if ((cx->hashobj == hash_obj) && (cx->hash != NULL)) {
        return hmac_initKey(cx, secret, secret_len, isFIPS);
    }
    /* otherwise we destroy the contents of the context and
     * initalize it from scratch. We need to preseve the current state
     * of wasAllocated to the final destroy works correctly */
    wasAllocated = cx->wasAllocated;
    cx->wasAllocated = PR_FALSE;
    HMAC_Destroy(cx, PR_FALSE);
    rv = HMAC_Init(cx, hash_obj, secret, secret_len, isFIPS);
    if (rv != SECSuccess) {
        return rv;
    }
    cx->wasAllocated = wasAllocated;
    return SECSuccess;
}

void
HMAC_Begin(HMACContext *cx)
{
    /* start inner hash */
    cx->hashobj->begin(cx->hash);
    cx->hashobj->update(cx->hash, cx->ipad, cx->hashobj->blocklength);
}

void
HMAC_Update(HMACContext *cx, const unsigned char *data, unsigned int data_len)
{
    cx->hashobj->update(cx->hash, data, data_len);
}

SECStatus
HMAC_Finish(HMACContext *cx, unsigned char *result, unsigned int *result_len,
            unsigned int max_result_len)
{
    if (max_result_len < cx->hashobj->length) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    cx->hashobj->end(cx->hash, result, result_len, max_result_len);
    if (*result_len != cx->hashobj->length)
        return SECFailure;

    cx->hashobj->begin(cx->hash);
    cx->hashobj->update(cx->hash, cx->opad, cx->hashobj->blocklength);
    cx->hashobj->update(cx->hash, result, *result_len);
    cx->hashobj->end(cx->hash, result, result_len, max_result_len);
    return SECSuccess;
}

HMACContext *
HMAC_Clone(HMACContext *cx)
{
    HMACContext *newcx;

    newcx = (HMACContext *)PORT_ZAlloc(sizeof(HMACContext));
    if (newcx == NULL)
        goto loser;

    newcx->wasAllocated = PR_TRUE;
    newcx->hashobj = cx->hashobj;
    newcx->hash = cx->hashobj->clone(cx->hash);
    if (newcx->hash == NULL)
        goto loser;
    PORT_Memcpy(newcx->ipad, cx->ipad, cx->hashobj->blocklength);
    PORT_Memcpy(newcx->opad, cx->opad, cx->hashobj->blocklength);
    return newcx;

loser:
    HMAC_Destroy(newcx, PR_TRUE);
    return NULL;
}
