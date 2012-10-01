/*
 * PKCS#1 encoding and decoding functions.
 * This file is believed to contain no code licensed from other parties.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: rsawrapr.c,v 1.21 2012/06/26 22:27:31 rrelyea%redhat.com Exp $ */

#include "blapi.h"
#include "softoken.h"

#include "lowkeyi.h"
#include "secerr.h"

#define RSA_BLOCK_MIN_PAD_LEN		8
#define RSA_BLOCK_FIRST_OCTET		0x00
#define RSA_BLOCK_PRIVATE0_PAD_OCTET	0x00
#define RSA_BLOCK_PRIVATE_PAD_OCTET	0xff
#define RSA_BLOCK_AFTER_PAD_OCTET	0x00

#define OAEP_SALT_LEN		8
#define OAEP_PAD_LEN		8
#define OAEP_PAD_OCTET		0x00

#define FLAT_BUFSIZE 512	/* bytes to hold flattened SHA1Context. */

/* Needed for RSA-PSS functions */
static const unsigned char eightZeros[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static SHA1Context *
SHA1_CloneContext(SHA1Context *original)
{
    SHA1Context *  clone	= NULL;
    unsigned char *pBuf;
    int            sha1ContextSize = SHA1_FlattenSize(original);
    SECStatus      frv;
    unsigned char  buf[FLAT_BUFSIZE];

    PORT_Assert(sizeof buf >= sha1ContextSize);
    if (sizeof buf >= sha1ContextSize) {
    	pBuf = buf;
    } else {
        pBuf = PORT_Alloc(sha1ContextSize);
	if (!pBuf)
	    goto done;
    }

    frv = SHA1_Flatten(original, pBuf);
    if (frv == SECSuccess) {
	clone = SHA1_Resurrect(pBuf, NULL);
	memset(pBuf, 0, sha1ContextSize);
    }
done:
    if (pBuf != buf)
    	PORT_Free(pBuf);
    return clone;
}

/*
 * Modify data by XORing it with a special hash of salt.
 */
static SECStatus
oaep_xor_with_h1(unsigned char *data, unsigned int datalen,
		 unsigned char *salt, unsigned int saltlen)
{
    SHA1Context *sha1cx;
    unsigned char *dp, *dataend;
    unsigned char end_octet;

    sha1cx = SHA1_NewContext();
    if (sha1cx == NULL) {
	return SECFailure;
    }

    /*
     * Get a hash of salt started; we will use it several times,
     * adding in a different end octet (x00, x01, x02, ...).
     */
    SHA1_Begin (sha1cx);
    SHA1_Update (sha1cx, salt, saltlen);
    end_octet = 0;

    dp = data;
    dataend = data + datalen;

    while (dp < dataend) {
	SHA1Context *sha1cx_h1;
	unsigned int sha1len, sha1off;
	unsigned char sha1[SHA1_LENGTH];

	/*
	 * Create hash of (salt || end_octet)
	 */
	sha1cx_h1 = SHA1_CloneContext (sha1cx);
	SHA1_Update (sha1cx_h1, &end_octet, 1);
	SHA1_End (sha1cx_h1, sha1, &sha1len, sizeof(sha1));
	SHA1_DestroyContext (sha1cx_h1, PR_TRUE);
	PORT_Assert (sha1len == SHA1_LENGTH);

	/*
	 * XOR that hash with the data.
	 * When we have fewer than SHA1_LENGTH octets of data
	 * left to xor, use just the low-order ones of the hash.
	 */
	sha1off = 0;
	if ((dataend - dp) < SHA1_LENGTH)
	    sha1off = SHA1_LENGTH - (dataend - dp);
	while (sha1off < SHA1_LENGTH)
	    *dp++ ^= sha1[sha1off++];

	/*
	 * Bump for next hash chunk.
	 */
	end_octet++;
    }

    SHA1_DestroyContext (sha1cx, PR_TRUE);
    return SECSuccess;
}

/*
 * Modify salt by XORing it with a special hash of data.
 */
static SECStatus
oaep_xor_with_h2(unsigned char *salt, unsigned int saltlen,
		 unsigned char *data, unsigned int datalen)
{
    unsigned char sha1[SHA1_LENGTH];
    unsigned char *psalt, *psha1, *saltend;
    SECStatus rv;

    /*
     * Create a hash of data.
     */
    rv = SHA1_HashBuf (sha1, data, datalen);
    if (rv != SECSuccess) {
	return rv;
    }

    /*
     * XOR the low-order octets of that hash with salt.
     */
    PORT_Assert (saltlen <= SHA1_LENGTH);
    saltend = salt + saltlen;
    psalt = salt;
    psha1 = sha1 + SHA1_LENGTH - saltlen;
    while (psalt < saltend) {
	*psalt++ ^= *psha1++;
    }

    return SECSuccess;
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

      /*
       * Blocks intended for public-key operation, using
       * Optimal Asymmetric Encryption Padding (OAEP).
       */
      case RSA_BlockOAEP:
	/*
	 * 0x00 || BT || Modified2(Salt) || Modified1(PaddedData)
	 *   1      1     OAEP_SALT_LEN     OAEP_PAD_LEN + data->len [+ N]
	 *
	 * where:
	 *   PaddedData is "Pad1 || ActualData [|| Pad2]"
	 *   Salt is random data.
	 *   Pad1 is all zeros.
	 *   Pad2, if present, is random data.
	 *   (The "modified" fields are all the same length as the original
	 * unmodified values; they are just xor'd with other values.)
	 *
	 *   Modified1 is an XOR of PaddedData with a special octet
	 * string constructed of iterated hashing of Salt (see below).
	 *   Modified2 is an XOR of Salt with the low-order octets of
	 * the hash of Modified1 (see farther below ;-).
	 *
	 * Whew!
	 */


	/*
	 * Salt
	 */
	rv = RNG_GenerateGlobalRandomBytes(bp, OAEP_SALT_LEN);
	if (rv != SECSuccess) {
	    sftk_fatalError = PR_TRUE;
	    PORT_Free (block);
	    return NULL;
	}
	bp += OAEP_SALT_LEN;

	/*
	 * Pad1
	 */
	PORT_Memset (bp, OAEP_PAD_OCTET, OAEP_PAD_LEN);
	bp += OAEP_PAD_LEN;

	/*
	 * Data
	 */
	PORT_Memcpy (bp, data->data, data->len);
	bp += data->len;

	/*
	 * Pad2
	 */
	if (bp < (block + modulusLen)) {
	    rv = RNG_GenerateGlobalRandomBytes(bp, block - bp + modulusLen);
	    if (rv != SECSuccess) {
		sftk_fatalError = PR_TRUE;
		PORT_Free (block);
		return NULL;
	    }
	}

	/*
	 * Now we have the following:
	 * 0x00 || BT || Salt || PaddedData
	 * (From this point on, "Pad1 || Data [|| Pad2]" is treated
	 * as the one entity PaddedData.)
	 *
	 * We need to turn PaddedData into Modified1.
	 */
	if (oaep_xor_with_h1(block + 2 + OAEP_SALT_LEN,
			     modulusLen - 2 - OAEP_SALT_LEN,
			     block + 2, OAEP_SALT_LEN) != SECSuccess) {
	    PORT_Free (block);
	    return NULL;
	}

	/*
	 * Now we have:
	 * 0x00 || BT || Salt || Modified1(PaddedData)
	 *
	 * The remaining task is to turn Salt into Modified2.
	 */
	if (oaep_xor_with_h2(block + 2, OAEP_SALT_LEN,
			     block + 2 + OAEP_SALT_LEN,
			     modulusLen - 2 - OAEP_SALT_LEN) != SECSuccess) {
	    PORT_Free (block);
	    return NULL;
	}

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

      case RSA_BlockOAEP:
	/*
	 * 0x00 || BT || M1(Salt) || M2(Pad1||ActualData[||Pad2])
	 *
	 * The "2" below is the first octet + the second octet.
	 * (The other fields do not contain the clear values, but are
	 * the same length as the clear values.)
	 */
	PORT_Assert (data->len <= (modulusLen - (2 + OAEP_SALT_LEN
						 + OAEP_PAD_LEN)));

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
    memset(em, 0, dbMaskLen - sLen - 1);
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
