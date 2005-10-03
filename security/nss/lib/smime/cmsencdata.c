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

/*
 * CMS encryptedData methods.
 *
 * $Id: cmsencdata.c,v 1.8 2005/10/03 22:01:57 relyea%netscape.com Exp $
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
NSS_CMSEncryptedData_Create(NSSCMSMessage *cmsg, SECOidTag algorithm, int keysize)
{
    void *mark;
    NSSCMSEncryptedData *encd;
    PLArenaPool *poolp;
    SECAlgorithmID *pbe_algid;
    SECStatus rv;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    encd = (NSSCMSEncryptedData *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSEncryptedData));
    if (encd == NULL)
	goto loser;

    encd->cmsg = cmsg;

    /* version is set in NSS_CMSEncryptedData_Encode_BeforeStart() */

    switch (algorithm) {
    /* XXX hmmm... hardcoded algorithms? */
    case SEC_OID_RC2_CBC:
    case SEC_OID_DES_EDE3_CBC:
    case SEC_OID_DES_CBC:
	rv = NSS_CMSContentInfo_SetContentEncAlg(poolp, &(encd->contentInfo), algorithm, NULL, keysize);
	break;
    default:
	/* Assume password-based-encryption.  At least, try that. */
	pbe_algid = PK11_CreatePBEAlgorithmID(algorithm, 1, NULL);
	if (pbe_algid == NULL) {
	    rv = SECFailure;
	    break;
	}
	rv = NSS_CMSContentInfo_SetContentEncAlgID(poolp, &(encd->contentInfo), pbe_algid, keysize);
	SECOID_DestroyAlgorithmID (pbe_algid, PR_TRUE);
	break;
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

    cinfo = &(encd->contentInfo);

    /* find bulkkey and algorithm - must have been set by NSS_CMSEncryptedData_Encode_BeforeStart */
    bulkkey = NSS_CMSContentInfo_GetBulkKey(cinfo);
    if (bulkkey == NULL)
	return SECFailure;
    algid = NSS_CMSContentInfo_GetContentEncAlg(cinfo);
    if (algid == NULL)
	return SECFailure;

    /* this may modify algid (with IVs generated in a token).
     * it is therefore essential that algid is a pointer to the "real" contentEncAlg,
     * not just to a copy */
    cinfo->ciphcx = NSS_CMSCipherContext_StartEncrypt(encd->cmsg->poolp, bulkkey, algid);
    PK11_FreeSymKey(bulkkey);
    if (cinfo->ciphcx == NULL)
	return SECFailure;

    return SECSuccess;
}

/*
 * NSS_CMSEncryptedData_Encode_AfterData - finalize this encryptedData for encoding
 */
SECStatus
NSS_CMSEncryptedData_Encode_AfterData(NSSCMSEncryptedData *encd)
{
    if (encd->contentInfo.ciphcx) {
	NSS_CMSCipherContext_Destroy(encd->contentInfo.ciphcx);
	encd->contentInfo.ciphcx = NULL;
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

    cinfo->ciphcx = NSS_CMSCipherContext_StartDecrypt(bulkkey, bulkalg);
    if (cinfo->ciphcx == NULL)
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
    if (encd->contentInfo.ciphcx) {
	NSS_CMSCipherContext_Destroy(encd->contentInfo.ciphcx);
	encd->contentInfo.ciphcx = NULL;
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
