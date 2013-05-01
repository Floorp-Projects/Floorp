/*
 * PKCS#1 encoding and decoding functions.
 * This file is believed to contain no code licensed from other parties.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

#include "blapi.h"
#include "softoken.h"

#include "lowkeyi.h"
#include "secerr.h"

#define RSA_BLOCK_MIN_PAD_LEN		8
#define RSA_BLOCK_FIRST_OCTET		0x00
#define RSA_BLOCK_PRIVATE0_PAD_OCTET	0x00
#define RSA_BLOCK_PRIVATE_PAD_OCTET	0xff
#define RSA_BLOCK_AFTER_PAD_OCTET	0x00

/* Needed for RSA-PSS functions */
static const unsigned char eightZeros[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Constant time comparison of a single byte.
 * Returns 1 iff a == b, otherwise returns 0.
 * Note: For ranges of bytes, use constantTimeCompare.
 */
static unsigned char constantTimeEQ8(unsigned char a, unsigned char b) {
    unsigned char c = ~(a - b | b - a);
    c >>= 7;
    return c;
}

/* Constant time comparison of a range of bytes.
 * Returns 1 iff len bytes of a are identical to len bytes of b, otherwise
 * returns 0.
 */
static unsigned char constantTimeCompare(const unsigned char *a,
                                         const unsigned char *b,
                                         unsigned int len) {
    unsigned char tmp = 0;
    unsigned int i;
    for (i = 0; i < len; ++i, ++a, ++b)
        tmp |= *a ^ *b;
    return constantTimeEQ8(0x00, tmp);
}

/* Constant time conditional.
 * Returns a if c is 1, or b if c is 0. The result is undefined if c is
 * not 0 or 1.
 */
static unsigned int constantTimeCondition(unsigned int c,
                                          unsigned int a,
                                          unsigned int b)
{
    return (~(c - 1) & a) | ((c - 1) & b);
}

/*
 * Format one block of data for public/private key encryption using
 * the rules defined in PKCS #1.
 */
static unsigned char *
rsa_FormatOneBlock(unsigned modulusLen, RSA_BlockType blockType,
		   SECItem *data)
{
    unsigned char *block;
    unsigned char *bp;
    int padLen;
    int i, j;
    SECStatus rv;

    block = (unsigned char *) PORT_Alloc(modulusLen);
    if (block == NULL)
	return NULL;

    bp = block;

    /*
     * All RSA blocks start with two octets:
     *	0x00 || BlockType
     */
    *bp++ = RSA_BLOCK_FIRST_OCTET;
    *bp++ = (unsigned char) blockType;

    switch (blockType) {

      /*
       * Blocks intended for private-key operation.
       */
      case RSA_BlockPrivate0: /* essentially unused */
      case RSA_BlockPrivate:	 /* preferred method */
	/*
	 * 0x00 || BT || Pad || 0x00 || ActualData
	 *   1      1   padLen    1      data->len
	 * Pad is either all 0x00 or all 0xff bytes, depending on blockType.
	 */
	padLen = modulusLen - data->len - 3;
	PORT_Assert (padLen >= RSA_BLOCK_MIN_PAD_LEN);
	if (padLen < RSA_BLOCK_MIN_PAD_LEN) {
	    PORT_Free (block);
	    return NULL;
	}
	PORT_Memset (bp,
		   blockType == RSA_BlockPrivate0
			? RSA_BLOCK_PRIVATE0_PAD_OCTET
			: RSA_BLOCK_PRIVATE_PAD_OCTET,
		   padLen);
	bp += padLen;
	*bp++ = RSA_BLOCK_AFTER_PAD_OCTET;
	PORT_Memcpy (bp, data->data, data->len);
	break;

      /*
       * Blocks intended for public-key operation.
       */
      case RSA_BlockPublic:

	/*
	 * 0x00 || BT || Pad || 0x00 || ActualData
	 *   1      1   padLen    1      data->len
	 * Pad is all non-zero random bytes.
	 *
	 * Build the block left to right.
	 * Fill the entire block from Pad to the end with random bytes.
	 * Use the bytes after Pad as a supply of extra random bytes from 
	 * which to find replacements for the zero bytes in Pad.
	 * If we need more than that, refill the bytes after Pad with 
	 * new random bytes as necessary.
	 */
	padLen = modulusLen - (data->len + 3);
	PORT_Assert (padLen >= RSA_BLOCK_MIN_PAD_LEN);
	if (padLen < RSA_BLOCK_MIN_PAD_LEN) {
	    PORT_Free (block);
	    return NULL;
	}
	j = modulusLen - 2;
	rv = RNG_GenerateGlobalRandomBytes(bp, j);
	if (rv == SECSuccess) {
	    for (i = 0; i < padLen; ) {
		unsigned char repl;
		/* Pad with non-zero random data. */
		if (bp[i] != RSA_BLOCK_AFTER_PAD_OCTET) {
		    ++i;
		    continue;
		}
		if (j <= padLen) {
		    rv = RNG_GenerateGlobalRandomBytes(bp + padLen,
					  modulusLen - (2 + padLen));
		    if (rv != SECSuccess)
		    	break;
		    j = modulusLen - 2;
		}
		do {
		    repl = bp[--j];
		} while (repl == RSA_BLOCK_AFTER_PAD_OCTET && j > padLen);
		if (repl != RSA_BLOCK_AFTER_PAD_OCTET) {
		    bp[i++] = repl;
		}
	    }
	}
	if (rv != SECSuccess) {
	    sftk_fatalError = PR_TRUE;
	    PORT_Free (block);
	    return NULL;
	}
	bp += padLen;
	*bp++ = RSA_BLOCK_AFTER_PAD_OCTET;
	PORT_Memcpy (bp, data->data, data->len);
	break;

      default:
	PORT_Assert (0);
	PORT_Free (block);
	return NULL;
    }

    return block;
}

static SECStatus
rsa_FormatBlock(SECItem *result, unsigned modulusLen,
		RSA_BlockType blockType, SECItem *data)
{
    /*
     * XXX For now assume that the data length fits in a single
     * XXX encryption block; the ASSERTs below force this.
     * XXX To fix it, each case will have to loop over chunks whose
     * XXX lengths satisfy the assertions, until all data is handled.
     * XXX (Unless RSA has more to say about how to handle data
     * XXX which does not fit in a single encryption block?)
     * XXX And I do not know what the result is supposed to be,
     * XXX so the interface to this function may need to change
     * XXX to allow for returning multiple blocks, if they are
     * XXX not wanted simply concatenated one after the other.
     */

    switch (blockType) {
      case RSA_BlockPrivate0:
      case RSA_BlockPrivate:
      case RSA_BlockPublic:
	/*
	 * 0x00 || BT || Pad || 0x00 || ActualData
	 *
	 * The "3" below is the first octet + the second octet + the 0x00
	 * octet that always comes just before the ActualData.
	 */
	PORT_Assert (data->len <= (modulusLen - (3 + RSA_BLOCK_MIN_PAD_LEN)));

	result->data = rsa_FormatOneBlock(modulusLen, blockType, data);
	if (result->data == NULL) {
	    result->len = 0;
	    return SECFailure;
	}
	result->len = modulusLen;

	break;

      case RSA_BlockRaw:
	/*
	 * Pad || ActualData
	 * Pad is zeros. The application is responsible for recovering
	 * the actual data.
	 */
	if (data->len > modulusLen ) {
	    return SECFailure;
	}
	result->data = (unsigned char*)PORT_ZAlloc(modulusLen);
	result->len = modulusLen;
	PORT_Memcpy(result->data+(modulusLen-data->len),data->data,data->len);
	break;

      default:
	PORT_Assert (0);
	result->data = NULL;
	result->len = 0;
	return SECFailure;
    }

    return SECSuccess;
}

/* XXX Doesn't set error code */
SECStatus
RSA_Sign(NSSLOWKEYPrivateKey *key, 
         unsigned char *      output, 
	 unsigned int *       output_len,
         unsigned int         maxOutputLen, 
	 unsigned char *      input, 
	 unsigned int         input_len)
{
    SECStatus     rv          = SECSuccess;
    unsigned int  modulus_len = nsslowkey_PrivateModulusLen(key);
    SECItem       formatted;
    SECItem       unformatted;

    if (maxOutputLen < modulus_len) 
    	return SECFailure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	return SECFailure;

    unformatted.len  = input_len;
    unformatted.data = input;
    formatted.data   = NULL;
    rv = rsa_FormatBlock(&formatted, modulus_len, RSA_BlockPrivate,
			 &unformatted);
    if (rv != SECSuccess) 
    	goto done;

    rv = RSA_PrivateKeyOpDoubleChecked(&key->u.rsa, output, formatted.data);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
	sftk_fatalError = PR_TRUE;
    }
    *output_len = modulus_len;

    goto done;

done:
    if (formatted.data != NULL) 
    	PORT_ZFree(formatted.data, modulus_len);
    return rv;
}

/* XXX Doesn't set error code */
SECStatus
RSA_CheckSign(NSSLOWKEYPublicKey *key,
              unsigned char *     sign, 
	      unsigned int        sign_len, 
	      unsigned char *     hash, 
	      unsigned int        hash_len)
{
    SECStatus       rv;
    unsigned int    modulus_len = nsslowkey_PublicModulusLen(key);
    unsigned int    i;
    unsigned char * buffer;

    modulus_len = nsslowkey_PublicModulusLen(key);
    if (sign_len != modulus_len) 
    	goto failure;
    /*
     * 0x00 || BT || Pad || 0x00 || ActualData
     *
     * The "3" below is the first octet + the second octet + the 0x00
     * octet that always comes just before the ActualData.
     */
    if (hash_len > modulus_len - (3 + RSA_BLOCK_MIN_PAD_LEN)) 
    	goto failure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	goto failure;

    buffer = (unsigned char *)PORT_Alloc(modulus_len + 1);
    if (!buffer)
    	goto failure;

    rv = RSA_PublicKeyOp(&key->u.rsa, buffer, sign);
    if (rv != SECSuccess)
	goto loser;

    /*
     * check the padding that was used
     */
    if (buffer[0] != 0 || buffer[1] != 1) 
    	goto loser;
    for (i = 2; i < modulus_len - hash_len - 1; i++) {
	if (buffer[i] != 0xff) 
	    goto loser;
    }
    if (buffer[i] != 0) 
	goto loser;

    /*
     * make sure we get the same results
     */
    if (PORT_Memcmp(buffer + modulus_len - hash_len, hash, hash_len) != 0)
	goto loser;

    PORT_Free(buffer);
    return SECSuccess;

loser:
    PORT_Free(buffer);
failure:
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_CheckSignRecover(NSSLOWKEYPublicKey *key,
                     unsigned char *     data,
                     unsigned int *      data_len, 
		     unsigned int        max_output_len, 
		     unsigned char *     sign,
		     unsigned int        sign_len)
{
    SECStatus       rv;
    unsigned int    modulus_len = nsslowkey_PublicModulusLen(key);
    unsigned int    i;
    unsigned char * buffer;

    if (sign_len != modulus_len) 
    	goto failure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	goto failure;

    buffer = (unsigned char *)PORT_Alloc(modulus_len + 1);
    if (!buffer)
    	goto failure;

    rv = RSA_PublicKeyOp(&key->u.rsa, buffer, sign);
    if (rv != SECSuccess)
    	goto loser;
    *data_len = 0;

    /*
     * check the padding that was used
     */
    if (buffer[0] != 0 || buffer[1] != 1) 
    	goto loser;
    for (i = 2; i < modulus_len; i++) {
	if (buffer[i] == 0) {
	    *data_len = modulus_len - i - 1;
	    break;
	}
	if (buffer[i] != 0xff) 
	    goto loser;
    }
    if (*data_len == 0) 
    	goto loser;
    if (*data_len > max_output_len) 
    	goto loser;

    /*
     * make sure we get the same results
     */
    PORT_Memcpy(data,buffer + modulus_len - *data_len, *data_len);

    PORT_Free(buffer);
    return SECSuccess;

loser:
    PORT_Free(buffer);
failure:
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_EncryptBlock(NSSLOWKEYPublicKey *key, 
                 unsigned char *     output, 
		 unsigned int *      output_len,
                 unsigned int        max_output_len, 
		 unsigned char *     input, 
		 unsigned int        input_len)
{
    SECStatus     rv;
    unsigned int  modulus_len = nsslowkey_PublicModulusLen(key);
    SECItem       formatted;
    SECItem       unformatted;

    formatted.data = NULL;
    if (max_output_len < modulus_len) 
    	goto failure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	goto failure;

    unformatted.len  = input_len;
    unformatted.data = input;
    formatted.data   = NULL;
    rv = rsa_FormatBlock(&formatted, modulus_len, RSA_BlockPublic,
			 &unformatted);
    if (rv != SECSuccess) 
	goto failure;

    rv = RSA_PublicKeyOp(&key->u.rsa, output, formatted.data);
    if (rv != SECSuccess) 
    	goto failure;

    PORT_ZFree(formatted.data, modulus_len);
    *output_len = modulus_len;
    return SECSuccess;

failure:
    if (formatted.data != NULL) 
	PORT_ZFree(formatted.data, modulus_len);
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_DecryptBlock(NSSLOWKEYPrivateKey *key, 
                 unsigned char *      output, 
		 unsigned int *       output_len,
                 unsigned int         max_output_len, 
		 unsigned char *      input, 
		 unsigned int         input_len)
{
    SECStatus       rv;
    unsigned int    modulus_len = nsslowkey_PrivateModulusLen(key);
    unsigned int    i;
    unsigned char * buffer;

    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	goto failure;
    if (input_len != modulus_len)
    	goto failure;

    buffer = (unsigned char *)PORT_Alloc(modulus_len + 1);
    if (!buffer)
    	goto failure;

    rv = RSA_PrivateKeyOp(&key->u.rsa, buffer, input);
    if (rv != SECSuccess) {
	if (PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
	    sftk_fatalError = PR_TRUE;
	}
    	goto loser;
    }

    if (buffer[0] != 0 || buffer[1] != 2) 
    	goto loser;
    *output_len = 0;
    for (i = 2; i < modulus_len; i++) {
	if (buffer[i] == 0) {
	    *output_len = modulus_len - i - 1;
	    break;
	}
    }
    if (*output_len == 0) 
    	goto loser;
    if (*output_len > max_output_len) 
    	goto loser;

    PORT_Memcpy(output, buffer + modulus_len - *output_len, *output_len);

    PORT_Free(buffer);
    return SECSuccess;

loser:
    PORT_Free(buffer);
failure:
    return SECFailure;
}

/* XXX Doesn't set error code */
/*
 * added to make pkcs #11 happy
 *   RAW is RSA_X_509
 */
SECStatus
RSA_SignRaw(NSSLOWKEYPrivateKey *key, 
            unsigned char *      output, 
	    unsigned int *       output_len,
            unsigned int         maxOutputLen, 
	    unsigned char *      input, 
	    unsigned int         input_len)
{
    SECStatus    rv          = SECSuccess;
    unsigned int modulus_len = nsslowkey_PrivateModulusLen(key);
    SECItem      formatted;
    SECItem      unformatted;

    if (maxOutputLen < modulus_len) 
    	return SECFailure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	return SECFailure;

    unformatted.len  = input_len;
    unformatted.data = input;
    formatted.data   = NULL;
    rv = rsa_FormatBlock(&formatted, modulus_len, RSA_BlockRaw, &unformatted);
    if (rv != SECSuccess) 
    	goto done;

    rv = RSA_PrivateKeyOpDoubleChecked(&key->u.rsa, output, formatted.data);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
	sftk_fatalError = PR_TRUE;
    }
    *output_len = modulus_len;

done:
    if (formatted.data != NULL) 
    	PORT_ZFree(formatted.data, modulus_len);
    return rv;
}

/* XXX Doesn't set error code */
SECStatus
RSA_CheckSignRaw(NSSLOWKEYPublicKey *key,
                 unsigned char *     sign, 
		 unsigned int        sign_len, 
		 unsigned char *     hash, 
		 unsigned int        hash_len)
{
    SECStatus       rv;
    unsigned int    modulus_len = nsslowkey_PublicModulusLen(key);
    unsigned char * buffer;

    if (sign_len != modulus_len) 
    	goto failure;
    if (hash_len > modulus_len) 
    	goto failure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	goto failure;

    buffer = (unsigned char *)PORT_Alloc(modulus_len + 1);
    if (!buffer)
    	goto failure;

    rv = RSA_PublicKeyOp(&key->u.rsa, buffer, sign);
    if (rv != SECSuccess)
	goto loser;

    /*
     * make sure we get the same results
     */
    /* NOTE: should we verify the leading zeros? */
    if (PORT_Memcmp(buffer + (modulus_len-hash_len), hash, hash_len) != 0)
	goto loser;

    PORT_Free(buffer);
    return SECSuccess;

loser:
    PORT_Free(buffer);
failure:
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_CheckSignRecoverRaw(NSSLOWKEYPublicKey *key,
                        unsigned char *     data,
                        unsigned int *      data_len, 
			unsigned int        max_output_len, 
			unsigned char *     sign,
			unsigned int        sign_len)
{
    SECStatus      rv;
    unsigned int   modulus_len = nsslowkey_PublicModulusLen(key);

    if (sign_len != modulus_len) 
    	goto failure;
    if (max_output_len < modulus_len) 
    	goto failure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	goto failure;

    rv = RSA_PublicKeyOp(&key->u.rsa, data, sign);
    if (rv != SECSuccess)
	goto failure;

    *data_len = modulus_len;
    return SECSuccess;

failure:
    return SECFailure;
}


/* XXX Doesn't set error code */
SECStatus
RSA_EncryptRaw(NSSLOWKEYPublicKey *key, 
	       unsigned char *     output, 
	       unsigned int *      output_len,
               unsigned int        max_output_len, 
	       unsigned char *     input, 
	       unsigned int        input_len)
{
    SECStatus rv;
    unsigned int  modulus_len = nsslowkey_PublicModulusLen(key);
    SECItem       formatted;
    SECItem       unformatted;

    formatted.data = NULL;
    if (max_output_len < modulus_len) 
    	goto failure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	goto failure;

    unformatted.len  = input_len;
    unformatted.data = input;
    formatted.data   = NULL;
    rv = rsa_FormatBlock(&formatted, modulus_len, RSA_BlockRaw, &unformatted);
    if (rv != SECSuccess)
	goto failure;

    rv = RSA_PublicKeyOp(&key->u.rsa, output, formatted.data);
    if (rv != SECSuccess) 
    	goto failure;

    PORT_ZFree(formatted.data, modulus_len);
    *output_len = modulus_len;
    return SECSuccess;

failure:
    if (formatted.data != NULL) 
	PORT_ZFree(formatted.data, modulus_len);
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_DecryptRaw(NSSLOWKEYPrivateKey *key, 
               unsigned char *      output, 
	       unsigned int *       output_len,
               unsigned int         max_output_len, 
	       unsigned char *      input, 
	       unsigned int         input_len)
{
    SECStatus     rv;
    unsigned int  modulus_len = nsslowkey_PrivateModulusLen(key);

    if (modulus_len <= 0) 
    	goto failure;
    if (modulus_len > max_output_len) 
    	goto failure;
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey)
    	goto failure;
    if (input_len != modulus_len) 
    	goto failure;

    rv = RSA_PrivateKeyOp(&key->u.rsa, output, input);
    if (rv != SECSuccess) {
	if (PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
	    sftk_fatalError = PR_TRUE;
	}
    	goto failure;
    }

    *output_len = modulus_len;
    return SECSuccess;

failure:
    return SECFailure;
}

/*
 * Mask generation function MGF1 as defined in PKCS #1 v2.1 / RFC 3447.
 */
static SECStatus
MGF1(HASH_HashType hashAlg, unsigned char *mask, unsigned int maskLen,
     const unsigned char *mgfSeed, unsigned int mgfSeedLen)
{
    unsigned int digestLen;
    PRUint32 counter, rounds;
    unsigned char *tempHash, *temp;
    const SECHashObject *hash;
    void *hashContext;
    unsigned char C[4];

    hash = HASH_GetRawHashObject(hashAlg);
    if (hash == NULL)
        return SECFailure;

    hashContext = (*hash->create)();
    rounds = (maskLen + hash->length - 1) / hash->length;
    for (counter = 0; counter < rounds; counter++) {
        C[0] = (unsigned char)((counter >> 24) & 0xff);
        C[1] = (unsigned char)((counter >> 16) & 0xff);
        C[2] = (unsigned char)((counter >> 8) & 0xff);
        C[3] = (unsigned char)(counter & 0xff);

        /* This could be optimized when the clone functions in
         * rawhash.c are implemented. */
        (*hash->begin)(hashContext);
        (*hash->update)(hashContext, mgfSeed, mgfSeedLen); 
        (*hash->update)(hashContext, C, sizeof C);

        tempHash = mask + counter * hash->length;
        if (counter != (rounds-1)) {
            (*hash->end)(hashContext, tempHash, &digestLen, hash->length);
        } else { /* we're in the last round and need to cut the hash */
            temp = PORT_Alloc(hash->length);
            (*hash->end)(hashContext, temp, &digestLen, hash->length);
            PORT_Memcpy(tempHash, temp, maskLen - counter * hash->length);
            PORT_Free(temp);
        }
    }
    (*hash->destroy)(hashContext, PR_TRUE);

    return SECSuccess;
}

/*
 * Decodes an EME-OAEP encoded block, validating the encoding in constant
 * time.
 * Described in RFC 3447, section 7.1.2.
 * input contains the encoded block, after decryption.
 * label is the optional value L that was associated with the message.
 * On success, the original message and message length will be stored in
 * output and outputLen.
 */
static SECStatus
eme_oaep_decode(unsigned char *output, unsigned int *outputLen,
                unsigned int maxOutputLen,
                const unsigned char *input, unsigned int inputLen,
                HASH_HashType hashAlg, HASH_HashType maskHashAlg,
                const unsigned char *label, unsigned int labelLen)
{
    const SECHashObject *hash;
    void *hashContext;
    SECStatus rv = SECFailure;
    unsigned char labelHash[HASH_LENGTH_MAX];
    unsigned int i, maskLen, paddingOffset;
    unsigned char *mask = NULL, *tmpOutput = NULL;
    unsigned char isGood, foundPaddingEnd;

    hash = HASH_GetRawHashObject(hashAlg);

    /* 1.c */
    if (inputLen < (hash->length * 2) + 2) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    /* Step 3.a - Generate lHash */
    hashContext = (*hash->create)();
    if (hashContext == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    (*hash->begin)(hashContext);
    if (labelLen > 0)
        (*hash->update)(hashContext, label, labelLen);
    (*hash->end)(hashContext, labelHash, &i, sizeof(labelHash));
    (*hash->destroy)(hashContext, PR_TRUE);

    tmpOutput = (unsigned char*)PORT_Alloc(inputLen);
    if (tmpOutput == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto done;
    }

    maskLen = inputLen - hash->length - 1;
    mask = (unsigned char*)PORT_Alloc(maskLen);
    if (mask == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto done;
    }

    PORT_Memcpy(tmpOutput, input, inputLen);

    /* 3.c - Generate seedMask */
    MGF1(maskHashAlg, mask, hash->length, &tmpOutput[1 + hash->length],
         inputLen - hash->length - 1);
    /* 3.d - Unmask seed */
    for (i = 0; i < hash->length; ++i)
        tmpOutput[1 + i] ^= mask[i];

    /* 3.e - Generate dbMask */
    MGF1(maskHashAlg, mask, maskLen, &tmpOutput[1], hash->length);
    /* 3.f - Unmask DB */
    for (i = 0; i < maskLen; ++i)
        tmpOutput[1 + hash->length + i] ^= mask[i];

    /* 3.g - Compare Y, lHash, and PS in constant time
     * Warning: This code is timing dependent and must not disclose which of
     * these were invalid.
     */
    paddingOffset = 0;
    isGood = 1;
    foundPaddingEnd = 0;

    /* Compare Y */
    isGood &= constantTimeEQ8(0x00, tmpOutput[0]);

    /* Compare lHash and lHash' */
    isGood &= constantTimeCompare(&labelHash[0],
                                  &tmpOutput[1 + hash->length],
                                  hash->length);

    /* Compare that the padding is zero or more zero octets, followed by a
     * 0x01 octet */
    for (i = 1 + (hash->length * 2); i < inputLen; ++i) {
        unsigned char isZero = constantTimeEQ8(0x00, tmpOutput[i]);
        unsigned char isOne = constantTimeEQ8(0x01, tmpOutput[i]);
        /* non-constant time equivalent:
         * if (tmpOutput[i] == 0x01 && !foundPaddingEnd)
         *     paddingOffset = i;
         */
        paddingOffset = constantTimeCondition(isOne & ~foundPaddingEnd, i,
                                              paddingOffset);
        /* non-constant time equivalent:
         * if (tmpOutput[i] == 0x01)
         *    foundPaddingEnd = true;
         *
         * Note: This may yield false positives, as it will be set whenever
         * a 0x01 byte is encountered. If there was bad padding (eg:
         * 0x03 0x02 0x01), foundPaddingEnd will still be set to true, and
         * paddingOffset will still be set to 2.
         */
        foundPaddingEnd = constantTimeCondition(isOne, 1, foundPaddingEnd);
        /* non-constant time equivalent:
         * if (tmpOutput[i] != 0x00 && tmpOutput[i] != 0x01 &&
         *     !foundPaddingEnd) {
         *    isGood = false;
         * }
         *
         * Note: This may yield false positives, as a message (and padding)
         * that is entirely zeros will result in isGood still being true. Thus
         * it's necessary to check foundPaddingEnd is positive below.
         */
        isGood = constantTimeCondition(~foundPaddingEnd & ~isZero, 0, isGood);
    }

    /* While both isGood and foundPaddingEnd may have false positives, they
     * cannot BOTH have false positives. If both are not true, then an invalid
     * message was received. Note, this comparison must still be done in constant
     * time so as not to leak either condition.
     */
    if (!(isGood & foundPaddingEnd)) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        goto done;
    }

    /* End timing dependent code */

    ++paddingOffset; /* Skip the 0x01 following the end of PS */

    *outputLen = inputLen - paddingOffset;
    if (*outputLen > maxOutputLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        goto done;
    }

    if (*outputLen)
        PORT_Memcpy(output, &tmpOutput[paddingOffset], *outputLen);
    rv = SECSuccess;

done:
    if (mask)
        PORT_ZFree(mask, maskLen);
    if (tmpOutput)
        PORT_ZFree(tmpOutput, inputLen);
    return rv;
}

/*
 * Generate an EME-OAEP encoded block for encryption
 * Described in RFC 3447, section 7.1.1
 * We use input instead of M for the message to be encrypted
 * label is the optional value L to be associated with the message.
 */
static SECStatus
eme_oaep_encode(unsigned char *em, unsigned int emLen,
                const unsigned char *input, unsigned int inputLen,
                HASH_HashType hashAlg, HASH_HashType maskHashAlg,
                const unsigned char *label, unsigned int labelLen)
{
    const SECHashObject *hash;
    void *hashContext;
    SECStatus rv;
    unsigned char *mask;
    unsigned int reservedLen, dbMaskLen, i;

    hash = HASH_GetRawHashObject(hashAlg);

    /* Step 1.b */
    reservedLen = (2 * hash->length) + 2;
    if (emLen < reservedLen || inputLen > (emLen - reservedLen)) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    /*
     * From RFC 3447, Section 7.1
     *                      +----------+---------+-------+
     *                 DB = |  lHash   |    PS   |   M   |
     *                      +----------+---------+-------+
     *                                     |
     *           +----------+              V
     *           |   seed   |--> MGF ---> xor
     *           +----------+              |
     *                 |                   |
     *        +--+     V                   |
     *        |00|    xor <----- MGF <-----|
     *        +--+     |                   |
     *          |      |                   |
     *          V      V                   V
     *        +--+----------+----------------------------+
     *  EM =  |00|maskedSeed|          maskedDB          |
     *        +--+----------+----------------------------+
     *
     * We use mask to hold the result of the MGF functions, and all other
     * values are generated in their final resting place.
     */
    *em = 0x00;

    /* Step 2.a - Generate lHash */
    hashContext = (*hash->create)();
    if (hashContext == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    (*hash->begin)(hashContext);
    if (labelLen > 0)
        (*hash->update)(hashContext, label, labelLen);
    (*hash->end)(hashContext, &em[1 + hash->length], &i, hash->length);
    (*hash->destroy)(hashContext, PR_TRUE);

    /* Step 2.b - Generate PS */
    if (emLen - reservedLen - inputLen > 0) {
        PORT_Memset(em + 1 + (hash->length * 2), 0x00,
                    emLen - reservedLen - inputLen);
    }

    /* Step 2.c. - Generate DB
     * DB = lHash || PS || 0x01 || M
     * Note that PS and lHash have already been placed into em at their
     * appropriate offsets. This just copies M into place
     */
    em[emLen - inputLen - 1] = 0x01;
    if (inputLen)
        PORT_Memcpy(em + emLen - inputLen, input, inputLen);

    /* Step 2.d - Generate seed */
    rv = RNG_GenerateGlobalRandomBytes(em + 1, hash->length);
    if (rv != SECSuccess) {
        return rv;
    }

    /* Step 2.e - Generate dbMask*/
    dbMaskLen = emLen - hash->length - 1;
    mask = (unsigned char*)PORT_Alloc(dbMaskLen);
    if (mask == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    MGF1(maskHashAlg, mask, dbMaskLen, em + 1, hash->length);
    /* Step 2.f - Compute maskedDB*/
    for (i = 0; i < dbMaskLen; ++i)
        em[1 + hash->length + i] ^= mask[i];

    /* Step 2.g - Generate seedMask */
    MGF1(maskHashAlg, mask, hash->length, &em[1 + hash->length], dbMaskLen);
    /* Step 2.h - Compute maskedSeed */
    for (i = 0; i < hash->length; ++i)
        em[1 + i] ^= mask[i];

    PORT_ZFree(mask, dbMaskLen);
    return SECSuccess;
}

/*
 * Encode a RSA-PSS signature.
 * Described in RFC 3447, section 9.1.1.
 * We use mHash instead of M as input.
 * emBits from the RFC is just modBits - 1, see section 8.1.1.
 * We only support MGF1 as the MGF.
 *
 * NOTE: this code assumes modBits is a multiple of 8.
 */
static SECStatus
emsa_pss_encode(unsigned char *em, unsigned int emLen,
                const unsigned char *mHash, HASH_HashType hashAlg,
                HASH_HashType maskHashAlg, unsigned int sLen)
{
    const SECHashObject *hash;
    void *hash_context;
    unsigned char *dbMask;
    unsigned int dbMaskLen, i;
    SECStatus rv;

    hash = HASH_GetRawHashObject(hashAlg);
    dbMaskLen = emLen - hash->length - 1;

    /* Step 3 */
    if (emLen < hash->length + sLen + 2) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }

    /* Step 4 */
    rv = RNG_GenerateGlobalRandomBytes(&em[dbMaskLen - sLen], sLen);
    if (rv != SECSuccess) {
	return rv;
    }

    /* Step 5 + 6 */
    /* Compute H and store it at its final location &em[dbMaskLen]. */
    hash_context = (*hash->create)();
    if (hash_context == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    (*hash->begin)(hash_context);
    (*hash->update)(hash_context, eightZeros, 8);
    (*hash->update)(hash_context, mHash, hash->length);
    (*hash->update)(hash_context, &em[dbMaskLen - sLen], sLen);
    (*hash->end)(hash_context, &em[dbMaskLen], &i, hash->length);
    (*hash->destroy)(hash_context, PR_TRUE);

    /* Step 7 + 8 */
    PORT_Memset(em, 0, dbMaskLen - sLen - 1);
    em[dbMaskLen - sLen - 1] = 0x01;

    /* Step 9 */
    dbMask = (unsigned char *)PORT_Alloc(dbMaskLen);
    if (dbMask == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    MGF1(maskHashAlg, dbMask, dbMaskLen, &em[dbMaskLen], hash->length);

    /* Step 10 */
    for (i = 0; i < dbMaskLen; i++)
	em[i] ^= dbMask[i];
    PORT_Free(dbMask);

    /* Step 11 */
    em[0] &= 0x7f;

    /* Step 12 */
    em[emLen - 1] = 0xbc;

    return SECSuccess;
}

/*
 * Verify a RSA-PSS signature.
 * Described in RFC 3447, section 9.1.2.
 * We use mHash instead of M as input.
 * emBits from the RFC is just modBits - 1, see section 8.1.2.
 * We only support MGF1 as the MGF.
 *
 * NOTE: this code assumes modBits is a multiple of 8.
 */
static SECStatus
emsa_pss_verify(const unsigned char *mHash,
                const unsigned char *em, unsigned int emLen,
                HASH_HashType hashAlg, HASH_HashType maskHashAlg,
                unsigned int sLen)
{
    const SECHashObject *hash;
    void *hash_context;
    unsigned char *db;
    unsigned char *H_;  /* H' from the RFC */
    unsigned int i, dbMaskLen;
    SECStatus rv;

    hash = HASH_GetRawHashObject(hashAlg);
    dbMaskLen = emLen - hash->length - 1;

    /* Step 3 + 4 + 6 */
    if ((emLen < (hash->length + sLen + 2)) ||
	(em[emLen - 1] != 0xbc) ||
	((em[0] & 0x80) != 0)) {
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	return SECFailure;
    }

    /* Step 7 */
    db = (unsigned char *)PORT_Alloc(dbMaskLen);
    if (db == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    /* &em[dbMaskLen] points to H, used as mgfSeed */
    MGF1(maskHashAlg, db, dbMaskLen, &em[dbMaskLen], hash->length);

    /* Step 8 */
    for (i = 0; i < dbMaskLen; i++) {
	db[i] ^= em[i];
    }

    /* Step 9 */
    db[0] &= 0x7f;

    /* Step 10 */
    for (i = 0; i < (dbMaskLen - sLen - 1); i++) {
	if (db[i] != 0) {
	    PORT_Free(db);
	    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	    return SECFailure;
	}
    }
    if (db[dbMaskLen - sLen - 1] != 0x01) {
	PORT_Free(db);
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	return SECFailure;
    }

    /* Step 12 + 13 */
    H_ = (unsigned char *)PORT_Alloc(hash->length);
    if (H_ == NULL) {
	PORT_Free(db);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    hash_context = (*hash->create)();
    if (hash_context == NULL) {
	PORT_Free(db);
	PORT_Free(H_);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    (*hash->begin)(hash_context);
    (*hash->update)(hash_context, eightZeros, 8);
    (*hash->update)(hash_context, mHash, hash->length);
    (*hash->update)(hash_context, &db[dbMaskLen - sLen], sLen);
    (*hash->end)(hash_context, H_, &i, hash->length);
    (*hash->destroy)(hash_context, PR_TRUE);

    PORT_Free(db);

    /* Step 14 */
    if (PORT_Memcmp(H_, &em[dbMaskLen], hash->length) != 0) {
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	rv = SECFailure;
    } else {
	rv = SECSuccess;
    }

    PORT_Free(H_);
    return rv;
}

static HASH_HashType
GetHashTypeFromMechanism(CK_MECHANISM_TYPE mech)
{
    switch (mech) {
        case CKM_SHA_1:
        case CKG_MGF1_SHA1:
	    return HASH_AlgSHA1;
        case CKM_SHA224:
        case CKG_MGF1_SHA224:
	    return HASH_AlgSHA224;
        case CKM_SHA256:
        case CKG_MGF1_SHA256:
	    return HASH_AlgSHA256;
        case CKM_SHA384:
        case CKG_MGF1_SHA384:
	    return HASH_AlgSHA384;
        case CKM_SHA512:
        case CKG_MGF1_SHA512:
	    return HASH_AlgSHA512;
        default:
	    return HASH_AlgNULL;
    }
}

/* MGF1 is the only supported MGF. */
SECStatus
RSA_CheckSignPSS(CK_RSA_PKCS_PSS_PARAMS *pss_params,
		 NSSLOWKEYPublicKey *key,
		 const unsigned char *sign, unsigned int sign_len,
		 const unsigned char *hash, unsigned int hash_len)
{
    HASH_HashType hashAlg;
    HASH_HashType maskHashAlg;
    SECStatus rv;
    unsigned int modulus_len = nsslowkey_PublicModulusLen(key);
    unsigned char *buffer;

    if (sign_len != modulus_len) {
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	return SECFailure;
    }

    hashAlg = GetHashTypeFromMechanism(pss_params->hashAlg);
    maskHashAlg = GetHashTypeFromMechanism(pss_params->mgf);
    if ((hashAlg == HASH_AlgNULL) || (maskHashAlg == HASH_AlgNULL)) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return SECFailure;
    }

    buffer = (unsigned char *)PORT_Alloc(modulus_len);
    if (!buffer) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    rv = RSA_PublicKeyOp(&key->u.rsa, buffer, sign);
    if (rv != SECSuccess) {
	PORT_Free(buffer);
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	return SECFailure;
    }

    rv = emsa_pss_verify(hash, buffer, modulus_len, hashAlg,
			 maskHashAlg, pss_params->sLen);
    PORT_Free(buffer);

    return rv;
}

/* MGF1 is the only supported MGF. */
SECStatus
RSA_SignPSS(CK_RSA_PKCS_PSS_PARAMS *pss_params, NSSLOWKEYPrivateKey *key,
	    unsigned char *output, unsigned int *output_len,
	    unsigned int max_output_len,
	    const unsigned char *input, unsigned int input_len)
{
    SECStatus rv = SECSuccess;
    unsigned int modulus_len = nsslowkey_PrivateModulusLen(key);
    unsigned char *pss_encoded = NULL;
    HASH_HashType hashAlg;
    HASH_HashType maskHashAlg;

    if (max_output_len < modulus_len) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
	PORT_SetError(SEC_ERROR_INVALID_KEY);
	return SECFailure;
    }

    hashAlg = GetHashTypeFromMechanism(pss_params->hashAlg);
    maskHashAlg = GetHashTypeFromMechanism(pss_params->mgf);
    if ((hashAlg == HASH_AlgNULL) || (maskHashAlg == HASH_AlgNULL)) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return SECFailure;
    }

    pss_encoded = (unsigned char *)PORT_Alloc(modulus_len);
    if (pss_encoded == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    rv = emsa_pss_encode(pss_encoded, modulus_len, input, hashAlg,
			 maskHashAlg, pss_params->sLen);
    if (rv != SECSuccess) 
	goto done;

    rv = RSA_PrivateKeyOpDoubleChecked(&key->u.rsa, output, pss_encoded);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
	sftk_fatalError = PR_TRUE;
    }
    *output_len = modulus_len;

done:
    PORT_Free(pss_encoded);
    return rv;
}

/* MGF1 is the only supported MGF. */
SECStatus
RSA_EncryptOAEP(CK_RSA_PKCS_OAEP_PARAMS *oaepParams,
                NSSLOWKEYPublicKey *key,
                unsigned char *output, unsigned int *outputLen,
                unsigned int maxOutputLen,
                const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;
    unsigned int modulusLen = nsslowkey_PublicModulusLen(key);
    unsigned char *oaepEncoded = NULL;
    unsigned char *sourceData = NULL;
    unsigned int sourceDataLen = 0;

    HASH_HashType hashAlg;
    HASH_HashType maskHashAlg;

    if (maxOutputLen < modulusLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    hashAlg = GetHashTypeFromMechanism(oaepParams->hashAlg);
    maskHashAlg = GetHashTypeFromMechanism(oaepParams->mgf);
    if ((hashAlg == HASH_AlgNULL) || (maskHashAlg == HASH_AlgNULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    /* The PKCS#11 source parameter is the "source" of the label parameter.
     * The only defined source is explicitly specified, in which case, the
     * label is an optional byte string in pSourceData. If ulSourceDataLen is
     * zero, then pSourceData MUST be NULL - otherwise, it must be non-NULL.
     */
    if (oaepParams->source != CKZ_DATA_SPECIFIED) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }
    sourceData = (unsigned char*)oaepParams->pSourceData;
    sourceDataLen = oaepParams->ulSourceDataLen;
    if ((sourceDataLen == 0 && sourceData != NULL) ||
        (sourceDataLen > 0 && sourceData == NULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    oaepEncoded = (unsigned char *)PORT_Alloc(modulusLen);
    if (oaepEncoded == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    rv = eme_oaep_encode(oaepEncoded, modulusLen, input, inputLen,
                         hashAlg, maskHashAlg, sourceData, sourceDataLen);
    if (rv != SECSuccess)
        goto done;

    rv = RSA_PublicKeyOp(&key->u.rsa, output, oaepEncoded);
    if (rv != SECSuccess)
        goto done;
    *outputLen = modulusLen;

done:
    PORT_Free(oaepEncoded);
    return rv;
}

/* MGF1 is the only supported MGF. */
SECStatus
RSA_DecryptOAEP(CK_RSA_PKCS_OAEP_PARAMS *oaepParams,
                NSSLOWKEYPrivateKey *key,
                unsigned char *output, unsigned int *outputLen,
                unsigned int maxOutputLen,
                const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;
    unsigned int modulusLen = nsslowkey_PrivateModulusLen(key);
    unsigned char *oaepEncoded = NULL;
    unsigned char *sourceData = NULL;
    unsigned int sourceDataLen = 0;

    HASH_HashType hashAlg = GetHashTypeFromMechanism(oaepParams->hashAlg);
    HASH_HashType maskHashAlg = GetHashTypeFromMechanism(oaepParams->mgf);

    if ((hashAlg == HASH_AlgNULL) || (maskHashAlg == HASH_AlgNULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    if (inputLen != modulusLen) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    /* The PKCS#11 source parameter is the "source" of the label parameter.
     * The only defined source is explicitly specified, in which case, the
     * label is an optional byte string in pSourceData. If ulSourceDataLen is
     * zero, then pSourceData MUST be NULL - otherwise, it must be non-NULL.
     */
    if (oaepParams->source != CKZ_DATA_SPECIFIED) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }
    sourceData = (unsigned char*)oaepParams->pSourceData;
    sourceDataLen = oaepParams->ulSourceDataLen;
    if ((sourceDataLen == 0 && sourceData != NULL) ||
        (sourceDataLen > 0 && sourceData == NULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    oaepEncoded = (unsigned char *)PORT_Alloc(modulusLen);
    if (oaepEncoded == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    rv = RSA_PrivateKeyOpDoubleChecked(&key->u.rsa, oaepEncoded, input);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
        goto done;
    }

    rv = eme_oaep_decode(output, outputLen, maxOutputLen, oaepEncoded,
                         modulusLen, hashAlg, maskHashAlg, sourceData,
                         sourceDataLen);

done:
    if (oaepEncoded)
        PORT_ZFree(oaepEncoded, modulusLen);
    return rv;
}
