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
#include "secasn1.h"
#include "secitem.h"
#include "prerr.h"

#ifndef DSA_SUBPRIME_LEN
#define DSA_SUBPRIME_LEN 20	/* bytes */
#endif

typedef struct {
    SECItem r;
    SECItem s;
} DSA_ASN1Signature;

const SEC_ASN1Template DSA_SignatureTemplate[] =
{
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(DSA_ASN1Signature) },
    { SEC_ASN1_INTEGER, offsetof(DSA_ASN1Signature,r) },
    { SEC_ASN1_INTEGER, offsetof(DSA_ASN1Signature,s) },
    { 0, }
};

/* Input is variable length multi-byte integer, MSB first (big endian).
** Most signficant bit of first byte is NOT treated as a sign bit. 
** May be one or more leading bytes of zeros. 
** Output is variable length multi-byte integer, MSB first (big endian).
** Most significant bit of first byte will be zero (positive sign bit)
** No more than one leading zero byte.
** Caller supplies dest buffer, and assures that it is long enough,
** e.g. at least one byte longer that src's buffer.
*/
void
DSAU_ConvertUnsignedToSigned(SECItem *dest, SECItem *src)
{
    unsigned char *pSrc = src->data;
    unsigned char *pDst = dest->data;
    unsigned int   cntSrc = src->len;
    unsigned int   cntDst = dest->len;
    unsigned char  c;

    /* skip any leading zeros. */
    while (cntSrc && !(c = *pSrc)) { 
    	pSrc++; 
	cntSrc--;
    }
    if (!cntSrc) {
    	*pDst = 0; 
	dest->len = 1; 
	return; 
    }

    if (c & 0x80)
    	*pDst++ = 0;

    PORT_Memcpy(pDst, pSrc, cntSrc);
    dest->len = (pDst - dest->data) + cntSrc;
}

/*
** src is a buffer holding a signed variable length integer.
** dest is a buffer which will be filled with an unsigned integer,
** MSB first (big endian) with leading zeros, so that the last byte
** of src will be the LSB of the integer.  The result will be exactly
** the length specified by the caller in dest->len.
** src can be shorter than dest.  src can be longer than dst, but only
** if the extra leading bytes are zeros.
*/
SECStatus
DSAU_ConvertSignedToFixedUnsigned(SECItem *dest, SECItem *src)
{
    unsigned char *pSrc = src->data;
    unsigned char *pDst = dest->data;
    unsigned int   cntSrc = src->len;
    unsigned int   cntDst = dest->len;
    int            zCount = cntDst - cntSrc;

    if (zCount > 0) {
    	PORT_Memset(pDst, 0, zCount);
	PORT_Memcpy(pDst + zCount, pSrc, cntSrc);
	return SECSuccess;
    }
    if (zCount <= 0) {
	/* Source is longer than destination.  Check for leading zeros. */
	while (zCount++ < 0) {
	    if (*pSrc++ != 0)
		goto loser;
	}
    }
    PORT_Memcpy(pDst, pSrc, cntDst);
    return SECSuccess;

loser:
    PORT_SetError( PR_INVALID_ARGUMENT_ERROR );
    return SECFailure;
}

/* src is a "raw" DSA signature, 20 bytes of r followed by 20 bytes of s.
** dest is the signature DER encoded. ?
*/
SECStatus
DSAU_EncodeDerSig(SECItem *dest, SECItem *src)
{
    SECItem *         item;
    SECItem           srcItem;
    DSA_ASN1Signature sig;
    unsigned char     signedR[DSA_SUBPRIME_LEN + 1];
    unsigned char     signedS[DSA_SUBPRIME_LEN + 1];

    PORT_Memset(&sig, 0, sizeof(sig));

    PORT_Assert(src->len == 2 * DSA_SUBPRIME_LEN);
    if (src->len != 2 * DSA_SUBPRIME_LEN) {
    	PORT_SetError( PR_INVALID_ARGUMENT_ERROR );
	return SECFailure;
    }

    /* Must convert r and s from "unsigned" integers to "signed" integers.
    ** If the high order bit of the first byte (MSB) is 1, then must
    ** prepend with leading zero.  
    ** Must remove all but one leading zero byte from numbers.
    */
    sig.r.data = signedR;
    sig.r.len  = sizeof signedR;
    sig.s.data = signedS;
    sig.s.len  = sizeof signedR;

    srcItem.data = src->data;
    srcItem.len  = DSA_SUBPRIME_LEN;

    DSAU_ConvertUnsignedToSigned(&sig.r, &srcItem);
    srcItem.data += DSA_SUBPRIME_LEN;
    DSAU_ConvertUnsignedToSigned(&sig.s, &srcItem);

    item = SEC_ASN1EncodeItem(NULL, dest, &sig, DSA_SignatureTemplate);
    if (item == NULL)
	return SECFailure;

    /* XXX leak item? */
    return SECSuccess;
}

/* src is a DER-encoded DSA signature.
** Returns a newly-allocated SECItem structure, pointing at a newly allocated
** buffer containing the "raw" DSA signature, which is 20 bytes of r,
** followed by 20 bytes of s.
*/
SECItem *
DSAU_DecodeDerSig(SECItem *item)
{
    SECItem *         result = NULL;
    SECStatus         status;
    DSA_ASN1Signature sig;
    SECItem           dst;

    PORT_Memset(&sig, 0, sizeof(sig));

    result = PORT_ZNew(SECItem);
    if (result == NULL)
	goto loser;

    result->len  = 2 * DSA_SUBPRIME_LEN;
    result->data = (unsigned char*)PORT_Alloc(2 * DSA_SUBPRIME_LEN);
    if (result->data == NULL)
	goto loser;

    status = SEC_ASN1DecodeItem(NULL, &sig, DSA_SignatureTemplate, item);
    if (status != SECSuccess)
	goto loser;

    /* Convert sig.r and sig.s from variable  length signed integers to 
    ** fixed length unsigned integers.
    */
    dst.data = result->data;
    dst.len  = DSA_SUBPRIME_LEN;
    status = DSAU_ConvertSignedToFixedUnsigned(&dst, &sig.r);
    if (status != SECSuccess)
    	goto loser;

    dst.data += DSA_SUBPRIME_LEN;
    status = DSAU_ConvertSignedToFixedUnsigned(&dst, &sig.s);
    if (status != SECSuccess)
    	goto loser;

done:
    if (sig.r.data != NULL)
	PORT_Free(sig.r.data);
    if (sig.s.data != NULL)
	PORT_Free(sig.s.data);

    return result;

loser:
    if (result != NULL) {
	SECITEM_FreeItem(result, PR_TRUE);
	result = NULL;
    }
    goto done;
}
