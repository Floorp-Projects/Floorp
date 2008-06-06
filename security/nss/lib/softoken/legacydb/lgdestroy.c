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
/*
 * Internal PKCS #11 functions. Should only be called by pkcs11.c
 */
#include "pkcs11.h"
#include "lgdb.h"
#include "pcert.h"
#include "lowkeyi.h"

/*
 * remove an object.
 */
CK_RV
lg_DestroyObject(SDB *sdb, CK_OBJECT_HANDLE object_id)
{
    CK_RV crv = CKR_OK;
    SECStatus rv;
    NSSLOWCERTCertificate *cert;
    NSSLOWCERTCertTrust tmptrust;
    PRBool isKrl;
    NSSLOWKEYDBHandle *keyHandle;
    NSSLOWCERTCertDBHandle *certHandle;
    const SECItem *dbKey;

    object_id &= ~LG_TOKEN_MASK;
    dbKey = lg_lookupTokenKeyByHandle(sdb,object_id);
    if (dbKey == NULL) {
	return CKR_OBJECT_HANDLE_INVALID;
    }

    /* remove the objects from the real data base */
    switch (object_id & LG_TOKEN_TYPE_MASK) {
    case LG_TOKEN_TYPE_PRIV:
    case LG_TOKEN_TYPE_KEY:
	/* KEYID is the public KEY for DSA and DH, and the MODULUS for
	 *  RSA */
	keyHandle = lg_getKeyDB(sdb);
	if (!keyHandle) {
	    crv = CKR_TOKEN_WRITE_PROTECTED;
	    break;
	}
	rv = nsslowkey_DeleteKey(keyHandle, dbKey);
	if (rv != SECSuccess) {
	    crv = CKR_DEVICE_ERROR;
	}
	break;
    case LG_TOKEN_TYPE_PUB:
	break; /* public keys only exist at the behest of the priv key */
    case LG_TOKEN_TYPE_CERT:
	certHandle = lg_getCertDB(sdb);
	if (!certHandle) {
	    crv = CKR_TOKEN_WRITE_PROTECTED;
	    break;
	}
	cert = nsslowcert_FindCertByKey(certHandle,dbKey);
	if (cert == NULL) {
	    crv = CKR_DEVICE_ERROR;
	    break;
	}
	rv = nsslowcert_DeletePermCertificate(cert);
	if (rv != SECSuccess) {
	    crv = CKR_DEVICE_ERROR;
	}
	nsslowcert_DestroyCertificate(cert);
	break;
    case LG_TOKEN_TYPE_CRL:
	certHandle = lg_getCertDB(sdb);
	if (!certHandle) {
	    crv = CKR_TOKEN_WRITE_PROTECTED;
	    break;
	}
	isKrl = (PRBool) (object_id == LG_TOKEN_KRL_HANDLE);
	rv = nsslowcert_DeletePermCRL(certHandle, dbKey, isKrl);
	if (rv == SECFailure) crv = CKR_DEVICE_ERROR;
	break;
    case LG_TOKEN_TYPE_TRUST:
	certHandle = lg_getCertDB(sdb);
	if (!certHandle) {
	    crv = CKR_TOKEN_WRITE_PROTECTED;
	    break;
	}
	cert = nsslowcert_FindCertByKey(certHandle, dbKey);
	if (cert == NULL) {
	    crv = CKR_DEVICE_ERROR;
	    break;
	}
	tmptrust = *cert->trust;
	tmptrust.sslFlags &= CERTDB_PRESERVE_TRUST_BITS;
	tmptrust.emailFlags &= CERTDB_PRESERVE_TRUST_BITS;
	tmptrust.objectSigningFlags &= CERTDB_PRESERVE_TRUST_BITS;
	tmptrust.sslFlags |= CERTDB_TRUSTED_UNKNOWN;
	tmptrust.emailFlags |= CERTDB_TRUSTED_UNKNOWN;
	tmptrust.objectSigningFlags |= CERTDB_TRUSTED_UNKNOWN;
	rv = nsslowcert_ChangeCertTrust(certHandle, cert, &tmptrust);
	if (rv != SECSuccess) crv = CKR_DEVICE_ERROR;
	nsslowcert_DestroyCertificate(cert);
	break;
    default:
	break;
    }
    lg_DBLock(sdb);
    lg_deleteTokenKeyByHandle(sdb,object_id);
    lg_DBUnlock(sdb);

    return crv;
}



