/*
 * PKCS#1 encoding and decoding functions.
 * This file is believed to contain no code licensed from other parties.
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

#include "blapi.h"
#include "softoken.h"
#include "sechash.h"

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
    int i;

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
	 */
	padLen = modulusLen - data->len - 3;
	PORT_Assert (padLen >= RSA_BLOCK_MIN_PAD_LEN);
	if (padLen < RSA_BLOCK_MIN_PAD_LEN) {
	    PORT_Free (block);
	    return NULL;
	}
	for (i = 0; i < padLen; i++) {
	    /* Pad with non-zero random data. */
	    do {
		RNG_GenerateGlobalRandomBytes(bp + i, 1);
	    } while (bp[i] == RSA_BLOCK_AFTER_PAD_OCTET);
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
	RNG_GenerateGlobalRandomBytes(bp, OAEP_SALT_LEN);
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
	if (bp < (block + modulusLen))
	    RNG_GenerateGlobalRandomBytes(bp, block - bp + modulusLen);

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
    if (hash_len > modulus_len - 8) 
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
	if (buffer[i] == 0) 
	    break;
	if (buffer[i] != 0xff) 
	    goto loser;
    }

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
    if (rv != SECSuccess) 
    	goto loser;

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
    if (rv != SECSuccess)
    	goto failure;

    *output_len = modulus_len;
    return SECSuccess;

failure:
    return SECFailure;
}
