/* tlsprfalg.c - TLS Pseudo Random Function (PRF) implementation
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/* $Id: tlsprfalg.c,v 1.7 2010/08/10 22:03:36 rrelyea%redhat.com Exp $ */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "sechash.h"
#include "alghmac.h"
#include "blapi.h"


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

