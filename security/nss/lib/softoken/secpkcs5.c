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

#include "plarena.h"

#include "seccomon.h"
#include "secitem.h"
#include "secport.h"
#include "hasht.h"
#include "pkcs11t.h"
#include "blapi.h"
#include "sechash.h"
#include "secasn1.h"
#include "secder.h"
#include "secpkcs5.h"
#include "secoid.h"
#include "alghmac.h"
#include "softoken.h"
#include "secerr.h"

#define DES_IV_LENGTH		8
#define RC2_IV_LENGTH		8
#define MD2_LENGTH		16
#define MD5_LENGTH		16
#define SHA1_LENGTH		20
#define SEED_LENGTH 		16
#define SALT_LENGTH		8
#define PBE_SALT_LENGTH		16

/* template for PKCS 5 PBE Parameter.  This template has been expanded
 * based upon the additions in PKCS 12.  This should eventually be moved
 * if RSA updates PKCS 5.
 */
const SEC_ASN1Template SEC_PKCS5PBEParameterTemplate[] =
{
    { SEC_ASN1_SEQUENCE, 
	0, NULL, sizeof(SEC_PKCS5PBEParameter) },
    { SEC_ASN1_OCTET_STRING, 
	offsetof(SEC_PKCS5PBEParameter, salt) },
    { SEC_ASN1_INTEGER,
	offsetof(SEC_PKCS5PBEParameter, iteration) },
    { 0 }
};

const SEC_ASN1Template SEC_V2PKCS12PBEParameterTemplate[] =
{   
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS5PBEParameter) },
    { SEC_ASN1_OCTET_STRING, offsetof(SEC_PKCS5PBEParameter, salt) },
    { SEC_ASN1_INTEGER, offsetof(SEC_PKCS5PBEParameter, iteration) },
    { 0 }
};

pbeBitGenParameters pbeHashAlgorithmParams[] = {
    {   0,   0, SEC_OID_UNKNOWN },
    { 128, 512, SEC_OID_MD2 },
    { 128, 512, SEC_OID_MD5 },
    { 160, 512, SEC_OID_SHA1 },
};

/* generate some random bytes.  this is used to generate the 
 * salt if it is not specified.
 */
static SECStatus 
sec_pkcs5_generate_random_bytes(PRArenaPool *poolp, 
				SECItem *dest, int len)
{
    SECStatus rv = SECFailure;

    if(dest != NULL)
    {
	void *mark = PORT_ArenaMark(poolp);
	dest->data = (unsigned char *)PORT_ArenaZAlloc(poolp, len);
	if(dest->data != NULL)
	{
	    dest->len = len;
	    RNG_GenerateGlobalRandomBytes(dest->data, dest->len);
	    PORT_ArenaUnmark(poolp, mark);
	    rv = SECSuccess;
	} else
	    PORT_ArenaRelease(poolp, mark);
    }

    return rv;
}

/* maps hash algorithm from PBE algorithm.
 */
static SECOidTag 
sec_pkcs5_hash_algorithm(SECOidTag algorithm)
{
    switch(algorithm)
    {
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	    return SEC_OID_SHA1;
	case SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC:
	    return SEC_OID_MD5;
	case SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC:
	    return SEC_OID_MD2;
	default:
	    break;
    }
    return SEC_OID_UNKNOWN;
}

/* get the iv length needed for the PBE algorithm 
 */
static int 
sec_pkcs5_iv_length(SECOidTag algorithm)
{
    switch(algorithm)
    {
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
	    return DES_IV_LENGTH;
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	    return RC2_IV_LENGTH;
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
	    return 0;
	default:
	    break;
    }
    return -1;
}

/* get the key length needed for the PBE algorithm
 */
static int 
sec_pkcs5_key_length(SECOidTag algorithm)
{
    switch(algorithm)
    {
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
	    return 24;
	case SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC:
	    return 8;
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	    return 5;
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
	    return 16;
	default:
	    break;
    }
    return -1;
}

/* the V2 algorithms only encode the salt, there is no iteration
 * count so we need a check for V2 algorithm parameters.
 */
static PRBool
sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(SECOidTag algorithm)
{
    switch(algorithm) 
    {
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	    return PR_TRUE;
	default:
	    break;
    }

    return PR_FALSE;
}

/* creates a PBE parameter based on the PBE algorithm.  the only required
 * parameters are algorithm and interation.  the return is a PBE parameter
 * which conforms to PKCS 5 parameter unless an extended parameter is needed.
 * this is primarily if keyLen and a variable key length algorithm are
 * specified.
 *   salt -  if null, a salt will be generated from random bytes.
 *   iteration - number of iterations to perform hashing.
 *   keyLen - only used in variable key length algorithms
 *   iv - if null, the IV will be generated based on PKCS 5 when needed.
 *   params - optional, currently unsupported additional parameters.
 * once a parameter is allocated, it should be destroyed calling 
 * sec_pkcs5_destroy_pbe_parameter or SEC_PKCS5DestroyPBEParameter.
 */
static SEC_PKCS5PBEParameter *
sec_pkcs5_create_pbe_parameter(SECOidTag algorithm, 
			SECItem *salt, 
			int iteration)
{
    PRArenaPool *poolp = NULL;
    SEC_PKCS5PBEParameter *pbe_param = NULL;
    SECStatus rv; 
    void *dummy = NULL;

    if(iteration < 0) {
	return NULL;
    }

    poolp = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(poolp == NULL)
	return NULL;

    pbe_param = (SEC_PKCS5PBEParameter *)PORT_ArenaZAlloc(poolp,
	sizeof(SEC_PKCS5PBEParameter));
    if(!pbe_param) {
	PORT_FreeArena(poolp, PR_TRUE);
	return NULL;
    }

    pbe_param->poolp = poolp;
    pbe_param->algorithm = algorithm;

    /* should we generate the salt? */
    if(!salt || !salt->data) {
	rv = sec_pkcs5_generate_random_bytes(poolp, &pbe_param->salt, 
						SALT_LENGTH);
    } else {
	rv = SECITEM_CopyItem(poolp, &pbe_param->salt, salt);
    }

    if(rv != SECSuccess) {
	PORT_FreeArena(poolp, PR_TRUE);
	return NULL;
    }

    /* encode the integer */
    dummy = SEC_ASN1EncodeInteger(poolp, &pbe_param->iteration, 
		iteration);
    rv = (dummy) ? SECSuccess : SECFailure;

    if(rv != SECSuccess) {
	PORT_FreeArena(poolp, PR_FALSE);
	return NULL;
    }

    return pbe_param;
}

/* generate bits for key and iv using MD5 hashing 
 */
static SECItem *
sec_pkcs5_compute_md5_hash(SECItem *salt, SECItem *pwd, int iter,
			   PRBool dummy)
{
    SECItem *hash = NULL, *pre_hash = NULL;
    SECStatus rv = SECFailure;

    if((salt == NULL) || (pwd == NULL) || (iter < 0)) {
	return NULL;
    }
	
    hash = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    pre_hash = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if((hash != NULL) && (pre_hash != NULL)) {
	unsigned int i, ph_len;

	ph_len = MD5_LENGTH;
	if(ph_len < (salt->len + pwd->len)) {
	    ph_len = salt->len + pwd->len;
	}

	rv = SECFailure;
	hash->data = (unsigned char *)PORT_ZAlloc(MD5_LENGTH);
	hash->len = MD5_LENGTH;
	pre_hash->data = (unsigned char *)PORT_ZAlloc(ph_len);
	pre_hash->len = salt->len + pwd->len;

	if((hash->data != NULL) && (pre_hash->data != NULL)) {
	    rv = SECSuccess;
	    /* handle 0 length password */
	    if(pwd->len > 0) {
		PORT_Memcpy(pre_hash->data, pwd->data, pwd->len);
	    }
	    if(salt->len > 0) {
	 	PORT_Memcpy((pre_hash->data+pwd->len), salt->data, salt->len);
	    }
	    for(i = 0; ((i < (unsigned int)iter) && (rv == SECSuccess)); i++) {
		rv = MD5_HashBuf(hash->data, pre_hash->data, pre_hash->len);
		if(rv != SECFailure) {
		    PORT_Memcpy(pre_hash->data, hash->data, MD5_LENGTH);
		    pre_hash->len = MD5_LENGTH;
		}
	    }
	}
    }

    if(pre_hash != NULL)
	SECITEM_FreeItem(pre_hash, PR_TRUE);

    if((rv == SECFailure) && (hash)) {
	SECITEM_FreeItem(hash, PR_TRUE);
	hash = NULL;
    }

    return hash;
}

/* generate bits for key and iv using MD2 hashing 
 */
static SECItem *
sec_pkcs5_compute_md2_hash(SECItem *salt, SECItem *pwd, int iter,
			   PRBool dummy)
{
    SECItem *hash = NULL, *pre_hash = NULL;
    SECStatus rv = SECFailure;

    if((salt == NULL) || (pwd == NULL) || (iter < 0))
	return NULL;

    hash = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    pre_hash = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if((hash != NULL) && (pre_hash != NULL))
    {
	int i, ph_len;

	ph_len = MD2_LENGTH;
	if((salt->len + pwd->len) > MD2_LENGTH)
	    ph_len = salt->len+pwd->len;

	rv = SECFailure;
	hash->data = (unsigned char *)PORT_ZAlloc(MD2_LENGTH);
	hash->len = MD2_LENGTH;
	pre_hash->data = (unsigned char *)PORT_ZAlloc(ph_len);
	pre_hash->len = salt->len + pwd->len;

	if((hash->data != NULL) && (pre_hash->data != NULL))
	{
	    MD2Context *ctxt;

	    rv = SECSuccess;
	    if(pwd->len > 0) {
		PORT_Memcpy(pre_hash->data, pwd->data, pwd->len);
	    }
	    if(salt->len > 0) {
		PORT_Memcpy((pre_hash->data+pwd->len), salt->data, salt->len);
	    }

	    for(i = 0; ((i < iter) && (rv == SECSuccess)); i++)
	    {
		ctxt = MD2_NewContext();
		if(ctxt == NULL)
		    rv = SECFailure;		
		else
		{
		    MD2_Update(ctxt, pre_hash->data, pre_hash->len);
		    MD2_End(ctxt, hash->data, &hash->len, hash->len);
		    PORT_Memcpy(pre_hash->data, hash->data, MD2_LENGTH);
		    pre_hash->len = MD2_LENGTH;
		    MD2_DestroyContext(ctxt, PR_TRUE);
		}
	    }		
	}
    }

    if(pre_hash != NULL)
	SECITEM_FreeItem(pre_hash, PR_TRUE);

    if(rv != SECSuccess)
	if(hash != NULL)
	{
	    SECITEM_FreeItem(hash, PR_TRUE);
	    hash = NULL;
	}

    return hash;
}

/* generate bits using SHA1 hash 
 */
static SECItem *
sec_pkcs5_compute_sha1_hash(SECItem *salt, SECItem *pwd, int iter, 
			    PRBool faulty3DES) 
{
    SECItem *hash = NULL, *pre_hash = NULL;
    SECStatus rv = SECFailure;

    if((salt == NULL) || (pwd == NULL) || (iter < 0)) {
	return NULL;
    }
	
    hash = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    pre_hash = (SECItem *)PORT_ZAlloc(sizeof(SECItem));

    if((hash != NULL) && (pre_hash != NULL)) {
	int i, ph_len;

	ph_len = SHA1_LENGTH;
	if((salt->len + pwd->len) > SHA1_LENGTH) {
	    ph_len = salt->len + pwd->len;
	}

	rv = SECFailure;

	/* allocate buffers */
	hash->data = (unsigned char *)PORT_ZAlloc(SHA1_LENGTH);
	hash->len = SHA1_LENGTH;
	pre_hash->data = (unsigned char *)PORT_ZAlloc(ph_len);

	/* in pbeSHA1TripleDESCBC there was an allocation error that made
	 * it into the caller.  We do not want to propagate those errors
	 * further, so we are doing it correctly, but reading the old method.
	 */
	if(faulty3DES) {
	    pre_hash->len = ph_len;
	} else {
	    pre_hash->len = salt->len + pwd->len;
	}

	/* preform hash */
	if((hash->data != NULL) && (pre_hash->data != NULL)) {
	    rv = SECSuccess;
	    /* check for 0 length password */
	    if(pwd->len > 0) {
		PORT_Memcpy(pre_hash->data, pwd->data, pwd->len);
	    }
	    if(salt->len > 0) {
		PORT_Memcpy((pre_hash->data+pwd->len), salt->data, salt->len);
	    }
	    for(i = 0; ((i < iter) && (rv == SECSuccess)); i++) {
		rv = SHA1_HashBuf(hash->data, pre_hash->data, pre_hash->len);
		if(rv != SECFailure) {
		    pre_hash->len = SHA1_LENGTH;
		    PORT_Memcpy(pre_hash->data, hash->data, SHA1_LENGTH);
		}
	    }
	}
    }

    if(pre_hash != NULL) {
	SECITEM_FreeItem(pre_hash, PR_TRUE);
    }

    if((rv != SECSuccess) && (hash != NULL)) {
	SECITEM_FreeItem(hash, PR_TRUE);
	hash = NULL;
    }

    return hash;
}

/* bit generation/key and iv generation routines. */
typedef SECItem *(* sec_pkcs5_hash_func)(SECItem *s, SECItem *p, 
					 int iter, PRBool faulty3DES);

/* generates bits needed for the key and iv based on PKCS 5,
 * be concatenating the password and salt and using the appropriate
 * hash algorithm.  This function serves as a front end to the 
 * specific hash functions above.  a return of NULL indicates an
 * error.
 */
static SECItem *
sec_pkcs5_compute_hash(SEC_PKCS5PBEParameter *pbe_param, SECItem *pwitem,
		       PRBool faulty3DES)
{
    sec_pkcs5_hash_func hash_func;
    SECOidTag hash_alg;
    SECItem *hash = NULL;
    SECItem *salt = NULL;
	
    hash_alg = sec_pkcs5_hash_algorithm(pbe_param->algorithm);
    salt = &(pbe_param->salt);
    switch(hash_alg)
    {
	case SEC_OID_SHA1:
	    hash_func = sec_pkcs5_compute_sha1_hash;
	    break;
	case SEC_OID_MD2:
	    hash_func = sec_pkcs5_compute_md2_hash;
	    break;
	case SEC_OID_MD5:
	    hash_func = sec_pkcs5_compute_md5_hash;
	    break;
	default:
	    hash_func = NULL;
    }

    if(hash_func) {
	hash = (* hash_func)(salt, pwitem, pbe_param->iter, faulty3DES);
    }

    return hash;
}

/* determines the number of bits needed for key and iv generation
 * based upon the algorithm identifier.  if a number of
 * bits greater than the hash algorithm can produce are needed,
 * the bits will be generated based upon the extended PKCS 5 
 * described in PKCS 12.
 *
 * a return of -1 indicates an error.
 */
static int 
sec_pkcs5_bits_needed(SEC_PKCS5PBEParameter *pbe_param)
{
    int iv_bits;
    int key_bits;
    
    if(pbe_param == NULL) {
	return -1;
    }

    iv_bits = sec_pkcs5_iv_length(pbe_param->algorithm) * 8;
    key_bits = sec_pkcs5_key_length(pbe_param->algorithm) * 8;

    if(key_bits != 0) {
	return iv_bits + key_bits;
    }

    return -1;
}

/* determines the number of bits generated by each hash algorithm.
 * in case of an error, -1 is returned. 
 */
static int
sec_pkcs5_hash_bits_generated(SEC_PKCS5PBEParameter *pbe_param)
{
    if(pbe_param == NULL) {
	return -1;
    }

    switch(sec_pkcs5_hash_algorithm(pbe_param->algorithm)) {
	case SEC_OID_SHA1:
	    return SHA1_LENGTH * 8;
	case SEC_OID_MD2:
	    return MD2_LENGTH * 8;
	case SEC_OID_MD5:
	    return MD5_LENGTH * 8;
	default:
	    break;
    }

    return -1;
}

/* this bit generation routine is described in PKCS 12 and the proposed
 * extensions to PKCS 5.  an initial hash is generated following the
 * instructions laid out in PKCS 5.  If the number of bits generated is
 * insufficient, then the method discussed in the proposed extensions to
 * PKCS 5 in PKCS 12 are used.  This extension makes use of the HMAC
 * function.  And the P_Hash function from the TLS standard.
 */
static SECItem *
sec_pkcs5_bit_generator(SEC_PKCS5PBEParameter *pbe_param,
			SECItem *init_hash, 
			unsigned int bits_needed)
{
    SECItem *ret_bits = NULL;
    int hash_size = 0;
    unsigned int i;
    unsigned int hash_iter;
    unsigned int dig_len;
    SECStatus rv = SECFailure;
    unsigned char *state = NULL;
    unsigned int state_len;
    HMACContext *cx = NULL;

    hash_size = sec_pkcs5_hash_bits_generated(pbe_param);
    if(hash_size == -1)
	return NULL;

    hash_iter = (bits_needed + (unsigned int)hash_size - 1) / hash_size;
    hash_size /= 8;

    /* allocate return buffer */
    ret_bits = (SECItem  *)PORT_ZAlloc(sizeof(SECItem));
    if(ret_bits == NULL)
	return NULL;
    ret_bits->data = (unsigned char *)PORT_ZAlloc((hash_iter * hash_size) + 1);
    ret_bits->len = (hash_iter * hash_size);
    if(ret_bits->data == NULL) {
	PORT_Free(ret_bits);
	return NULL;
    }

    /* allocate intermediate hash buffer.  8 is for the 8 bytes of
     * data which are added based on iteration number 
     */

    if ((unsigned int)hash_size > pbe_param->salt.len) {
	state_len = hash_size;
    } else {
	state_len = pbe_param->salt.len;
    }
    state = (unsigned char *)PORT_ZAlloc(state_len);
    if(state == NULL) {
	rv = SECFailure;
	goto loser;
    }
    if(pbe_param->salt.len > 0) {
	PORT_Memcpy(state, pbe_param->salt.data, pbe_param->salt.len);
    }

    cx = HMAC_Create(sec_pkcs5_hash_algorithm(pbe_param->algorithm),
		     init_hash->data, init_hash->len);
    if (cx == NULL) {
	rv = SECFailure;
	goto loser;
    }

    for(i = 0; i < hash_iter; i++) { 

	/* generate output bits */
	HMAC_Begin(cx);
	HMAC_Update(cx, state, state_len);
	HMAC_Update(cx, pbe_param->salt.data, pbe_param->salt.len);
	rv = HMAC_Finish(cx, ret_bits->data + (i * hash_size),
			 &dig_len, hash_size);
	if (rv != SECSuccess)
	    goto loser;
	PORT_Assert((unsigned int)hash_size == dig_len);

	/* generate new state */
	HMAC_Begin(cx);
	HMAC_Update(cx, state, state_len);
	rv = HMAC_Finish(cx, state, &state_len, state_len);
	if (rv != SECSuccess)
	    goto loser;
	PORT_Assert(state_len == dig_len);
    }

loser:
    if (state != NULL)
	PORT_ZFree(state, state_len);
    HMAC_Destroy(cx);

    if(rv != SECSuccess) {
	SECITEM_ZfreeItem(ret_bits, PR_TRUE);
	ret_bits = NULL;
    }

    return ret_bits;
}

/* generate bits for the key and iv determination.  if enough bits
 * are not generated using PKCS 5, then we need to generate more bits
 * based on the extension proposed in PKCS 12
 */
static SECItem *
sec_pkcs5_generate_bits(SEC_PKCS5PBEParameter *pbe_param, SECItem *pwitem, 
			PRBool faulty3DES)
{
    SECItem * hash 		= NULL;
    SECItem * newHash 		= NULL;
    int       bits_needed;
    int       bits_available;
    
    bits_needed = sec_pkcs5_bits_needed(pbe_param);
    bits_available = sec_pkcs5_hash_bits_generated(pbe_param);

    if((bits_needed == -1) || (bits_available == -1)) {
	return NULL;
    }

    hash = sec_pkcs5_compute_hash(pbe_param, pwitem, faulty3DES);
    if(hash == NULL) {
	return NULL;
    }

    if(bits_needed <= bits_available) {
	return hash;
    } 

    newHash = sec_pkcs5_bit_generator(pbe_param, hash, bits_needed);
    if (hash != newHash)
	SECITEM_FreeItem(hash, PR_TRUE);
    return newHash;
}

/* compute the IV as per PKCS 5
 */
static SECItem *
sec_pkcs5_compute_iv(SEC_PKCS5PBEParameter *pbe_param, SECItem *pwitem,
		     PRBool faulty3DES)
{
    SECItem *hash = NULL, *iv = NULL;

    if((pbe_param == NULL) || (pwitem == NULL)) {
	return NULL;
    }

    /* generate iv */
    iv = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(!iv) {
	return NULL;
    }

    iv->len = sec_pkcs5_iv_length(pbe_param->algorithm);
    if(iv->len == -1) {
	PORT_Free(iv);
	return NULL;
    }

    iv->data = (unsigned char *)PORT_ZAlloc(iv->len);
    if(iv->data == NULL) {
	PORT_Free(iv);
	return NULL;
    }

    if(sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(pbe_param->algorithm)) {
	SECOidTag hashAlg;
	PBEBitGenContext *ctxt;
	hashAlg = sec_pkcs5_hash_algorithm(pbe_param->algorithm);
	ctxt = PBE_CreateContext(hashAlg, pbeBitGenCipherIV,
				 pwitem, &pbe_param->salt,
				 iv->len * 8, pbe_param->iter);
	if(!ctxt) {
	    SECITEM_FreeItem(iv, PR_TRUE);
	    return NULL;
	}

	hash = PBE_GenerateBits(ctxt);
	PBE_DestroyContext(ctxt);
    } else {
	hash = sec_pkcs5_generate_bits(pbe_param, pwitem, faulty3DES);
    }

    if(!hash) {
	SECITEM_FreeItem(iv, PR_TRUE);
	return NULL;
    }

    PORT_Memcpy(iv->data, (hash->data+(hash->len - iv->len)), iv->len);
    SECITEM_FreeItem(hash, PR_TRUE);

    return iv;
}

/* generate key as per PKCS 5
 */
static SECItem *
sec_pkcs5_compute_key(SEC_PKCS5PBEParameter *pbe_param, SECItem *pwitem,
		      PRBool faulty3DES)
{
    SECItem *hash = NULL, *key = NULL;

    if((pbe_param == NULL) || (pwitem == NULL)) {
	return NULL;
    }

    key = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(!key) {
	return NULL;
    }

    key->len = sec_pkcs5_key_length(pbe_param->algorithm);
    if(key->len == -1) {
	PORT_Free(key);
	return NULL;
    }
	
    key->data = (unsigned char *)PORT_ZAlloc(sizeof(unsigned char) *
					     key->len);
    if(!key->data) {
	PORT_Free(key);
	return NULL;
    }


    if(sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(pbe_param->algorithm)) {
	SECOidTag hashAlg;
	PBEBitGenContext *ctxt;
	hashAlg = sec_pkcs5_hash_algorithm(pbe_param->algorithm);
	ctxt = PBE_CreateContext(hashAlg, pbeBitGenCipherKey,
				 pwitem, &pbe_param->salt,
				 key->len * 8, pbe_param->iter);
	if(!ctxt) {
	    SECITEM_FreeItem(key, PR_TRUE);
	    return NULL;
	}

	hash = PBE_GenerateBits(ctxt);
	PBE_DestroyContext(ctxt);
    } else {
	hash = sec_pkcs5_generate_bits(pbe_param, pwitem, faulty3DES);
    }
   
    if(!hash) {
	SECITEM_FreeItem(key, PR_TRUE);
	return NULL;
    }

    if(pbe_param->algorithm ==
		   SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC) {
	PORT_Memcpy(key->data, hash->data, (key->len * 2) / 3);
	PORT_Memcpy(&(key->data[(key->len  * 2) / 3]), key->data,
		    key->len / 3);
    } else {
	PORT_Memcpy(key->data, hash->data, key->len);
    }

    SECITEM_FreeItem(hash, PR_TRUE);
    return key;
}

/* decode the algid and generate a PKCS 5 parameter from it
 */
static SEC_PKCS5PBEParameter *
sec_pkcs5_convert_algid(SECAlgorithmID *algid)
{
    PRArenaPool *poolp;
    SEC_PKCS5PBEParameter *pbe_param = NULL;
    SECOidTag algorithm;
    SECStatus rv = SECFailure;

    if(algid == NULL)
	return NULL;

    algorithm = SECOID_GetAlgorithmTag(algid);
   
    if(sec_pkcs5_hash_algorithm(algorithm) == SEC_OID_UNKNOWN)
	return NULL;

    poolp = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(poolp == NULL)
	return NULL;

    /* allocate memory for the parameter */
    pbe_param = (SEC_PKCS5PBEParameter *)PORT_ArenaZAlloc(poolp, 
	sizeof(SEC_PKCS5PBEParameter));

    /* decode parameter */
    if(pbe_param && !sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(algorithm)) {
	pbe_param->poolp = poolp;
	rv = SEC_ASN1DecodeItem(poolp, pbe_param, 
	    SEC_PKCS5PBEParameterTemplate, &algid->parameters);
	if(rv != SECSuccess) {
	    goto loser;
	}
	pbe_param->algorithm = algorithm;
	pbe_param->iter = DER_GetInteger(&pbe_param->iteration);
    } else if(sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(algorithm)) {
	pbe_param->algorithm = algorithm;
	pbe_param->poolp = poolp;
	rv = SEC_ASN1DecodeItem(poolp, pbe_param, SEC_V2PKCS12PBEParameterTemplate,
				&algid->parameters);
	if(rv != SECSuccess) {
	    goto loser;
	}
	pbe_param->iter = DER_GetInteger(&pbe_param->iteration);
    }

loser:
    if((pbe_param == NULL) || (rv != SECSuccess)) {
	PORT_FreeArena(poolp, PR_TRUE);
	pbe_param = NULL;
    }

    return pbe_param;
}

/* destroy a pbe parameter.  it assumes that the parameter was 
 * generated using the appropriate create function and therefor
 * contains an arena pool.
 */
static void 
sec_pkcs5_destroy_pbe_param(SEC_PKCS5PBEParameter *pbe_param)
{
    if(pbe_param != NULL)
	PORT_FreeArena(pbe_param->poolp, PR_TRUE);
}


/* crypto routines */

/* function pointer template for crypto functions */
typedef SECItem *(* pkcs5_crypto_func)(SECItem *key, SECItem *iv, 
					 SECItem *src, PRBool op1, PRBool op2);

/* map PBE algorithm to crypto algorithm */
static SECOidTag 
sec_pkcs5_encryption_algorithm(SECOidTag algorithm)
{
    switch(algorithm)
    {
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC:
	    return SEC_OID_DES_EDE3_CBC;
	case SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC:
	    return SEC_OID_DES_CBC;
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	    return SEC_OID_RC2_CBC;
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
	    return SEC_OID_RC4;
	default:
	    break;
    }
    return SEC_OID_UNKNOWN;
}

/* perform DES encryption and decryption.  these routines are called
 * by SEC_PKCS5CipherData.  In the case of an error, NULL is returned.
 */
static SECItem *
sec_pkcs5_des(SECItem *key, 
	      SECItem *iv, 
	      SECItem *src,
	      PRBool triple_des, 
	      PRBool encrypt)
{
    SECItem *dest;
    SECItem *dup_src;
    SECStatus rv = SECFailure;
    int pad;

    if((src == NULL) || (key == NULL) || (iv == NULL))
	return NULL;

    dup_src = SECITEM_DupItem(src);
    if(dup_src == NULL) {
	return NULL;
    }

    if(encrypt != PR_FALSE) {
	void *dummy;

	dummy = DES_PadBuffer(NULL, dup_src->data, 
	    dup_src->len, &dup_src->len);
	if(dummy == NULL) {
	    SECITEM_FreeItem(dup_src, PR_TRUE);
	    return NULL;
	}
	dup_src->data = (unsigned char*)dummy;
    }

    dest = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(dest != NULL) {
	/* allocate with over flow */
	dest->data = (unsigned char *)PORT_ZAlloc(dup_src->len + 64);
	if(dest->data != NULL) {
	    DESContext *ctxt;
	    ctxt = DES_CreateContext(key->data, iv->data, 
			(triple_des ? NSS_DES_EDE3_CBC : NSS_DES_CBC), 
			encrypt);
	
	    if(ctxt != NULL) {
		rv = ((encrypt != PR_TRUE) ? DES_Decrypt : DES_Encrypt)(
			ctxt, dest->data, &dest->len,
			dup_src->len + 64, dup_src->data, dup_src->len);

		/* remove padding -- assumes 64 bit blocks */
		if((encrypt == PR_FALSE) && (rv == SECSuccess)) {
		    pad = dest->data[dest->len-1];
		    if((pad > 0) && (pad <= 8)) {
			if(dest->data[dest->len-pad] != pad) {
			    rv = SECFailure;
			    PORT_SetError(SEC_ERROR_BAD_PASSWORD);
			} else {
			    dest->len -= pad;
			}
		    } else {
			rv = SECFailure;
			PORT_SetError(SEC_ERROR_BAD_PASSWORD);
		    }
		}
		DES_DestroyContext(ctxt, PR_TRUE);
	    }
	}
    }

    if(rv == SECFailure) {
	if(dest != NULL) {
	    SECITEM_FreeItem(dest, PR_TRUE);
	}
	dest = NULL;
    }

    if(dup_src != NULL) {
	SECITEM_FreeItem(dup_src, PR_TRUE);
    }

    return dest;
}

/* perform rc2 encryption/decryption if an error occurs, NULL is returned
 */
static SECItem *
sec_pkcs5_rc2(SECItem *key, 
	      SECItem *iv, 
	      SECItem *src,
	      PRBool cbc_mode, 
	      PRBool encrypt)
{
    SECItem *dest;
    SECItem *dup_src;
    SECStatus rv = SECFailure;
    int pad;

    if((src == NULL) || (key == NULL) || (iv == NULL)) {
	return NULL;
    }

    dup_src = SECITEM_DupItem(src);
    if(dup_src == NULL) {
	return NULL;
    }

    if(encrypt != PR_FALSE) {
	void *dummy;

	dummy = DES_PadBuffer(NULL, dup_src->data, 
			      dup_src->len, &dup_src->len);
	if(dummy == NULL) {
	    SECITEM_FreeItem(dup_src, PR_TRUE);
	    return NULL;
	}
	dup_src->data = (unsigned char*)dummy;
    }

    dest = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(dest != NULL) {
	dest->data = (unsigned char *)PORT_ZAlloc(dup_src->len + 64);
	if(dest->data != NULL) {
	    RC2Context *ctxt;

	    ctxt = RC2_CreateContext(key->data, key->len, iv->data, 
			((cbc_mode != PR_TRUE) ? NSS_RC2 : NSS_RC2_CBC), 
			key->len);

	    if(ctxt != NULL) {
		rv = ((encrypt != PR_TRUE) ? RC2_Decrypt : RC2_Encrypt)(
			ctxt, dest->data, &dest->len,
			dup_src->len + 64, dup_src->data, dup_src->len);

		/* assumes 8 byte blocks  -- remove padding */	
		if((rv == SECSuccess) && (encrypt != PR_TRUE) && 
			(cbc_mode == PR_TRUE)) {
		    pad = dest->data[dest->len-1];
		    if((pad > 0) && (pad <= 8)) {
			if(dest->data[dest->len-pad] != pad) {
			    PORT_SetError(SEC_ERROR_BAD_PASSWORD);
			    rv = SECFailure;
			} else {
			    dest->len -= pad;
			}
		    } else {
			PORT_SetError(SEC_ERROR_BAD_PASSWORD);
			rv = SECFailure;
		    }
		}

	    }
	}
    }

    if((rv != SECSuccess) && (dest != NULL)) {
	SECITEM_FreeItem(dest, PR_TRUE);
	dest = NULL;
    }

    if(dup_src != NULL) {
	SECITEM_FreeItem(dup_src, PR_TRUE);
    }

    return dest;
}

/* perform rc4 encryption and decryption */
static SECItem *
sec_pkcs5_rc4(SECItem *key, 
	      SECItem *iv, 
	      SECItem *src, 
	      PRBool dummy_op,
	      PRBool encrypt)
{
    SECItem *dest;
    SECStatus rv = SECFailure;

    if((src == NULL) || (key == NULL) || (iv == NULL)) {
	return NULL;
    }

    dest = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(dest != NULL) {
	dest->data = (unsigned char *)PORT_ZAlloc(sizeof(unsigned char) *
	    (src->len + 64));
	if(dest->data != NULL) {
	    RC4Context *ctxt;

	    ctxt = RC4_CreateContext(key->data, key->len);
	    if(ctxt) { 
		rv = ((encrypt != PR_FALSE) ? RC4_Decrypt : RC4_Encrypt)(
				ctxt, dest->data, &dest->len,
				src->len + 64, src->data, src->len);
		RC4_DestroyContext(ctxt, PR_TRUE);
	    }
	}
    }

    if((rv != SECSuccess) && (dest)) {
	SECITEM_FreeItem(dest, PR_TRUE);
	dest = NULL;
    }

    return dest;
}

/* performs the cipher operation on the src and returns the result.
 * if an error occurs, NULL is returned. 
 *
 * a null length password is allowed.  this corresponds to encrypting
 * the data with ust the salt.
 */
/* change this to use PKCS 11? */
SECItem *
SEC_PKCS5CipherData(SECAlgorithmID *algid, 
		    SECItem *pwitem,
		    SECItem *src, 
		    PRBool encrypt, PRBool *update)
{
    SEC_PKCS5PBEParameter *pbe_param;
    SECOidTag enc_alg;
    SECItem *key = NULL, *iv = NULL;
    SECItem *dest = NULL;
    int iv_len;

    if (update) { 
        *update = PR_FALSE;
    }

    if((algid == NULL) || (pwitem == NULL) || (src == NULL)) {
	return NULL;
    }

    /* convert algid to pbe parameter */
    pbe_param = sec_pkcs5_convert_algid(algid);
    if(pbe_param == NULL) {
	return NULL;
    }

    /* get algorithm, key, and iv */
    enc_alg = sec_pkcs5_encryption_algorithm(pbe_param->algorithm);
    key = sec_pkcs5_compute_key(pbe_param, pwitem, PR_FALSE);
    if(key != NULL) {
	iv_len = sec_pkcs5_iv_length(pbe_param->algorithm);
    	iv = sec_pkcs5_compute_iv(pbe_param, pwitem, PR_FALSE);

	if((iv != NULL) || (iv_len == 0)) {
	    /*perform encryption / decryption */
	    PRBool op1 = PR_TRUE;
	    pkcs5_crypto_func cryptof;

	    switch(enc_alg) {
		case SEC_OID_DES_EDE3_CBC:
		    cryptof = sec_pkcs5_des;
		    break;
		case SEC_OID_DES_CBC:
		    cryptof = sec_pkcs5_des;
		    op1 = PR_FALSE;
		    break;
		case SEC_OID_RC2_CBC:
		    cryptof = sec_pkcs5_rc2;
		    break;
		case SEC_OID_RC4:
		    cryptof = sec_pkcs5_rc4;
		    break;
		default:
		    cryptof = NULL;
		    break;
	    }

	    if(cryptof) {
		dest = (*cryptof)(key, iv, src, op1, encrypt);
		/* 
		 * it's possible for some keys and keydb's to claim to
		 * be triple des when they're really des. In this case
		 * we simply try des. If des works we set the update flag
		 * so the key db knows it needs to update all it's entries.
		 *  The case can only happen on decrypted of a 
		 *  SEC_OID_DES_EDE3_CBD.
		 */
		if ((dest == NULL) && (encrypt == PR_FALSE) && 
					(enc_alg == SEC_OID_DES_EDE3_CBC)) {
		    dest = (*cryptof)(key, iv, src, PR_FALSE, encrypt);
		    if (update && (dest != NULL)) *update = PR_TRUE;
		}
	    }
	}
    }

    sec_pkcs5_destroy_pbe_param(pbe_param);

    if(key != NULL) {
	SECITEM_ZfreeItem(key, PR_TRUE);
    }
    if(iv != NULL) {
	SECITEM_ZfreeItem(iv, PR_TRUE);
    }

    return dest;
}

/* creates a algorithm ID containing the PBE algorithm and appropriate
 * parameters.  the required parameter is the algorithm.  if salt is
 * not specified, it is generated randomly.  if IV is specified, it overrides
 * the PKCS 5 generation of the IV.  
 *
 * the returned SECAlgorithmID should be destroyed using 
 * SECOID_DestroyAlgorithmID
 */
SECAlgorithmID *
SEC_PKCS5CreateAlgorithmID(SECOidTag algorithm, 
			   SECItem *salt, 
			   int iteration)
{
    PRArenaPool *poolp = NULL;
    SECAlgorithmID *algid, *ret_algid;
    SECItem der_param;
    SECStatus rv = SECFailure;
    SEC_PKCS5PBEParameter *pbe_param;

    if(sec_pkcs5_hash_algorithm(algorithm) == SEC_OID_UNKNOWN)
	return NULL;

    if(iteration <= 0) {
	return NULL;
    }

    der_param.data = NULL;
    der_param.len = 0;

    /* generate the parameter */
    pbe_param = sec_pkcs5_create_pbe_parameter(algorithm, salt, iteration);
    if(!pbe_param) {
	return NULL;
    }

    poolp = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(!poolp) {
	sec_pkcs5_destroy_pbe_param(pbe_param);
	return NULL;
    }

    /* generate the algorithm id */
    algid = (SECAlgorithmID *)PORT_ArenaZAlloc(poolp, sizeof(SECAlgorithmID));
    if(algid != NULL) {
	void *dummy;
	if(!sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(algorithm)) {
	    dummy = SEC_ASN1EncodeItem(poolp, &der_param, pbe_param,
					SEC_PKCS5PBEParameterTemplate);
	} else {
	    dummy = SEC_ASN1EncodeItem(poolp, &der_param, pbe_param,
	    				SEC_V2PKCS12PBEParameterTemplate);
	}
	
	if(dummy) {
	    rv = SECOID_SetAlgorithmID(poolp, algid, algorithm, &der_param);
	}
    }

    ret_algid = NULL;
    if(algid != NULL) {
	ret_algid = (SECAlgorithmID *)PORT_ZAlloc(sizeof(SECAlgorithmID));
	if(ret_algid != NULL) {
	    rv = SECOID_CopyAlgorithmID(NULL, ret_algid, algid);
	    if(rv != SECSuccess) {
		SECOID_DestroyAlgorithmID(ret_algid, PR_TRUE);
		ret_algid = NULL;
	    }
	}
    }
	
    if(poolp != NULL) {
	PORT_FreeArena(poolp, PR_TRUE);
	algid = NULL;
    }

    sec_pkcs5_destroy_pbe_param(pbe_param);

    return ret_algid;
}

/* wrapper for converting the algid to a pbe parameter. 
 */
SEC_PKCS5PBEParameter *
SEC_PKCS5GetPBEParameter(SECAlgorithmID *algid)
{
    if(algid) {
	return sec_pkcs5_convert_algid(algid);
    }

    return NULL;
}

/* destroy a pbe parameter */
void
SEC_PKCS5DestroyPBEParameter(SEC_PKCS5PBEParameter *pbe_param)
{
    sec_pkcs5_destroy_pbe_param(pbe_param);
}

/* return the initialization vector either the preset one if it 
 * exists or generated based on pkcs 5.
 *
 * a null length password is allowed...but not a null password
 * secitem.
 */
SECItem *
SEC_PKCS5GetIV(SECAlgorithmID *algid, SECItem *pwitem, PRBool faulty3DES)
{
    SECItem *iv;
    SEC_PKCS5PBEParameter *pbe_param;

    if((algid == NULL) || (pwitem == NULL)) {
	return NULL;
    }

    pbe_param = sec_pkcs5_convert_algid(algid);
    if(!pbe_param) {
	return NULL;
    }

    iv = sec_pkcs5_compute_iv(pbe_param, pwitem, faulty3DES);

    sec_pkcs5_destroy_pbe_param(pbe_param);

    return iv;
}

/* generate the key 
 * a 0 length password is allowed.  corresponds to a key generated
 * from just the salt.
 */
SECItem *
SEC_PKCS5GetKey(SECAlgorithmID *algid, SECItem *pwitem, PRBool faulty3DES)
{
    SECItem *key;
    SEC_PKCS5PBEParameter *pbe_param;

    if((algid == NULL) || (pwitem == NULL)) {
	return NULL;
    }

    pbe_param = sec_pkcs5_convert_algid(algid);
    if(pbe_param == NULL) {
	return NULL;
    }

    key = sec_pkcs5_compute_key(pbe_param, pwitem, faulty3DES);

    sec_pkcs5_destroy_pbe_param(pbe_param);

    return key;
}

/* retrieve the salt */
SECItem *
SEC_PKCS5GetSalt(SECAlgorithmID *algid)
{
    SECItem *salt;
    SEC_PKCS5PBEParameter *pbe_param;

    if(algid == NULL)
	return NULL;

    pbe_param = sec_pkcs5_convert_algid(algid);
    if(pbe_param == NULL)
	return NULL;

    if(pbe_param->salt.data) {
	salt = SECITEM_DupItem(&pbe_param->salt);
    } else {
	salt = NULL;
    }

    sec_pkcs5_destroy_pbe_param(pbe_param);

    return salt;
}

/* check to see if an oid is a pbe algorithm
 */ 
PRBool 
SEC_PKCS5IsAlgorithmPBEAlg(SECAlgorithmID *algid)
{
    SECOidTag algorithm;

    algorithm = SECOID_GetAlgorithmTag(algid);
    if(sec_pkcs5_hash_algorithm(algorithm) == SEC_OID_UNKNOWN) {
	return PR_FALSE;
    }

    return PR_TRUE;
}

int 
SEC_PKCS5GetKeyLength(SECAlgorithmID *algid)
{
    SEC_PKCS5PBEParameter *pbe_param;
    int keyLen = -1;

    if(algid == NULL)
	return -1;

    pbe_param = sec_pkcs5_convert_algid(algid);
    if(pbe_param == NULL)
	return -1;

    keyLen = sec_pkcs5_key_length(pbe_param->algorithm);

    sec_pkcs5_destroy_pbe_param(pbe_param);

    return keyLen;
}

/* maps crypto algorithm from PBE algorithm.
 */
SECOidTag 
SEC_PKCS5GetCryptoAlgorithm(SECAlgorithmID *algid)
{
    SEC_PKCS5PBEParameter *pbe_param;

    if(algid == NULL)
	return SEC_OID_UNKNOWN;

    pbe_param = sec_pkcs5_convert_algid(algid);
    if(pbe_param == NULL)
	return SEC_OID_UNKNOWN;

    switch(pbe_param->algorithm)
    {
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC:
	    return SEC_OID_DES_EDE3_CBC;
	case SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC:
	case SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC:
	    return SEC_OID_DES_CBC;
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
	    return SEC_OID_RC2_CBC;
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4:
	case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
	case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
	    return SEC_OID_RC4;
	default:
	    break;
    }

    sec_pkcs5_destroy_pbe_param(pbe_param);

    return SEC_OID_UNKNOWN;
}

/* maps PBE algorithm from crypto algorithm, assumes SHA1 hashing.
 */
SECOidTag 
SEC_PKCS5GetPBEAlgorithm(SECOidTag algTag, int keyLen)
{
    switch(algTag)
    {
	case SEC_OID_DES_EDE3_CBC:
	    switch(keyLen) {
		case 168:
		case 192:
		    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC;
		case 128:
		case 92:
		    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC;
		default:
		    break;
	    }
	    break;
	case SEC_OID_DES_CBC:
	    return SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC;
	case SEC_OID_RC2_CBC:
	    switch(keyLen) {
		case 40:
		    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC;
		case 128:
		    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC;
		default:
		    break;
	    }
	    break;
	case SEC_OID_RC4:
	    switch(keyLen) {
		case 40:
		    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4;
		case 128:
		    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4;
		default:
		    break;
	    }
	    break;
	default:
	    break;
    }

    return SEC_OID_UNKNOWN;
}

/* zero length password and salts are allowed.  however, the items
 * containing the salt and password must be non-null.
 */
PBEBitGenContext *
__PBE_CreateContext(SECOidTag hashAlgorithm, PBEBitGenID bitGenPurpose,
		  SECItem *pwitem, SECItem *salt, unsigned int bitsNeeded,
		  unsigned int iterations)
{
    PRArenaPool *arena = NULL;
    PBEBitGenContext *pbeCtxt = NULL;
    HASH_HashType pbeHash;
    int vbytes, ubytes;
    
    unsigned int c;

    if(!pwitem || !salt) {
	return NULL;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if(!arena) {
	return NULL;
    }

    pbeCtxt = (PBEBitGenContext*)PORT_ArenaZAlloc(arena, sizeof(PBEBitGenContext));
    if(!pbeCtxt) {
	goto loser;
    }

    switch(hashAlgorithm) {
	case SEC_OID_MD2:
	    pbeHash = HASH_AlgMD2;
	    break;
	case SEC_OID_MD5:
	    pbeHash = HASH_AlgMD5;
	    break;
	case SEC_OID_SHA1:
	    pbeHash = HASH_AlgSHA1;
	    break;
	default:
	    goto loser;
    }

    pbeCtxt->hashObject = &SECRawHashObjects[pbeHash]; 
    PORT_Memcpy(&pbeCtxt->pbeParams, &pbeHashAlgorithmParams[pbeHash], 
    		sizeof(pbeBitGenParameters));
    PORT_Assert(pbeCtxt->pbeParams.hashAlgorithm == hashAlgorithm);

    vbytes = pbeCtxt->pbeParams.v / 8;
    ubytes = pbeCtxt->pbeParams.u / 8;

    c = (bitsNeeded / pbeCtxt->pbeParams.u);
    c += ((bitsNeeded - (pbeCtxt->pbeParams.u * c)) > 0) ? 1 : 0;
    pbeCtxt->c = c;
    pbeCtxt->n = bitsNeeded;
    pbeCtxt->iterations = iterations;

    /* allocate buffers */
    pbeCtxt->D.len = vbytes;
    pbeCtxt->S.len = (((salt->len * 8) / pbeCtxt->pbeParams.v) + 
    	      ((((salt->len * 8) % pbeCtxt->pbeParams.v) > 0) ? 1 : 0)) *
    	      vbytes;
    pbeCtxt->P.len = (((pwitem->len * 8) / pbeCtxt->pbeParams.v) + 
    	      ((((pwitem->len * 8) % pbeCtxt->pbeParams.v) > 0) ? 1 : 0)) *
	      vbytes;
    pbeCtxt->I.len = pbeCtxt->S.len + pbeCtxt->P.len;
    pbeCtxt->A.len = c * ubytes;
    pbeCtxt->B.len = pbeCtxt->D.len;

    pbeCtxt->D.data = (unsigned char*)PORT_ArenaZAlloc(arena, pbeCtxt->D.len);
    if(pbeCtxt->S.len) {
	pbeCtxt->S.data = (unsigned char*)PORT_ArenaZAlloc(arena, pbeCtxt->S.len);
    }
    if(pbeCtxt->P.len) {
	pbeCtxt->P.data = (unsigned char*)PORT_ArenaZAlloc(arena, pbeCtxt->P.len);
    }
    if(pbeCtxt->I.len) {
	pbeCtxt->I.data = (unsigned char*)PORT_ArenaZAlloc(arena, pbeCtxt->I.len);
    }
    pbeCtxt->A.data = (unsigned char*)PORT_ArenaZAlloc(arena, pbeCtxt->A.len);
    pbeCtxt->B.data = (unsigned char*)PORT_ArenaZAlloc(arena, pbeCtxt->B.len);

    if(!pbeCtxt->D.data || !pbeCtxt->A.data || !pbeCtxt->B.data ||
       (!pbeCtxt->S.data && pbeCtxt->S.len) || 
       (!pbeCtxt->P.data && pbeCtxt->P.len) ||
       (!pbeCtxt->I.data && pbeCtxt->I.len)) {
	goto loser;
    }

    PORT_Memset(pbeCtxt->D.data, (char)bitGenPurpose, pbeCtxt->D.len);
    if(pbeCtxt->P.len) {
	unsigned int z = 0;
	while(z < pbeCtxt->P.len) {
	    PORT_Memcpy(&(pbeCtxt->P.data[z]), pwitem->data,  
	    	((z + pwitem->len > pbeCtxt->P.len) ? (pbeCtxt->P.len - z) :
	    					      (pwitem->len)));
	    z += pwitem->len;
	}
    } 
    if(pbeCtxt->S.len) {
	unsigned int z = 0;
	while(z < pbeCtxt->S.len) {
	    PORT_Memcpy(&(pbeCtxt->S.data[z]), salt->data,  
	    	((z + salt->len > pbeCtxt->S.len) ? (pbeCtxt->S.len - z) :
	    					    (salt->len)));
	    z += salt->len;
	}
    } 
    if(pbeCtxt->I.len) {
	if(pbeCtxt->S.len) {
	    PORT_Memcpy(pbeCtxt->I.data, pbeCtxt->S.data, pbeCtxt->S.len);
	}
	if(pbeCtxt->P.len) {
	    PORT_Memcpy(&(pbeCtxt->I.data[pbeCtxt->S.len]), pbeCtxt->P.data, 
	    		pbeCtxt->P.len);
	}
    }

    pbeCtxt->arena = arena;

    return pbeCtxt;

loser:
    if(arena) {
	PORT_FreeArena(arena, PR_TRUE);
    }

    return NULL;
}

PBEBitGenContext *
PBE_CreateContext(SECOidTag hashAlgorithm, PBEBitGenID bitGenPurpose,
		  SECItem *pwitem, SECItem *salt, unsigned int bitsNeeded,
		  unsigned int iterations)
{
    return __PBE_CreateContext(hashAlgorithm, bitGenPurpose, pwitem,
                               salt, bitsNeeded, iterations);
}

SECItem *
__PBE_GenerateBits(PBEBitGenContext *pbeCtxt)
{
    unsigned int i;
    SECItem *A, *D, *I, *B, *S, *P;
    unsigned int u, v, c, z, hashLen, n, iter;
    unsigned int vbyte, ubyte;
    unsigned char *iterBuf;
    void *hash = NULL;

    if(!pbeCtxt) {
	return NULL;
    }

    A = &pbeCtxt->A;
    B = &pbeCtxt->B;
    I = &pbeCtxt->I;
    D = &pbeCtxt->D;
    S = &pbeCtxt->S;
    P = &pbeCtxt->P;
    u = pbeCtxt->pbeParams.u;
    v = pbeCtxt->pbeParams.v;
    vbyte = v / 8;
    ubyte = u / 8;
    c = pbeCtxt->c;
    n = pbeCtxt->n;
    z = 0;

    iterBuf = (unsigned char*)PORT_Alloc(ubyte);
    if(!iterBuf) {
	goto loser;
    }

    for(i = 1; i <= c; i++) {
	unsigned int Bidx;
	unsigned int k, j;


	for(iter = 1; iter <= pbeCtxt->iterations; iter++) {
	    hash = pbeCtxt->hashObject->create();
	    if(!hash) {
		goto loser;
	    }

	    pbeCtxt->hashObject->begin(hash);

	    if(iter == 1) {
		pbeCtxt->hashObject->update(hash, D->data, D->len);
		pbeCtxt->hashObject->update(hash, I->data, I->len); 
	    } else {
		pbeCtxt->hashObject->update(hash, iterBuf, hashLen);
	    }

	    pbeCtxt->hashObject->end(hash, iterBuf, &hashLen, (ubyte));
	    pbeCtxt->hashObject->destroy(hash, PR_TRUE);
	    if(hashLen != (ubyte)) {
		goto loser;
	    }
	}

	PORT_Memcpy(&(A->data[z]), iterBuf, (ubyte));

	Bidx = 0;
	while(Bidx < B->len) {
	    PORT_Memcpy(&(B->data[Bidx]), &(A->data[z]), 
			(((Bidx + (ubyte)) > B->len) ? (B->len - Bidx) : 
						       (ubyte)));
	    Bidx += (ubyte);
	}

	k = (S->len / (vbyte)) + (P->len / (vbyte));
	for(j = 0; j < k; j++) {
	    unsigned int byteIdx = (vbyte);
	    unsigned int q, carryBit = 0;

	    while(byteIdx > 0) {
		q = (unsigned int)I->data[(j * (vbyte)) + byteIdx - 1];
		q += (unsigned int)B->data[byteIdx - 1];
		q += carryBit;
		if(byteIdx == (vbyte)) {
		    q += 1;
		}

		carryBit = ((q > 255) ? 1 : 0);

		I->data[(j * (vbyte)) + byteIdx - 1] = (unsigned char)(q & 255);

		byteIdx--;
	    }
	}

	z += (ubyte);
    }

    A->len = (n / 8);
    
    return SECITEM_DupItem(A);

loser:
    return NULL;
} 

SECItem *
PBE_GenerateBits(PBEBitGenContext *pbeCtxt)
{
    return __PBE_GenerateBits(pbeCtxt);
}

void
__PBE_DestroyContext(PBEBitGenContext *pbeCtxt)
{
    if(pbeCtxt) {
	PORT_FreeArena(pbeCtxt->arena, PR_TRUE);
    }
}

void
PBE_DestroyContext(PBEBitGenContext *pbeCtxt)
{
    __PBE_DestroyContext(pbeCtxt);
}

SECStatus
PBE_PK11ParamToAlgid(SECOidTag algTag, SECItem *param, PRArenaPool *arena, 
		     SECAlgorithmID *algId)
{
    CK_PBE_PARAMS *pbe_param;
    SECItem pbeSalt;
    SECAlgorithmID *pbeAlgID = NULL;
    SECStatus rv;

    if(!param || !algId) {
	return SECFailure;
    }

    pbe_param = (CK_PBE_PARAMS *)param->data;
    pbeSalt.data = (unsigned char *)pbe_param->pSalt;
    pbeSalt.len = pbe_param->ulSaltLen;
    pbeAlgID = SEC_PKCS5CreateAlgorithmID(algTag, &pbeSalt, 
					  (int)pbe_param->ulIteration);
    if(!pbeAlgID) {
	return SECFailure;
    }

    rv = SECOID_CopyAlgorithmID(arena, algId, pbeAlgID);
    SECOID_DestroyAlgorithmID(pbeAlgID, PR_TRUE);
    return rv;
}
