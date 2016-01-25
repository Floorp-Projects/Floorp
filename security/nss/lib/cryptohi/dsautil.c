/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cryptohi.h"
#include "secasn1.h"
#include "secitem.h"
#include "prerr.h"

#ifndef DSA1_SUBPRIME_LEN
#define DSA1_SUBPRIME_LEN 20 /* bytes */
#endif

typedef struct {
    SECItem r;
    SECItem s;
} DSA_ASN1Signature;

const SEC_ASN1Template DSA_SignatureTemplate[] =
    {
      { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(DSA_ASN1Signature) },
      { SEC_ASN1_INTEGER, offsetof(DSA_ASN1Signature, r) },
      { SEC_ASN1_INTEGER, offsetof(DSA_ASN1Signature, s) },
      { 0 }
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
    unsigned int cntSrc = src->len;

    /* skip any leading zeros. */
    while (cntSrc && !(*pSrc)) {
        pSrc++;
        cntSrc--;
    }
    if (!cntSrc) {
        *pDst = 0;
        dest->len = 1;
        return;
    }

    if (*pSrc & 0x80)
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
    unsigned int cntSrc = src->len;
    unsigned int cntDst = dest->len;
    int zCount = cntDst - cntSrc;

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
    PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
    return SECFailure;
}

/* src is a "raw" ECDSA or DSA signature, the first half contains r
 * and the second half contains s. dest is the DER encoded signature.
*/
static SECStatus
common_EncodeDerSig(SECItem *dest, SECItem *src)
{
    SECItem *item;
    SECItem srcItem;
    DSA_ASN1Signature sig;
    unsigned char *signedR;
    unsigned char *signedS;
    unsigned int len;

    /* Allocate memory with room for an extra byte that
     * may be required if the top bit in the first byte
     * is already set.
     */
    len = src->len / 2;
    signedR = (unsigned char *)PORT_Alloc(len + 1);
    if (!signedR)
        return SECFailure;
    signedS = (unsigned char *)PORT_ZAlloc(len + 1);
    if (!signedS) {
        if (signedR)
            PORT_Free(signedR);
        return SECFailure;
    }

    PORT_Memset(&sig, 0, sizeof(sig));

    /* Must convert r and s from "unsigned" integers to "signed" integers.
    ** If the high order bit of the first byte (MSB) is 1, then must
    ** prepend with leading zero.
    ** Must remove all but one leading zero byte from numbers.
    */
    sig.r.type = siUnsignedInteger;
    sig.r.data = signedR;
    sig.r.len = sizeof signedR;
    sig.s.type = siUnsignedInteger;
    sig.s.data = signedS;
    sig.s.len = sizeof signedR;

    srcItem.data = src->data;
    srcItem.len = len;

    DSAU_ConvertUnsignedToSigned(&sig.r, &srcItem);
    srcItem.data += len;
    DSAU_ConvertUnsignedToSigned(&sig.s, &srcItem);

    item = SEC_ASN1EncodeItem(NULL, dest, &sig, DSA_SignatureTemplate);
    if (signedR)
        PORT_Free(signedR);
    if (signedS)
        PORT_Free(signedS);
    if (item == NULL)
        return SECFailure;

    /* XXX leak item? */
    return SECSuccess;
}

/* src is a DER-encoded ECDSA or DSA signature.
** Returns a newly-allocated SECItem structure, pointing at a newly allocated
** buffer containing the "raw" signature, which is len bytes of r,
** followed by len bytes of s. For DSA, len is the length of q.
** For ECDSA, len depends on the key size used to create the signature.
*/
static SECItem *
common_DecodeDerSig(const SECItem *item, unsigned int len)
{
    SECItem *result = NULL;
    SECStatus status;
    DSA_ASN1Signature sig;
    SECItem dst;

    PORT_Memset(&sig, 0, sizeof(sig));

    result = PORT_ZNew(SECItem);
    if (result == NULL)
        goto loser;

    result->len = 2 * len;
    result->data = (unsigned char *)PORT_Alloc(2 * len);
    if (result->data == NULL)
        goto loser;

    sig.r.type = siUnsignedInteger;
    sig.s.type = siUnsignedInteger;
    status = SEC_ASN1DecodeItem(NULL, &sig, DSA_SignatureTemplate, item);
    if (status != SECSuccess)
        goto loser;

    /* Convert sig.r and sig.s from variable  length signed integers to
    ** fixed length unsigned integers.
    */
    dst.data = result->data;
    dst.len = len;
    status = DSAU_ConvertSignedToFixedUnsigned(&dst, &sig.r);
    if (status != SECSuccess)
        goto loser;

    dst.data += len;
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

/* src is a "raw" DSA1 signature, 20 bytes of r followed by 20 bytes of s.
** dest is the signature DER encoded. ?
*/
SECStatus
DSAU_EncodeDerSig(SECItem *dest, SECItem *src)
{
    PORT_Assert(src->len == 2 * DSA1_SUBPRIME_LEN);
    if (src->len != 2 * DSA1_SUBPRIME_LEN) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    return common_EncodeDerSig(dest, src);
}

/* src is a "raw" DSA signature of length len (len/2 bytes of r followed
** by len/2 bytes of s). dest is the signature DER encoded.
*/
SECStatus
DSAU_EncodeDerSigWithLen(SECItem *dest, SECItem *src, unsigned int len)
{

    PORT_Assert((src->len == len) && (len % 2 == 0));
    if ((src->len != len) || (src->len % 2 != 0)) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    return common_EncodeDerSig(dest, src);
}

/* src is a DER-encoded DSA signature.
** Returns a newly-allocated SECItem structure, pointing at a newly allocated
** buffer containing the "raw" DSA1 signature, which is 20 bytes of r,
** followed by 20 bytes of s.
*/
SECItem *
DSAU_DecodeDerSig(const SECItem *item)
{
    return common_DecodeDerSig(item, DSA1_SUBPRIME_LEN);
}

/* src is a DER-encoded ECDSA signature.
** Returns a newly-allocated SECItem structure, pointing at a newly allocated
** buffer containing the "raw" ECDSA signature of length len containing
** r followed by s (both padded to take up exactly len/2 bytes).
*/
SECItem *
DSAU_DecodeDerSigToLen(const SECItem *item, unsigned int len)
{
    return common_DecodeDerSig(item, len / 2);
}
