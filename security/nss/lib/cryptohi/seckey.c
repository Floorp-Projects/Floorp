/* ***** BEGIN LICENSE BLOCK *****
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
 *   Dr Stephen Henson <stephen.henson@gemplus.com>
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
#include "cryptohi.h"
#include "keyhi.h"
#include "secoid.h"
#include "secitem.h"
#include "secder.h"
#include "base64.h"
#include "secasn1.h"
#include "cert.h"
#include "pk11func.h"
#include "secerr.h"
#include "secdig.h"
#include "prtime.h"
#include "ec.h"

const SEC_ASN1Template CERT_SubjectPublicKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTSubjectPublicKeyInfo) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTSubjectPublicKeyInfo,algorithm),
	  SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(CERTSubjectPublicKeyInfo,subjectPublicKey), },
    { 0, }
};

const SEC_ASN1Template CERT_PublicKeyAndChallengeTemplate[] =
{
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CERTPublicKeyAndChallenge) },
    { SEC_ASN1_ANY, offsetof(CERTPublicKeyAndChallenge,spki) },
    { SEC_ASN1_IA5_STRING, offsetof(CERTPublicKeyAndChallenge,challenge) },
    { 0 }
};

const SEC_ASN1Template SECKEY_RSAPublicKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYPublicKey) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.rsa.modulus), },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.rsa.publicExponent), },
    { 0, }
};

const SEC_ASN1Template SECKEY_DSAPublicKeyTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.dsa.publicValue), },
    { 0, }
};

const SEC_ASN1Template SECKEY_PQGParamsTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYPQGParams) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,prime) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,subPrime) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,base) },
    { 0, }
};

const SEC_ASN1Template SECKEY_DHPublicKeyTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.dh.publicValue), },
    { 0, }
};

const SEC_ASN1Template SECKEY_DHParamKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE,  0, NULL, sizeof(SECKEYPublicKey) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.dh.prime), },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.dh.base), },
    /* XXX chrisk: this needs to be expanded for decoding of j and validationParms (RFC2459 7.3.2) */
    { SEC_ASN1_SKIP_REST },
    { 0, }
};

const SEC_ASN1Template SECKEY_FortezzaParameterTemplate[] = {
    { SEC_ASN1_SEQUENCE,  0, NULL, sizeof(SECKEYPQGParams) },
    { SEC_ASN1_OCTET_STRING, offsetof(SECKEYPQGParams,prime), },
    { SEC_ASN1_OCTET_STRING, offsetof(SECKEYPQGParams,subPrime), },
    { SEC_ASN1_OCTET_STRING, offsetof(SECKEYPQGParams,base), },
    { 0 },
};
 
const SEC_ASN1Template SECKEY_FortezzaDiffParameterTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYDiffPQGParams) },
    { SEC_ASN1_INLINE, offsetof(SECKEYDiffPQGParams,DiffKEAParams), 
                       SECKEY_FortezzaParameterTemplate},
    { SEC_ASN1_INLINE, offsetof(SECKEYDiffPQGParams,DiffDSAParams), 
                       SECKEY_FortezzaParameterTemplate},
    { 0 },
};

const SEC_ASN1Template SECKEY_FortezzaPreParamTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
      SEC_ASN1_CONTEXT_SPECIFIC | 1, offsetof(SECKEYPQGDualParams,CommParams),
                SECKEY_FortezzaParameterTemplate},
    { 0, }
};

const SEC_ASN1Template SECKEY_FortezzaAltPreParamTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
      SEC_ASN1_CONTEXT_SPECIFIC | 0, offsetof(SECKEYPQGDualParams,DiffParams),
                SECKEY_FortezzaDiffParameterTemplate},
    { 0, }
};

const SEC_ASN1Template SECKEY_KEAPublicKeyTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.kea.publicValue), },
    { 0, }
};

const SEC_ASN1Template SECKEY_KEAParamsTemplate[] = {
    { SEC_ASN1_OCTET_STRING, offsetof(SECKEYPublicKey,u.kea.params.hash), }, 
    { 0, }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SECKEY_DSAPublicKeyTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SECKEY_RSAPublicKeyTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(CERT_SubjectPublicKeyInfoTemplate)

/*
 * See bugzilla bug 125359
 * Since NSS (via PKCS#11) wants to handle big integers as unsigned ints,
 * all of the templates above that en/decode into integers must be converted
 * from ASN.1's signed integer type.  This is done by marking either the
 * source or destination (encoding or decoding, respectively) type as
 * siUnsignedInteger.
 */
static void
prepare_rsa_pub_key_for_asn1(SECKEYPublicKey *pubk)
{
    pubk->u.rsa.modulus.type = siUnsignedInteger;
    pubk->u.rsa.publicExponent.type = siUnsignedInteger;
}

static void
prepare_dsa_pub_key_for_asn1(SECKEYPublicKey *pubk)
{
    pubk->u.dsa.publicValue.type = siUnsignedInteger;
}

static void
prepare_pqg_params_for_asn1(SECKEYPQGParams *params)
{
    params->prime.type = siUnsignedInteger;
    params->subPrime.type = siUnsignedInteger;
    params->base.type = siUnsignedInteger;
}

static void
prepare_dh_pub_key_for_asn1(SECKEYPublicKey *pubk)
{
    pubk->u.dh.prime.type = siUnsignedInteger;
    pubk->u.dh.base.type = siUnsignedInteger;
    pubk->u.dh.publicValue.type = siUnsignedInteger;
}

static void
prepare_kea_pub_key_for_asn1(SECKEYPublicKey *pubk)
{
    pubk->u.kea.publicValue.type = siUnsignedInteger;
}

/* Create an RSA key pair is any slot able to do so.
** The created keys are "session" (temporary), not "token" (permanent), 
** and they are "sensitive", which makes them costly to move to another token.
*/
SECKEYPrivateKey *
SECKEY_CreateRSAPrivateKey(int keySizeInBits,SECKEYPublicKey **pubk, void *cx)
{
    SECKEYPrivateKey *privk;
    PK11SlotInfo *slot = PK11_GetBestSlot(CKM_RSA_PKCS_KEY_PAIR_GEN,cx);
    PK11RSAGenParams param;

    param.keySizeInBits = keySizeInBits;
    param.pe = 65537L;
    
    privk = PK11_GenerateKeyPair(slot,CKM_RSA_PKCS_KEY_PAIR_GEN,&param,pubk,
					PR_FALSE, PR_TRUE, cx);
    PK11_FreeSlot(slot);
    return(privk);
}

/* Create a DH key pair in any slot able to do so, 
** This is a "session" (temporary), not "token" (permanent) key. 
** Because of the high probability that this key will need to be moved to
** another token, and the high cost of moving "sensitive" keys, we attempt
** to create this key pair without the "sensitive" attribute, but revert to 
** creating a "sensitive" key if necessary.
*/
SECKEYPrivateKey *
SECKEY_CreateDHPrivateKey(SECKEYDHParams *param, SECKEYPublicKey **pubk, void *cx)
{
    SECKEYPrivateKey *privk;
    PK11SlotInfo *slot = PK11_GetBestSlot(CKM_DH_PKCS_KEY_PAIR_GEN,cx);

    privk = PK11_GenerateKeyPair(slot, CKM_DH_PKCS_KEY_PAIR_GEN, param, 
                                 pubk, PR_FALSE, PR_FALSE, cx);
    if (!privk) 
	privk = PK11_GenerateKeyPair(slot, CKM_DH_PKCS_KEY_PAIR_GEN, param, 
	                             pubk, PR_FALSE, PR_TRUE, cx);

    PK11_FreeSlot(slot);
    return(privk);
}

/* Create an EC key pair in any slot able to do so, 
** This is a "session" (temporary), not "token" (permanent) key. 
** Because of the high probability that this key will need to be moved to
** another token, and the high cost of moving "sensitive" keys, we attempt
** to create this key pair without the "sensitive" attribute, but revert to 
** creating a "sensitive" key if necessary.
*/
SECKEYPrivateKey *
SECKEY_CreateECPrivateKey(SECKEYECParams *param, SECKEYPublicKey **pubk, void *cx)
{
#ifdef NSS_ENABLE_ECC
    SECKEYPrivateKey *privk;
    PK11SlotInfo *slot = PK11_GetBestSlot(CKM_EC_KEY_PAIR_GEN,cx);

    privk = PK11_GenerateKeyPair(slot, CKM_EC_KEY_PAIR_GEN, param, 
                                 pubk, PR_FALSE, PR_FALSE, cx);
    if (!privk) 
	privk = PK11_GenerateKeyPair(slot, CKM_EC_KEY_PAIR_GEN, param, 
	                             pubk, PR_FALSE, PR_TRUE, cx);

    PK11_FreeSlot(slot);
    return(privk);
#else
    return NULL;
#endif /* NSS_ENABLE_ECC */
}

void
SECKEY_DestroyPrivateKey(SECKEYPrivateKey *privk)
{
    if (privk) {
	if (privk->pkcs11Slot) {
	    if (privk->pkcs11IsTemp) {
	    	PK11_DestroyObject(privk->pkcs11Slot,privk->pkcs11ID);
	    }
	    PK11_FreeSlot(privk->pkcs11Slot);

	}
    	if (privk->arena) {
	    PORT_FreeArena(privk->arena, PR_TRUE);
	}
    }
}

void
SECKEY_DestroyPublicKey(SECKEYPublicKey *pubk)
{
    if (pubk) {
	if (pubk->pkcs11Slot) {
	    if (!PK11_IsPermObject(pubk->pkcs11Slot,pubk->pkcs11ID)) {
		PK11_DestroyObject(pubk->pkcs11Slot,pubk->pkcs11ID);
	    }
	    PK11_FreeSlot(pubk->pkcs11Slot);
	}
    	if (pubk->arena) {
	    PORT_FreeArena(pubk->arena, PR_FALSE);
	}
    }
}

SECStatus
SECKEY_CopySubjectPublicKeyInfo(PRArenaPool *arena,
			     CERTSubjectPublicKeyInfo *to,
			     CERTSubjectPublicKeyInfo *from)
{
    SECStatus rv;

    rv = SECOID_CopyAlgorithmID(arena, &to->algorithm, &from->algorithm);
    if (rv == SECSuccess)
	rv = SECITEM_CopyItem(arena, &to->subjectPublicKey, &from->subjectPublicKey);

    return rv;
}

SECStatus
SECKEY_KEASetParams(SECKEYKEAParams * params, SECKEYPublicKey * pubKey) {

    if (pubKey->keyType == fortezzaKey) {
        /* the key is a fortezza V1 public key  */

	/* obtain hash of pubkey->u.fortezza.params.prime.data +
		          pubkey->u.fortezza.params.subPrime.data +
			  pubkey->u.fortezza.params.base.data  */

	/* store hash in params->hash */

    } else if (pubKey->keyType == keaKey) {

        /* the key is a new fortezza KEA public key. */
        SECITEM_CopyItem(pubKey->arena, &params->hash, 
	                 &pubKey->u.kea.params.hash );

    } else {

	/* the key has no KEA parameters */
	return SECFailure;
    }
    return SECSuccess;
}


SECStatus
SECKEY_KEAParamCompare(CERTCertificate *cert1,CERTCertificate *cert2) 
{

    SECStatus rv;

    SECKEYPublicKey *pubKey1 = 0;
    SECKEYPublicKey *pubKey2 = 0;

    SECKEYKEAParams params1;
    SECKEYKEAParams params2;


    rv = SECFailure;

    /* get cert1's public key */
    pubKey1 = CERT_ExtractPublicKey(cert1);
    if ( !pubKey1 ) {
	return(SECFailure);
    }
    

    /* get cert2's public key */
    pubKey2 = CERT_ExtractPublicKey(cert2);
    if ( !pubKey2 ) {
	return(SECFailure);
    }

    /* handle the case when both public keys are new
     * fortezza KEA public keys.    */

    if ((pubKey1->keyType == keaKey) &&
        (pubKey2->keyType == keaKey) ) {

        rv = (SECStatus)SECITEM_CompareItem(&pubKey1->u.kea.params.hash,
	                         &pubKey2->u.kea.params.hash);
	goto done;
    }

    /* handle the case when both public keys are old fortezza
     * public keys.              */

    if ((pubKey1->keyType == fortezzaKey) &&
        (pubKey2->keyType == fortezzaKey) ) {

        rv = (SECStatus)SECITEM_CompareItem(&pubKey1->u.fortezza.keaParams.prime,
	                         &pubKey2->u.fortezza.keaParams.prime);

	if (rv == SECEqual) {
	    rv = (SECStatus)SECITEM_CompareItem(&pubKey1->u.fortezza.keaParams.subPrime,
	                             &pubKey2->u.fortezza.keaParams.subPrime);
	}

	if (rv == SECEqual) {
	    rv = (SECStatus)SECITEM_CompareItem(&pubKey1->u.fortezza.keaParams.base,
	                             &pubKey2->u.fortezza.keaParams.base);
	}
	
	goto done;
    }


    /* handle the case when the public keys are a mixture of 
     * old and new.                          */

    rv = SECKEY_KEASetParams(&params1, pubKey1);
    if (rv != SECSuccess) return rv;

    rv = SECKEY_KEASetParams(&params2, pubKey2);
    if (rv != SECSuccess) return rv;

    rv = (SECStatus)SECITEM_CompareItem(&params1.hash, &params2.hash);

done:
    SECKEY_DestroyPublicKey(pubKey1);
    SECKEY_DestroyPublicKey(pubKey2);

    return rv;   /* returns SECEqual if parameters are equal */

}


/* Procedure to update the pqg parameters for a cert's public key.
 * pqg parameters only need to be updated for DSA and fortezza certificates.
 * The procedure uses calls to itself recursively to update a certificate
 * issuer's pqg parameters.  Some important rules are:
 *    - Do nothing if the cert already has PQG parameters.
 *    - If the cert does not have PQG parameters, obtain them from the issuer.
 *    - A valid cert chain cannot have a DSA or Fortezza cert without
 *      pqg parameters that has a parent that is not a DSA or Fortezza cert.
 *    - pqg paramters are stored in two different formats: the standard
 *      DER encoded format and the fortezza-only wrapped format.  The params
 *      should be copied from issuer to subject cert without modifying the
 *      formats.  The public key extraction code will deal with the different
 *      formats at the time of extraction.  */

static SECStatus
seckey_UpdateCertPQGChain(CERTCertificate * subjectCert, int count)
{
    SECStatus rv, rvCompare;
    SECOidData *oid=NULL;
    int tag;
    CERTSubjectPublicKeyInfo * subjectSpki=NULL;
    CERTSubjectPublicKeyInfo * issuerSpki=NULL;
    CERTCertificate *issuerCert = NULL;

    rv = SECSuccess;

    /* increment cert chain length counter*/
    count++;

    /* check if cert chain length exceeds the maximum length*/
    if (count > CERT_MAX_CERT_CHAIN) {
	return SECFailure;
    }

    oid = SECOID_FindOID(&subjectCert->subjectPublicKeyInfo.algorithm.algorithm);            
    if (oid != NULL) {  
        tag = oid->offset;
             
        /* Check if cert has a DSA or Fortezza public key. If not, return
         * success since no PQG params need to be updated.  */

	if ( (tag != SEC_OID_MISSI_KEA_DSS_OLD) &&
	     (tag != SEC_OID_MISSI_DSS_OLD) &&
             (tag != SEC_OID_MISSI_KEA_DSS) &&
             (tag != SEC_OID_MISSI_DSS) &&               
             (tag != SEC_OID_ANSIX9_DSA_SIGNATURE) &&
             (tag != SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST) &&
             (tag != SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST) &&
             (tag != SEC_OID_SDN702_DSA_SIGNATURE) &&
             (tag != SEC_OID_ANSIX962_EC_PUBLIC_KEY) ) {
            
            return SECSuccess;
        }
    } else {
        return SECFailure;  /* return failure if oid is NULL */  
    }

    /* if cert has PQG parameters, return success */

    subjectSpki=&subjectCert->subjectPublicKeyInfo;

    if (subjectSpki->algorithm.parameters.len != 0) {
        return SECSuccess;
    }

    /* check if the cert is self-signed */
    rvCompare = (SECStatus)SECITEM_CompareItem(&subjectCert->derSubject,
				    &subjectCert->derIssuer);
    if (rvCompare == SECEqual) {
      /* fail since cert is self-signed and has no pqg params. */
	return SECFailure;     
    }
     
    /* get issuer cert */
    issuerCert = CERT_FindCertIssuer(subjectCert, PR_Now(), certUsageAnyCA);
    if ( ! issuerCert ) {
	return SECFailure;
    }

    /* if parent is not DSA or fortezza, return failure since
       we don't allow this case. */

    oid = SECOID_FindOID(&issuerCert->subjectPublicKeyInfo.algorithm.algorithm);
    if (oid != NULL) {  
        tag = oid->offset;
             
        /* Check if issuer cert has a DSA or Fortezza public key. If not,
         * return failure.   */

	if ( (tag != SEC_OID_MISSI_KEA_DSS_OLD) &&
	     (tag != SEC_OID_MISSI_DSS_OLD) &&
             (tag != SEC_OID_MISSI_KEA_DSS) &&
             (tag != SEC_OID_MISSI_DSS) &&               
             (tag != SEC_OID_ANSIX9_DSA_SIGNATURE) &&
             (tag != SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST) &&
             (tag != SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST) &&
             (tag != SEC_OID_SDN702_DSA_SIGNATURE) &&
             (tag != SEC_OID_ANSIX962_EC_PUBLIC_KEY) ) {            
            rv = SECFailure;
            goto loser;
        }
    } else {
        rv = SECFailure;  /* return failure if oid is NULL */  
        goto loser;
    }


    /* at this point the subject cert has no pqg parameters and the
     * issuer cert has a DSA or fortezza public key.  Update the issuer's
     * pqg parameters with a recursive call to this same function. */

    rv = seckey_UpdateCertPQGChain(issuerCert, count);
    if (rv != SECSuccess) {
        rv = SECFailure;
        goto loser;
    }

    /* ensure issuer has pqg parameters */

    issuerSpki=&issuerCert->subjectPublicKeyInfo;
    if (issuerSpki->algorithm.parameters.len == 0) {
        rv = SECFailure; 
    }

    /* if update was successful and pqg params present, then copy the
     * parameters to the subject cert's key. */

    if (rv == SECSuccess) {
        rv = SECITEM_CopyItem(subjectCert->arena,
                              &subjectSpki->algorithm.parameters, 
	   		      &issuerSpki->algorithm.parameters);
    }

loser:
    if (issuerCert) {
        CERT_DestroyCertificate(issuerCert);
    }
    return rv;

}
 

SECStatus
SECKEY_UpdateCertPQG(CERTCertificate * subjectCert)
{
    if (!subjectCert) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    return seckey_UpdateCertPQGChain(subjectCert,0);
}
   

/* Decode the PQG parameters.  The params could be stored in two
 * possible formats, the old fortezza-only wrapped format or
 * the standard DER encoded format.   Store the decoded parameters in an
 * old fortezza cert data structure */
 
SECStatus
SECKEY_FortezzaDecodePQGtoOld(PRArenaPool *arena, SECKEYPublicKey *pubk,
                              SECItem *params) {
    SECStatus rv;
    SECKEYPQGDualParams dual_params;
    SECItem newparams;

    PORT_Assert(arena);

    if (params == NULL) return SECFailure; 
    
    if (params->data == NULL) return SECFailure;

    /* make a copy of the data into the arena so QuickDER output is valid */
    rv = SECITEM_CopyItem(arena, &newparams, params);

    /* Check if params use the standard format.
     * The value 0xa1 will appear in the first byte of the parameter data
     * if the PQG parameters are not using the standard format. This
     * code should be changed to use a better method to detect non-standard
     * parameters.    */

    if ((newparams.data[0] != 0xa1) &&
        (newparams.data[0] != 0xa0)) {

        if (SECSuccess == rv) {
            /* PQG params are in the standard format */

	    /* Store DSA PQG parameters */
	    prepare_pqg_params_for_asn1(&pubk->u.fortezza.params);
            rv = SEC_QuickDERDecodeItem(arena, &pubk->u.fortezza.params,
                              SECKEY_PQGParamsTemplate,
                              &newparams);
        }

	if (SECSuccess == rv) {

	    /* Copy the DSA PQG parameters to the KEA PQG parameters. */
	    rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.prime,
                                  &pubk->u.fortezza.params.prime);
        }
        if (SECSuccess == rv) {
            rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.subPrime,
                                  &pubk->u.fortezza.params.subPrime);
        }
        if (SECSuccess == rv) {
            rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.base,
                                  &pubk->u.fortezza.params.base);
        }
    } else {

	dual_params.CommParams.prime.len = 0;
        dual_params.CommParams.subPrime.len = 0;
	dual_params.CommParams.base.len = 0;
	dual_params.DiffParams.DiffDSAParams.prime.len = 0;
        dual_params.DiffParams.DiffDSAParams.subPrime.len = 0;
	dual_params.DiffParams.DiffDSAParams.base.len = 0;

        /* else the old fortezza-only wrapped format is used. */

        if (SECSuccess == rv) {
	    if (newparams.data[0] == 0xa1) {
                rv = SEC_QuickDERDecodeItem(arena, &dual_params, 
				    SECKEY_FortezzaPreParamTemplate, &newparams);
	    } else {
                rv = SEC_QuickDERDecodeItem(arena, &dual_params, 
	   			        SECKEY_FortezzaAltPreParamTemplate, &newparams);
            }
        }
	
        if ( (dual_params.CommParams.prime.len > 0) &&
             (dual_params.CommParams.subPrime.len > 0) && 
             (dual_params.CommParams.base.len > 0) ) {
            /* copy in common params */
	    if (SECSuccess == rv) {
	        rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.params.prime,
                                      &dual_params.CommParams.prime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.params.subPrime,
                                      &dual_params.CommParams.subPrime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.params.base,
                                      &dual_params.CommParams.base);
            }

	    /* Copy the DSA PQG parameters to the KEA PQG parameters. */
            if (SECSuccess == rv) {
	        rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.prime,
                                      &pubk->u.fortezza.params.prime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.subPrime,
                                      &pubk->u.fortezza.params.subPrime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.base,
                                      &pubk->u.fortezza.params.base);
            }
        } else {

	    /* else copy in different params */

	    /* copy DSA PQG parameters */
            if (SECSuccess == rv) {
	        rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.params.prime,
                                  &dual_params.DiffParams.DiffDSAParams.prime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.params.subPrime,
                                  &dual_params.DiffParams.DiffDSAParams.subPrime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.params.base,
                                  &dual_params.DiffParams.DiffDSAParams.base);
            }

	    /* copy KEA PQG parameters */

            if (SECSuccess == rv) {
	        rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.prime,
                                  &dual_params.DiffParams.DiffKEAParams.prime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.subPrime,
                                  &dual_params.DiffParams.DiffKEAParams.subPrime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.keaParams.base,
                                  &dual_params.DiffParams.DiffKEAParams.base);
            }
        }
    }
    return rv;
}


/* Decode the DSA PQG parameters.  The params could be stored in two
 * possible formats, the old fortezza-only wrapped format or
 * the normal standard format.  Store the decoded parameters in
 * a V3 certificate data structure.  */ 

SECStatus
SECKEY_DSADecodePQG(PRArenaPool *arena, SECKEYPublicKey *pubk, SECItem *params) {
    SECStatus rv;
    SECKEYPQGDualParams dual_params;
    SECItem newparams;

    if (params == NULL) return SECFailure; 
    
    if (params->data == NULL) return SECFailure;

    PORT_Assert(arena);

    /* make a copy of the data into the arena so QuickDER output is valid */
    rv = SECITEM_CopyItem(arena, &newparams, params);

    /* Check if params use the standard format.
     * The value 0xa1 will appear in the first byte of the parameter data
     * if the PQG parameters are not using the standard format.  This
     * code should be changed to use a better method to detect non-standard
     * parameters.    */

    if ((newparams.data[0] != 0xa1) &&
        (newparams.data[0] != 0xa0)) {
    
        if (SECSuccess == rv) {
             /* PQG params are in the standard format */
             prepare_pqg_params_for_asn1(&pubk->u.dsa.params);
             rv = SEC_QuickDERDecodeItem(arena, &pubk->u.dsa.params,
                                 SECKEY_PQGParamsTemplate,
                                 &newparams);
        }
    } else {

	dual_params.CommParams.prime.len = 0;
        dual_params.CommParams.subPrime.len = 0;
	dual_params.CommParams.base.len = 0;
	dual_params.DiffParams.DiffDSAParams.prime.len = 0;
        dual_params.DiffParams.DiffDSAParams.subPrime.len = 0;
	dual_params.DiffParams.DiffDSAParams.base.len = 0;

        if (SECSuccess == rv) {
            /* else the old fortezza-only wrapped format is used. */
            if (newparams.data[0] == 0xa1) {
                rv = SEC_QuickDERDecodeItem(arena, &dual_params, 
				    SECKEY_FortezzaPreParamTemplate, &newparams);
	    } else {
                rv = SEC_QuickDERDecodeItem(arena, &dual_params, 
	   			        SECKEY_FortezzaAltPreParamTemplate, &newparams);
            }
        }

        if ( (dual_params.CommParams.prime.len > 0) &&
             (dual_params.CommParams.subPrime.len > 0) && 
             (dual_params.CommParams.base.len > 0) ) {
            /* copy in common params */

            if (SECSuccess == rv) {	    
	        rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.prime,
                                      &dual_params.CommParams.prime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.subPrime,
                                      &dual_params.CommParams.subPrime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.base,
                                    &dual_params.CommParams.base);
            }
        } else {

	    /* else copy in different params */

	    /* copy DSA PQG parameters */
            if (SECSuccess == rv) {
	        rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.prime,
                                      &dual_params.DiffParams.DiffDSAParams.prime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.subPrime,
                                      &dual_params.DiffParams.DiffDSAParams.subPrime);
            }
            if (SECSuccess == rv) {
                rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.base,
                                      &dual_params.DiffParams.DiffDSAParams.base);
            }
        }
    }
    return rv;
}


/* Decodes the DER encoded fortezza public key and stores the results in a
 * structure of type SECKEYPublicKey. */

SECStatus
SECKEY_FortezzaDecodeCertKey(PRArenaPool *arena, SECKEYPublicKey *pubk,
                             SECItem *rawkey, SECItem *params) {

	unsigned char *rawptr = rawkey->data;
	unsigned char *end = rawkey->data + rawkey->len;
	unsigned char *clearptr;

	/* first march down and decode the raw key data */

	/* version */	
	pubk->u.fortezza.KEAversion = *rawptr++;
	if (*rawptr++ != 0x01) {
		return SECFailure;
	}

	/* KMID */
	PORT_Memcpy(pubk->u.fortezza.KMID,rawptr,
				sizeof(pubk->u.fortezza.KMID));
	rawptr += sizeof(pubk->u.fortezza.KMID);

	/* clearance (the string up to the first byte with the hi-bit on */
	clearptr = rawptr;
	while ((rawptr < end) && (*rawptr++ & 0x80));

	if (rawptr >= end) { return SECFailure; }
	pubk->u.fortezza.clearance.len = rawptr - clearptr;
	pubk->u.fortezza.clearance.data = 
		(unsigned char*)PORT_ArenaZAlloc(arena,pubk->u.fortezza.clearance.len);
	if (pubk->u.fortezza.clearance.data == NULL) {
		return SECFailure;
	}
	PORT_Memcpy(pubk->u.fortezza.clearance.data,clearptr,
					pubk->u.fortezza.clearance.len);

	/* KEAPrivilege (the string up to the first byte with the hi-bit on */
	clearptr = rawptr;
	while ((rawptr < end) && (*rawptr++ & 0x80));
	if (rawptr >= end) { return SECFailure; }
	pubk->u.fortezza.KEApriviledge.len = rawptr - clearptr;
	pubk->u.fortezza.KEApriviledge.data = 
		(unsigned char*)PORT_ArenaZAlloc(arena,pubk->u.fortezza.KEApriviledge.len);
	if (pubk->u.fortezza.KEApriviledge.data == NULL) {
		return SECFailure;
	}
	PORT_Memcpy(pubk->u.fortezza.KEApriviledge.data,clearptr,
				pubk->u.fortezza.KEApriviledge.len);


	/* now copy the key. The next to bytes are the key length, and the
	 * key follows */
	pubk->u.fortezza.KEAKey.len = (*rawptr << 8) | rawptr[1];

	rawptr += 2;
	if (rawptr+pubk->u.fortezza.KEAKey.len > end) { return SECFailure; }
	pubk->u.fortezza.KEAKey.data = 
			(unsigned char*)PORT_ArenaZAlloc(arena,pubk->u.fortezza.KEAKey.len);
	if (pubk->u.fortezza.KEAKey.data == NULL) {
		return SECFailure;
	}
	PORT_Memcpy(pubk->u.fortezza.KEAKey.data,rawptr,
					pubk->u.fortezza.KEAKey.len);
	rawptr += pubk->u.fortezza.KEAKey.len;

	/* shared key */
	if (rawptr >= end) {
	    pubk->u.fortezza.DSSKey.len = pubk->u.fortezza.KEAKey.len;
	    /* this depends on the fact that we are going to get freed with an
	     * ArenaFree call. We cannot free DSSKey and KEAKey separately */
	    pubk->u.fortezza.DSSKey.data=
					pubk->u.fortezza.KEAKey.data;
	    pubk->u.fortezza.DSSpriviledge.len = 
				pubk->u.fortezza.KEApriviledge.len;
	    pubk->u.fortezza.DSSpriviledge.data =
			pubk->u.fortezza.DSSpriviledge.data;
	    goto done;
	}
		

	/* DSS Version is next */
	pubk->u.fortezza.DSSversion = *rawptr++;

	if (*rawptr++ != 2) {
		return SECFailure;
	}

	/* DSSPrivilege (the string up to the first byte with the hi-bit on */
	clearptr = rawptr;
	while ((rawptr < end) && (*rawptr++ & 0x80));
	if (rawptr >= end) { return SECFailure; }
	pubk->u.fortezza.DSSpriviledge.len = rawptr - clearptr;
	pubk->u.fortezza.DSSpriviledge.data = 
		(unsigned char*)PORT_ArenaZAlloc(arena,pubk->u.fortezza.DSSpriviledge.len);
	if (pubk->u.fortezza.DSSpriviledge.data == NULL) {
		return SECFailure;
	}
	PORT_Memcpy(pubk->u.fortezza.DSSpriviledge.data,clearptr,
				pubk->u.fortezza.DSSpriviledge.len);

	/* finally copy the DSS key. The next to bytes are the key length,
	 *  and the key follows */
	pubk->u.fortezza.DSSKey.len = (*rawptr << 8) | rawptr[1];

	rawptr += 2;
	if (rawptr+pubk->u.fortezza.DSSKey.len > end){ return SECFailure; }
	pubk->u.fortezza.DSSKey.data = 
			(unsigned char*)PORT_ArenaZAlloc(arena,pubk->u.fortezza.DSSKey.len);
	if (pubk->u.fortezza.DSSKey.data == NULL) {
		return SECFailure;
	}
	PORT_Memcpy(pubk->u.fortezza.DSSKey.data,rawptr,
					pubk->u.fortezza.DSSKey.len);

	/* ok, now we decode the parameters */
done:

        return SECKEY_FortezzaDecodePQGtoOld(arena, pubk, params);
}


/* Function used to determine what kind of cert we are dealing with. */
KeyType 
CERT_GetCertKeyType (CERTSubjectPublicKeyInfo *spki) {
    int tag;
    KeyType keyType;

    tag = SECOID_GetAlgorithmTag(&spki->algorithm);
    switch (tag) {
      case SEC_OID_X500_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_RSA_ENCRYPTION:
	keyType = rsaKey;
	break;
      case SEC_OID_ANSIX9_DSA_SIGNATURE:
	keyType = dsaKey;
	break;
      case SEC_OID_MISSI_KEA_DSS_OLD:
      case SEC_OID_MISSI_KEA_DSS:
      case SEC_OID_MISSI_DSS_OLD:
      case SEC_OID_MISSI_DSS:  
	keyType = fortezzaKey;
	break;
      case SEC_OID_MISSI_KEA:
      case SEC_OID_MISSI_ALT_KEA:
	keyType = keaKey;
	break;
      case SEC_OID_X942_DIFFIE_HELMAN_KEY:
	keyType = dhKey;
	break;
      case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
	keyType = ecKey;
	break;
      default:
	keyType = nullKey;
    }
    return keyType;
}

static SECKEYPublicKey *
seckey_ExtractPublicKey(CERTSubjectPublicKeyInfo *spki)
{
    SECKEYPublicKey *pubk;
    SECItem os, newOs, newParms;
    SECStatus rv;
    PRArenaPool *arena;
    SECOidTag tag;

    arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	return NULL;

    pubk = (SECKEYPublicKey *) PORT_ArenaZAlloc(arena, sizeof(SECKEYPublicKey));
    if (pubk == NULL) {
	PORT_FreeArena (arena, PR_FALSE);
	return NULL;
    }

    pubk->arena = arena;
    pubk->pkcs11Slot = 0;
    pubk->pkcs11ID = CK_INVALID_HANDLE;


    /* Convert bit string length from bits to bytes */
    os = spki->subjectPublicKey;
    DER_ConvertBitString (&os);

    tag = SECOID_GetAlgorithmTag(&spki->algorithm);

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newOs, &os);
    if ( rv == SECSuccess )
    switch ( tag ) {
      case SEC_OID_X500_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_RSA_ENCRYPTION:
	pubk->keyType = rsaKey;
	prepare_rsa_pub_key_for_asn1(pubk);        
        rv = SEC_QuickDERDecodeItem(arena, pubk, SECKEY_RSAPublicKeyTemplate, &newOs);
	if (rv == SECSuccess)
	    return pubk;
	break;
      case SEC_OID_ANSIX9_DSA_SIGNATURE:
      case SEC_OID_SDN702_DSA_SIGNATURE:
	pubk->keyType = dsaKey;
	prepare_dsa_pub_key_for_asn1(pubk);
	rv = SEC_QuickDERDecodeItem(arena, pubk, SECKEY_DSAPublicKeyTemplate, &newOs);
	if (rv != SECSuccess) break;

        rv = SECKEY_DSADecodePQG(arena, pubk,
                                 &spki->algorithm.parameters); 

	if (rv == SECSuccess) return pubk;
	break;
      case SEC_OID_X942_DIFFIE_HELMAN_KEY:
	pubk->keyType = dhKey;
	prepare_dh_pub_key_for_asn1(pubk);
	rv = SEC_QuickDERDecodeItem(arena, pubk, SECKEY_DHPublicKeyTemplate, &newOs);
	if (rv != SECSuccess) break;

        /* copy the DER into the arena, since Quick DER returns data that points
           into the DER input, which may get freed by the caller */
        rv = SECITEM_CopyItem(arena, &newParms, &spki->algorithm.parameters);
        if ( rv != SECSuccess )
            break;

        rv = SEC_QuickDERDecodeItem(arena, pubk, SECKEY_DHParamKeyTemplate,
                                 &newParms); 

	if (rv == SECSuccess) return pubk;
	break;
      case SEC_OID_MISSI_KEA_DSS_OLD:
      case SEC_OID_MISSI_KEA_DSS:
      case SEC_OID_MISSI_DSS_OLD:
      case SEC_OID_MISSI_DSS:
	pubk->keyType = fortezzaKey;
	rv = SECKEY_FortezzaDecodeCertKey(arena, pubk, &newOs,
				          &spki->algorithm.parameters);
	if (rv == SECSuccess)
	    return pubk;
	break;

      case SEC_OID_MISSI_KEA:
	pubk->keyType = keaKey;

	prepare_kea_pub_key_for_asn1(pubk);
        rv = SEC_QuickDERDecodeItem(arena, pubk,
                                SECKEY_KEAPublicKeyTemplate, &newOs);
        if (rv != SECSuccess) break;

        /* copy the DER into the arena, since Quick DER returns data that points
           into the DER input, which may get freed by the caller */
        rv = SECITEM_CopyItem(arena, &newParms, &spki->algorithm.parameters);
        if ( rv != SECSuccess )
            break;

        rv = SEC_QuickDERDecodeItem(arena, pubk, SECKEY_KEAParamsTemplate,
                        &newParms);

	if (rv == SECSuccess)
	    return pubk;

        break;

      case SEC_OID_MISSI_ALT_KEA:
	pubk->keyType = keaKey;

        rv = SECITEM_CopyItem(arena,&pubk->u.kea.publicValue,&newOs);
        if (rv != SECSuccess) break;
 
        /* copy the DER into the arena, since Quick DER returns data that points
           into the DER input, which may get freed by the caller */
        rv = SECITEM_CopyItem(arena, &newParms, &spki->algorithm.parameters);
        if ( rv != SECSuccess )
            break;

        rv = SEC_QuickDERDecodeItem(arena, pubk, SECKEY_KEAParamsTemplate,
                        &newParms);

	if (rv == SECSuccess)
	    return pubk;

        break;

#ifdef NSS_ENABLE_ECC
      case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
	pubk->keyType = ecKey;
	pubk->u.ec.size = 0;

	/* Since PKCS#11 directly takes the DER encoding of EC params
	 * and public value, we don't need any decoding here.
	 */
        rv = SECITEM_CopyItem(arena, &pubk->u.ec.DEREncodedParams, 
	    &spki->algorithm.parameters);
        if ( rv != SECSuccess )
            break;
        rv = SECITEM_CopyItem(arena, &pubk->u.ec.publicValue, &newOs);
	if (rv == SECSuccess) return pubk;
	break;
#endif /* NSS_ENABLE_ECC */

      default:
	rv = SECFailure;
	break;
    }

    SECKEY_DestroyPublicKey (pubk);
    return NULL;
}


/* required for JSS */
SECKEYPublicKey *
SECKEY_ExtractPublicKey(CERTSubjectPublicKeyInfo *spki)
{
    return seckey_ExtractPublicKey(spki);
}

SECKEYPublicKey *
CERT_ExtractPublicKey(CERTCertificate *cert)
{
    SECStatus rv;

    if (!cert) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    rv = SECKEY_UpdateCertPQG(cert);
    if (rv != SECSuccess) return NULL;

    return seckey_ExtractPublicKey(&cert->subjectPublicKeyInfo);
}

/*
 * Get the public key for the fortezza KMID. NOTE this requires the
 * PQG paramters to be set. We probably should have a fortezza call that 
 * just extracts the kmid for us directly so this function can work
 * without having the whole cert chain
 */
SECKEYPublicKey *
CERT_KMIDPublicKey(CERTCertificate *cert)
{
    return seckey_ExtractPublicKey(&cert->subjectPublicKeyInfo);
}

int
SECKEY_ECParamsToKeySize(const SECItem *encodedParams)
{
    SECOidTag tag;
    SECItem oid = { siBuffer, NULL, 0};
	
    /* The encodedParams data contains 0x06 (SEC_ASN1_OBJECT_ID),
     * followed by the length of the curve oid and the curve oid.
     */
    oid.len = encodedParams->data[1];
    oid.data = encodedParams->data + 2;
    if ((tag = SECOID_FindOIDTag(&oid)) == SEC_OID_UNKNOWN)
	return 0;

    switch (tag) {
    case SEC_OID_SECG_EC_SECP112R1:
    case SEC_OID_SECG_EC_SECP112R2:
        return 112;

    case SEC_OID_SECG_EC_SECT113R1:
    case SEC_OID_SECG_EC_SECT113R2:
	return 113;

    case SEC_OID_SECG_EC_SECP128R1:
    case SEC_OID_SECG_EC_SECP128R2:
	return 128;

    case SEC_OID_SECG_EC_SECT131R1:
    case SEC_OID_SECG_EC_SECT131R2:
	return 131;

    case SEC_OID_SECG_EC_SECP160K1:
    case SEC_OID_SECG_EC_SECP160R1:
    case SEC_OID_SECG_EC_SECP160R2:
	return 160;

    case SEC_OID_SECG_EC_SECT163K1:
    case SEC_OID_SECG_EC_SECT163R1:
    case SEC_OID_SECG_EC_SECT163R2:
    case SEC_OID_ANSIX962_EC_C2PNB163V1:
    case SEC_OID_ANSIX962_EC_C2PNB163V2:
    case SEC_OID_ANSIX962_EC_C2PNB163V3:
	return 163;

    case SEC_OID_ANSIX962_EC_C2PNB176V1:
	return 176;

    case SEC_OID_ANSIX962_EC_C2TNB191V1:
    case SEC_OID_ANSIX962_EC_C2TNB191V2:
    case SEC_OID_ANSIX962_EC_C2TNB191V3:
    case SEC_OID_ANSIX962_EC_C2ONB191V4:
    case SEC_OID_ANSIX962_EC_C2ONB191V5:
	return 191;

    case SEC_OID_SECG_EC_SECP192K1:
    case SEC_OID_ANSIX962_EC_PRIME192V1:
    case SEC_OID_ANSIX962_EC_PRIME192V2:
    case SEC_OID_ANSIX962_EC_PRIME192V3:
	return 192;

    case SEC_OID_SECG_EC_SECT193R1:
    case SEC_OID_SECG_EC_SECT193R2:
	return 193;

    case SEC_OID_ANSIX962_EC_C2PNB208W1:
	return 208;

    case SEC_OID_SECG_EC_SECP224K1:
    case SEC_OID_SECG_EC_SECP224R1:
	return 224;

    case SEC_OID_SECG_EC_SECT233K1:
    case SEC_OID_SECG_EC_SECT233R1:
	return 233;

    case SEC_OID_SECG_EC_SECT239K1:
    case SEC_OID_ANSIX962_EC_C2TNB239V1:
    case SEC_OID_ANSIX962_EC_C2TNB239V2:
    case SEC_OID_ANSIX962_EC_C2TNB239V3:
    case SEC_OID_ANSIX962_EC_C2ONB239V4:
    case SEC_OID_ANSIX962_EC_C2ONB239V5:
    case SEC_OID_ANSIX962_EC_PRIME239V1:
    case SEC_OID_ANSIX962_EC_PRIME239V2:
    case SEC_OID_ANSIX962_EC_PRIME239V3:
	return 239;

    case SEC_OID_SECG_EC_SECP256K1:
    case SEC_OID_ANSIX962_EC_PRIME256V1:
	return 256;

    case SEC_OID_ANSIX962_EC_C2PNB272W1:
	return 272;

    case SEC_OID_SECG_EC_SECT283K1:
    case SEC_OID_SECG_EC_SECT283R1:
	return 283;

    case SEC_OID_ANSIX962_EC_C2PNB304W1:
	return 304;

    case SEC_OID_ANSIX962_EC_C2TNB359V1:
	return 359;

    case SEC_OID_ANSIX962_EC_C2PNB368W1:
	return 368;

    case SEC_OID_SECG_EC_SECP384R1:
	return 384;

    case SEC_OID_SECG_EC_SECT409K1:
    case SEC_OID_SECG_EC_SECT409R1:
	return 409;

    case SEC_OID_ANSIX962_EC_C2TNB431R1:
	return 431;

    case SEC_OID_SECG_EC_SECP521R1:
	return 521;

    case SEC_OID_SECG_EC_SECT571K1:
    case SEC_OID_SECG_EC_SECT571R1:
	return 571;

    default:
	    return 0;
    }
}

/* returns key strength in bytes (not bits) */
unsigned
SECKEY_PublicKeyStrength(SECKEYPublicKey *pubk)
{
    unsigned char b0;

    /* interpret modulus length as key strength... in
     * fortezza that's the public key length */

    switch (pubk->keyType) {
    case rsaKey:
    	b0 = pubk->u.rsa.modulus.data[0];
    	return b0 ? pubk->u.rsa.modulus.len : pubk->u.rsa.modulus.len - 1;
    case dsaKey:
    	b0 = pubk->u.dsa.publicValue.data[0];
    	return b0 ? pubk->u.dsa.publicValue.len :
	    pubk->u.dsa.publicValue.len - 1;
    case dhKey:
    	b0 = pubk->u.dh.publicValue.data[0];
    	return b0 ? pubk->u.dh.publicValue.len :
	    pubk->u.dh.publicValue.len - 1;
    case fortezzaKey:
	return PR_MAX(pubk->u.fortezza.KEAKey.len, pubk->u.fortezza.DSSKey.len);
#ifdef NSS_ENABLE_ECC
    case ecKey:
	/* Get the key size in bits and adjust */
	if (pubk->u.ec.size == 0) {
	    pubk->u.ec.size = 
		SECKEY_ECParamsToKeySize(&pubk->u.ec.DEREncodedParams);
	} 
	return (pubk->u.ec.size + 7)/8;
#endif /* NSS_ENABLE_ECC */
    default:
	break;
    }
    return 0;
}

/* returns key strength in bits */
unsigned
SECKEY_PublicKeyStrengthInBits(SECKEYPublicKey *pubk)
{
    switch (pubk->keyType) {
    case rsaKey:
    case dsaKey:
    case dhKey:
    case fortezzaKey:
	return SECKEY_PublicKeyStrength(pubk) * 8; /* 1 byte = 8 bits */
#ifdef NSS_ENABLE_ECC
    case ecKey:
	if (pubk->u.ec.size == 0) {
	    pubk->u.ec.size = 
		SECKEY_ECParamsToKeySize(&pubk->u.ec.DEREncodedParams);
	} 
	return pubk->u.ec.size;
#endif /* NSS_ENABLE_ECC */
    default:
	break;
    }
    return 0;
}

SECKEYPrivateKey *
SECKEY_CopyPrivateKey(SECKEYPrivateKey *privk)
{
    SECKEYPrivateKey *copyk;
    PRArenaPool *arena;
    
    if (privk == NULL) {
	return NULL;
    }
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    copyk = (SECKEYPrivateKey *) PORT_ArenaZAlloc (arena, sizeof (SECKEYPrivateKey));
    if (copyk) {
	copyk->arena = arena;
	copyk->keyType = privk->keyType;

	/* copy the PKCS #11 parameters */
	copyk->pkcs11Slot = PK11_ReferenceSlot(privk->pkcs11Slot);
	/* if the key we're referencing was a temparary key we have just
	 * created, that we want to go away when we're through, we need
	 * to make a copy of it */
	if (privk->pkcs11IsTemp) {
	    copyk->pkcs11ID = 
			PK11_CopyKey(privk->pkcs11Slot,privk->pkcs11ID);
	    if (copyk->pkcs11ID == CK_INVALID_HANDLE) goto fail;
	} else {
	    copyk->pkcs11ID = privk->pkcs11ID;
	}
	copyk->pkcs11IsTemp = privk->pkcs11IsTemp;
	copyk->wincx = privk->wincx;
	return copyk;
    } else {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
    }

fail:
    PORT_FreeArena (arena, PR_FALSE);
    return NULL;
}

SECKEYPublicKey *
SECKEY_CopyPublicKey(SECKEYPublicKey *pubk)
{
    SECKEYPublicKey *copyk;
    PRArenaPool *arena;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    copyk = (SECKEYPublicKey *) PORT_ArenaZAlloc (arena, sizeof (SECKEYPublicKey));
    if (copyk != NULL) {
	SECStatus rv = SECSuccess;

	copyk->arena = arena;
	copyk->keyType = pubk->keyType;
	if (pubk->pkcs11Slot && 
			PK11_IsPermObject(pubk->pkcs11Slot,pubk->pkcs11ID)) {
	    copyk->pkcs11Slot = PK11_ReferenceSlot(pubk->pkcs11Slot);
	    copyk->pkcs11ID = pubk->pkcs11ID;
	} else {
	    copyk->pkcs11Slot = NULL;	/* go get own reference */
	    copyk->pkcs11ID = CK_INVALID_HANDLE;
	}
	switch (pubk->keyType) {
	  case rsaKey:
	    rv = SECITEM_CopyItem(arena, &copyk->u.rsa.modulus,
				  &pubk->u.rsa.modulus);
	    if (rv == SECSuccess) {
		rv = SECITEM_CopyItem (arena, &copyk->u.rsa.publicExponent,
				       &pubk->u.rsa.publicExponent);
		if (rv == SECSuccess)
		    return copyk;
	    }
	    break;
	  case dsaKey:
	    rv = SECITEM_CopyItem(arena, &copyk->u.dsa.publicValue,
				  &pubk->u.dsa.publicValue);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.dsa.params.prime,
				  &pubk->u.dsa.params.prime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.dsa.params.subPrime,
				  &pubk->u.dsa.params.subPrime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.dsa.params.base,
				  &pubk->u.dsa.params.base);
	    break;
          case keaKey:
            rv = SECITEM_CopyItem(arena, &copyk->u.kea.publicValue,
                                  &pubk->u.kea.publicValue);
            if (rv != SECSuccess) break;
            rv = SECITEM_CopyItem(arena, &copyk->u.kea.params.hash,
                                  &pubk->u.kea.params.hash);
            break;
	  case fortezzaKey:
	    copyk->u.fortezza.KEAversion = pubk->u.fortezza.KEAversion;
	    copyk->u.fortezza.DSSversion = pubk->u.fortezza.DSSversion;
	    PORT_Memcpy(copyk->u.fortezza.KMID, pubk->u.fortezza.KMID,
			sizeof(pubk->u.fortezza.KMID));
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.clearance, 
				  &pubk->u.fortezza.clearance);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.KEApriviledge, 
				&pubk->u.fortezza.KEApriviledge);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.DSSpriviledge, 
				&pubk->u.fortezza.DSSpriviledge);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.KEAKey, 
				&pubk->u.fortezza.KEAKey);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.DSSKey, 
				&pubk->u.fortezza.DSSKey);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.params.prime, 
				  &pubk->u.fortezza.params.prime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.params.subPrime, 
				&pubk->u.fortezza.params.subPrime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.params.base, 
				&pubk->u.fortezza.params.base);
            if (rv != SECSuccess) break;
            rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.keaParams.prime, 
				  &pubk->u.fortezza.keaParams.prime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.keaParams.subPrime, 
				&pubk->u.fortezza.keaParams.subPrime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.fortezza.keaParams.base, 
				&pubk->u.fortezza.keaParams.base);
	    break;
	  case dhKey:
            rv = SECITEM_CopyItem(arena,&copyk->u.dh.prime,&pubk->u.dh.prime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena,&copyk->u.dh.base,&pubk->u.dh.base);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &copyk->u.dh.publicValue, 
				&pubk->u.dh.publicValue);
	    break;
#ifdef NSS_ENABLE_ECC
	  case ecKey:
	    copyk->u.ec.size = pubk->u.ec.size;
            rv = SECITEM_CopyItem(arena,&copyk->u.ec.DEREncodedParams,
		&pubk->u.ec.DEREncodedParams);
	    if (rv != SECSuccess) break;
            rv = SECITEM_CopyItem(arena,&copyk->u.ec.publicValue,
		&pubk->u.ec.publicValue);
	    break;
#endif /* NSS_ENABLE_ECC */
	  case nullKey:
	    return copyk;
	  default:
	    rv = SECFailure;
	    break;
	}
	if (rv == SECSuccess)
	    return copyk;

	SECKEY_DestroyPublicKey (copyk);
    } else {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
    }

    PORT_FreeArena (arena, PR_FALSE);
    return NULL;
}


SECKEYPublicKey *
SECKEY_ConvertToPublicKey(SECKEYPrivateKey *privk)
{
    SECKEYPublicKey *pubk;
    PRArenaPool *arena;
    CERTCertificate *cert;
    SECStatus rv;

    /*
     * First try to look up the cert.
     */
    cert = PK11_GetCertFromPrivateKey(privk);
    if (cert) {
	pubk = CERT_ExtractPublicKey(cert);
	CERT_DestroyCertificate(cert);
	return pubk;
    }

    /* couldn't find the cert, build pub key by hand */
    arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }
    pubk = (SECKEYPublicKey *)PORT_ArenaZAlloc(arena,
						   sizeof (SECKEYPublicKey));
    if (pubk == NULL) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
    pubk->keyType = privk->keyType;
    pubk->pkcs11Slot = NULL;
    pubk->pkcs11ID = CK_INVALID_HANDLE;
    pubk->arena = arena;

    /*
     * fortezza is at the head of this switch, since we don't want to
     * allocate an arena... CERT_ExtractPublicKey will to that for us.
     */
    switch(privk->keyType) {
      case fortezzaKey:
      case nullKey:
      case dhKey:
      case dsaKey:
	/* Nothing to query, if the cert isn't there, we're done -- no way
	 * to get the public key */
	break;
      case rsaKey:
	rv = PK11_ReadAttribute(privk->pkcs11Slot,privk->pkcs11ID,
				CKA_MODULUS,arena,&pubk->u.rsa.modulus);
	if (rv != SECSuccess)  break;
	rv = PK11_ReadAttribute(privk->pkcs11Slot,privk->pkcs11ID,
			CKA_PUBLIC_EXPONENT,arena,&pubk->u.rsa.publicExponent);
	if (rv != SECSuccess)  break;
	return pubk;
	break;
    default:
	break;
    }

    PORT_FreeArena (arena, PR_FALSE);
    return NULL;
}

CERTSubjectPublicKeyInfo *
SECKEY_CreateSubjectPublicKeyInfo(SECKEYPublicKey *pubk)
{
    CERTSubjectPublicKeyInfo *spki;
    PRArenaPool *arena;
    SECItem params = { siBuffer, NULL, 0 };

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    spki = (CERTSubjectPublicKeyInfo *) PORT_ArenaZAlloc(arena, sizeof (*spki));
    if (spki != NULL) {
	SECStatus rv;
	SECItem *rv_item;
	
	spki->arena = arena;
	switch(pubk->keyType) {
	  case rsaKey:
	    rv = SECOID_SetAlgorithmID(arena, &spki->algorithm,
				     SEC_OID_PKCS1_RSA_ENCRYPTION, 0);
	    if (rv == SECSuccess) {
		/*
		 * DER encode the public key into the subjectPublicKeyInfo.
		 */
		prepare_rsa_pub_key_for_asn1(pubk);
		rv_item = SEC_ASN1EncodeItem(arena, &spki->subjectPublicKey,
					     pubk, SECKEY_RSAPublicKeyTemplate);
		if (rv_item != NULL) {
		    /*
		     * The stored value is supposed to be a BIT_STRING,
		     * so convert the length.
		     */
		    spki->subjectPublicKey.len <<= 3;
		    /*
		     * We got a good one; return it.
		     */
		    return spki;
		}
	    }
	    break;
	  case dsaKey:
	    /* DER encode the params. */
	    prepare_pqg_params_for_asn1(&pubk->u.dsa.params);
	    rv_item = SEC_ASN1EncodeItem(arena, &params, &pubk->u.dsa.params,
					 SECKEY_PQGParamsTemplate);
	    if (rv_item != NULL) {
		rv = SECOID_SetAlgorithmID(arena, &spki->algorithm,
					   SEC_OID_ANSIX9_DSA_SIGNATURE,
					   &params);
		if (rv == SECSuccess) {
		    /*
		     * DER encode the public key into the subjectPublicKeyInfo.
		     */
		    prepare_dsa_pub_key_for_asn1(pubk);
		    rv_item = SEC_ASN1EncodeItem(arena, &spki->subjectPublicKey,
						 pubk,
						 SECKEY_DSAPublicKeyTemplate);
		    if (rv_item != NULL) {
			/*
			 * The stored value is supposed to be a BIT_STRING,
			 * so convert the length.
			 */
			spki->subjectPublicKey.len <<= 3;
			/*
			 * We got a good one; return it.
			 */
			return spki;
		    }
		}
	    }
	    SECITEM_FreeItem(&params, PR_FALSE);
	    break;
#ifdef NSS_ENABLE_ECC
	  case ecKey:
	    rv = SECITEM_CopyItem(arena, &params, 
				  &pubk->u.ec.DEREncodedParams);
	    if (rv != SECSuccess) break;

	    rv = SECOID_SetAlgorithmID(arena, &spki->algorithm,
				       SEC_OID_ANSIX962_EC_PUBLIC_KEY,
				       &params);
	    if (rv != SECSuccess) break;

	    rv = SECITEM_CopyItem(arena, &spki->subjectPublicKey,
				  &pubk->u.ec.publicValue);

	    if (rv == SECSuccess) {
	        /*
		 * The stored value is supposed to be a BIT_STRING,
		 * so convert the length.
		 */
	        spki->subjectPublicKey.len <<= 3;
		/*
		 * We got a good one; return it.
		 */
		return spki;
	    }
	    break;
#endif /* NSS_ENABLE_ECC */
	  case keaKey:
	  case dhKey: /* later... */

	  break;  
	  case fortezzaKey:
#ifdef notdef
	    /* encode the DSS parameters (PQG) */
	    rv = FortezzaBuildParams(&params,pubk);
	    if (rv != SECSuccess) break;

	    /* set the algorithm */
	    rv = SECOID_SetAlgorithmID(arena, &spki->algorithm,
				       SEC_OID_MISSI_KEA_DSS, &params);
	    PORT_Free(params.data);
	    if (rv == SECSuccess) {
		/*
		 * Encode the public key into the subjectPublicKeyInfo.
		 * Fortezza key material is not standard DER
		 */
		rv = FortezzaEncodeCertKey(arena,&spki->subjectPublicKey,pubk);
		if (rv == SECSuccess) {
		    /*
		     * The stored value is supposed to be a BIT_STRING,
		     * so convert the length.
		     */
		    spki->subjectPublicKey.len <<= 3;

		    /*
		     * We got a good one; return it.
		     */
		    return spki;
		}
	    }
#endif
	    break;
	  default:
	    break;
	}
    } else {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    }

    PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

void
SECKEY_DestroySubjectPublicKeyInfo(CERTSubjectPublicKeyInfo *spki)
{
    if (spki && spki->arena) {
	PORT_FreeArena(spki->arena, PR_FALSE);
    }
}

/*
 * this only works for RSA keys... need to do something
 * similiar to CERT_ExtractPublicKey for other key times.
 */
SECKEYPublicKey *
SECKEY_DecodeDERPublicKey(SECItem *pubkder)
{
    PRArenaPool *arena;
    SECKEYPublicKey *pubk;
    SECStatus rv;
    SECItem newPubkder;

    arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    pubk = (SECKEYPublicKey *) PORT_ArenaZAlloc (arena, sizeof (SECKEYPublicKey));
    if (pubk != NULL) {
	pubk->arena = arena;
	pubk->pkcs11Slot = NULL;
	pubk->pkcs11ID = 0;
	prepare_rsa_pub_key_for_asn1(pubk);
        /* copy the DER into the arena, since Quick DER returns data that points
           into the DER input, which may get freed by the caller */
        rv = SECITEM_CopyItem(arena, &newPubkder, pubkder);
        if ( rv == SECSuccess ) {
	    rv = SEC_QuickDERDecodeItem(arena, pubk, SECKEY_RSAPublicKeyTemplate,
				&newPubkder);
        }
	if (rv == SECSuccess)
	    return pubk;
	SECKEY_DestroyPublicKey (pubk);
    } else {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
    }

    PORT_FreeArena (arena, PR_FALSE);
    return NULL;
}

/*
 * Decode a base64 ascii encoded DER encoded public key.
 */
SECKEYPublicKey *
SECKEY_ConvertAndDecodePublicKey(char *pubkstr)
{
    SECKEYPublicKey *pubk;
    SECStatus rv;
    SECItem der;

    rv = ATOB_ConvertAsciiToItem (&der, pubkstr);
    if (rv != SECSuccess)
	return NULL;

    pubk = SECKEY_DecodeDERPublicKey (&der);

    PORT_Free (der.data);
    return pubk;
}

SECItem *
SECKEY_EncodeDERSubjectPublicKeyInfo(SECKEYPublicKey *pubk)
{
    CERTSubjectPublicKeyInfo *spki=NULL;
    SECItem *spkiDER=NULL;

    /* get the subjectpublickeyinfo */
    spki = SECKEY_CreateSubjectPublicKeyInfo(pubk);
    if( spki == NULL ) {
	goto finish;
    }

    /* DER-encode the subjectpublickeyinfo */
    spkiDER = SEC_ASN1EncodeItem(NULL /*arena*/, NULL/*dest*/, spki,
					CERT_SubjectPublicKeyInfoTemplate);
finish:
    if (spki!=NULL) {
	SECKEY_DestroySubjectPublicKeyInfo(spki);
    }
    return spkiDER;
}


CERTSubjectPublicKeyInfo *
SECKEY_DecodeDERSubjectPublicKeyInfo(SECItem *spkider)
{
    PRArenaPool *arena;
    CERTSubjectPublicKeyInfo *spki;
    SECStatus rv;
    SECItem newSpkider;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    spki = (CERTSubjectPublicKeyInfo *)
		PORT_ArenaZAlloc(arena, sizeof (CERTSubjectPublicKeyInfo));
    if (spki != NULL) {
	spki->arena = arena;

        /* copy the DER into the arena, since Quick DER returns data that points
           into the DER input, which may get freed by the caller */
        rv = SECITEM_CopyItem(arena, &newSpkider, spkider);
        if ( rv == SECSuccess ) {
            rv = SEC_QuickDERDecodeItem(arena,spki,
				    CERT_SubjectPublicKeyInfoTemplate, &newSpkider);
        }
	if (rv == SECSuccess)
	    return spki;
	SECKEY_DestroySubjectPublicKeyInfo(spki);
    } else {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    }

    PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

/*
 * Decode a base64 ascii encoded DER encoded subject public key info.
 */
CERTSubjectPublicKeyInfo *
SECKEY_ConvertAndDecodeSubjectPublicKeyInfo(char *spkistr)
{
    CERTSubjectPublicKeyInfo *spki;
    SECStatus rv;
    SECItem der;

    rv = ATOB_ConvertAsciiToItem(&der, spkistr);
    if (rv != SECSuccess)
	return NULL;

    spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&der);

    PORT_Free(der.data);
    return spki;
}

/*
 * Decode a base64 ascii encoded DER encoded public key and challenge
 * Verify digital signature and make sure challenge matches
 */
CERTSubjectPublicKeyInfo *
SECKEY_ConvertAndDecodePublicKeyAndChallenge(char *pkacstr, char *challenge,
								void *wincx)
{
    CERTSubjectPublicKeyInfo *spki = NULL;
    CERTPublicKeyAndChallenge pkac;
    SECStatus rv;
    SECItem signedItem;
    PRArenaPool *arena = NULL;
    CERTSignedData sd;
    SECItem sig;
    SECKEYPublicKey *pubKey = NULL;
    unsigned int len;
    
    signedItem.data = NULL;
    
    /* convert the base64 encoded data to binary */
    rv = ATOB_ConvertAsciiToItem(&signedItem, pkacstr);
    if (rv != SECSuccess) {
	goto loser;
    }

    /* create an arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto loser;
    }

    /* decode the outer wrapping of signed data */
    PORT_Memset(&sd, 0, sizeof(CERTSignedData));
    rv = SEC_QuickDERDecodeItem(arena, &sd, CERT_SignedDataTemplate, &signedItem );
    if ( rv ) {
	goto loser;
    }

    /* decode the public key and challenge wrapper */
    PORT_Memset(&pkac, 0, sizeof(CERTPublicKeyAndChallenge));
    rv = SEC_QuickDERDecodeItem(arena, &pkac, CERT_PublicKeyAndChallengeTemplate, 
			    &sd.data);
    if ( rv ) {
	goto loser;
    }

    /* decode the subject public key info */
    spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&pkac.spki);
    if ( spki == NULL ) {
	goto loser;
    }
    
    /* get the public key */
    pubKey = seckey_ExtractPublicKey(spki);
    if ( pubKey == NULL ) {
	goto loser;
    }

    /* check the signature */
    sig = sd.signature;
    DER_ConvertBitString(&sig);
    rv = VFY_VerifyData(sd.data.data, sd.data.len, pubKey, &sig,
			SECOID_GetAlgorithmTag(&(sd.signatureAlgorithm)), wincx);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    /* check the challenge */
    if ( challenge ) {
	len = PORT_Strlen(challenge);
	/* length is right */
	if ( len != pkac.challenge.len ) {
	    goto loser;
	}
	/* actual data is right */
	if ( PORT_Memcmp(challenge, pkac.challenge.data, len) != 0 ) {
	    goto loser;
	}
    }
    goto done;

loser:
    /* make sure that we return null if we got an error */
    if ( spki ) {
	SECKEY_DestroySubjectPublicKeyInfo(spki);
    }
    spki = NULL;
    
done:
    if ( signedItem.data ) {
	PORT_Free(signedItem.data);
    }
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    if ( pubKey ) {
	SECKEY_DestroyPublicKey(pubKey);
    }
    
    return spki;
}

void
SECKEY_DestroyPrivateKeyInfo(SECKEYPrivateKeyInfo *pvk,
			     PRBool freeit)
{
    PRArenaPool *poolp;

    if(pvk != NULL) {
	if(pvk->arena) {
	    poolp = pvk->arena;
	    /* zero structure since PORT_FreeArena does not support
	     * this yet.
	     */
	    PORT_Memset(pvk->privateKey.data, 0, pvk->privateKey.len);
	    PORT_Memset((char *)pvk, 0, sizeof(*pvk));
	    if(freeit == PR_TRUE) {
		PORT_FreeArena(poolp, PR_TRUE);
	    } else {
		pvk->arena = poolp;
	    }
	} else {
	    SECITEM_ZfreeItem(&pvk->version, PR_FALSE);
	    SECITEM_ZfreeItem(&pvk->privateKey, PR_FALSE);
	    SECOID_DestroyAlgorithmID(&pvk->algorithm, PR_FALSE);
	    PORT_Memset((char *)pvk, 0, sizeof(pvk));
	    if(freeit == PR_TRUE) {
		PORT_Free(pvk);
	    }
	}
    }
}

void
SECKEY_DestroyEncryptedPrivateKeyInfo(SECKEYEncryptedPrivateKeyInfo *epki,
				      PRBool freeit)
{
    PRArenaPool *poolp;

    if(epki != NULL) {
	if(epki->arena) {
	    poolp = epki->arena;
	    /* zero structure since PORT_FreeArena does not support
	     * this yet.
	     */
	    PORT_Memset(epki->encryptedData.data, 0, epki->encryptedData.len);
	    PORT_Memset((char *)epki, 0, sizeof(*epki));
	    if(freeit == PR_TRUE) {
		PORT_FreeArena(poolp, PR_TRUE);
	    } else {
		epki->arena = poolp;
	    }
	} else {
	    SECITEM_ZfreeItem(&epki->encryptedData, PR_FALSE);
	    SECOID_DestroyAlgorithmID(&epki->algorithm, PR_FALSE);
	    PORT_Memset((char *)epki, 0, sizeof(epki));
	    if(freeit == PR_TRUE) {
		PORT_Free(epki);
	    }
	}
    }
}

SECStatus
SECKEY_CopyPrivateKeyInfo(PRArenaPool *poolp,
			  SECKEYPrivateKeyInfo *to,
			  SECKEYPrivateKeyInfo *from)
{
    SECStatus rv = SECFailure;

    if((to == NULL) || (from == NULL)) {
	return SECFailure;
    }

    rv = SECOID_CopyAlgorithmID(poolp, &to->algorithm, &from->algorithm);
    if(rv != SECSuccess) {
	return SECFailure;
    }
    rv = SECITEM_CopyItem(poolp, &to->privateKey, &from->privateKey);
    if(rv != SECSuccess) {
	return SECFailure;
    }
    rv = SECITEM_CopyItem(poolp, &to->version, &from->version);

    return rv;
}

SECStatus
SECKEY_CopyEncryptedPrivateKeyInfo(PRArenaPool *poolp, 
				   SECKEYEncryptedPrivateKeyInfo *to,
				   SECKEYEncryptedPrivateKeyInfo *from)
{
    SECStatus rv = SECFailure;

    if((to == NULL) || (from == NULL)) {
	return SECFailure;
    }

    rv = SECOID_CopyAlgorithmID(poolp, &to->algorithm, &from->algorithm);
    if(rv != SECSuccess) {
	return SECFailure;
    }
    rv = SECITEM_CopyItem(poolp, &to->encryptedData, &from->encryptedData);

    return rv;
}

KeyType
SECKEY_GetPrivateKeyType(SECKEYPrivateKey *privKey)
{
   return privKey->keyType;
}

KeyType
SECKEY_GetPublicKeyType(SECKEYPublicKey *pubKey)
{
   return pubKey->keyType;
}

SECKEYPublicKey*
SECKEY_ImportDERPublicKey(SECItem *derKey, CK_KEY_TYPE type)
{
    SECKEYPublicKey *pubk = NULL;
    SECStatus rv = SECFailure;
    SECItem newDerKey;

    if (!derKey) {
        return NULL;
    } 

    pubk = PORT_ZNew(SECKEYPublicKey);
    if(pubk == NULL) {
        goto finish;
    }
    pubk->arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (NULL == pubk->arena) {
        goto finish;
    }
    rv = SECITEM_CopyItem(pubk->arena, &newDerKey, derKey);
    if (SECSuccess != rv) {
        goto finish;
    }

    pubk->pkcs11Slot = NULL;
    pubk->pkcs11ID = CK_INVALID_HANDLE;

    switch( type ) {
      case CKK_RSA:
	prepare_rsa_pub_key_for_asn1(pubk);
        rv = SEC_QuickDERDecodeItem(pubk->arena, pubk, SECKEY_RSAPublicKeyTemplate, &newDerKey);
        pubk->keyType = rsaKey;
        break;
      case CKK_DSA:
	prepare_dsa_pub_key_for_asn1(pubk);
        rv = SEC_QuickDERDecodeItem(pubk->arena, pubk, SECKEY_DSAPublicKeyTemplate, &newDerKey);
        pubk->keyType = dsaKey;
        break;
      case CKK_DH:
	prepare_dh_pub_key_for_asn1(pubk);
        rv = SEC_QuickDERDecodeItem(pubk->arena, pubk, SECKEY_DHPublicKeyTemplate, &newDerKey);
        pubk->keyType = dhKey;
        break;
      default:
        rv = SECFailure;
        break;
    }

finish:
    if( rv != SECSuccess && pubk != NULL) {
        if (pubk->arena) {
            PORT_FreeArena(pubk->arena, PR_TRUE);
        }
        PORT_Free(pubk);
        pubk = NULL;
    }
    return pubk;
}

SECKEYPrivateKeyList*
SECKEY_NewPrivateKeyList(void)
{
    PRArenaPool *arena = NULL;
    SECKEYPrivateKeyList *ret = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
        goto loser;
    }

    ret = (SECKEYPrivateKeyList *)PORT_ArenaZAlloc(arena,
                sizeof(SECKEYPrivateKeyList));
    if ( ret == NULL ) {
        goto loser;
    }

    ret->arena = arena;

    PR_INIT_CLIST(&ret->list);

    return(ret);

loser:
    if ( arena != NULL ) {
        PORT_FreeArena(arena, PR_FALSE);
    }

    return(NULL);
}

void
SECKEY_DestroyPrivateKeyList(SECKEYPrivateKeyList *keys)
{
    while( !PR_CLIST_IS_EMPTY(&keys->list) ) {
        SECKEY_RemovePrivateKeyListNode(
            (SECKEYPrivateKeyListNode*)(PR_LIST_HEAD(&keys->list)) );
    }

    PORT_FreeArena(keys->arena, PR_FALSE);

    return;
}


void
SECKEY_RemovePrivateKeyListNode(SECKEYPrivateKeyListNode *node)
{
    PR_ASSERT(node->key);
    SECKEY_DestroyPrivateKey(node->key);
    node->key = NULL;
    PR_REMOVE_LINK(&node->links);
    return;

}

SECStatus
SECKEY_AddPrivateKeyToListTail( SECKEYPrivateKeyList *list,
                                SECKEYPrivateKey *key)
{
    SECKEYPrivateKeyListNode *node;

    node = (SECKEYPrivateKeyListNode *)PORT_ArenaZAlloc(list->arena,
                sizeof(SECKEYPrivateKeyListNode));
    if ( node == NULL ) {
        goto loser;
    }

    PR_INSERT_BEFORE(&node->links, &list->list);
    node->key = key;
    return(SECSuccess);

loser:
    return(SECFailure);
}


SECKEYPublicKeyList*
SECKEY_NewPublicKeyList(void)
{
    PRArenaPool *arena = NULL;
    SECKEYPublicKeyList *ret = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
        goto loser;
    }

    ret = (SECKEYPublicKeyList *)PORT_ArenaZAlloc(arena,
                sizeof(SECKEYPublicKeyList));
    if ( ret == NULL ) {
        goto loser;
    }

    ret->arena = arena;

    PR_INIT_CLIST(&ret->list);

    return(ret);

loser:
    if ( arena != NULL ) {
        PORT_FreeArena(arena, PR_FALSE);
    }

    return(NULL);
}

void
SECKEY_DestroyPublicKeyList(SECKEYPublicKeyList *keys)
{
    while( !PR_CLIST_IS_EMPTY(&keys->list) ) {
        SECKEY_RemovePublicKeyListNode(
            (SECKEYPublicKeyListNode*)(PR_LIST_HEAD(&keys->list)) );
    }

    PORT_FreeArena(keys->arena, PR_FALSE);

    return;
}


void
SECKEY_RemovePublicKeyListNode(SECKEYPublicKeyListNode *node)
{
    PR_ASSERT(node->key);
    SECKEY_DestroyPublicKey(node->key);
    node->key = NULL;
    PR_REMOVE_LINK(&node->links);
    return;

}

SECStatus
SECKEY_AddPublicKeyToListTail( SECKEYPublicKeyList *list,
                                SECKEYPublicKey *key)
{
    SECKEYPublicKeyListNode *node;

    node = (SECKEYPublicKeyListNode *)PORT_ArenaZAlloc(list->arena,
                sizeof(SECKEYPublicKeyListNode));
    if ( node == NULL ) {
        goto loser;
    }

    PR_INSERT_BEFORE(&node->links, &list->list);
    node->key = key;
    return(SECSuccess);

loser:
    return(SECFailure);
}
