/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Support routines for PKCS7 implementation, none of which are exported.
 * This file should only contain things that are needed by both the
 * encoding/creation side *and* the decoding/decryption side.  Anything
 * else should be static routines in the appropriate file.
 */

#include "p7local.h"

#include "cryptohi.h" 
#include "secasn1.h"
#include "secoid.h"
#include "secitem.h"
#include "pk11func.h"
#include "secpkcs5.h"
#include "secerr.h"

/*
 * -------------------------------------------------------------------
 * Cipher stuff.
 */

typedef SECStatus (*sec_pkcs7_cipher_function) (void *,
						unsigned char *,
						unsigned *,
						unsigned int,
						const unsigned char *,
						unsigned int);
typedef SECStatus (*sec_pkcs7_cipher_destroy) (void *, PRBool);

#define BLOCK_SIZE 4096

struct sec_pkcs7_cipher_object {
    void *cx;
    sec_pkcs7_cipher_function doit;
    sec_pkcs7_cipher_destroy destroy;
    PRBool encrypt;
    int block_size;
    int pad_size;
    int pending_count;
    unsigned char pending_buf[BLOCK_SIZE];
};

SEC_ASN1_MKSUB(CERT_IssuerAndSNTemplate)
SEC_ASN1_MKSUB(CERT_SetOfSignedCrlTemplate)
SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(SEC_OctetStringTemplate)
SEC_ASN1_MKSUB(SEC_SetOfAnyTemplate)

/*
 * Create a cipher object to do decryption,  based on the given bulk
 * encryption key and algorithm identifier (which may include an iv).
 *
 * XXX This interface, or one similar, would be really nice available
 * in general...  I tried to keep the pkcs7-specific stuff (mostly
 * having to do with padding) out of here.
 *
 * XXX Once both are working, it might be nice to combine this and the
 * function below (for starting up encryption) into one routine, and just
 * have two simple cover functions which call it. 
 */
sec_PKCS7CipherObject *
sec_PKCS7CreateDecryptObject (PK11SymKey *key, SECAlgorithmID *algid)
{
    sec_PKCS7CipherObject *result;
    SECOidTag algtag;
    void *ciphercx;
    CK_MECHANISM_TYPE cryptoMechType;
    PK11SlotInfo *slot;
    SECItem *param = NULL;

    result = (struct sec_pkcs7_cipher_object*)
      PORT_ZAlloc (sizeof(struct sec_pkcs7_cipher_object));
    if (result == NULL)
	return NULL;

    ciphercx = NULL;
    algtag = SECOID_GetAlgorithmTag (algid);

    if (SEC_PKCS5IsAlgorithmPBEAlg(algid)) {
	SECItem *pwitem;

	pwitem = (SECItem *)PK11_GetSymKeyUserData(key);
	if (!pwitem) {
	    PORT_Free(result);
	    return NULL;
	}

	cryptoMechType = PK11_GetPBECryptoMechanism(algid, &param, pwitem);
	if (cryptoMechType == CKM_INVALID_MECHANISM) {
	    PORT_Free(result);
	    SECITEM_FreeItem(param,PR_TRUE);
	    return NULL;
	}
    } else {
	cryptoMechType = PK11_AlgtagToMechanism(algtag);
	param = PK11_ParamFromAlgid(algid);
	if (param == NULL) {
	    PORT_Free(result);
	    return NULL;
	}
    }

    result->pad_size = PK11_GetBlockSize(cryptoMechType, param);
    slot = PK11_GetSlotFromKey(key);
    result->block_size = PK11_IsHW(slot) ? BLOCK_SIZE : result->pad_size;
    PK11_FreeSlot(slot);
    ciphercx = PK11_CreateContextBySymKey(cryptoMechType, CKA_DECRYPT, 
					  key, param);
    SECITEM_FreeItem(param,PR_TRUE);
    if (ciphercx == NULL) {
	PORT_Free (result);
	return NULL;
    }

    result->cx = ciphercx;
    result->doit =  (sec_pkcs7_cipher_function) PK11_CipherOp;
    result->destroy = (sec_pkcs7_cipher_destroy) PK11_DestroyContext;
    result->encrypt = PR_FALSE;
    result->pending_count = 0;

    return result;
}

/*
 * Create a cipher object to do encryption, based on the given bulk
 * encryption key and algorithm tag.  Fill in the algorithm identifier
 * (which may include an iv) appropriately.
 *
 * XXX This interface, or one similar, would be really nice available
 * in general...  I tried to keep the pkcs7-specific stuff (mostly
 * having to do with padding) out of here.
 *
 * XXX Once both are working, it might be nice to combine this and the
 * function above (for starting up decryption) into one routine, and just
 * have two simple cover functions which call it. 
 */
sec_PKCS7CipherObject *
sec_PKCS7CreateEncryptObject (PLArenaPool *poolp, PK11SymKey *key,
			      SECOidTag algtag, SECAlgorithmID *algid)
{
    sec_PKCS7CipherObject *result;
    void *ciphercx;
    SECStatus rv;
    CK_MECHANISM_TYPE cryptoMechType;
    PK11SlotInfo *slot;
    SECItem *param = NULL;
    PRBool needToEncodeAlgid = PR_FALSE;

    result = (struct sec_pkcs7_cipher_object*)
	      PORT_ZAlloc (sizeof(struct sec_pkcs7_cipher_object));
    if (result == NULL)
	return NULL;

    ciphercx = NULL;
    if (SEC_PKCS5IsAlgorithmPBEAlg(algid)) {
	SECItem *pwitem;

	pwitem = (SECItem *)PK11_GetSymKeyUserData(key);
	if (!pwitem) {
	    PORT_Free(result);
	    return NULL;
	}

	cryptoMechType = PK11_GetPBECryptoMechanism(algid, &param, pwitem);
	if (cryptoMechType == CKM_INVALID_MECHANISM) {
	    PORT_Free(result);
	    SECITEM_FreeItem(param,PR_TRUE);
	    return NULL;
	}
    } else {
	cryptoMechType = PK11_AlgtagToMechanism(algtag);
	param = PK11_GenerateNewParam(cryptoMechType, key);
	if (param == NULL) {
	    PORT_Free(result);
	    return NULL;
	}
	needToEncodeAlgid = PR_TRUE;
    }

    result->pad_size = PK11_GetBlockSize(cryptoMechType,param);
    slot = PK11_GetSlotFromKey(key);
    result->block_size = PK11_IsHW(slot) ? BLOCK_SIZE : result->pad_size;
    PK11_FreeSlot(slot);
    ciphercx = PK11_CreateContextBySymKey(cryptoMechType, CKA_ENCRYPT, 
    					  key, param);
    if (ciphercx == NULL) {
	PORT_Free (result);
        SECITEM_FreeItem(param,PR_TRUE);
	return NULL;
    }

    /*
     * These are placed after the CreateContextBySymKey() because some
     * mechanisms have to generate their IVs from their card (i.e. FORTEZZA).
     * Don't move it from here.
     */
    if (needToEncodeAlgid) {
	rv = PK11_ParamToAlgid(algtag,param,poolp,algid);
	if(rv != SECSuccess) {
	    PORT_Free (result);
	    SECITEM_FreeItem(param,PR_TRUE);
	    PK11_DestroyContext(ciphercx, PR_TRUE);
	    return NULL;
	}
    }
    SECITEM_FreeItem(param,PR_TRUE);

    result->cx = ciphercx;
    result->doit = (sec_pkcs7_cipher_function) PK11_CipherOp;
    result->destroy = (sec_pkcs7_cipher_destroy) PK11_DestroyContext;
    result->encrypt = PR_TRUE;
    result->pending_count = 0;

    return result;
}


/*
 * Destroy the cipher object.
 */
static void
sec_pkcs7_destroy_cipher (sec_PKCS7CipherObject *obj)
{
    (* obj->destroy) (obj->cx, PR_TRUE);
    PORT_Free (obj);
}

void
sec_PKCS7DestroyDecryptObject (sec_PKCS7CipherObject *obj)
{
    PORT_Assert (obj != NULL);
    if (obj == NULL)
	return;
    PORT_Assert (! obj->encrypt);
    sec_pkcs7_destroy_cipher (obj);
}

void
sec_PKCS7DestroyEncryptObject (sec_PKCS7CipherObject *obj)
{
    PORT_Assert (obj != NULL);
    if (obj == NULL)
	return;
    PORT_Assert (obj->encrypt);
    sec_pkcs7_destroy_cipher (obj);
}


/*
 * XXX I think all of the following lengths should be longs instead
 * of ints, but our current crypto interface uses ints, so I did too.
 */


/*
 * What will be the output length of the next call to decrypt?
 * Result can be used to perform memory allocations.  Note that the amount
 * is exactly accurate only when not doing a block cipher or when final
 * is false, otherwise it is an upper bound on the amount because until
 * we see the data we do not know how many padding bytes there are
 * (always between 1 and bsize).
 *
 * Note that this can return zero, which does not mean that the decrypt
 * operation can be skipped!  (It simply means that there are not enough
 * bytes to make up an entire block; the bytes will be reserved until
 * there are enough to encrypt/decrypt at least one block.)  However,
 * if zero is returned it *does* mean that no output buffer need be
 * passed in to the subsequent decrypt operation, as no output bytes
 * will be stored.
 */
unsigned int
sec_PKCS7DecryptLength (sec_PKCS7CipherObject *obj, unsigned int input_len,
			PRBool final)
{
    int blocks, block_size;

    PORT_Assert (! obj->encrypt);

    block_size = obj->block_size;

    /*
     * If this is not a block cipher, then we always have the same
     * number of output bytes as we had input bytes.
     */
    if (block_size == 0)
	return input_len;

    /*
     * On the final call, we will always use up all of the pending
     * bytes plus all of the input bytes, *but*, there will be padding
     * at the end and we cannot predict how many bytes of padding we
     * will end up removing.  The amount given here is actually known
     * to be at least 1 byte too long (because we know we will have
     * at least 1 byte of padding), but seemed clearer/better to me.
     */
    if (final)
	return obj->pending_count + input_len;

    /*
     * Okay, this amount is exactly what we will output on the
     * next cipher operation.  We will always hang onto the last
     * 1 - block_size bytes for non-final operations.  That is,
     * we will do as many complete blocks as we can *except* the
     * last block (complete or partial).  (This is because until
     * we know we are at the end, we cannot know when to interpret
     * and removing the padding byte(s), which are guaranteed to
     * be there.)
     */
    blocks = (obj->pending_count + input_len - 1) / block_size;
    return blocks * block_size;
}

/*
 * What will be the output length of the next call to encrypt?
 * Result can be used to perform memory allocations.
 *
 * Note that this can return zero, which does not mean that the encrypt
 * operation can be skipped!  (It simply means that there are not enough
 * bytes to make up an entire block; the bytes will be reserved until
 * there are enough to encrypt/decrypt at least one block.)  However,
 * if zero is returned it *does* mean that no output buffer need be
 * passed in to the subsequent encrypt operation, as no output bytes
 * will be stored.
 */
unsigned int
sec_PKCS7EncryptLength (sec_PKCS7CipherObject *obj, unsigned int input_len,
			PRBool final)
{
    int blocks, block_size;
    int pad_size;

    PORT_Assert (obj->encrypt);

    block_size = obj->block_size;
    pad_size = obj->pad_size;

    /*
     * If this is not a block cipher, then we always have the same
     * number of output bytes as we had input bytes.
     */
    if (block_size == 0)
	return input_len;

    /*
     * On the final call, we only send out what we need for
     * remaining bytes plus the padding.  (There is always padding,
     * so even if we have an exact number of blocks as input, we
     * will add another full block that is just padding.)
     */
    if (final) {
	if (pad_size == 0) {
    	    return obj->pending_count + input_len;
	} else {
    	    blocks = (obj->pending_count + input_len) / pad_size;
	    blocks++;
	    return blocks*pad_size;
	}
    }

    /*
     * Now, count the number of complete blocks of data we have.
     */
    blocks = (obj->pending_count + input_len) / block_size;


    return blocks * block_size;
}


/*
 * Decrypt a given length of input buffer (starting at "input" and
 * containing "input_len" bytes), placing the decrypted bytes in
 * "output" and storing the output length in "*output_len_p".
 * "obj" is the return value from sec_PKCS7CreateDecryptObject.
 * When "final" is true, this is the last of the data to be decrypted.
 *
 * This is much more complicated than it sounds when the cipher is
 * a block-type, meaning that the decryption function will only
 * operate on whole blocks.  But our caller is operating stream-wise,
 * and can pass in any number of bytes.  So we need to keep track
 * of block boundaries.  We save excess bytes between calls in "obj".
 * We also need to determine which bytes are padding, and remove
 * them from the output.  We can only do this step when we know we
 * have the final block of data.  PKCS #7 specifies that the padding
 * used for a block cipher is a string of bytes, each of whose value is
 * the same as the length of the padding, and that all data is padded.
 * (Even data that starts out with an exact multiple of blocks gets
 * added to it another block, all of which is padding.)
 */ 
SECStatus
sec_PKCS7Decrypt (sec_PKCS7CipherObject *obj, unsigned char *output,
		  unsigned int *output_len_p, unsigned int max_output_len,
		  const unsigned char *input, unsigned int input_len,
		  PRBool final)
{
    unsigned int blocks, bsize, pcount, padsize;
    unsigned int max_needed, ifraglen, ofraglen, output_len;
    unsigned char *pbuf;
    SECStatus rv;

    PORT_Assert (! obj->encrypt);

    /*
     * Check that we have enough room for the output.  Our caller should
     * already handle this; failure is really an internal error (i.e. bug).
     */
    max_needed = sec_PKCS7DecryptLength (obj, input_len, final);
    PORT_Assert (max_output_len >= max_needed);
    if (max_output_len < max_needed) {
	/* PORT_SetError (XXX); */
	return SECFailure;
    }

    /*
     * hardware encryption does not like small decryption sizes here, so we
     * allow both blocking and padding.
     */
    bsize = obj->block_size;
    padsize = obj->pad_size;

    /*
     * When no blocking or padding work to do, we can simply call the
     * cipher function and we are done.
     */
    if (bsize == 0) {
	return (* obj->doit) (obj->cx, output, output_len_p, max_output_len,
			      input, input_len);
    }

    pcount = obj->pending_count;
    pbuf = obj->pending_buf;

    output_len = 0;

    if (pcount) {
	/*
	 * Try to fill in an entire block, starting with the bytes
	 * we already have saved away.
	 */
	while (input_len && pcount < bsize) {
	    pbuf[pcount++] = *input++;
	    input_len--;
	}
	/*
	 * If we have at most a whole block and this is not our last call,
	 * then we are done for now.  (We do not try to decrypt a lone
	 * single block because we cannot interpret the padding bytes
	 * until we know we are handling the very last block of all input.)
	 */
	if (input_len == 0 && !final) {
	    obj->pending_count = pcount;
	    if (output_len_p)
		*output_len_p = 0;
	    return SECSuccess;
	}
	/*
	 * Given the logic above, we expect to have a full block by now.
	 * If we do not, there is something wrong, either with our own
	 * logic or with (length of) the data given to us.
	 */
	PORT_Assert ((padsize == 0) || (pcount % padsize) == 0);
	if ((padsize != 0) && (pcount % padsize) != 0) {
	    PORT_Assert (final);	
	    PORT_SetError (SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}
	/*
	 * Decrypt the block.
	 */
	rv = (* obj->doit) (obj->cx, output, &ofraglen, max_output_len,
			    pbuf, pcount);
	if (rv != SECSuccess)
	    return rv;

	/*
	 * For now anyway, all of our ciphers have the same number of
	 * bytes of output as they do input.  If this ever becomes untrue,
	 * then sec_PKCS7DecryptLength needs to be made smarter!
	 */
	PORT_Assert (ofraglen == pcount);

	/*
	 * Account for the bytes now in output.
	 */
	max_output_len -= ofraglen;
	output_len += ofraglen;
	output += ofraglen;
    }

    /*
     * If this is our last call, we expect to have an exact number of
     * blocks left to be decrypted; we will decrypt them all.
     * 
     * If not our last call, we always save between 1 and bsize bytes
     * until next time.  (We must do this because we cannot be sure
     * that none of the decrypted bytes are padding bytes until we
     * have at least another whole block of data.  You cannot tell by
     * looking -- the data could be anything -- you can only tell by
     * context, knowing you are looking at the last block.)  We could
     * decrypt a whole block now but it is easier if we just treat it
     * the same way we treat partial block bytes.
     */
    if (final) {
	if (padsize) {
	    blocks = input_len / padsize;
	    ifraglen = blocks * padsize;
	} else ifraglen = input_len;
	PORT_Assert (ifraglen == input_len);

	if (ifraglen != input_len) {
	    PORT_SetError (SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}
    } else {
	blocks = (input_len - 1) / bsize;
	ifraglen = blocks * bsize;
	PORT_Assert (ifraglen < input_len);

	pcount = input_len - ifraglen;
	PORT_Memcpy (pbuf, input + ifraglen, pcount);
	obj->pending_count = pcount;
    }

    if (ifraglen) {
	rv = (* obj->doit) (obj->cx, output, &ofraglen, max_output_len,
			    input, ifraglen);
	if (rv != SECSuccess)
	    return rv;

	/*
	 * For now anyway, all of our ciphers have the same number of
	 * bytes of output as they do input.  If this ever becomes untrue,
	 * then sec_PKCS7DecryptLength needs to be made smarter!
	 */
	PORT_Assert (ifraglen == ofraglen);
	if (ifraglen != ofraglen) {
	    PORT_SetError (SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}

	output_len += ofraglen;
    } else {
	ofraglen = 0;
    }

    /*
     * If we just did our very last block, "remove" the padding by
     * adjusting the output length.
     */
    if (final && (padsize != 0)) {
	unsigned int padlen = *(output + ofraglen - 1);
	if (padlen == 0 || padlen > padsize) {
	    PORT_SetError (SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}
	output_len -= padlen;
    }

    PORT_Assert (output_len_p != NULL || output_len == 0);
    if (output_len_p != NULL)
	*output_len_p = output_len;

    return SECSuccess;
}

/*
 * Encrypt a given length of input buffer (starting at "input" and
 * containing "input_len" bytes), placing the encrypted bytes in
 * "output" and storing the output length in "*output_len_p".
 * "obj" is the return value from sec_PKCS7CreateEncryptObject.
 * When "final" is true, this is the last of the data to be encrypted.
 *
 * This is much more complicated than it sounds when the cipher is
 * a block-type, meaning that the encryption function will only
 * operate on whole blocks.  But our caller is operating stream-wise,
 * and can pass in any number of bytes.  So we need to keep track
 * of block boundaries.  We save excess bytes between calls in "obj".
 * We also need to add padding bytes at the end.  PKCS #7 specifies
 * that the padding used for a block cipher is a string of bytes,
 * each of whose value is the same as the length of the padding,
 * and that all data is padded.  (Even data that starts out with
 * an exact multiple of blocks gets added to it another block,
 * all of which is padding.)
 *
 * XXX I would kind of like to combine this with the function above
 * which does decryption, since they have a lot in common.  But the
 * tricky parts about padding and filling blocks would be much
 * harder to read that way, so I left them separate.  At least for
 * now until it is clear that they are right.
 */ 
SECStatus
sec_PKCS7Encrypt (sec_PKCS7CipherObject *obj, unsigned char *output,
		  unsigned int *output_len_p, unsigned int max_output_len,
		  const unsigned char *input, unsigned int input_len,
		  PRBool final)
{
    int blocks, bsize, padlen, pcount, padsize;
    unsigned int max_needed, ifraglen, ofraglen, output_len;
    unsigned char *pbuf;
    SECStatus rv;

    PORT_Assert (obj->encrypt);

    /*
     * Check that we have enough room for the output.  Our caller should
     * already handle this; failure is really an internal error (i.e. bug).
     */
    max_needed = sec_PKCS7EncryptLength (obj, input_len, final);
    PORT_Assert (max_output_len >= max_needed);
    if (max_output_len < max_needed) {
	/* PORT_SetError (XXX); */
	return SECFailure;
    }

    bsize = obj->block_size;
    padsize = obj->pad_size;

    /*
     * When no blocking and padding work to do, we can simply call the
     * cipher function and we are done.
     */
    if (bsize == 0) {
	return (* obj->doit) (obj->cx, output, output_len_p, max_output_len,
			      input, input_len);
    }

    pcount = obj->pending_count;
    pbuf = obj->pending_buf;

    output_len = 0;

    if (pcount) {
	/*
	 * Try to fill in an entire block, starting with the bytes
	 * we already have saved away.
	 */
	while (input_len && pcount < bsize) {
	    pbuf[pcount++] = *input++;
	    input_len--;
	}
	/*
	 * If we do not have a full block and we know we will be
	 * called again, then we are done for now.
	 */
	if (pcount < bsize && !final) {
	    obj->pending_count = pcount;
	    if (output_len_p != NULL)
		*output_len_p = 0;
	    return SECSuccess;
	}
	/*
	 * If we have a whole block available, encrypt it.
	 */
	if ((padsize == 0) || (pcount % padsize) == 0) {
	    rv = (* obj->doit) (obj->cx, output, &ofraglen, max_output_len,
				pbuf, pcount);
	    if (rv != SECSuccess)
		return rv;

	    /*
	     * For now anyway, all of our ciphers have the same number of
	     * bytes of output as they do input.  If this ever becomes untrue,
	     * then sec_PKCS7EncryptLength needs to be made smarter!
	     */
	    PORT_Assert (ofraglen == pcount);

	    /*
	     * Account for the bytes now in output.
	     */
	    max_output_len -= ofraglen;
	    output_len += ofraglen;
	    output += ofraglen;

	    pcount = 0;
	}
    }

    if (input_len) {
	PORT_Assert (pcount == 0);

	blocks = input_len / bsize;
	ifraglen = blocks * bsize;

	if (ifraglen) {
	    rv = (* obj->doit) (obj->cx, output, &ofraglen, max_output_len,
				input, ifraglen);
	    if (rv != SECSuccess)
		return rv;

	    /*
	     * For now anyway, all of our ciphers have the same number of
	     * bytes of output as they do input.  If this ever becomes untrue,
	     * then sec_PKCS7EncryptLength needs to be made smarter!
	     */
	    PORT_Assert (ifraglen == ofraglen);

	    max_output_len -= ofraglen;
	    output_len += ofraglen;
	    output += ofraglen;
	}

	pcount = input_len - ifraglen;
	PORT_Assert (pcount < bsize);
	if (pcount)
	    PORT_Memcpy (pbuf, input + ifraglen, pcount);
    }

    if (final) {
        if (padsize) {
	    padlen = padsize - (pcount % padsize);
	    PORT_Memset (pbuf + pcount, padlen, padlen);
        } else {
	    padlen = 0;
        }
	rv = (* obj->doit) (obj->cx, output, &ofraglen, max_output_len,
			    pbuf, pcount+padlen);
	if (rv != SECSuccess)
	    return rv;

	/*
	 * For now anyway, all of our ciphers have the same number of
	 * bytes of output as they do input.  If this ever becomes untrue,
	 * then sec_PKCS7EncryptLength needs to be made smarter!
	 */
	PORT_Assert (ofraglen == (pcount+padlen));
	output_len += ofraglen;
    } else {
	obj->pending_count = pcount;
    }

    PORT_Assert (output_len_p != NULL || output_len == 0);
    if (output_len_p != NULL)
	*output_len_p = output_len;

    return SECSuccess;
}

/*
 * End of cipher stuff.
 * -------------------------------------------------------------------
 */


/*
 * -------------------------------------------------------------------
 * XXX The following Attribute stuff really belongs elsewhere.
 * The Attribute type is *not* part of pkcs7 but rather X.501.
 * But for now, since PKCS7 is the only customer of attributes,
 * we define them here.  Once there is a use outside of PKCS7,
 * then change the attribute types and functions from internal
 * to external naming convention, and move them elsewhere!
 */

/*
 * Look through a set of attributes and find one that matches the
 * specified object ID.  If "only" is true, then make sure that
 * there is not more than one attribute of the same type.  Otherwise,
 * just return the first one found. (XXX Does anybody really want
 * that first-found behavior?  It was like that when I found it...)
 */
SEC_PKCS7Attribute *
sec_PKCS7FindAttribute (SEC_PKCS7Attribute **attrs, SECOidTag oidtag,
			PRBool only)
{
    SECOidData *oid;
    SEC_PKCS7Attribute *attr1, *attr2;

    if (attrs == NULL)
	return NULL;

    oid = SECOID_FindOIDByTag(oidtag);
    if (oid == NULL)
	return NULL;

    while ((attr1 = *attrs++) != NULL) {
	if (attr1->type.len == oid->oid.len && PORT_Memcmp (attr1->type.data,
							    oid->oid.data,
							    oid->oid.len) == 0)
	    break;
    }

    if (attr1 == NULL)
	return NULL;

    if (!only)
	return attr1;

    while ((attr2 = *attrs++) != NULL) {
	if (attr2->type.len == oid->oid.len && PORT_Memcmp (attr2->type.data,
							    oid->oid.data,
							    oid->oid.len) == 0)
	    break;
    }

    if (attr2 != NULL)
	return NULL;

    return attr1;
}


/*
 * Return the single attribute value, doing some sanity checking first:
 * - Multiple values are *not* expected.
 * - Empty values are *not* expected.
 */
SECItem *
sec_PKCS7AttributeValue(SEC_PKCS7Attribute *attr)
{
    SECItem *value;

    if (attr == NULL)
	return NULL;

    value = attr->values[0];

    if (value == NULL || value->data == NULL || value->len == 0)
	return NULL;

    if (attr->values[1] != NULL)
	return NULL;

    return value;
}

static const SEC_ASN1Template *
sec_attr_choose_attr_value_template(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;

    SEC_PKCS7Attribute *attribute;
    SECOidData *oiddata;
    PRBool encoded;

    PORT_Assert (src_or_dest != NULL);
    if (src_or_dest == NULL)
	return NULL;

    attribute = (SEC_PKCS7Attribute*)src_or_dest;

    if (encoding && attribute->encoded)
	return SEC_ASN1_GET(SEC_AnyTemplate);

    oiddata = attribute->typeTag;
    if (oiddata == NULL) {
	oiddata = SECOID_FindOID(&attribute->type);
	attribute->typeTag = oiddata;
    }

    if (oiddata == NULL) {
	encoded = PR_TRUE;
	theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
    } else {
	switch (oiddata->offset) {
	  default:
	    encoded = PR_TRUE;
	    theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
	    break;
	  case SEC_OID_PKCS9_EMAIL_ADDRESS:
	  case SEC_OID_RFC1274_MAIL:
	  case SEC_OID_PKCS9_UNSTRUCTURED_NAME:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_IA5StringTemplate);
	    break;
	  case SEC_OID_PKCS9_CONTENT_TYPE:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_ObjectIDTemplate);
	    break;
	  case SEC_OID_PKCS9_MESSAGE_DIGEST:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_OctetStringTemplate);
	    break;
	  case SEC_OID_PKCS9_SIGNING_TIME:
	    encoded = PR_FALSE;
            theTemplate = SEC_ASN1_GET(CERT_TimeChoiceTemplate);
	    break;
	  /* XXX Want other types here, too */
	}
    }

    if (encoding) {
	/*
	 * If we are encoding and we think we have an already-encoded value,
	 * then the code which initialized this attribute should have set
	 * the "encoded" property to true (and we would have returned early,
	 * up above).  No devastating error, but that code should be fixed.
	 * (It could indicate that the resulting encoded bytes are wrong.)
	 */
	PORT_Assert (!encoded);
    } else {
	/*
	 * We are decoding; record whether the resulting value is
	 * still encoded or not.
	 */
	attribute->encoded = encoded;
    }
    return theTemplate;
}

static const SEC_ASN1TemplateChooserPtr sec_attr_chooser
	= sec_attr_choose_attr_value_template;

static const SEC_ASN1Template sec_pkcs7_attribute_template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SEC_PKCS7Attribute) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SEC_PKCS7Attribute,type) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_SET_OF,
	  offsetof(SEC_PKCS7Attribute,values),
	  &sec_attr_chooser },
    { 0 }
};

static const SEC_ASN1Template sec_pkcs7_set_of_attribute_template[] = {
    { SEC_ASN1_SET_OF, 0, sec_pkcs7_attribute_template },
};

/*
 * If you are wondering why this routine does not reorder the attributes
 * first, and might be tempted to make it do so, see the comment by the
 * call to ReorderAttributes in p7encode.c.  (Or, see who else calls this
 * and think long and hard about the implications of making it always
 * do the reordering.)
 */
SECItem *
sec_PKCS7EncodeAttributes (PLArenaPool *poolp, SECItem *dest, void *src)
{
    return SEC_ASN1EncodeItem (poolp, dest, src,
			       sec_pkcs7_set_of_attribute_template);
}

/*
 * Make sure that the order of the attributes guarantees valid DER
 * (which must be in lexigraphically ascending order for a SET OF);
 * if reordering is necessary it will be done in place (in attrs).
 */
SECStatus
sec_PKCS7ReorderAttributes (SEC_PKCS7Attribute **attrs)
{
    PLArenaPool *poolp;
    int num_attrs, i, pass, besti;
    unsigned int j;
    SECItem **enc_attrs;
    SEC_PKCS7Attribute **new_attrs;

    /*
     * I think we should not be called with NULL.  But if we are,
     * call it a success anyway, because the order *is* okay.
     */
    PORT_Assert (attrs != NULL);
    if (attrs == NULL)
	return SECSuccess;

    /*
     * Count how many attributes we are dealing with here.
     */
    num_attrs = 0;
    while (attrs[num_attrs] != NULL)
	num_attrs++;

    /*
     * Again, I think we should have some attributes here.
     * But if we do not, or if there is only one, then call it
     * a success because it also already has a fine order.
     */
    PORT_Assert (num_attrs);
    if (num_attrs == 0 || num_attrs == 1)
	return SECSuccess;

    /*
     * Allocate an arena for us to work with, so it is easy to
     * clean up all of the memory (fairly small pieces, really).
     */
    poolp = PORT_NewArena (1024);	/* XXX what is right value? */
    if (poolp == NULL)
	return SECFailure;		/* no memory; nothing we can do... */

    /*
     * Allocate arrays to hold the individual encodings which we will use
     * for comparisons and the reordered attributes as they are sorted.
     */
    enc_attrs=(SECItem**)PORT_ArenaZAlloc(poolp, num_attrs*sizeof(SECItem *));
    new_attrs = (SEC_PKCS7Attribute**)PORT_ArenaZAlloc (poolp,
				  num_attrs * sizeof(SEC_PKCS7Attribute *));
    if (enc_attrs == NULL || new_attrs == NULL) {
	PORT_FreeArena (poolp, PR_FALSE);
	return SECFailure;
    }

    /*
     * DER encode each individual attribute.
     */
    for (i = 0; i < num_attrs; i++) {
	enc_attrs[i] = SEC_ASN1EncodeItem (poolp, NULL, attrs[i],
					   sec_pkcs7_attribute_template);
	if (enc_attrs[i] == NULL) {
	    PORT_FreeArena (poolp, PR_FALSE);
	    return SECFailure;
	}
    }

    /*
     * Now compare and sort them; this is not the most efficient sorting
     * method, but it is just fine for the problem at hand, because the
     * number of attributes is (always) going to be small.
     */
    for (pass = 0; pass < num_attrs; pass++) {
	/*
	 * Find the first not-yet-accepted attribute.  (Once one is
	 * sorted into the other array, it is cleared from enc_attrs.)
	 */
	for (i = 0; i < num_attrs; i++) {
	    if (enc_attrs[i] != NULL)
		break;
	}
	PORT_Assert (i < num_attrs);
	besti = i;

	/*
	 * Find the lowest (lexigraphically) encoding.  One that is
	 * shorter than all the rest is known to be "less" because each
	 * attribute is of the same type (a SEQUENCE) and so thus the
	 * first octet of each is the same, and the second octet is
	 * the length (or the length of the length with the high bit
	 * set, followed by the length, which also works out to always
	 * order the shorter first).  Two (or more) that have the
	 * same length need to be compared byte by byte until a mismatch
	 * is found.
	 */
	for (i = besti + 1; i < num_attrs; i++) {
	    if (enc_attrs[i] == NULL)	/* slot already handled */
		continue;

	    if (enc_attrs[i]->len != enc_attrs[besti]->len) {
		if (enc_attrs[i]->len < enc_attrs[besti]->len)
		    besti = i;
		continue;
	    }

	    for (j = 0; j < enc_attrs[i]->len; j++) {
		if (enc_attrs[i]->data[j] < enc_attrs[besti]->data[j]) {
		    besti = i;
		    break;
		}
	    }

	    /*
	     * For this not to be true, we would have to have encountered	
	     * two *identical* attributes, which I think we should not see.
	     * So assert if it happens, but even if it does, let it go
	     * through; the ordering of the two does not matter.
	     */
	    PORT_Assert (j < enc_attrs[i]->len);
	}

	/*
	 * Now we have found the next-lowest one; copy it over and
	 * remove it from enc_attrs.
	 */
	new_attrs[pass] = attrs[besti];
	enc_attrs[besti] = NULL;
    }

    /*
     * Now new_attrs has the attributes in the order we want;
     * copy them back into the attrs array we started with.
     */
    for (i = 0; i < num_attrs; i++)
	attrs[i] = new_attrs[i];

    PORT_FreeArena (poolp, PR_FALSE);
    return SECSuccess;
}

/*
 * End of attribute stuff.
 * -------------------------------------------------------------------
 */


/*
 * Templates and stuff.  Keep these at the end of the file.
 */

/* forward declaration */
static const SEC_ASN1Template *
sec_pkcs7_choose_content_template(void *src_or_dest, PRBool encoding);

static const SEC_ASN1TemplateChooserPtr sec_pkcs7_chooser
	= sec_pkcs7_choose_content_template;

const SEC_ASN1Template sec_PKCS7ContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7ContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SEC_PKCS7ContentInfo,contentType) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_DYNAMIC | SEC_ASN1_MAY_STREAM
     | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SEC_PKCS7ContentInfo,content),
	  &sec_pkcs7_chooser },
    { 0 }
};

/* XXX These names should change from external to internal convention. */

static const SEC_ASN1Template SEC_PKCS7SignerInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SEC_PKCS7SignerInfo) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7SignerInfo,version) },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignerInfo,issuerAndSN),
	  SEC_ASN1_SUB(CERT_IssuerAndSNTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignerInfo,digestAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SEC_PKCS7SignerInfo,authAttr),
	  sec_pkcs7_set_of_attribute_template },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignerInfo,digestEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SEC_PKCS7SignerInfo,encDigest) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(SEC_PKCS7SignerInfo,unAuthAttr),
	  sec_pkcs7_set_of_attribute_template },
    { 0 }
};

static const SEC_ASN1Template SEC_PKCS7SignedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7SignedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7SignedData,version) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignedData,digestAlgorithms),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE,
	  offsetof(SEC_PKCS7SignedData,contentInfo),
	  sec_PKCS7ContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC  |
      SEC_ASN1_XTRN | 0,
	  offsetof(SEC_PKCS7SignedData,rawCerts),
	  SEC_ASN1_SUB(SEC_SetOfAnyTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC  |
      SEC_ASN1_XTRN | 1,
	  offsetof(SEC_PKCS7SignedData,crls),
	  SEC_ASN1_SUB(CERT_SetOfSignedCrlTemplate) },
    { SEC_ASN1_SET_OF,
	  offsetof(SEC_PKCS7SignedData,signerInfos),
	  SEC_PKCS7SignerInfoTemplate },
    { 0 }
};

static const SEC_ASN1Template SEC_PointerToPKCS7SignedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS7SignedDataTemplate }
};

static const SEC_ASN1Template SEC_PKCS7RecipientInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SEC_PKCS7RecipientInfo) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7RecipientInfo,version) },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7RecipientInfo,issuerAndSN),
	  SEC_ASN1_SUB(CERT_IssuerAndSNTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7RecipientInfo,keyEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SEC_PKCS7RecipientInfo,encKey) },
    { 0 }
};

static const SEC_ASN1Template SEC_PKCS7EncryptedContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7EncryptedContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SEC_PKCS7EncryptedContentInfo,contentType) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7EncryptedContentInfo,contentEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_MAY_STREAM | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 0,
	  offsetof(SEC_PKCS7EncryptedContentInfo,encContent),
	  SEC_ASN1_SUB(SEC_OctetStringTemplate) },
    { 0 }
};

static const SEC_ASN1Template SEC_PKCS7EnvelopedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7EnvelopedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7EnvelopedData,version) },
    { SEC_ASN1_SET_OF,
	  offsetof(SEC_PKCS7EnvelopedData,recipientInfos),
	  SEC_PKCS7RecipientInfoTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(SEC_PKCS7EnvelopedData,encContentInfo),
	  SEC_PKCS7EncryptedContentInfoTemplate },
    { 0 }
};

static const SEC_ASN1Template SEC_PointerToPKCS7EnvelopedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS7EnvelopedDataTemplate }
};

static const SEC_ASN1Template SEC_PKCS7SignedAndEnvelopedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7SignedAndEnvelopedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7SignedAndEnvelopedData,version) },
    { SEC_ASN1_SET_OF,
	  offsetof(SEC_PKCS7SignedAndEnvelopedData,recipientInfos),
	  SEC_PKCS7RecipientInfoTemplate },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignedAndEnvelopedData,digestAlgorithms),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE,
	  offsetof(SEC_PKCS7SignedAndEnvelopedData,encContentInfo),
	  SEC_PKCS7EncryptedContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 0,
	  offsetof(SEC_PKCS7SignedAndEnvelopedData,rawCerts),
	  SEC_ASN1_SUB(SEC_SetOfAnyTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 1,
	  offsetof(SEC_PKCS7SignedAndEnvelopedData,crls),
	  SEC_ASN1_SUB(CERT_SetOfSignedCrlTemplate) },
    { SEC_ASN1_SET_OF,
	  offsetof(SEC_PKCS7SignedAndEnvelopedData,signerInfos),
	  SEC_PKCS7SignerInfoTemplate },
    { 0 }
};

static const SEC_ASN1Template
SEC_PointerToPKCS7SignedAndEnvelopedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS7SignedAndEnvelopedDataTemplate }
};

static const SEC_ASN1Template SEC_PKCS7DigestedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7DigestedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7DigestedData,version) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7DigestedData,digestAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE,
	  offsetof(SEC_PKCS7DigestedData,contentInfo),
	  sec_PKCS7ContentInfoTemplate },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SEC_PKCS7DigestedData,digest) },
    { 0 }
};

static const SEC_ASN1Template SEC_PointerToPKCS7DigestedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS7DigestedDataTemplate }
};

static const SEC_ASN1Template SEC_PKCS7EncryptedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7EncryptedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7EncryptedData,version) },
    { SEC_ASN1_INLINE,
	  offsetof(SEC_PKCS7EncryptedData,encContentInfo),
	  SEC_PKCS7EncryptedContentInfoTemplate },
    { 0 }
};

static const SEC_ASN1Template SEC_PointerToPKCS7EncryptedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS7EncryptedDataTemplate }
};

static const SEC_ASN1Template *
sec_pkcs7_choose_content_template(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    SEC_PKCS7ContentInfo *cinfo;
    SECOidTag kind;

    PORT_Assert (src_or_dest != NULL);
    if (src_or_dest == NULL)
	return NULL;

    cinfo = (SEC_PKCS7ContentInfo*)src_or_dest;
    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      default:
	theTemplate = SEC_ASN1_GET(SEC_PointerToAnyTemplate);
	break;
      case SEC_OID_PKCS7_DATA:
	theTemplate = SEC_ASN1_GET(SEC_PointerToOctetStringTemplate);
	break;
      case SEC_OID_PKCS7_SIGNED_DATA:
	theTemplate = SEC_PointerToPKCS7SignedDataTemplate;
	break;
      case SEC_OID_PKCS7_ENVELOPED_DATA:
	theTemplate = SEC_PointerToPKCS7EnvelopedDataTemplate;
	break;
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	theTemplate = SEC_PointerToPKCS7SignedAndEnvelopedDataTemplate;
	break;
      case SEC_OID_PKCS7_DIGESTED_DATA:
	theTemplate = SEC_PointerToPKCS7DigestedDataTemplate;
	break;
      case SEC_OID_PKCS7_ENCRYPTED_DATA:
	theTemplate = SEC_PointerToPKCS7EncryptedDataTemplate;
	break;
    }
    return theTemplate;
}

/*
 * End of templates.  Do not add stuff after this; put new code
 * up above the start of the template definitions.
 */
