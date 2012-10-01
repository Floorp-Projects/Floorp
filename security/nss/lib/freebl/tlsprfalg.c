/* tlsprfalg.c - TLS Pseudo Random Function (PRF) implementation
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: tlsprfalg.c,v 1.9 2012/06/26 22:27:29 rrelyea%redhat.com Exp $ */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "blapi.h"
#include "hasht.h"
#include "alghmac.h"


#define PHASH_STATE_MAX_LEN HASH_LENGTH_MAX

/* TLS P_hash function */
SECStatus
TLS_P_hash(HASH_HashType hashType, const SECItem *secret, const char *label, 
	SECItem *seed, SECItem *result, PRBool isFIPS)
{
    unsigned char state[PHASH_STATE_MAX_LEN];
    unsigned char outbuf[PHASH_STATE_MAX_LEN];
    unsigned int state_len = 0, label_len = 0, outbuf_len = 0, chunk_size;
    unsigned int remaining;
    unsigned char *res;
    SECStatus status;
    HMACContext *cx;
    SECStatus rv = SECFailure;
    const SECHashObject *hashObj = HASH_GetRawHashObject(hashType);

    PORT_Assert((secret != NULL) && (secret->data != NULL || !secret->len));
    PORT_Assert((seed != NULL) && (seed->data != NULL));
    PORT_Assert((result != NULL) && (result->data != NULL));

    remaining = result->len;
    res = result->data;

    if (label != NULL)
	label_len = PORT_Strlen(label);

    cx = HMAC_Create(hashObj, secret->data, secret->len, isFIPS);
    if (cx == NULL)
	goto loser;

    /* initialize the state = A(1) = HMAC_hash(secret, seed) */
    HMAC_Begin(cx);
    HMAC_Update(cx, (unsigned char *)label, label_len);
    HMAC_Update(cx, seed->data, seed->len);
    status = HMAC_Finish(cx, state, &state_len, sizeof(state));
    if (status != SECSuccess)
	goto loser;

    /* generate a block at a time until we're done */
    while (remaining > 0) {

	HMAC_Begin(cx);
	HMAC_Update(cx, state, state_len);
	if (label_len)
	    HMAC_Update(cx, (unsigned char *)label, label_len);
	HMAC_Update(cx, seed->data, seed->len);
	status = HMAC_Finish(cx, outbuf, &outbuf_len, sizeof(outbuf));
	if (status != SECSuccess)
	    goto loser;

        /* Update the state = A(i) = HMAC_hash(secret, A(i-1)) */
	HMAC_Begin(cx); 
	HMAC_Update(cx, state, state_len); 
	status = HMAC_Finish(cx, state, &state_len, sizeof(state));
	if (status != SECSuccess)
	    goto loser;

	chunk_size = PR_MIN(outbuf_len, remaining);
	PORT_Memcpy(res, &outbuf, chunk_size);
	res += chunk_size;
	remaining -= chunk_size;
    }

    rv = SECSuccess;

loser:
    /* clear out state so it's not left on the stack */
    if (cx) 
    	HMAC_Destroy(cx, PR_TRUE);
    PORT_Memset(state, 0, sizeof(state));
    PORT_Memset(outbuf, 0, sizeof(outbuf));
    return rv;
}

SECStatus
TLS_PRF(const SECItem *secret, const char *label, SECItem *seed, 
         SECItem *result, PRBool isFIPS)
{
    SECStatus rv = SECFailure, status;
    unsigned int i;
    SECItem tmp = { siBuffer, NULL, 0};
    SECItem S1;
    SECItem S2;

    PORT_Assert((secret != NULL) && (secret->data != NULL || !secret->len));
    PORT_Assert((seed != NULL) && (seed->data != NULL));
    PORT_Assert((result != NULL) && (result->data != NULL));

    S1.type = siBuffer;
    S1.len  = (secret->len / 2) + (secret->len & 1);
    S1.data = secret->data;

    S2.type = siBuffer;
    S2.len  = S1.len;
    S2.data = secret->data + (secret->len - S2.len);

    tmp.data = (unsigned char*)PORT_Alloc(result->len);
    if (tmp.data == NULL)
	goto loser;
    tmp.len = result->len;

    status = TLS_P_hash(HASH_AlgMD5, &S1, label, seed, result, isFIPS);
    if (status != SECSuccess)
	goto loser;

    status = TLS_P_hash(HASH_AlgSHA1, &S2, label, seed, &tmp, isFIPS);
    if (status != SECSuccess)
	goto loser;

    for (i = 0; i < result->len; i++)
	result->data[i] ^= tmp.data[i];

    rv = SECSuccess;

loser:
    if (tmp.data != NULL)
	PORT_ZFree(tmp.data, tmp.len);
    return rv;
}

