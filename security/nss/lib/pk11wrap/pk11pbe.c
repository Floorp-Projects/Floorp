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
#include "sechash.h"
#include "secasn1.h"
#include "secder.h"
#include "secoid.h"
#include "alghmac.h"
#include "secerr.h"
#include "secmod.h"
#include "pk11func.h"
#include "secpkcs5.h"

typedef struct SEC_PKCS5PBEParameterStr SEC_PKCS5PBEParameter;
struct SEC_PKCS5PBEParameterStr {
    PRArenaPool *poolp;
    SECItem     salt;           /* octet string */
    SECItem     iteration;      /* integer */
};


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

/* maps crypto algorithm from PBE algorithm.
 */
SECOidTag 
SEC_PKCS5GetCryptoAlgorithm(SECAlgorithmID *algid)
{

    SECOidTag algorithm;

    if(algid == NULL)
	return SEC_OID_UNKNOWN;

    algorithm = SECOID_GetAlgorithmTag(algid);
    switch(algorithm)
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

    return SEC_OID_UNKNOWN;
}

/* check to see if an oid is a pbe algorithm
 */ 
PRBool 
SEC_PKCS5IsAlgorithmPBEAlg(SECAlgorithmID *algid)
{
    return (PRBool)(SEC_PKCS5GetCryptoAlgorithm(algid) != SEC_OID_UNKNOWN);
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


/* get the key length needed for the PBE algorithm
 */

int 
SEC_PKCS5GetKeyLength(SECAlgorithmID *algid)
{

    SECOidTag algorithm;

    if(algid == NULL)
	return SEC_OID_UNKNOWN;

    algorithm = SECOID_GetAlgorithmTag(algid);

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
#define DEFAULT_SALT_LENGTH 16
static SEC_PKCS5PBEParameter *
sec_pkcs5_create_pbe_parameter(SECOidTag algorithm, 
			SECItem *salt, 
			int iteration)
{
    PRArenaPool *poolp = NULL;
    SEC_PKCS5PBEParameter *pbe_param = NULL;
    SECStatus rv= SECSuccess; 
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

    rv = SECFailure;
    if (salt && salt->data) {
    	rv = SECITEM_CopyItem(poolp, &pbe_param->salt, salt);
    } else {
	/* sigh, the old interface generated salt on the fly, so we have to
	 * preserve the semantics */
	pbe_param->salt.len = DEFAULT_SALT_LENGTH;
	pbe_param->salt.data = PORT_ArenaZAlloc(poolp,DEFAULT_SALT_LENGTH);
	if (pbe_param->salt.data) {
	   rv = PK11_GenerateRandom(pbe_param->salt.data,DEFAULT_SALT_LENGTH);
	}
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

SECStatus
pbe_PK11AlgidToParam(SECAlgorithmID *algid,SECItem *mech)
{
    CK_PBE_PARAMS *pbe_params = NULL;
    SEC_PKCS5PBEParameter p5_param;
    SECItem *salt = NULL;
    SECOidTag algorithm = SECOID_GetAlgorithmTag(algid);
    PRArenaPool *arena = NULL;
    SECStatus rv = SECFailure;
    int iv_len;
    

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (arena == NULL) {
	goto loser;
    }
    iv_len = PK11_GetIVLength(PK11_AlgtagToMechanism(algorithm));
    if (iv_len < 0) {
	goto loser;
    }

    if (sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(algorithm)) {
        rv = SEC_ASN1DecodeItem(arena, &p5_param,
			 SEC_V2PKCS12PBEParameterTemplate, &algid->parameters);
    } else {
        rv = SEC_ASN1DecodeItem(arena,&p5_param,SEC_PKCS5PBEParameterTemplate, 
						&algid->parameters);
    }

    if (rv != SECSuccess) {
	goto loser;
    }
        
    salt = &p5_param.salt;

    pbe_params = (CK_PBE_PARAMS *)PORT_ZAlloc(sizeof(CK_PBE_PARAMS)+
						salt->len+iv_len);
    if (pbe_params == NULL) {
	goto loser;
    }

    /* get salt */
    pbe_params->pSalt = ((CK_CHAR_PTR) pbe_params)+sizeof(CK_PBE_PARAMS);
    if (iv_len) {
	pbe_params->pInitVector = ((CK_CHAR_PTR) pbe_params)+
					sizeof(CK_PBE_PARAMS)+salt->len;
    }
    PORT_Memcpy(pbe_params->pSalt, salt->data, salt->len);
    pbe_params->ulSaltLen = (CK_ULONG) salt->len;

    /* get iteration count */
    pbe_params->ulIteration = (CK_ULONG) DER_GetInteger(&p5_param.iteration);

    /* copy into the mechanism sec item */
    mech->data = (unsigned char *)pbe_params;
    mech->len = sizeof(*pbe_params);
    if (arena) {
	PORT_FreeArena(arena,PR_TRUE);
    }
    return SECSuccess;

loser:
    if (pbe_params) {
	PORT_Free(pbe_params);
    }
    if (arena) {
	PORT_FreeArena(arena,PR_TRUE);
    }
    return SECFailure;
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

PBEBitGenContext *
PBE_CreateContext(SECOidTag hashAlgorithm, PBEBitGenID bitGenPurpose,
	SECItem *pwitem, SECItem *salt, unsigned int bitsNeeded,
	unsigned int iterations)
{
    SECItem *context = NULL;
    SECItem mechItem;
    CK_PBE_PARAMS pbe_params;
    CK_MECHANISM_TYPE mechanism = CKM_INVALID_MECHANISM;
    PK11SymKey *symKey = NULL;
    unsigned char ivData[8];
    

    /* use the purpose to select the low level keygen algorithm */
    switch (bitGenPurpose) {
    case pbeBitGenIntegrityKey:
	switch (hashAlgorithm) {
	case SEC_OID_SHA1:
	    mechanism = CKM_PBA_SHA1_WITH_SHA1_HMAC;
	    break;
	case SEC_OID_MD2:
	    mechanism = CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN;
	    break;
	case SEC_OID_MD5:
	    mechanism = CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN;
	    break;
	default:
	    break;
	}
	break;
    case pbeBitGenCipherIV:
	if (bitsNeeded > 64) {
	    break;
	}
	if (hashAlgorithm != SEC_OID_SHA1) {
	    break;
	}
	mechanism = CKM_PBE_SHA1_DES3_EDE_CBC;
    case pbeBitGenCipherKey:
	if (hashAlgorithm != SEC_OID_SHA1) {
	    break;
	}
	switch (bitsNeeded) {
	case 40:
	    mechanism = CKM_PBE_SHA1_RC4_40;
	    break;
	case 128:
	    mechanism = CKM_PBE_SHA1_RC4_128;
	    break;
	default:
	    break;
	}
    case pbeBitGenIDNull:
	break;
    }

    if (mechanism == CKM_INVALID_MECHANISM) {
	/* we should set an error, but this is a depricated function, and
	 * we are keeping bug for bug compatibility;)... */
	    return NULL;
    } 

    pbe_params.pInitVector = ivData;
    pbe_params.pPassword = pwitem->data;
    pbe_params.ulPasswordLen = pwitem->len;
    pbe_params.pSalt = salt->data;
    pbe_params.ulSaltLen = salt->len;
    pbe_params.ulIteration = iterations;
    mechItem.data = (unsigned char *) &pbe_params;
    mechItem.len = sizeof(pbe_params);


    symKey = PK11_RawPBEKeyGen(PK11_GetInternalSlot(),mechanism,
					&mechItem, pwitem, PR_FALSE, NULL);
    if (symKey == NULL) {
	if (bitGenPurpose == pbeBitGenCipherIV) {
	    /* NOTE: this assumes that bitsNeeded is a multiple of 8! */
	    SECItem ivItem;

	    ivItem.data = ivData;
	    ivItem.len = bitsNeeded/8;
	    context = SECITEM_DupItem(&ivItem);
	} else {
	    SECItem *keyData;
	    PK11_ExtractKeyValue(symKey);
	    keyData = PK11_GetKeyData(symKey);

	    /* assert bitsNeeded with length? */
	    if (keyData) {
	    	context = SECITEM_DupItem(keyData);
	    }
	}
	PK11_FreeSymKey(symKey);
    }

    return (PBEBitGenContext *)context;
}

SECItem *
PBE_GenerateBits(PBEBitGenContext *context)
{
    return (SECItem *)context;
}

void
PBE_DestroyContext(PBEBitGenContext *context)
{
    SECITEM_FreeItem((SECItem *)context,PR_TRUE);
}

SECItem *
SEC_PKCS5GetIV(SECAlgorithmID *algid, SECItem *pwitem, PRBool faulty3DES)
{
    SECItem mechItem;
    SECOidTag algorithm = SECOID_GetAlgorithmTag(algid);
    CK_PBE_PARAMS *pbe_params;
    CK_MECHANISM_TYPE mechanism;
    SECItem *iv = NULL;
    SECStatus rv;
    int iv_len;
    PK11SymKey *symKey;

    rv = pbe_PK11AlgidToParam(algid,&mechItem);
    if (rv != SECSuccess) {
	return NULL;
    }

    mechanism = PK11_AlgtagToMechanism(algorithm);
    iv_len = PK11_GetIVLength(mechanism);
    pbe_params = (CK_PBE_PARAMS_PTR)mechItem.data;

    symKey = PK11_RawPBEKeyGen(PK11_GetInternalSlot(),mechanism,
					&mechItem, pwitem, faulty3DES,NULL);

    if (symKey) {
	SECItem tmp;

	tmp.data = pbe_params->pInitVector;
	tmp.len = iv_len;
	iv = SECITEM_DupItem(&tmp);
        PK11_FreeSymKey(symKey);
    }

    if (mechItem.data) {
	PORT_ZFree(mechItem.data,mechItem.len);
    }

    return iv;
}

/*
 * Subs from nss 3.x that are depricated
 */
PBEBitGenContext *
__PBE_CreateContext(SECOidTag hashAlgorithm, PBEBitGenID bitGenPurpose,
	SECItem *pwitem, SECItem *salt, unsigned int bitsNeeded,
	unsigned int iterations)
{
    PORT_Assert("__PBE_CreateContext is Depricated" == NULL);
    return NULL;
}

SECItem *
__PBE_GenerateBits(PBEBitGenContext *context)
{
    PORT_Assert("__PBE_GenerateBits is Depricated" == NULL);
    return NULL;
}

void
__PBE_DestroyContext(PBEBitGenContext *context)
{
    PORT_Assert("__PBE_DestroyContext is Depricated" == NULL);
}

SECStatus
RSA_FormatBlock(SECItem *result, unsigned modulusLen,
                int blockType, SECItem *data)
{
    PORT_Assert("RSA_FormatBlock is Depricated" == NULL);
    return SECFailure;
}

