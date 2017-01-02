/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS encryptedData methods.
 */

#include "cmslocal.h"

#include "key.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "prtime.h"
#include "secerr.h"
#include "secpkcs5.h"

/*
 * NSS_CMSEncryptedData_Create - create an empty encryptedData object.
 *
 * "algorithm" specifies the bulk encryption algorithm to use.
 * "keysize" is the key size.
 * 
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
NSSCMSEncryptedData *
NSS_CMSEncryptedData_Create(NSSCMSMessage *cmsg, SECOidTag algorithm, 
			    int keysize)
{
    void *mark;
    NSSCMSEncryptedData *encd;
    PLArenaPool *poolp;
    SECAlgorithmID *pbe_algid;
    SECStatus rv;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    encd = PORT_ArenaZNew(poolp, NSSCMSEncryptedData);
    if (encd == NULL)
	goto loser;

    encd->cmsg = cmsg;

    /* version is set in NSS_CMSEncryptedData_Encode_BeforeStart() */

    if (!SEC_PKCS5IsAlgorithmPBEAlgTag(algorithm)) {
	rv = NSS_CMSContentInfo_SetContentEncAlg(poolp, &(encd->contentInfo), 
						 algorithm, NULL, keysize);
    } else {
	/* Assume password-based-encryption.  
	 * Note: we can't generate pkcs5v2 from this interface.
	 * PK11_CreateBPEAlgorithmID generates pkcs5v2 by accepting
	 * non-PBE oids and assuming that they are pkcs5v2 oids, but
	 * NSS_CMSEncryptedData_Create accepts non-PBE oids as regular
	 * CMS encrypted data, so we can't tell NSS_CMS_EncryptedData_Create
	 * to create pkcs5v2 PBEs */
	pbe_algid = PK11_CreatePBEAlgorithmID(algorithm, 1, NULL);
	if (pbe_algid == NULL) {
	    rv = SECFailure;
	} else {
	    rv = NSS_CMSContentInfo_SetContentEncAlgID(poolp, 
				&(encd->contentInfo), pbe_algid, keysize);
	    SECOID_DestroyAlgorithmID (pbe_algid, PR_TRUE);
	}
    }
    if (rv != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return encd;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * NSS_CMSEncryptedData_Destroy - destroy an encryptedData object
 */
void
NSS_CMSEncryptedData_Destroy(NSSCMSEncryptedData *encd)
{
    /* everything's in a pool, so don't worry about the storage */
    NSS_CMSContentInfo_Destroy(&(encd->contentInfo));
    return;
}

/*
 * NSS_CMSEncryptedData_GetContentInfo - return pointer to encryptedData object's contentInfo
 */
NSSCMSContentInfo *
NSS_CMSEncryptedData_GetContentInfo(NSSCMSEncryptedData *encd)
{
    return &(encd->contentInfo);
}

/*
 * NSS_CMSEncryptedData_Encode_BeforeStart - do all the necessary things to a EncryptedData
 *     before encoding begins.
 *
 * In particular:
 *  - set the correct version value.
 *  - get the encryption key
 */
SECStatus
NSS_CMSEncryptedData_Encode_BeforeStart(NSSCMSEncryptedData *encd)
{
    int version;
    PK11SymKey *bulkkey = NULL;
    SECItem *dummy;
    NSSCMSContentInfo *cinfo = &(encd->contentInfo);

    if (NSS_CMSArray_IsEmpty((void **)encd->unprotectedAttr))
	version = NSS_CMS_ENCRYPTED_DATA_VERSION;
    else
	version = NSS_CMS_ENCRYPTED_DATA_VERSION_UPATTR;
    
    dummy = SEC_ASN1EncodeInteger (encd->cmsg->poolp, &(encd->version), version);
    if (dummy == NULL)
	return SECFailure;

    /* now get content encryption key (bulk key) by using our cmsg callback */
    if (encd->cmsg->decrypt_key_cb)
	bulkkey = (*encd->cmsg->decrypt_key_cb)(encd->cmsg->decrypt_key_cb_arg, 
		    NSS_CMSContentInfo_GetContentEncAlg(cinfo));
    if (bulkkey == NULL)
	return SECFailure;

    /* store the bulk key in the contentInfo so that the encoder can find it */
    NSS_CMSContentInfo_SetBulkKey(cinfo, bulkkey);
    PK11_FreeSymKey (bulkkey);

    return SECSuccess;
}

/*
 * NSS_CMSEncryptedData_Encode_BeforeData - set up encryption
 */
SECStatus
NSS_CMSEncryptedData_Encode_BeforeData(NSSCMSEncryptedData *encd)
{
    NSSCMSContentInfo *cinfo;
    PK11SymKey *bulkkey;
    SECAlgorithmID *algid;
    SECStatus rv;

    cinfo = &(encd->contentInfo);

    /* find bulkkey and algorithm - must have been set by NSS_CMSEncryptedData_Encode_BeforeStart */
    bulkkey = NSS_CMSContentInfo_GetBulkKey(cinfo);
    if (bulkkey == NULL)
	return SECFailure;
    algid = NSS_CMSContentInfo_GetContentEncAlg(cinfo);
    if (algid == NULL)
	return SECFailure;

    rv = NSS_CMSContentInfo_Private_Init(cinfo);
    if (rv != SECSuccess) {
	return SECFailure;
    }
    /* this may modify algid (with IVs generated in a token).
     * it is therefore essential that algid is a pointer to the "real" contentEncAlg,
     * not just to a copy */
    cinfo->privateInfo->ciphcx = NSS_CMSCipherContext_StartEncrypt(encd->cmsg->poolp, bulkkey, algid);
    PK11_FreeSymKey(bulkkey);
    if (cinfo->privateInfo->ciphcx == NULL)
	return SECFailure;

    return SECSuccess;
}

/*
 * NSS_CMSEncryptedData_Encode_AfterData - finalize this encryptedData for encoding
 */
SECStatus
NSS_CMSEncryptedData_Encode_AfterData(NSSCMSEncryptedData *encd)
{
    if (encd->contentInfo.privateInfo && encd->contentInfo.privateInfo->ciphcx) {
	NSS_CMSCipherContext_Destroy(encd->contentInfo.privateInfo->ciphcx);
	encd->contentInfo.privateInfo->ciphcx = NULL;
    }

    /* nothing to do after data */
    return SECSuccess;
}


/*
 * NSS_CMSEncryptedData_Decode_BeforeData - find bulk key & set up decryption
 */
SECStatus
NSS_CMSEncryptedData_Decode_BeforeData(NSSCMSEncryptedData *encd)
{
    PK11SymKey *bulkkey = NULL;
    NSSCMSContentInfo *cinfo;
    SECAlgorithmID *bulkalg;
    SECStatus rv = SECFailure;

    cinfo = &(encd->contentInfo);

    bulkalg = NSS_CMSContentInfo_GetContentEncAlg(cinfo);

    if (encd->cmsg->decrypt_key_cb == NULL)	/* no callback? no key../ */
	goto loser;

    bulkkey = (*encd->cmsg->decrypt_key_cb)(encd->cmsg->decrypt_key_cb_arg, bulkalg);
    if (bulkkey == NULL)
	/* no success finding a bulk key */
	goto loser;

    NSS_CMSContentInfo_SetBulkKey(cinfo, bulkkey);

    rv = NSS_CMSContentInfo_Private_Init(cinfo);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = SECFailure;

    cinfo->privateInfo->ciphcx = NSS_CMSCipherContext_StartDecrypt(bulkkey, bulkalg);
    if (cinfo->privateInfo->ciphcx == NULL)
	goto loser;		/* error has been set by NSS_CMSCipherContext_StartDecrypt */


    /* we are done with (this) bulkkey now. */
    PK11_FreeSymKey(bulkkey);

    rv = SECSuccess;

loser:
    return rv;
}

/*
 * NSS_CMSEncryptedData_Decode_AfterData - finish decrypting this encryptedData's content
 */
SECStatus
NSS_CMSEncryptedData_Decode_AfterData(NSSCMSEncryptedData *encd)
{
    if (encd->contentInfo.privateInfo && encd->contentInfo.privateInfo->ciphcx) {
	NSS_CMSCipherContext_Destroy(encd->contentInfo.privateInfo->ciphcx);
	encd->contentInfo.privateInfo->ciphcx = NULL;
    }

    return SECSuccess;
}

/*
 * NSS_CMSEncryptedData_Decode_AfterEnd - finish decoding this encryptedData
 */
SECStatus
NSS_CMSEncryptedData_Decode_AfterEnd(NSSCMSEncryptedData *encd)
{
    /* apply final touches */
    return SECSuccess;
}
