/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "alghmac.h"
#include "sechash.h"
#include "secoid.h"
#include "secport.h"

#define HMAC_PAD_SIZE 64

struct HMACContextStr {
    void *hash;
    const SECHashObject *hashobj;
    unsigned char ipad[HMAC_PAD_SIZE];
    unsigned char opad[HMAC_PAD_SIZE];
};

void
HMAC_Destroy(HMACContext *cx)
{
    if (cx == NULL)
	return;

    if (cx->hash != NULL)
	cx->hashobj->destroy(cx->hash, PR_TRUE);
    PORT_ZFree(cx, sizeof(HMACContext));
}

HMACContext *
HMAC_Create(SECOidTag      hash_alg, 
      const unsigned char *secret, 
            unsigned int   secret_len)
{
    HMACContext *cx;
    int i;
    unsigned char hashed_secret[SHA1_LENGTH];

    cx = (HMACContext*)PORT_ZAlloc(sizeof(HMACContext));
    if (cx == NULL)
	return NULL;

    switch (hash_alg) {
      case SEC_OID_MD5:
	cx->hashobj = &SECRawHashObjects[HASH_AlgMD5];
	break;
      case SEC_OID_MD2:
	cx->hashobj = &SECRawHashObjects[HASH_AlgMD2];
	break;
      case SEC_OID_SHA1:
	cx->hashobj = &SECRawHashObjects[HASH_AlgSHA1];
	break;
      default:
	goto loser;
    }

    cx->hash = cx->hashobj->create();
    if (cx->hash == NULL)
	goto loser;

    if (secret_len > HMAC_PAD_SIZE) {
	cx->hashobj->begin( cx->hash);
	cx->hashobj->update(cx->hash, secret, secret_len);
	cx->hashobj->end(   cx->hash, hashed_secret, &secret_len, 
	                 sizeof hashed_secret);
	if (secret_len != cx->hashobj->length)
	    goto loser;
	secret = (const unsigned char *)&hashed_secret[0];
    }

    PORT_Memset(cx->ipad, 0x36, sizeof cx->ipad);
    PORT_Memset(cx->opad, 0x5c, sizeof cx->opad);

    /* fold secret into padding */
    for (i = 0; i < secret_len; i++) {
	cx->ipad[i] ^= secret[i];
	cx->opad[i] ^= secret[i];
    }
    PORT_Memset(hashed_secret, 0, sizeof hashed_secret);
    return cx;

loser:
    PORT_Memset(hashed_secret, 0, sizeof hashed_secret);
    HMAC_Destroy(cx);
    return NULL;
}

void
HMAC_Begin(HMACContext *cx)
{
    /* start inner hash */
    cx->hashobj->begin(cx->hash);
    cx->hashobj->update(cx->hash, cx->ipad, sizeof(cx->ipad));
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
    if (max_result_len < cx->hashobj->length)
	return SECFailure;

    cx->hashobj->end(cx->hash, result, result_len, max_result_len);
    if (*result_len != cx->hashobj->length)
	return SECFailure;

    cx->hashobj->begin(cx->hash);
    cx->hashobj->update(cx->hash, cx->opad, sizeof(cx->opad));
    cx->hashobj->update(cx->hash, result, *result_len);
    cx->hashobj->end(cx->hash, result, result_len, max_result_len);
    return SECSuccess;
}

HMACContext *
HMAC_Clone(HMACContext *cx)
{
    HMACContext *newcx;

    newcx = (HMACContext*)PORT_ZAlloc(sizeof(HMACContext));
    if (newcx == NULL)
	goto loser;

    newcx->hashobj = cx->hashobj;
    newcx->hash = cx->hashobj->clone(cx->hash);
    if (newcx->hash == NULL)
	goto loser;
    PORT_Memcpy(newcx->ipad, cx->ipad, sizeof(cx->ipad));
    PORT_Memcpy(newcx->opad, cx->opad, sizeof(cx->opad));
    return newcx;

loser:
    HMAC_Destroy(newcx);
    return NULL;
}
