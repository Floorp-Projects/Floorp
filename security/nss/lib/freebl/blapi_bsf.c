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
 * Communications Corporation.	Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*****************************************************************************
**
**	Implementation of BLAPI using RSA BSAFE Crypto-C 4.1
**
******************************************************************************/

/*
**  Notes:
**
**  1.  SHA1, MD2, and MD5 are not implemented here.  This is because 
**      BSAFE Crypto-C 4.1 does not provide a mechanism for saving and 
**      restoring intermediate hash values.  BLAPI uses the functions 
**      <hash>_Flatten and <hash>_Resurrect to accomplish this, so the 
**      hashes are implemented elsewhere.
**
**  2.  The DSA and PQG functions which use seeds are not consistent across
**      implementations.  In this BSAFE-dependent implementation of BLAPI, 
**      the seed is understood to be an initial value sent to a random number 
**      generator used during the key/param generation.  In the implementation
**      used by Netscape-branded products, the seed is understood to be actual 
**      bytes used for the creation of a key or parameters.  This means that 
**      while the BSAFE-dependent implementation may be self-consistent (using 
**      the same seed will produce the same key/parameters), it is not 
**      consistent with the Netscape-branded implementation.  
**      Also, according to the BSAFE Crypto-C 4.0 Library Reference Manual, the
**      SHA1 Random Number Generator is implemented according to the X9.62
**      Draft Standard.  Random number generation in the Netscape-branded
**      implementation of BLAPI is compliant with FIPS-186, thus random
**      numbers generated using the same seed will differ across 
**      implementations.
**
**  3.  PQG_VerifyParams is not implemented here.  BSAFE Crypto-C 4.1 
**      allows access to the seed and counter values used in generating
**      p and q, but does not provide a mechanism for verifying that
**      p, q, and g were generated from that seed and counter.  At this
**      time, this implementation will set a PR_NOT_IMPLEMENTED_ERROR 
**      in a call to PQG_VerifyParams.
**
*/

#include "prerr.h"
#include "secerr.h"

/* BSAFE headers */
#include "aglobal.h"
#include "bsafe.h"

/* BLAPI definition */
#include "blapi.h"

/* default block sizes for algorithms */
#define DES_BLOCK_SIZE       8
#define RC2_BLOCK_SIZE       8
#define RC5_BLOCK_SIZE       8

#define MAX_RC5_KEY_BYTES 255
#define MAX_RC5_ROUNDS    255
#define RC5_VERSION_NUMBER   0x10
#define NSS_FREEBL_DEFAULT_CHUNKSIZE 2048

#define SECITEMFROMITEM(arena, to, from) \
	tmp.data = from.data; tmp.len = from.len; to.type = siBuffer; \
	if (SECITEM_CopyItem(arena, &to, &tmp) != SECSuccess) goto loser;

#define ITEMFROMSECITEM(to, from) \
	to.data = from.data; to.len = from.len;

static const B_ALGORITHM_METHOD *rand_chooser[] = {
	&AM_SHA_RANDOM,
	(B_ALGORITHM_METHOD *)NULL_PTR
};

static B_ALGORITHM_OBJ 
generateRandomAlgorithm(int numBytes, unsigned char *seedData)
{
	SECItem seed = { siBuffer, 0, 0 };
	B_ALGORITHM_OBJ randomAlgorithm = NULL_PTR;
	int status;

	/*  Allocate space for random seed.  */
	if (seedData) {
		seed.len = numBytes;
		seed.data = seedData;
	} else {
		if (SECITEM_AllocItem(NULL, &seed, numBytes) == NULL) {
			PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
			goto loser;
		}
		if ((status = RNG_GenerateGlobalRandomBytes(seed.data, seed.len)) 
		     != 0) {
			PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
			goto loser;
		}
	}

	/*  Generate the random seed.  */
	if ((status = B_CreateAlgorithmObject(&randomAlgorithm)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_SetAlgorithmInfo(randomAlgorithm, AI_SHA1Random, 
	                                 NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_RandomInit(randomAlgorithm, rand_chooser,
	                           (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_RandomUpdate(randomAlgorithm, seed.data, seed.len,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		goto loser;
	}

	if (seedData == NULL)
		SECITEM_FreeItem(&seed, PR_FALSE);

	return randomAlgorithm;

loser:
	if (randomAlgorithm != NULL_PTR)
		B_DestroyAlgorithmObject(&randomAlgorithm);
	if (seedData == NULL)
		SECITEM_FreeItem(&seed, PR_FALSE);
	return NULL_PTR;
}

/*****************************************************************************
** BLAPI implementation of DES
******************************************************************************/

struct DESContextStr {
    B_ALGORITHM_OBJ     algobj;
	B_ALGORITHM_METHOD *alg_chooser[3];
	B_KEY_OBJ           keyobj;
};

DESContext *
DES_CreateContext(unsigned char *key, unsigned char *iv,
                  int mode, PRBool encrypt)
{
    /* BLAPI */
	DESContext *cx;
	/* BSAFE */
	B_BLK_CIPHER_W_FEEDBACK_PARAMS fbParams;
	ITEM                           ivItem;
	unsigned int                   blockLength = DES_BLOCK_SIZE;
	int status;

	cx = (DESContext *)PORT_ZAlloc(sizeof(DESContext));
	if (cx == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		return NULL;
	}

	/* Create an encryption object. */
	cx->algobj = (B_ALGORITHM_OBJ)NULL_PTR;
	if ((status = B_CreateAlgorithmObject(&cx->algobj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	/* Set the IV. */
	ivItem.data = iv;
	ivItem.len = DES_BLOCK_SIZE;

	/* Create the key. */
	cx->keyobj = (B_KEY_OBJ)NULL_PTR;
	if ((status = B_CreateKeyObject(&cx->keyobj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	/* Set fields common to all DES modes. */
	fbParams.encryptionParams     = NULL_PTR;
	fbParams.paddingMethodName    = (unsigned char *)"nopad";
	fbParams.paddingParams        = NULL_PTR;

	/* Set mode-specific fields. */
	switch (mode) {
	case NSS_DES:
		fbParams.encryptionMethodName = (unsigned char *)"des";
		fbParams.feedbackMethodName   = (unsigned char *)"ecb";
		fbParams.feedbackParams       = (POINTER)&blockLength;
		if ((status = B_SetKeyInfo(cx->keyobj, KI_DES8Strong, (POINTER)key)) 
		     != 0) {
			PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
			goto loser;
		}
		if (encrypt) {
			cx->alg_chooser[0] = &AM_DES_ENCRYPT;
			cx->alg_chooser[1] = &AM_ECB_ENCRYPT;
		} else {
			cx->alg_chooser[0] = &AM_DES_DECRYPT;
			cx->alg_chooser[1] = &AM_ECB_DECRYPT;
		}
		cx->alg_chooser[2] = (B_ALGORITHM_METHOD *)NULL_PTR;
		break;

	case NSS_DES_CBC:
		fbParams.encryptionMethodName = (unsigned char *)"des";
		fbParams.feedbackMethodName   = (unsigned char *)"cbc";
		fbParams.feedbackParams       = (POINTER)&ivItem;
		if ((status = B_SetKeyInfo(cx->keyobj, KI_DES8Strong, (POINTER)key)) 
		     != 0) {
			PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
			goto loser;
		}
		if (encrypt) {
			cx->alg_chooser[0] = &AM_DES_ENCRYPT;
			cx->alg_chooser[1] = &AM_CBC_ENCRYPT;
		} else {
			cx->alg_chooser[0] = &AM_DES_DECRYPT;
			cx->alg_chooser[1] = &AM_CBC_DECRYPT;
		}
		cx->alg_chooser[2] = (B_ALGORITHM_METHOD *)NULL_PTR;
		break;

	case NSS_DES_EDE3:
		fbParams.encryptionMethodName = (unsigned char *)"des_ede";
		fbParams.feedbackMethodName   = (unsigned char *)"ecb";
		fbParams.feedbackParams       = (POINTER)&blockLength;
		if ((status = B_SetKeyInfo(cx->keyobj, KI_DES24Strong, (POINTER)key)) 
		     != 0) {
			PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
			goto loser;
		}
		if (encrypt) {
			cx->alg_chooser[0] = &AM_DES_EDE_ENCRYPT;
			cx->alg_chooser[1] = &AM_ECB_ENCRYPT;
		} else {
			cx->alg_chooser[0] = &AM_DES_EDE_DECRYPT;
			cx->alg_chooser[1] = &AM_ECB_DECRYPT;
		}
		cx->alg_chooser[2] = (B_ALGORITHM_METHOD *)NULL_PTR;
		break;

	case NSS_DES_EDE3_CBC:
		fbParams.encryptionMethodName = (unsigned char *)"des_ede";
		fbParams.feedbackMethodName   = (unsigned char *)"cbc";
		fbParams.feedbackParams       = (POINTER)&ivItem;
		if ((status = B_SetKeyInfo(cx->keyobj, KI_DES24Strong, (POINTER)key)) 
		     != 0) {
			PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
			goto loser;
		}
		if (encrypt) {
			cx->alg_chooser[0] = &AM_DES_EDE_ENCRYPT;
			cx->alg_chooser[1] = &AM_CBC_ENCRYPT;
		} else {
			cx->alg_chooser[0] = &AM_DES_EDE_DECRYPT;
			cx->alg_chooser[1] = &AM_CBC_DECRYPT;
		}
		cx->alg_chooser[2] = (B_ALGORITHM_METHOD *)NULL_PTR;
		break;

	default:
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SetAlgorithmInfo(cx->algobj, AI_FeedbackCipher,
	                                 (POINTER)&fbParams)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	return cx;

loser:
	DES_DestroyContext(cx, PR_TRUE);
	return NULL;
}

void 
DES_DestroyContext(DESContext *cx, PRBool freeit)
{
	if (freeit) {
		PORT_Assert(cx != NULL);
		if (cx == NULL) {
			PORT_SetError(SEC_ERROR_INVALID_ARGS);
			return;
		}
		if (cx->keyobj != NULL_PTR)
			B_DestroyKeyObject(&cx->keyobj);
		if (cx->algobj != NULL_PTR)
			B_DestroyAlgorithmObject(&cx->algobj);
		PORT_ZFree(cx, sizeof(DESContext));
	}
}

SECStatus 
DES_Encrypt(DESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            unsigned char *input, unsigned int inputLen)
{
	unsigned int outputLenUpdate, outputLenFinal;
	int status;

	PORT_Assert(cx != NULL);
	if (cx == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert((inputLen & (DES_BLOCK_SIZE -1 )) == 0);
	if (inputLen & (DES_BLOCK_SIZE -1 )) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert(maxOutputLen >= inputLen); /* check for enough room */
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	if ((status = B_EncryptInit(cx->algobj, cx->keyobj, cx->alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if ((status = B_EncryptUpdate(cx->algobj, 
	                              output, 
	                              &outputLenUpdate,
	                              maxOutputLen, 
	                              input, 
	                              inputLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	if ((status = B_EncryptFinal(cx->algobj, 
	                             output + outputLenUpdate, 
	                             &outputLenFinal, 
	                             maxOutputLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	*outputLen = outputLenUpdate + outputLenFinal;
	return SECSuccess;
}

SECStatus 
DES_Decrypt(DESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            unsigned char *input, unsigned int inputLen)
{
	unsigned int outputLenUpdate, outputLenFinal;
	int status;
	ptrdiff_t inpptr;
	unsigned char *inp = NULL;
	PRBool cpybuffer = PR_FALSE;

	/*  The BSAFE Crypto-C 4.1 library with which we tested on a Sun 
	 *  UltraSparc crashed when the input to an DES CBC decryption operation
	 *  was not 4-byte aligned.
	 *  So, we work around this problem by aligning unaligned input in a
	 *  temporary buffer.
	 */
	inpptr = (ptrdiff_t)input;
	if (inpptr & 0x03) {
		inp = PORT_ZAlloc(inputLen);
		if (inp == NULL) {
			PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
			return SECFailure;
		}
		PORT_Memcpy(inp, input, inputLen);
		cpybuffer = PR_TRUE;
	} else {
		inp = input;
	}

	PORT_Assert(cx != NULL);
	if (cx == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert((inputLen & (DES_BLOCK_SIZE - 1)) == 0);
	if (inputLen & (DES_BLOCK_SIZE - 1)) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert(maxOutputLen >= inputLen);  /* check for enough room */
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	if ((status = B_DecryptInit(cx->algobj, cx->keyobj, cx->alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if ((status = B_DecryptUpdate(cx->algobj, 
	                              output, 
	                              &outputLenUpdate,
	                              maxOutputLen, 
	                              input, 
	                              inputLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	if ((status = B_DecryptFinal(cx->algobj, 
	                             output + outputLenUpdate, 
	                             &outputLenFinal, 
	                             maxOutputLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	*outputLen = outputLenUpdate + outputLenFinal;
	return SECSuccess;
}

/*****************************************************************************
** BLAPI implementation of RC2
******************************************************************************/

struct RC2ContextStr
{
	B_ALGORITHM_OBJ     algobj;
	B_ALGORITHM_METHOD *alg_chooser[6];
	B_KEY_OBJ           keyobj;
};

RC2Context *
RC2_CreateContext(unsigned char *key, unsigned int len,
                  unsigned char *iv, int mode, unsigned effectiveKeyLen)
{
	/* BLAPI */
	RC2Context *cx;
	/* BSAFE */
	B_BLK_CIPHER_W_FEEDBACK_PARAMS fbParams;
	A_RC2_PARAMS                   rc2Params;
	ITEM                           ivItem;
	ITEM                           keyItem;
	unsigned int                   blockLength = RC2_BLOCK_SIZE;
	int status;

	if (mode == NSS_RC2_CBC && iv == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return NULL;
	}

	cx = (RC2Context *)PORT_ZAlloc(sizeof(RC2Context));
	if (cx == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		return NULL;
	}
	cx->algobj = (B_ALGORITHM_OBJ)NULL_PTR;
	if ((status = B_CreateAlgorithmObject(&cx->algobj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	cx->keyobj = (B_KEY_OBJ)NULL_PTR;
	if ((status = B_CreateKeyObject(&cx->keyobj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	rc2Params.effectiveKeyBits = effectiveKeyLen * BITS_PER_BYTE;
	ivItem.data = iv;
	ivItem.len = RC2_BLOCK_SIZE;

	fbParams.encryptionMethodName = (unsigned char *)"rc2";
	fbParams.encryptionParams = (POINTER)&rc2Params;
	fbParams.paddingMethodName = (unsigned char *)"nopad";
	fbParams.paddingParams = NULL_PTR;
	cx->alg_chooser[0] = &AM_RC2_ENCRYPT;
	cx->alg_chooser[1] = &AM_RC2_DECRYPT;
	cx->alg_chooser[4] = &AM_SHA_RANDOM;
	cx->alg_chooser[5] = (B_ALGORITHM_METHOD *)NULL;

	switch (mode) {
	case NSS_RC2:
		fbParams.feedbackMethodName = (unsigned char *)"ecb";
		fbParams.feedbackParams = (POINTER)&blockLength;
		cx->alg_chooser[2] = &AM_ECB_ENCRYPT;
		cx->alg_chooser[3] = &AM_ECB_DECRYPT;
		break;
	case NSS_RC2_CBC:
		fbParams.feedbackMethodName = (unsigned char *)"cbc";
		fbParams.feedbackParams = (POINTER)&ivItem;
		cx->alg_chooser[2] = &AM_CBC_ENCRYPT;
		cx->alg_chooser[3] = &AM_CBC_DECRYPT;
		break;
	default:
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SetAlgorithmInfo(cx->algobj, AI_FeedbackCipher,
	                                 (POINTER)&fbParams)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	keyItem.len = len;
	keyItem.data = key;
	if ((status = B_SetKeyInfo(cx->keyobj, KI_Item, (POINTER)&keyItem)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	return cx;

loser:
	RC2_DestroyContext(cx, PR_TRUE);
	return NULL;
}

void 
RC2_DestroyContext(RC2Context *cx, PRBool freeit)
{
	if (freeit) {
		PORT_Assert(cx != NULL);
		if (cx == NULL) {
			PORT_SetError(SEC_ERROR_INVALID_ARGS);
			return;
		}
		if (cx->keyobj != NULL_PTR)
			B_DestroyKeyObject(&cx->keyobj);
		if (cx->algobj != NULL_PTR)
			B_DestroyAlgorithmObject(&cx->algobj);
		PORT_ZFree(cx, sizeof(RC2Context));
	}
}

SECStatus 
RC2_Encrypt(RC2Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            unsigned char *input, unsigned int inputLen)
{
	int status;
	unsigned int outputLenUpdate, outputLenFinal;

	PORT_Assert(cx != NULL);
	if (cx == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert((inputLen & (RC2_BLOCK_SIZE - 1)) == 0);
	if (inputLen & (RC2_BLOCK_SIZE - 1)) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert(maxOutputLen >= inputLen);  /* check for enough room */
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	if ((status = B_EncryptInit(cx->algobj, cx->keyobj, cx->alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if ((status = B_EncryptUpdate(cx->algobj, 
	                              output, 
	                              &outputLenUpdate,
	                              maxOutputLen, 
	                              input, 
	                              inputLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	if ((status = B_EncryptFinal(cx->algobj, 
	                             output + outputLenUpdate,
	                             &outputLenFinal, 
	                             maxOutputLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	*outputLen = outputLenUpdate + outputLenFinal;
	return SECSuccess;
}

SECStatus 
RC2_Decrypt(RC2Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            unsigned char *input, unsigned int inputLen)
{
	int status;
	unsigned int outputLenUpdate, outputLenFinal;
	ptrdiff_t inpptr;
	unsigned char *inp = NULL;
	PRBool cpybuffer = PR_FALSE;

	/*  The BSAFE Crypto-C 4.1 library with which we tested on a Sun 
	 *  UltraSparc crashed when the input to an RC2 CBC decryption operation
	 *  was not 4-byte aligned.
	 *  So, we work around this problem by aligning unaligned input in a
	 *  temporary buffer.
	 */
	inpptr = (ptrdiff_t)input;
	if (inpptr & 0x03) {
		inp = PORT_ZAlloc(inputLen);
		if (inp == NULL) {
			PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
			return SECFailure;
		}
		PORT_Memcpy(inp, input, inputLen);
		cpybuffer = PR_TRUE;
	} else {
		inp = input;
	}

	PORT_Assert(cx != NULL);
	if (cx == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}
	PORT_Assert((inputLen & (RC2_BLOCK_SIZE - 1)) == 0);
	if (inputLen & (RC2_BLOCK_SIZE - 1)) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}
	PORT_Assert(maxOutputLen >= inputLen);  /* check for enough room */
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_DecryptInit(cx->algobj, cx->keyobj, cx->alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}
	if ((status = B_DecryptUpdate(cx->algobj, 
	                              output, 
	                              &outputLenUpdate,
	                              maxOutputLen, 
	                              inp,
	                              inputLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		goto loser;
	}
	if ((status = B_DecryptFinal(cx->algobj, 
	                             output + outputLenUpdate,
	                             &outputLenFinal, 
	                             maxOutputLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		goto loser;
	}
	*outputLen = outputLenUpdate + outputLenFinal;

	if (cpybuffer)
		PORT_ZFree(inp, inputLen);
	return SECSuccess;

loser:
	if (cpybuffer)
		PORT_ZFree(inp, inputLen);
	return SECFailure;
}

/*****************************************************************************
** BLAPI implementation of RC4
******************************************************************************/

struct RC4ContextStr
{
	B_ALGORITHM_OBJ     algobj;
	B_ALGORITHM_METHOD *alg_chooser[3];
	B_KEY_OBJ           keyobj;
};

RC4Context *
RC4_CreateContext(unsigned char *key, int len)
{
	/* BLAPI */
	RC4Context *cx;
	/* BSAFE */
	ITEM keyItem;
	int status;

	cx = (RC4Context *)PORT_ZAlloc(sizeof(RC4Context));
	if (cx == NULL) {
		/* set out of memory error */
		return NULL;
	}

	cx->algobj = (B_ALGORITHM_OBJ)NULL_PTR;
	if ((status = B_CreateAlgorithmObject(&cx->algobj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	cx->keyobj = (B_KEY_OBJ)NULL_PTR;
	if ((status = B_CreateKeyObject(&cx->keyobj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_SetAlgorithmInfo(cx->algobj, AI_RC4, NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	cx->alg_chooser[0] = &AM_RC4_ENCRYPT;
	cx->alg_chooser[1] = &AM_RC4_DECRYPT;
	cx->alg_chooser[2] = (B_ALGORITHM_METHOD *)NULL;

	keyItem.len = len;
	keyItem.data = key;
	if ((status = B_SetKeyInfo(cx->keyobj, KI_Item, (POINTER)&keyItem)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	return cx;

loser:
	RC4_DestroyContext(cx, PR_TRUE);
	return NULL;
}

void 
RC4_DestroyContext(RC4Context *cx, PRBool freeit)
{
	if (freeit) {
		PORT_Assert(cx != NULL);
		if (cx == NULL) {
			PORT_SetError(SEC_ERROR_INVALID_ARGS);
			return;
		}
		if (cx->keyobj != NULL_PTR)
			B_DestroyKeyObject(&cx->keyobj);
		if (cx->algobj != NULL_PTR)
			B_DestroyAlgorithmObject(&cx->algobj);
		PORT_ZFree(cx, sizeof(RC4Context));
	}
}

SECStatus 
RC4_Encrypt(RC4Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
	int status;
	unsigned int outputLenUpdate, outputLenFinal;

	PORT_Assert(cx != NULL);
	if (cx == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert(maxOutputLen >= inputLen);  /* check for enough room */
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	if ((status = B_EncryptInit(cx->algobj, cx->keyobj, cx->alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if ((status = B_EncryptUpdate(cx->algobj, 
	                              output, 
	                              &outputLenUpdate,
	                              maxOutputLen, 
	                              input, 
	                              inputLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	if ((status = B_EncryptFinal(cx->algobj, 
	                             output + outputLenUpdate,
	                             &outputLenFinal, 
	                             maxOutputLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	*outputLen = outputLenUpdate + outputLenFinal;
	return SECSuccess;
}

SECStatus 
RC4_Decrypt(RC4Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
	int status;
	unsigned int outputLenUpdate, outputLenFinal;

	PORT_Assert(cx != NULL);
	if (cx == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert(maxOutputLen >= inputLen);  /* check for enough room */
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	if ((status = B_DecryptInit(cx->algobj, cx->keyobj, cx->alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if ((status = B_DecryptUpdate(cx->algobj, 
	                              output, 
	                              &outputLenUpdate,
	                              maxOutputLen, 
	                              input, 
	                              inputLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	if ((status = B_DecryptFinal(cx->algobj, 
	                             output + outputLenUpdate,
	                             &outputLenFinal, 
	                             maxOutputLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	*outputLen = outputLenUpdate + outputLenFinal;
	return SECSuccess;
}


/*****************************************************************************
** BLAPI implementation of RC5
******************************************************************************/

struct RC5ContextStr
{
	B_ALGORITHM_OBJ     algobj;
	B_ALGORITHM_METHOD *alg_chooser[6];
	B_KEY_OBJ           keyobj;
	unsigned int        blocksize;
};

RC5Context *
RC5_CreateContext(SECItem *key, unsigned int rounds,
                  unsigned int wordSize, unsigned char *iv, int mode)
{
	/* BLAPI */
	RC5Context *cx;
	/* BSAFE */
	B_BLK_CIPHER_W_FEEDBACK_PARAMS fbParams;
	A_RC5_PARAMS                   rc5Params;
	ITEM                           keyItem;
	ITEM                           ivItem;
	unsigned int                   blocksize;
	int status;

	if (rounds > MAX_RC5_ROUNDS) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return NULL;
	}
	if (key->len > MAX_RC5_KEY_BYTES) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return NULL;
	}
	if (mode == NSS_RC5_CBC && (iv == NULL)) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return NULL;
	}

	cx = (RC5Context *)PORT_ZAlloc(sizeof(RC5Context));
	if (cx == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		return NULL;
	}
	cx->algobj = (B_ALGORITHM_OBJ)NULL_PTR;
	if ((status = B_CreateAlgorithmObject(&cx->algobj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	cx->keyobj = (B_KEY_OBJ)NULL_PTR;
	if ((status = B_CreateKeyObject(&cx->keyobj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	rc5Params.version = RC5_VERSION_NUMBER;
	rc5Params.rounds = rounds;
	rc5Params.wordSizeInBits = wordSize * BITS_PER_BYTE;
	if (rc5Params.wordSizeInBits == 64) {
		fbParams.encryptionMethodName = (unsigned char *)"rc5_64";
		cx->alg_chooser[0] = &AM_RC5_64ENCRYPT;
		cx->alg_chooser[1] = &AM_RC5_64DECRYPT;
	} else {
		fbParams.encryptionMethodName = (unsigned char *)"rc5";
		cx->alg_chooser[0] = &AM_RC5_ENCRYPT;
		cx->alg_chooser[1] = &AM_RC5_DECRYPT;
	}
	fbParams.encryptionParams = (POINTER)&rc5Params;
	fbParams.paddingMethodName = (unsigned char *)"nopad";
	fbParams.paddingParams = NULL_PTR;
	cx->alg_chooser[4] = &AM_SHA_RANDOM;
	cx->alg_chooser[5] = (B_ALGORITHM_METHOD *)NULL;
	switch (mode) {
	case NSS_RC5:  
		blocksize = 2 * wordSize;
		cx->blocksize = blocksize;
		fbParams.feedbackMethodName = (unsigned char *)"ecb";
		fbParams.feedbackParams = (POINTER)&blocksize;
		cx->alg_chooser[2] = &AM_ECB_ENCRYPT;
		cx->alg_chooser[3] = &AM_ECB_DECRYPT;
		break;
	case NSS_RC5_CBC: 
		ivItem.len = 2 * wordSize;
		ivItem.data = iv;
		cx->blocksize = RC5_BLOCK_SIZE;
		fbParams.feedbackMethodName = (unsigned char *)"cbc";
		fbParams.feedbackParams = (POINTER)&ivItem;
		cx->alg_chooser[2] = &AM_CBC_ENCRYPT;
		cx->alg_chooser[3] = &AM_CBC_DECRYPT;
		break;
	default:
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}
	if ((status = B_SetAlgorithmInfo(cx->algobj, AI_FeedbackCipher,
	                                 (POINTER)&fbParams)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	keyItem.len = key->len;
	keyItem.data = key->data;
	if ((status = B_SetKeyInfo(cx->keyobj, KI_Item, (POINTER)&keyItem)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	return cx;

loser:
	RC5_DestroyContext(cx, PR_TRUE);
	return NULL;
}

void RC5_DestroyContext(RC5Context *cx, PRBool freeit)
{
	if (freeit) {
		PORT_Assert(cx != NULL);
		if (cx == NULL) {
			PORT_SetError(SEC_ERROR_INVALID_ARGS);
			return;
		}
		if (cx->keyobj != NULL_PTR)
			B_DestroyKeyObject(&cx->keyobj);
		if (cx->algobj != NULL_PTR)
			B_DestroyAlgorithmObject(&cx->algobj);
		PORT_ZFree(cx, sizeof(RC5Context));
	}
}

SECStatus 
RC5_Encrypt(RC5Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            unsigned char *input, unsigned int inputLen)
{
	int status;
	unsigned int outputLenUpdate, outputLenFinal;

	PORT_Assert(cx != NULL);
	if (cx == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert((inputLen & (RC5_BLOCK_SIZE - 1)) == 0);
	if (inputLen & (RC5_BLOCK_SIZE - 1)) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert(maxOutputLen >= inputLen);  /* check for enough room */
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	if ((status = B_EncryptInit(cx->algobj, cx->keyobj, cx->alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if ((status = B_EncryptUpdate(cx->algobj, 
	                              output, 
	                              &outputLenUpdate,
	                              maxOutputLen, 
	                              input, 
	                              inputLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	if ((status = B_EncryptFinal(cx->algobj, 
	                             output + outputLenUpdate,
	                             &outputLenFinal, 
	                             maxOutputLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	*outputLen = outputLenUpdate + outputLenFinal;
	return SECSuccess;
}

SECStatus 
RC5_Decrypt(RC5Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            unsigned char *input, unsigned int inputLen)
{
	int status;
	unsigned int outputLenUpdate, outputLenFinal;
	ptrdiff_t inpptr;
	unsigned char *inp = NULL;
	PRBool cpybuffer = PR_FALSE;

	/*  The BSAFE Crypto-C 4.1 library with which we tested on a Sun 
	 *  UltraSparc crashed when the input to an RC5 CBC decryption operation
	 *  was not 4-byte aligned.
	 *  So, we work around this problem by aligning unaligned input in a
	 *  temporary buffer.
	 */
	inpptr = (ptrdiff_t)input;
	if (inpptr & 0x03) {
		inp = PORT_ZAlloc(inputLen);
		if (inp == NULL) {
			PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
			return SECFailure;
		}
		PORT_Memcpy(inp, input, inputLen);
		cpybuffer = PR_TRUE;
	} else {
		inp = input;
	}

	PORT_Assert(cx != NULL);
	if (cx == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert((inputLen & (cx->blocksize - 1)) == 0);
	if (inputLen & (cx->blocksize - 1)) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	PORT_Assert(maxOutputLen >= inputLen);  /* check for enough room */
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	if ((status = B_DecryptInit(cx->algobj, cx->keyobj, cx->alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if ((status = B_DecryptUpdate(cx->algobj, 
	                              output, 
	                              &outputLenUpdate,
	                              maxOutputLen, 
	                              input, 
	                              inputLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	if ((status = B_DecryptFinal(cx->algobj, 
	                             output + outputLenUpdate,
	                             &outputLenFinal, 
	                             maxOutputLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	*outputLen = outputLenUpdate + outputLenFinal;
	return SECSuccess;
}

/*****************************************************************************
** BLAPI implementation of RSA
******************************************************************************/

static const SECItem defaultPublicExponent = 
{
	siBuffer,
	(unsigned char *)"\001\000\001",
	3
};

static const B_ALGORITHM_METHOD *rsa_alg_chooser[] = {
	&AM_SHA_RANDOM,
	&AM_RSA_KEY_GEN,
	&AM_RSA_ENCRYPT,
	&AM_RSA_DECRYPT,
	&AM_RSA_CRT_ENCRYPT,
	&AM_RSA_CRT_DECRYPT,
	(B_ALGORITHM_METHOD *)NULL_PTR
};

static SECStatus
rsaZFreePrivateKeyInfo(A_PKCS_RSA_PRIVATE_KEY *privateKeyInfo)
{
	PORT_ZFree(privateKeyInfo->modulus.data, privateKeyInfo->modulus.len);
	PORT_ZFree(privateKeyInfo->publicExponent.data, 
	           privateKeyInfo->publicExponent.len);
	PORT_ZFree(privateKeyInfo->privateExponent.data, 
	           privateKeyInfo->privateExponent.len);
	PORT_ZFree(privateKeyInfo->prime[0].data, privateKeyInfo->prime[0].len);
	PORT_ZFree(privateKeyInfo->prime[1].data, privateKeyInfo->prime[1].len);
	PORT_ZFree(privateKeyInfo->primeExponent[0].data, 
	           privateKeyInfo->primeExponent[0].len);
	PORT_ZFree(privateKeyInfo->primeExponent[1].data, 
	           privateKeyInfo->primeExponent[1].len);
	PORT_ZFree(privateKeyInfo->coefficient.data, 
	           privateKeyInfo->coefficient.len);
	return SECSuccess;
}

static SECStatus
rsaConvertKeyInfoToBLKey(A_PKCS_RSA_PRIVATE_KEY *keyInfo, RSAPrivateKey *key)
{
	PRArenaPool *arena = key->arena;
	SECItem tmp;

	SECITEMFROMITEM(arena, key->modulus,         keyInfo->modulus);
	SECITEMFROMITEM(arena, key->publicExponent,  keyInfo->publicExponent);
	SECITEMFROMITEM(arena, key->privateExponent, keyInfo->privateExponent);
	SECITEMFROMITEM(arena, key->prime1,          keyInfo->prime[0]);
	SECITEMFROMITEM(arena, key->prime2,          keyInfo->prime[1]);
	SECITEMFROMITEM(arena, key->exponent1,       keyInfo->primeExponent[0]);
	SECITEMFROMITEM(arena, key->exponent2,       keyInfo->primeExponent[1]);
	SECITEMFROMITEM(arena, key->coefficient,     keyInfo->coefficient);
	/* Version field is to be handled at a higher level. */
	key->version.data = NULL;
	key->version.len = 0;
	return SECSuccess;

loser:
	PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
	return SECFailure;
}

static SECStatus
rsaConvertBLKeyToKeyInfo(RSAPrivateKey *key, A_PKCS_RSA_PRIVATE_KEY *keyInfo)
{
	ITEMFROMSECITEM(keyInfo->modulus,          key->modulus);
	ITEMFROMSECITEM(keyInfo->publicExponent,   key->publicExponent);
	ITEMFROMSECITEM(keyInfo->privateExponent,  key->privateExponent);
	ITEMFROMSECITEM(keyInfo->prime[0],         key->prime1);
	ITEMFROMSECITEM(keyInfo->prime[1],         key->prime2);
	ITEMFROMSECITEM(keyInfo->primeExponent[0], key->exponent1);
	ITEMFROMSECITEM(keyInfo->primeExponent[1], key->exponent2);
	ITEMFROMSECITEM(keyInfo->coefficient,      key->coefficient);
	return SECSuccess;
}

RSAPrivateKey *
RSA_NewKey(int         keySizeInBits,
           SECItem *   publicExponent)
{
	/* BLAPI */
	RSAPrivateKey *privateKey;
	/* BSAFE */
	A_RSA_KEY_GEN_PARAMS    keygenParams;
	A_PKCS_RSA_PRIVATE_KEY *privateKeyInfo = (A_PKCS_RSA_PRIVATE_KEY *)NULL_PTR;
	B_ALGORITHM_OBJ         keypairGenerator = (B_ALGORITHM_OBJ)NULL_PTR;
	B_ALGORITHM_OBJ         randomAlgorithm = NULL;
	B_KEY_OBJ               publicKeyObj = (B_KEY_OBJ)NULL_PTR;
	B_KEY_OBJ               privateKeyObj = (B_KEY_OBJ)NULL_PTR;
	PRArenaPool *arena;
	int status;

	/*  Allocate space for key structure. */
	arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
	if (arena == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}
	privateKey = (RSAPrivateKey *)PORT_ArenaZAlloc(arena, 
	                                               sizeof(RSAPrivateKey));
	if (privateKey == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}
	privateKey->arena = arena;

	randomAlgorithm = generateRandomAlgorithm(keySizeInBits / BITS_PER_BYTE, 0);
	if (randomAlgorithm == NULL_PTR) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateAlgorithmObject(&keypairGenerator)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateKeyObject(&publicKeyObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateKeyObject(&privateKeyObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if (publicExponent == NULL) publicExponent = &defaultPublicExponent;
	keygenParams.modulusBits = keySizeInBits;
	keygenParams.publicExponent.data = publicExponent->data;
	keygenParams.publicExponent.len = publicExponent->len;

	if ((status = B_SetAlgorithmInfo(keypairGenerator, AI_RSAKeyGen,
	                                 (POINTER)&keygenParams)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_GenerateInit(keypairGenerator, rsa_alg_chooser,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_GenerateKeypair(keypairGenerator, publicKeyObj,
	                                privateKeyObj, randomAlgorithm,
	                                (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_GetKeyInfo((POINTER *)&privateKeyInfo, privateKeyObj,
	                           KI_PKCS_RSAPrivate)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	/*  Convert the BSAFE key info to an RSAPrivateKey.  */
	if ((status = rsaConvertKeyInfoToBLKey(privateKeyInfo, privateKey)) != 0) {
		goto loser;
	}

	B_DestroyAlgorithmObject(&publicKeyObj);
	B_DestroyKeyObject(&publicKeyObj);
	B_DestroyKeyObject(&privateKeyObj);
	rsaZFreePrivateKeyInfo(privateKeyInfo);
	B_DestroyAlgorithmObject(&randomAlgorithm);
	return privateKey;

loser:
	if (keypairGenerator != NULL_PTR)
		B_DestroyAlgorithmObject(&keypairGenerator);
	if (publicKeyObj != NULL_PTR)
		B_DestroyKeyObject(&publicKeyObj);
	if (privateKeyObj != NULL_PTR)
		B_DestroyKeyObject(&privateKeyObj);
	if (privateKeyInfo != (A_PKCS_RSA_PRIVATE_KEY *)NULL_PTR)
		rsaZFreePrivateKeyInfo(privateKeyInfo);
	if (randomAlgorithm != NULL_PTR)
		B_DestroyAlgorithmObject(&randomAlgorithm);
	PORT_FreeArena(arena, PR_TRUE);
	return NULL;
}

static unsigned int
rsa_modulusLen(SECItem *modulus)
{
	unsigned char byteZero = modulus->data[0];
	unsigned int modLen = modulus->len - !byteZero;
	return modLen;
}

SECStatus 
RSA_PublicKeyOp(RSAPublicKey *   key,
                unsigned char *  output,
                unsigned char *  input)
{
	B_ALGORITHM_OBJ rsaPubKeyAlg = (B_ALGORITHM_OBJ)NULL_PTR;
	B_KEY_OBJ       publicKeyObj = (B_KEY_OBJ)NULL_PTR;
	A_RSA_KEY       pubKeyInfo;
	unsigned int outputLenUpdate;
	unsigned int modulusLen;
	int status;

	PORT_Assert(key != NULL);
	if (key == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	if ((status = B_CreateAlgorithmObject(&rsaPubKeyAlg)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateKeyObject(&publicKeyObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_SetAlgorithmInfo(rsaPubKeyAlg, AI_RSAPublic,
	                                 NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	modulusLen = rsa_modulusLen(&key->modulus);
	pubKeyInfo.modulus.len = key->modulus.len;
	pubKeyInfo.modulus.data = key->modulus.data;
	pubKeyInfo.exponent.len = key->publicExponent.len;
	pubKeyInfo.exponent.data = key->publicExponent.data;

	if ((status = B_SetKeyInfo(publicKeyObj, KI_RSAPublic, 
	                           (POINTER)&pubKeyInfo)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_EncryptInit(rsaPubKeyAlg, publicKeyObj, rsa_alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}
	if ((status = B_EncryptUpdate(rsaPubKeyAlg,
	                              output, 
	                              &outputLenUpdate,
	                              modulusLen, 
	                              input, 
	                              modulusLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		goto loser;
	}
	if ((status = B_EncryptFinal(rsaPubKeyAlg,
	                             output + outputLenUpdate,
	                             &outputLenUpdate, 
	                             modulusLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		goto loser;
	}

	B_DestroyAlgorithmObject(&rsaPubKeyAlg);
	B_DestroyAlgorithmObject(&publicKeyObj);
	/*  Don't delete pubKeyInfo data -- it was a shallow copy.  */
	return SECSuccess;

loser:
	if (rsaPubKeyAlg != NULL_PTR)
		B_DestroyAlgorithmObject(&rsaPubKeyAlg);
	if (publicKeyObj != NULL_PTR)
		B_DestroyAlgorithmObject(&publicKeyObj);
	return SECFailure;
}

SECStatus 
RSA_PrivateKeyOp(RSAPrivateKey *  key,
                 unsigned char *  output,
                 unsigned char *  input)
{
	A_PKCS_RSA_PRIVATE_KEY privKeyInfo;
	B_ALGORITHM_OBJ        rsaPrivKeyAlg = (B_ALGORITHM_OBJ)NULL_PTR;
	B_KEY_OBJ              privateKeyObj = (B_KEY_OBJ)NULL_PTR;
	unsigned int outputLenUpdate;
	unsigned int modulusLen;
	int status;

	if ((status = B_CreateAlgorithmObject(&rsaPrivKeyAlg)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateKeyObject(&privateKeyObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_SetAlgorithmInfo(rsaPrivKeyAlg, AI_RSAPrivate,
	                                 NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = rsaConvertBLKeyToKeyInfo(key, &privKeyInfo)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SetKeyInfo(privateKeyObj, KI_PKCS_RSAPrivate,
	                           (POINTER)&privKeyInfo)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	modulusLen = rsa_modulusLen(&key->modulus);

	if ((status = B_DecryptInit(rsaPrivKeyAlg, privateKeyObj, rsa_alg_chooser,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}
	if ((status = B_DecryptUpdate(rsaPrivKeyAlg,
	                              output, 
	                              &outputLenUpdate,
	                              modulusLen, 
	                              input, 
	                              modulusLen,
	                              (B_ALGORITHM_OBJ)NULL_PTR,
	                              (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		goto loser;
	}
	if ((status = B_DecryptFinal(rsaPrivKeyAlg,
	                             output + outputLenUpdate,
	                             &outputLenUpdate, 
	                             modulusLen - outputLenUpdate,
	                             (B_ALGORITHM_OBJ)NULL_PTR,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		goto loser;
	}

	B_DestroyAlgorithmObject(&rsaPrivKeyAlg);
	B_DestroyAlgorithmObject(&privateKeyObj);
	/*  Don't delete privKeyInfo data -- it was a shallow copy.  */
	return SECSuccess;

loser:
	if (rsaPrivKeyAlg != NULL_PTR)
		B_DestroyAlgorithmObject(&rsaPrivKeyAlg);
	if (privateKeyObj != NULL_PTR)
		B_DestroyAlgorithmObject(&privateKeyObj);
	return SECFailure;
}

/*****************************************************************************
** BLAPI implementation of DSA
******************************************************************************/

static const B_ALGORITHM_METHOD *dsa_pk_gen_chooser[] = {
	&AM_SHA_RANDOM,
	&AM_DSA_PARAM_GEN,
	&AM_DSA_KEY_GEN,
	(B_ALGORITHM_METHOD *)NULL_PTR
};

static SECStatus
dsaConvertKeyInfoToBLKey(A_DSA_PRIVATE_KEY *privateKeyInfo, 
                         A_DSA_PUBLIC_KEY *publicKeyInfo, 
                         DSAPrivateKey *privateKey)
{
	PRArenaPool *arena;
	SECItem tmp;

	arena = privateKey->params.arena;
	SECITEMFROMITEM(arena, privateKey->params.prime, 
	                       privateKeyInfo->params.prime);
	SECITEMFROMITEM(arena, privateKey->params.subPrime, 
	                       privateKeyInfo->params.subPrime);
	SECITEMFROMITEM(arena, privateKey->params.base, 
	                       privateKeyInfo->params.base);
	SECITEMFROMITEM(arena, privateKey->privateValue,
	                       privateKeyInfo->x);
	SECITEMFROMITEM(arena, privateKey->publicValue,
	                       publicKeyInfo->y);

	return SECSuccess;

loser:
	PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
	return SECFailure;
}

static SECStatus
dsaConvertBLKeyToPrKeyInfo(DSAPrivateKey *privateKey,
                           A_DSA_PRIVATE_KEY *privateKeyInfo)
{
	ITEMFROMSECITEM(privateKeyInfo->params.prime, privateKey->params.prime)
	ITEMFROMSECITEM(privateKeyInfo->params.subPrime,
	                privateKey->params.subPrime);
	ITEMFROMSECITEM(privateKeyInfo->params.base, privateKey->params.base);
	ITEMFROMSECITEM(privateKeyInfo->x, privateKey->privateValue);
	return SECSuccess;
}

static SECStatus
dsaConvertBLKeyToPubKeyInfo(DSAPublicKey *publicKey,
                            A_DSA_PUBLIC_KEY *publicKeyInfo)
{
	ITEMFROMSECITEM(publicKeyInfo->params.prime, publicKey->params.prime)
	ITEMFROMSECITEM(publicKeyInfo->params.subPrime,
	                publicKey->params.subPrime);
	ITEMFROMSECITEM(publicKeyInfo->params.base, publicKey->params.base);
	ITEMFROMSECITEM(publicKeyInfo->y, publicKey->publicValue);
	return SECSuccess;
}

static SECStatus
dsaZFreeKeyInfoParams(A_DSA_PARAMS *params)
{
	PORT_ZFree(params->prime.data, 
	           params->prime.len);
	PORT_ZFree(params->subPrime.data, 
	           params->subPrime.len);
	PORT_ZFree(params->base.data, 
	           params->base.len);
	return SECSuccess;
}

static SECStatus
dsaZFreePrivateKeyInfo(A_DSA_PRIVATE_KEY *privateKeyInfo)
{
	dsaZFreeKeyInfoParams(&privateKeyInfo->params);
	PORT_ZFree(privateKeyInfo->x.data,
	           privateKeyInfo->x.len);
	return SECSuccess;
}

static SECStatus
dsaZFreePublicKeyInfo(A_DSA_PUBLIC_KEY *publicKeyInfo)
{
	dsaZFreeKeyInfoParams(&publicKeyInfo->params);
	PORT_ZFree(publicKeyInfo->y.data,
	           publicKeyInfo->y.len);
	return SECSuccess;
}

SECStatus 
DSA_NewKey(PQGParams *           params, 
           DSAPrivateKey **      privKey)
{
	return DSA_NewKeyFromSeed(params, NULL, privKey);
}

SECStatus 
DSA_SignDigest(DSAPrivateKey *   key,
               SECItem *         signature,
               SECItem *         digest)
{
	return DSA_SignDigestWithSeed(key, signature, digest, NULL);
}

SECStatus 
DSA_VerifyDigest(DSAPublicKey *  key,
                 SECItem *       signature,
                 SECItem *       digest)
{
	B_ALGORITHM_OBJ           dsaVerifier = (B_ALGORITHM_OBJ)NULL_PTR;
	B_KEY_OBJ                 publicKeyObj = (B_KEY_OBJ)NULL_PTR;
	A_DSA_PUBLIC_KEY          publicKeyInfo;
	const B_ALGORITHM_METHOD *dsa_verify_chooser[] = {
	                            &AM_DSA_VERIFY,
	                            (B_ALGORITHM_METHOD *)NULL_PTR
	};
	int status;

	if ((status = B_CreateAlgorithmObject(&dsaVerifier)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateKeyObject(&publicKeyObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = dsaConvertBLKeyToPubKeyInfo(key, &publicKeyInfo)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SetKeyInfo(publicKeyObj, KI_DSAPublic, 
	                           (POINTER)&publicKeyInfo)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SetAlgorithmInfo(dsaVerifier, AI_DSA, NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_VerifyInit(dsaVerifier, publicKeyObj, dsa_verify_chooser,
	                           (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_VerifyUpdate(dsaVerifier, digest->data, digest->len,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		goto loser;
	}

	if ((status = B_VerifyFinal(dsaVerifier, signature->data, signature->len,
	                            (B_ALGORITHM_OBJ)NULL_PTR,
	                            (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		if (status == BE_SIGNATURE) {
			PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
		} else {
			PORT_SetError(SEC_ERROR_BAD_DATA);
			goto loser;
		}
	}

	B_DestroyAlgorithmObject(&dsaVerifier);
	B_DestroyKeyObject(&publicKeyObj);
	/*  publicKeyInfo data is shallow copy */
	return (status == BE_SIGNATURE) ? SECFailure : SECSuccess;

loser:
	if (dsaVerifier != NULL_PTR)
		B_DestroyAlgorithmObject(&dsaVerifier);
	if (publicKeyObj != NULL_PTR)
		B_DestroyKeyObject(&publicKeyObj);
	return SECFailure;
}

SECStatus 
DSA_NewKeyFromSeed(PQGParams *params, unsigned char * seed,
                   DSAPrivateKey **privKey)
{
	PRArenaPool *arena;
	DSAPrivateKey *privateKey;
	/* BSAFE */
	B_ALGORITHM_OBJ           dsaKeyGenObj = (B_ALGORITHM_OBJ)NULL_PTR;
	B_ALGORITHM_OBJ           randomAlgorithm = NULL_PTR;
	A_DSA_PRIVATE_KEY        *privateKeyInfo = (A_DSA_PRIVATE_KEY *)NULL_PTR;
	A_DSA_PUBLIC_KEY         *publicKeyInfo = (A_DSA_PUBLIC_KEY *)NULL_PTR;
	A_DSA_PARAMS              dsaParamInfo;
	B_KEY_OBJ                 publicKeyObj = (B_KEY_OBJ)NULL_PTR;
	B_KEY_OBJ                 privateKeyObj = (B_KEY_OBJ)NULL_PTR;
	int status;

	/*  Allocate space for key structure. */
	arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
	if (arena == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}
	privateKey = (DSAPrivateKey *)
	              PORT_ArenaZAlloc(arena, sizeof(DSAPrivateKey));
	if (privateKey == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}
	privateKey->params.arena = arena;

	if ((status = B_CreateAlgorithmObject(&dsaKeyGenObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateKeyObject(&publicKeyObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateKeyObject(&privateKeyObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	randomAlgorithm = generateRandomAlgorithm(DSA_SUBPRIME_LEN, seed);
	if (randomAlgorithm == NULL_PTR) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	ITEMFROMSECITEM(dsaParamInfo.prime,    params->prime);
	ITEMFROMSECITEM(dsaParamInfo.subPrime, params->subPrime);
	ITEMFROMSECITEM(dsaParamInfo.base,     params->base);

	if ((status = B_SetAlgorithmInfo(dsaKeyGenObj, AI_DSAKeyGen, 
	                                 (POINTER)&dsaParamInfo)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_GenerateInit(dsaKeyGenObj, dsa_pk_gen_chooser,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_GenerateKeypair(dsaKeyGenObj, publicKeyObj, privateKeyObj,
	                                randomAlgorithm,
	                                (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_GetKeyInfo((POINTER *)&privateKeyInfo, privateKeyObj,
	                           KI_DSAPrivate)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_GetKeyInfo((POINTER *)&publicKeyInfo, publicKeyObj,
	                           KI_DSAPublic)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = dsaConvertKeyInfoToBLKey(privateKeyInfo, publicKeyInfo,
	                                       privateKey)) != 0) {
		goto loser;
	}

	B_DestroyAlgorithmObject(&dsaKeyGenObj);
	B_DestroyAlgorithmObject(&randomAlgorithm);
	B_DestroyKeyObject(&publicKeyObj);
	B_DestroyKeyObject(&privateKeyObj);
	dsaZFreePrivateKeyInfo(privateKeyInfo);
	dsaZFreePublicKeyInfo(publicKeyInfo);
	/* dsaParamInfo contains only public info, no need to ZFree */

	*privKey = privateKey;
	return SECSuccess;

loser:
	if (dsaKeyGenObj != NULL_PTR)
		B_DestroyAlgorithmObject(&dsaKeyGenObj);
	if (randomAlgorithm != NULL_PTR)
		B_DestroyAlgorithmObject(&randomAlgorithm);
	if (privateKeyObj != NULL_PTR)
		B_DestroyKeyObject(&privateKeyObj);
	if (publicKeyObj != NULL_PTR)
		B_DestroyKeyObject(&publicKeyObj);
	if (privateKeyInfo != (A_DSA_PRIVATE_KEY *)NULL_PTR)
		dsaZFreePrivateKeyInfo(privateKeyInfo);
	if (publicKeyInfo != (A_DSA_PUBLIC_KEY *)NULL_PTR)
		dsaZFreePublicKeyInfo(publicKeyInfo);
	if (arena != NULL)
		PORT_FreeArena(arena, PR_TRUE);
	*privKey = NULL;
	return SECFailure;
}

SECStatus 
DSA_SignDigestWithSeed(DSAPrivateKey * key,
                       SECItem *       signature,
                       SECItem *       digest,
                       unsigned char * seed)
{
	B_ALGORITHM_OBJ           dsaSigner = (B_ALGORITHM_OBJ)NULL_PTR;
	B_ALGORITHM_OBJ           randomAlgorithm = NULL_PTR;
	B_KEY_OBJ                 privateKeyObj = (B_KEY_OBJ)NULL_PTR;
	A_DSA_PRIVATE_KEY         privateKeyInfo;
	const B_ALGORITHM_METHOD *dsa_sign_chooser[] = {
	                            &AM_DSA_SIGN,
	                            (B_ALGORITHM_METHOD *)NULL_PTR
	};
	int status;
	unsigned int siglen;

	randomAlgorithm = generateRandomAlgorithm(DSA_SUBPRIME_LEN, seed);

	if ((status = B_CreateAlgorithmObject(&dsaSigner)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateKeyObject(&privateKeyObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = dsaConvertBLKeyToPrKeyInfo(key, &privateKeyInfo)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SetKeyInfo(privateKeyObj, KI_DSAPrivate, 
	                           (POINTER)&privateKeyInfo)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SetAlgorithmInfo(dsaSigner, AI_DSA, NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SignInit(dsaSigner, privateKeyObj, dsa_sign_chooser,
	                         (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SignUpdate(dsaSigner, digest->data, digest->len,
	                           (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_SignFinal(dsaSigner, signature->data, &siglen,
	                          signature->len, randomAlgorithm,
	                          (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	SECITEM_ReallocItem(NULL, signature, signature->len, siglen);
	signature->len = siglen; /* shouldn't realloc do this? */

	B_DestroyAlgorithmObject(&dsaSigner);
	B_DestroyKeyObject(&privateKeyObj);
	B_DestroyAlgorithmObject(&randomAlgorithm);
	/*  privateKeyInfo is shallow copy */
	return SECSuccess;

loser:
	if (dsaSigner != NULL_PTR)
		B_DestroyAlgorithmObject(&dsaSigner);
	if (privateKeyObj != NULL_PTR)
		B_DestroyKeyObject(&privateKeyObj);
	if (randomAlgorithm != NULL_PTR)
		B_DestroyAlgorithmObject(&randomAlgorithm);
	return SECFailure;
}

SECStatus
PQG_ParamGen(unsigned int j, 	   /* input : determines length of P. */
             PQGParams **pParams,  /* output: P Q and G returned here */
             PQGVerify **pVfy)     /* output: counter and seed. */
{
	return PQG_ParamGenSeedLen(j, DSA_SUBPRIME_LEN, pParams, pVfy);
}

SECStatus
PQG_ParamGenSeedLen(
                  unsigned int j,         /* input : determines length of P. */
                  unsigned int seedBytes, /* input : length of seed in bytes.*/
                  PQGParams **pParams,    /* output: P Q and G returned here */
                  PQGVerify **pVfy)       /* output: counter and seed. */
{
	B_DSA_PARAM_GEN_PARAMS    dsaParams;
	B_ALGORITHM_OBJ           dsaKeyGenObj = (B_ALGORITHM_OBJ)NULL_PTR;
	B_ALGORITHM_OBJ           dsaParamGenerator = (B_ALGORITHM_OBJ)NULL_PTR;
	B_ALGORITHM_OBJ           randomAlgorithm = NULL_PTR;
	A_DSA_PARAMS             *dsaParamInfo;
	SECItem tmp;
	PQGParams *params;
	PRArenaPool *arena;
	int status;

	if (!pParams || j > 8) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}

	/*  Allocate space for key structure. */
	arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
	if (arena == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}
	params = (PQGParams *)PORT_ArenaZAlloc(arena, sizeof(PQGParams));
	if (params == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}
	params->arena = arena;

	if ((status = B_CreateAlgorithmObject(&dsaParamGenerator)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	if ((status = B_CreateAlgorithmObject(&dsaKeyGenObj)) != 0) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		goto loser;
	}

	randomAlgorithm = generateRandomAlgorithm(seedBytes, NULL);

	dsaParams.primeBits = 512 + (j * 64);
	if ((status = B_SetAlgorithmInfo(dsaParamGenerator, AI_DSAParamGen,
	                                 (POINTER)&dsaParams)) != 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		goto loser;
	}

	if ((status = B_GenerateInit(dsaParamGenerator, dsa_pk_gen_chooser,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_GenerateParameters(dsaParamGenerator, dsaKeyGenObj,
	                                   randomAlgorithm,
	                                   (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	if ((status = B_GetAlgorithmInfo((POINTER *)&dsaParamInfo, dsaKeyGenObj,
	                                 AI_DSAKeyGen)) != 0) {
		PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
		goto loser;
	}

	SECITEMFROMITEM(arena, params->prime,    dsaParamInfo->prime);
	SECITEMFROMITEM(arena, params->subPrime, dsaParamInfo->subPrime);
	SECITEMFROMITEM(arena, params->base,     dsaParamInfo->base);

	B_DestroyAlgorithmObject(&dsaKeyGenObj);
	B_DestroyAlgorithmObject(&dsaParamGenerator);
	B_DestroyAlgorithmObject(&randomAlgorithm);
	dsaZFreeKeyInfoParams(dsaParamInfo);

	*pParams = params;
	return SECSuccess;

loser:
	if (dsaParamGenerator != NULL_PTR)
		B_DestroyAlgorithmObject(&dsaParamGenerator);
	if (dsaKeyGenObj != NULL_PTR)
		B_DestroyAlgorithmObject(&dsaKeyGenObj);
	if (randomAlgorithm != NULL_PTR)
		B_DestroyAlgorithmObject(&randomAlgorithm);
	if (dsaParamInfo != NULL)
		dsaZFreeKeyInfoParams(dsaParamInfo);
	if (arena != NULL)
		PORT_FreeArena(arena, PR_TRUE);
	*pParams = NULL;
	return SECFailure;
}

SECStatus
PQG_VerifyParams(const PQGParams *params, 
                 const PQGVerify *vfy, SECStatus *result)
{
	/*  BSAFE does not provide access to h.
	 *  Verification is thus skipped.
	 */
	PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
	return SECFailure;
}

/*  Destroy functions are implemented in util/pqgutil.c */

/*****************************************************************************
** BLAPI implementation of RNG
******************************************************************************/

static SECItem globalseed;
static B_ALGORITHM_OBJ globalrng = NULL_PTR;

SECStatus 
RNG_RNGInit(void)
{
	int status;
	PRInt32 nBytes;
	if (globalrng == NULL) {
		globalseed.len = 20;
		globalseed.data = (unsigned char *)PORT_Alloc(globalseed.len);
	} else {
		B_DestroyAlgorithmObject(&globalrng);
	}
	nBytes = RNG_GetNoise(globalseed.data, globalseed.len);
	globalrng = generateRandomAlgorithm(globalseed.len, globalseed.data);
	if (globalrng == NULL_PTR) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		return SECFailure;
	}
	return SECSuccess;
}

SECStatus 
RNG_RandomUpdate(void *data, size_t bytes)
{
	int status;
	if (data == NULL || bytes <= 0) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if (globalrng == NULL_PTR) {
		PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
		return SECFailure;
	}
	if ((status = B_RandomUpdate(globalrng, data, bytes,
	                             (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	return SECSuccess;
}

SECStatus 
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
	int status;
	if (dest == NULL) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return SECFailure;
	}
	if (globalrng == NULL_PTR) {
		PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
		return SECFailure;
	}
	if ((status = B_GenerateRandomBytes(globalrng, dest, len,
	                                    (A_SURRENDER_CTX *)NULL_PTR)) != 0) {
		PORT_SetError(SEC_ERROR_BAD_DATA);
		return SECFailure;
	}
	return SECSuccess;
}

void 
RNG_RNGShutdown(void)
{
	if (globalrng == NULL_PTR)
		/* no-op */
		return;
	B_DestroyAlgorithmObject(&globalrng);
	SECITEM_ZfreeItem(&globalseed, PR_FALSE);
	globalrng = NULL_PTR;
}
