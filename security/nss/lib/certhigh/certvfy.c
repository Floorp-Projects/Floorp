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
#include "nspr.h"
#include "secerr.h"
#include "secport.h"
#include "seccomon.h"
#include "secoid.h"
#include "sslerr.h"
#include "genname.h"
#include "keyhi.h"
#include "cert.h"
#include "certdb.h"
#include "cryptohi.h"

#ifndef NSS_3_4_CODE
#define NSS_3_4_CODE
#endif /* NSS_3_4_CODE */
#include "nsspki.h"
#include "pkitm.h"
#include "pkim.h"
#include "pkinss3hack.h"
#include "base.h"

#define PENDING_SLOP (24L*60L*60L)

/*
 * WARNING - this function is depricated, and will go away in the near future.
 *	It has been superseded by CERT_CheckCertValidTimes().
 *
 * Check the validity times of a certificate
 */
SECStatus
CERT_CertTimesValid(CERTCertificate *c)
{
    int64 now, notBefore, notAfter, pendingSlop;
    SECStatus rv;
    
    /* if cert is already marked OK, then don't bother to check */
    if ( c->timeOK ) {
	return(SECSuccess);
    }
    
    /* get current UTC time */
    now = PR_Now();
    rv = CERT_GetCertTimes(c, &notBefore, &notAfter);
    
    if (rv) {
	return(SECFailure);
    }
    
    LL_I2L(pendingSlop, PENDING_SLOP);
    LL_SUB(notBefore, notBefore, pendingSlop);

    if (LL_CMP(now, <, notBefore) || LL_CMP(now, >, notAfter)) {
	PORT_SetError(SEC_ERROR_EXPIRED_CERTIFICATE);
	return(SECFailure);
    }

    return(SECSuccess);
}

/*
 * verify the signature of a signed data object with the given certificate
 */
SECStatus
CERT_VerifySignedData(CERTSignedData *sd, CERTCertificate *cert,
		      int64 t, void *wincx)
{
    SECItem sig;
    SECKEYPublicKey *pubKey = 0;
    SECStatus rv;
    SECCertTimeValidity validity;
    SECOidTag algid;

    /* check the certificate's validity */
    validity = CERT_CheckCertValidTimes(cert, t, PR_FALSE);
    if ( validity != secCertTimeValid ) {
	return(SECFailure);
    }

    /* get cert's public key */
    pubKey = CERT_ExtractPublicKey(cert);
    if ( !pubKey ) {
	return(SECFailure);
    }
    
    /* check the signature */
    sig = sd->signature;
    DER_ConvertBitString(&sig);

    algid = SECOID_GetAlgorithmTag(&sd->signatureAlgorithm);
    rv = VFY_VerifyData(sd->data.data, sd->data.len, pubKey, &sig,
			algid, wincx);

    SECKEY_DestroyPublicKey(pubKey);

    if ( rv ) {
	return(SECFailure);
    }

    return(SECSuccess);
}


/*
 * This must only be called on a cert that is known to have an issuer
 * with an invalid time
 */
CERTCertificate *
CERT_FindExpiredIssuer(CERTCertDBHandle *handle, CERTCertificate *cert)
{
    CERTCertificate *issuerCert = NULL;
    CERTCertificate *subjectCert;
    int              count;
    SECStatus        rv;
    SECComparison    rvCompare;
    
    subjectCert = CERT_DupCertificate(cert);
    if ( subjectCert == NULL ) {
	goto loser;
    }
    
    for ( count = 0; count < CERT_MAX_CERT_CHAIN; count++ ) {
	/* find the certificate of the issuer */
	issuerCert = CERT_FindCertByName(handle, &subjectCert->derIssuer);
    
	if ( ! issuerCert ) {
	    goto loser;
	}

	rv = CERT_CertTimesValid(issuerCert);
	if ( rv == SECFailure ) {
	    /* this is the invalid issuer */
	    CERT_DestroyCertificate(subjectCert);
	    return(issuerCert);
	}

	/* make sure that the issuer is not self signed.  If it is, then
	 * stop here to prevent looping.
	 */
	rvCompare = SECITEM_CompareItem(&issuerCert->derSubject,
				 &issuerCert->derIssuer);
	if (rvCompare == SECEqual) {
	    PORT_Assert(0);		/* No expired issuer! */
	    goto loser;
	}
	CERT_DestroyCertificate(subjectCert);
	subjectCert = issuerCert;
    }

loser:
    if ( issuerCert ) {
	CERT_DestroyCertificate(issuerCert);
    }
    
    if ( subjectCert ) {
	CERT_DestroyCertificate(subjectCert);
    }
    
    return(NULL);
}

/* Software FORTEZZA installation hack. The software fortezza installer does
 * not have access to the krl and cert.db file. Accept FORTEZZA Certs without
 * KRL's in this case. 
 */
static int dont_use_krl = 0;
/* not a public exposed function... */
void sec_SetCheckKRLState(int value) { dont_use_krl = value; }

SECStatus
SEC_CheckKRL(CERTCertDBHandle *handle,SECKEYPublicKey *key,
	     CERTCertificate *rootCert, int64 t, void * wincx)
{
    CERTSignedCrl *crl = NULL;
    SECStatus rv = SECFailure;
    SECStatus rv2;
    CERTCrlEntry **crlEntry;
    SECCertTimeValidity validity;
    CERTCertificate *issuerCert = NULL;

    if (dont_use_krl) return SECSuccess;

    /* first look up the KRL */
    crl = SEC_FindCrlByName(handle,&rootCert->derSubject, SEC_KRL_TYPE);
    if (crl == NULL) {
	PORT_SetError(SEC_ERROR_NO_KRL);
	goto done;
    }

    /* get the issuing certificate */
    issuerCert = CERT_FindCertByName(handle, &crl->crl.derName);
    if (issuerCert == NULL) {
        PORT_SetError(SEC_ERROR_KRL_BAD_SIGNATURE);
        goto done;
    }


    /* now verify the KRL signature */
    rv2 = CERT_VerifySignedData(&crl->signatureWrap, issuerCert, t, wincx);
    if (rv2 != SECSuccess) {
	PORT_SetError(SEC_ERROR_KRL_BAD_SIGNATURE);
    	goto done;
    }

    /* Verify the date validity of the KRL */
    validity = SEC_CheckCrlTimes(&crl->crl, t);
    if (validity == secCertTimeExpired) {
	PORT_SetError(SEC_ERROR_KRL_EXPIRED);
	goto done;
    }

    /* now make sure the key in this cert is still valid */
    if (key->keyType != fortezzaKey) {
	PORT_SetError(SSL_ERROR_BAD_CERT_DOMAIN);
	goto done; /* This should be an assert? */
    }

    /* now make sure the key is not on the revocation list */
    for (crlEntry = crl->crl.entries; crlEntry && *crlEntry; crlEntry++) {
	if (PORT_Memcmp((*crlEntry)->serialNumber.data,
				key->u.fortezza.KMID,
				    (*crlEntry)->serialNumber.len) == 0) {
	    PORT_SetError(SEC_ERROR_REVOKED_KEY);
	    goto done;
	}
    }
    rv = SECSuccess;

done:
    if (issuerCert) CERT_DestroyCertificate(issuerCert);
    if (crl) SEC_DestroyCrl(crl);
    return rv;
}

SECStatus
SEC_CheckCRL(CERTCertDBHandle *handle,CERTCertificate *cert,
	     CERTCertificate *caCert, int64 t, void * wincx)
{
    CERTSignedCrl *crl = NULL;
    SECStatus rv = SECSuccess;
    CERTCrlEntry **crlEntry;
    SECCertTimeValidity validity;

    /* first look up the CRL */
    crl = SEC_FindCrlByName(handle,&caCert->derSubject, SEC_CRL_TYPE);
    if (crl == NULL) {
	/* XXX for now no CRL is ok */
	goto done;
    }

    /* now verify the CRL signature */
    rv = CERT_VerifySignedData(&crl->signatureWrap, caCert, t, wincx);
    if (rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_CRL_BAD_SIGNATURE);
        rv = SECWouldBlock; /* Soft error, ask the user */
    	goto done;
    }

    /* Verify the date validity of the KRL */
    validity = SEC_CheckCrlTimes(&crl->crl,t);
    if (validity == secCertTimeExpired) {
	PORT_SetError(SEC_ERROR_CRL_EXPIRED);
        rv = SECWouldBlock; /* Soft error, ask the user */
    } else if (validity == secCertTimeNotValidYet) {
	PORT_SetError(SEC_ERROR_CRL_NOT_YET_VALID);
	rv = SECWouldBlock; /* Soft error, ask the user */
    }

    /* now make sure the key is not on the revocation list */
    for (crlEntry = crl->crl.entries; crlEntry && *crlEntry; crlEntry++) {
	if (SECITEM_CompareItem(&(*crlEntry)->serialNumber,&cert->serialNumber) == SECEqual) {
	    PORT_SetError(SEC_ERROR_REVOKED_CERTIFICATE);
	    rv = SECFailure; /* cert is revoked */
	    goto done;
	}
    }

done:
    if (crl) SEC_DestroyCrl(crl);
    return rv;
}

/*
 * Find the issuer of a cert.  Use the authorityKeyID if it exists.
 */
CERTCertificate *
CERT_FindCertIssuer(CERTCertificate *cert, int64 validTime, SECCertUsage usage)
{
#ifndef NSS_SOFTOKEN_MODULE
    CERTAuthKeyID *   authorityKeyID = NULL;  
    CERTCertificate * issuerCert     = NULL;
    SECItem *         caName;
    PRArenaPool       *tmpArena = NULL;
    SECItem           issuerCertKey;
    SECStatus         rv;

    tmpArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( !tmpArena ) {
	goto loser;
    }
    authorityKeyID = CERT_FindAuthKeyIDExten(tmpArena,cert);

    if ( authorityKeyID != NULL ) {
	/* has the authority key ID extension */
	if ( authorityKeyID->keyID.data != NULL ) {
	    /* extension contains a key ID, so lookup based on it */
	    issuerCert = CERT_FindCertByKeyID(cert->dbhandle, &cert->derIssuer,
					      &authorityKeyID->keyID);
	    if ( issuerCert == NULL ) {
		PORT_SetError (SEC_ERROR_UNKNOWN_ISSUER);
		goto loser;
	    }
	    
	} else if ( authorityKeyID->authCertIssuer != NULL ) {
	    /* no key ID, so try issuer and serial number */
	    caName = (SECItem*)CERT_GetGeneralNameByType(authorityKeyID->authCertIssuer,
					       		 certDirectoryName, PR_TRUE);

	    /*
	     * caName is NULL when the authCertIssuer field is not
	     * being used, or other name form is used instead.
	     * If the directoryName format and serialNumber fields are
	     * used, we use them to find the CA cert.
	     * Note:
	     *	By the time it gets here, we known for sure that if the
	     *	authCertIssuer exists, then the authCertSerialNumber
	     *	must also exists (CERT_DecodeAuthKeyID() ensures this).
	     *	We don't need to check again. 
	     */

	    if (caName != NULL) {
		rv = CERT_KeyFromIssuerAndSN(tmpArena, caName,
					     &authorityKeyID->authCertSerialNumber,
					     &issuerCertKey);
		if ( rv == SECSuccess ) {
		    issuerCert = CERT_FindCertByKey(cert->dbhandle,
						    &issuerCertKey);
		}
		
		if ( issuerCert == NULL ) {
		    PORT_SetError (SEC_ERROR_UNKNOWN_ISSUER);
		    goto loser;
		}
	    }
	}
    }
    if ( issuerCert == NULL ) {
	/* if there is not authorityKeyID, then try to find the issuer */
	/* find a valid CA cert with correct usage */
	issuerCert = CERT_FindMatchingCert(cert->dbhandle,
					   &cert->derIssuer,
					   certOwnerCA, usage, PR_TRUE,
					   validTime, PR_TRUE);

	/* if that fails, then fall back to grabbing any cert with right name*/
	if ( issuerCert == NULL ) {
	    issuerCert = CERT_FindCertByName(cert->dbhandle, &cert->derIssuer);
	    if ( issuerCert == NULL ) {
		PORT_SetError (SEC_ERROR_UNKNOWN_ISSUER);
	    }
	}
    }

loser:
    if (tmpArena != NULL) {
	PORT_FreeArena(tmpArena, PR_FALSE);
	tmpArena = NULL;
    }

    return(issuerCert);
#else
    NSSCertificate *me;
    NSSTime *nssTime;
    NSSUsage nssUsage;
    NSSCertificate *issuer;
    PRStatus status;
    me = STAN_GetNSSCertificate(cert);
    nssTime = NSSTime_SetPRTime(NULL, validTime);
    nssUsage.anyUsage = PR_FALSE;
    nssUsage.nss3usage = usage;
    nssUsage.nss3lookingForCA = PR_TRUE;
    (void)NSSCertificate_BuildChain(me, nssTime, &nssUsage, NULL, 
                                    &issuer, 1, NULL, &status);
    nss_ZFreeIf(nssTime);
    if (status == PR_SUCCESS) {
	CERTCertificate *rvc = STAN_GetCERTCertificate(issuer);
	return rvc;
    }
    return NULL;
#endif
}

/*
 * return required trust flags for various cert usages for CAs
 */
SECStatus
CERT_TrustFlagsForCACertUsage(SECCertUsage usage,
			      unsigned int *retFlags,
			      SECTrustType *retTrustType)
{
    unsigned int requiredFlags;
    SECTrustType trustType;

    switch ( usage ) {
      case certUsageSSLClient:
	requiredFlags = CERTDB_TRUSTED_CLIENT_CA;
	trustType = trustSSL;
        break;
      case certUsageSSLServer:
      case certUsageSSLCA:
	requiredFlags = CERTDB_TRUSTED_CA;
	trustType = trustSSL;
        break;
      case certUsageSSLServerWithStepUp:
	requiredFlags = CERTDB_TRUSTED_CA | CERTDB_GOVT_APPROVED_CA;
	trustType = trustSSL;
        break;
      case certUsageEmailSigner:
      case certUsageEmailRecipient:
	requiredFlags = CERTDB_TRUSTED_CA;
	trustType = trustEmail;
	break;
      case certUsageObjectSigner:
	requiredFlags = CERTDB_TRUSTED_CA;
	trustType = trustObjectSigning;
	break;
      case certUsageVerifyCA:
      case certUsageAnyCA:
      case certUsageStatusResponder:
	requiredFlags = CERTDB_TRUSTED_CA;
	trustType = trustTypeNone;
	break;
      default:
	PORT_Assert(0);
	goto loser;
    }
    if ( retFlags != NULL ) {
	*retFlags = requiredFlags;
    }
    if ( retTrustType != NULL ) {
	*retTrustType = trustType;
    }
    
    return(SECSuccess);
loser:
    return(SECFailure);
}





static void
AddToVerifyLog(CERTVerifyLog *log, CERTCertificate *cert, unsigned long error,
	       unsigned int depth, void *arg)
{
    CERTVerifyLogNode *node, *tnode;

    PORT_Assert(log != NULL);
    
    node = (CERTVerifyLogNode *)PORT_ArenaAlloc(log->arena,
						sizeof(CERTVerifyLogNode));
    if ( node != NULL ) {
	node->cert = CERT_DupCertificate(cert);
	node->error = error;
	node->depth = depth;
	node->arg = arg;
	
	if ( log->tail == NULL ) {
	    /* empty list */
	    log->head = log->tail = node;
	    node->prev = NULL;
	    node->next = NULL;
	} else if ( depth >= log->tail->depth ) {
	    /* add to tail */
	    node->prev = log->tail;
	    log->tail->next = node;
	    log->tail = node;
	    node->next = NULL;
	} else if ( depth < log->head->depth ) {
	    /* add at head */
	    node->prev = NULL;
	    node->next = log->head;
	    log->head->prev = node;
	    log->head = node;
	} else {
	    /* add in middle */
	    tnode = log->tail;
	    while ( tnode != NULL ) {
		if ( depth >= tnode->depth ) {
		    /* insert after tnode */
		    node->prev = tnode;
		    node->next = tnode->next;
		    tnode->next->prev = node;
		    tnode->next = node;
		    break;
		}

		tnode = tnode->prev;
	    }
	}

	log->count++;
    }
    return;
}

#define EXIT_IF_NOT_LOGGING(log) \
    if ( log == NULL ) { \
	goto loser; \
    }

#define LOG_ERROR_OR_EXIT(log,cert,depth,arg) \
    if ( log != NULL ) { \
	AddToVerifyLog(log, cert, PORT_GetError(), depth, (void *)arg); \
    } else { \
	goto loser; \
    }

#define LOG_ERROR(log,cert,depth,arg) \
    if ( log != NULL ) { \
	AddToVerifyLog(log, cert, PORT_GetError(), depth, (void *)arg); \
    }




SECStatus
CERT_VerifyCertChain(CERTCertDBHandle *handle, CERTCertificate *cert,
		     PRBool checkSig, SECCertUsage certUsage, int64 t,
		     void *wincx, CERTVerifyLog *log)
{
    SECTrustType trustType;
    CERTBasicConstraints basicConstraint;
    CERTCertificate *issuerCert = NULL;
    CERTCertificate *subjectCert = NULL;
    CERTCertificate *badCert = NULL;
    PRBool isca;
    PRBool isFortezzaV1 = PR_FALSE;
    SECStatus rv;
    SECComparison rvCompare;
    SECStatus rvFinal = SECSuccess;
    int count;
    int currentPathLen = -1;
    int flags;
    unsigned int caCertType;
    unsigned int requiredCAKeyUsage;
    unsigned int requiredFlags;
    PRArenaPool *arena = NULL;
    CERTGeneralName *namesList = NULL;
    CERTGeneralName *subjectNameList = NULL;
    SECItem *namesIndex = NULL;
    int namesIndexLen = 10;
    int namesCount = 0;

    enum { cbd_None, cbd_User, cbd_CA } last_type = cbd_None;
    SECKEYPublicKey *key;

    if (CERT_KeyUsageAndTypeForCertUsage(certUsage, PR_TRUE,
					 &requiredCAKeyUsage,
					 &caCertType)
	!= SECSuccess ) {
	PORT_Assert(0);
	EXIT_IF_NOT_LOGGING(log);
	requiredCAKeyUsage = 0;
	caCertType = 0;
    }

    switch ( certUsage ) {
      case certUsageSSLClient:
      case certUsageSSLServer:
      case certUsageSSLCA:
      case certUsageSSLServerWithStepUp:
      case certUsageEmailSigner:
      case certUsageEmailRecipient:
      case certUsageObjectSigner:
      case certUsageVerifyCA:
      case certUsageStatusResponder:
	if ( CERT_TrustFlagsForCACertUsage(certUsage, &requiredFlags,
					   &trustType) != SECSuccess ) {
	    PORT_Assert(0);
	    EXIT_IF_NOT_LOGGING(log);
	    requiredFlags = 0;
	    trustType = trustSSL;
	}
	break;
      default:
	PORT_Assert(0);
	EXIT_IF_NOT_LOGGING(log);
	requiredFlags = 0;
	trustType = trustSSL;/* This used to be 0, but we need something
			      * that matches the enumeration type.
			      */
	caCertType = 0;
    }
    
    subjectCert = CERT_DupCertificate(cert);
    if ( subjectCert == NULL ) {
	goto loser;
    }

    /* determine if the cert is fortezza. Getting the key is an easy
     * way to determine it, especially since we need to get the privillege
     * from the key anyway.
     */
    key = CERT_ExtractPublicKey(cert);

    if (key != NULL) {
	isFortezzaV1 = (PRBool)(key->keyType == fortezzaKey);

	/* find out what type of cert we are starting with */
	if (isFortezzaV1) {
	    unsigned char priv = 0;;

	    rv = SEC_CheckKRL(handle, key, NULL, t, wincx);
	    if (rv == SECFailure) {
		/**** PORT_SetError is already set by SEC_CheckKRL **/
		SECKEY_DestroyPublicKey(key);
		/**** Bob - should we log and continue when logging? **/
		LOG_ERROR(log,subjectCert,0,0);
		goto loser;
	    }                

	    if (key->u.fortezza.DSSpriviledge.len > 0) {
		priv = key->u.fortezza.DSSpriviledge.data[0];
	    }

	    last_type = (priv & 0x30) ? cbd_CA : cbd_User;
	}
		
	SECKEY_DestroyPublicKey(key);
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto loser;
    }

    namesIndex = (SECItem *) PORT_ZAlloc(sizeof(SECItem) * namesIndexLen);
    if (namesIndex == NULL) {
	goto loser;
    }

    for ( count = 0; count < CERT_MAX_CERT_CHAIN; count++ ) {
	int subjectNameListLen;
	int i;

	/* Construct a list of names for the current and all previous certifcates 
	   to be verified against the name constraints extension of the issuer
	   certificate. */
	subjectNameList = CERT_GetCertificateNames(subjectCert, arena);
	subjectNameListLen = CERT_GetNamesLength(subjectNameList);
	for (i = 0; i < subjectNameListLen; i++) {
	    if (namesIndexLen < namesCount + i) {
		namesIndexLen = namesIndexLen * 2;
		namesIndex = (SECItem *) PORT_Realloc(namesIndex, namesIndexLen * 
						       sizeof(SECItem));
		if (namesIndex == NULL) {
		    goto loser;
		}
	    }
	    rv = SECITEM_CopyItem(arena, &(namesIndex[namesCount + i]), &(subjectCert->derSubject));
	}
	namesCount += subjectNameListLen;
	namesList = cert_CombineNamesLists(namesList, subjectNameList);

	/* find the certificate of the issuer */
	issuerCert = CERT_FindCertIssuer(subjectCert, t, certUsage);
	if ( ! issuerCert ) {
	    PORT_SetError(SEC_ERROR_UNKNOWN_ISSUER);
	    LOG_ERROR(log,subjectCert,count,0);
	    goto loser;
	}

	/* verify the signature on the cert */
	if ( checkSig ) {
	    rv = CERT_VerifySignedData(&subjectCert->signatureWrap,
				       issuerCert, t, wincx);
    
	    if ( rv != SECSuccess ) {
		if ( PORT_GetError() == SEC_ERROR_EXPIRED_CERTIFICATE ) {
		    PORT_SetError(SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE);
		    LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
		} else {
		    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
		    LOG_ERROR_OR_EXIT(log,subjectCert,count,0);
		}
	    }
	}

	/*
	 * XXX - fortezza may need error logging stuff added
	 */
	if (isFortezzaV1) {
	    unsigned char priv = 0;

	    /* read the key */
	    key = CERT_ExtractPublicKey(issuerCert);

	    /* Cant' get Key? fail. */
	    if (key == NULL) {
	    	PORT_SetError(SEC_ERROR_BAD_KEY);
		LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
		goto fortezzaDone;
	    }


	    /* if the issuer is not an old fortezza cert, we bail */
	    if (key->keyType != fortezzaKey) {
	    	SECKEY_DestroyPublicKey(key);
		/* CA Cert not fortezza */
	    	PORT_SetError(SEC_ERROR_NOT_FORTEZZA_ISSUER);
		LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
		goto fortezzaDone;
	    }

	    /* get the privilege mask */
	    if (key->u.fortezza.DSSpriviledge.len > 0) {
		priv = key->u.fortezza.DSSpriviledge.data[0];
	    }

	    /*
	     * make sure the CA's keys are OK
	     */
            
	    rv = SEC_CheckKRL(handle, key, NULL, t, wincx);
	    if (rv != SECSuccess) {
	    	SECKEY_DestroyPublicKey(key);
		LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
		goto fortezzaDone;
		/** fall through looking for more stuff **/
	    } else {
	        SECKEY_DestroyPublicKey(key);
	    }

	    switch (last_type) {
	      case cbd_User:
		/* first check for subordination */
		/*rv = FortezzaSubordinateCheck(cert,issuerCert);*/
		rv = SECSuccess;

		/* now check for issuer privilege */
		if ((rv != SECSuccess) || ((priv & 0x10) == 0)) {
		    /* bail */
		    PORT_SetError (SEC_ERROR_CA_CERT_INVALID);
		    LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
		}
		break;
	      case cbd_CA:
	      case cbd_None:
		if ((priv & 0x20) == 0) {
		    /* bail */
		    PORT_SetError (SEC_ERROR_CA_CERT_INVALID);
		    LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
		}
		break;
	      default:
		/* bail */ /* shouldn't ever happen */
	    	PORT_SetError(SEC_ERROR_UNKNOWN_ISSUER);
		LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
	    }

fortezzaDone:
	    last_type =  cbd_CA;
	}

	/* If the basicConstraint extension is included in an immediate CA
	 * certificate, make sure that the isCA flag is on.  If the
	 * pathLenConstraint component exists, it must be greater than the
	 * number of CA certificates we have seen so far.  If the extension
	 * is omitted, we will assume that this is a CA certificate with
	 * an unlimited pathLenConstraint (since it already passes the
	 * netscape-cert-type extension checking).
	 *
	 * In the fortezza (V1) case, we've already checked the CA bits
	 * in the key, so we're presumed to be a CA; however we really don't
	 * want to bypass Basic constraint or netscape extension parsing.
         * 
         * In Fortezza V2, basicConstraint will be set for every CA,PCA,PAA
	 */

	rv = CERT_FindBasicConstraintExten(issuerCert, &basicConstraint);
	if ( rv != SECSuccess ) {
	    if (PORT_GetError() != SEC_ERROR_EXTENSION_NOT_FOUND) {
		LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
	    } else {
		currentPathLen = CERT_UNLIMITED_PATH_CONSTRAINT;
	    }
	    /* no basic constraints found, if we're fortezza, CA bit is already
	     * verified (isca = PR_TRUE). otherwise, we aren't (yet) a ca
	     * isca = PR_FALSE */
	    isca = isFortezzaV1;
	} else  {
	    if ( basicConstraint.isCA == PR_FALSE ) {
		PORT_SetError (SEC_ERROR_CA_CERT_INVALID);
		LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
	    }
	    
	    /* make sure that the path len constraint is properly set.
	     */
	    if ( basicConstraint.pathLenConstraint ==
		CERT_UNLIMITED_PATH_CONSTRAINT ) {
		currentPathLen = CERT_UNLIMITED_PATH_CONSTRAINT;
	    } else if ( currentPathLen == CERT_UNLIMITED_PATH_CONSTRAINT ) {
		/* error if the previous CA's path length constraint is
		 * unlimited but its CA's path is not.
		 */
		PORT_SetError (SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID);
		LOG_ERROR_OR_EXIT(log,issuerCert,count+1,basicConstraint.pathLenConstraint);
	    } else if (basicConstraint.pathLenConstraint > currentPathLen) {
		currentPathLen = basicConstraint.pathLenConstraint;
	    } else {
		PORT_SetError (SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID);
		LOG_ERROR_OR_EXIT(log,issuerCert,count+1,basicConstraint.pathLenConstraint);
	    }

	    isca = PR_TRUE;
	}
	
	/* XXX - the error logging may need to go down into CRL stuff at some
	 * point
	 */
	/* check revoked list (issuer) */
	rv = SEC_CheckCRL(handle, subjectCert, issuerCert, t, wincx);
	if (rv == SECFailure) {
	    LOG_ERROR_OR_EXIT(log,subjectCert,count,0);
	} else if (rv == SECWouldBlock) {
	    /* We found something fishy, so we intend to issue an
	     * error to the user, but the user may wish to continue
	     * processing, in which case we better make sure nothing
	     * worse has happened... so keep cranking the loop */
	    rvFinal = SECFailure;
	    LOG_ERROR(log,subjectCert,count,0);
	}


	if ( issuerCert->trust ) {
	    /*
	     * check the trust parms of the issuer
	     */
	    if ( certUsage == certUsageVerifyCA ) {
		if ( subjectCert->nsCertType & NS_CERT_TYPE_EMAIL_CA ) {
		    trustType = trustEmail;
		} else if ( subjectCert->nsCertType & NS_CERT_TYPE_SSL_CA ) {
		    trustType = trustSSL;
		} else {
		    trustType = trustObjectSigning;
		}
	    }
	    
	    flags = SEC_GET_TRUST_FLAGS(issuerCert->trust, trustType);
	    
	    if ( (flags & CERTDB_VALID_CA) ||
		 (certUsage == certUsageStatusResponder)) {
		if ( ( flags & requiredFlags ) == requiredFlags ||
		     certUsage == certUsageStatusResponder ) {
		    /* we found a trusted one, so return */
		    rv = rvFinal; 
		    goto done;
		}
	    }
	}

	/*
	 * Make sure that if this is an intermediate CA in the chain that
	 * it was given permission by its signer to be a CA.
	 */
	if ( isca ) {
	    /*
	     * if basicConstraints says it is a ca, then we check the
	     * nsCertType.  If the nsCertType has any CA bits set, then
	     * it must have the right one.
	     */
	    if ( issuerCert->nsCertType & NS_CERT_TYPE_CA ) {
		if ( issuerCert->nsCertType & caCertType ) {
		    isca = PR_TRUE;
		} else {
		    isca = PR_FALSE;
		}
	    }
	} else {
	    if ( issuerCert->nsCertType & caCertType ) {
		isca = PR_TRUE;
	    } else {
		isca = PR_FALSE;
	    }
	}
	
	if (  !isca  ) {
	    PORT_SetError(SEC_ERROR_CA_CERT_INVALID);
	    LOG_ERROR_OR_EXIT(log,issuerCert,count+1,0);
	}
	    
	/* make sure key usage allows cert signing */
	if (CERT_CheckKeyUsage(issuerCert, requiredCAKeyUsage) != SECSuccess) {
	    PORT_SetError(SEC_ERROR_INADEQUATE_KEY_USAGE);
	    LOG_ERROR_OR_EXIT(log,issuerCert,count+1,requiredCAKeyUsage);
	}
	/* make sure that the entire chain is within the name space of the current issuer
	 * certificate.
	 */

	badCert = CERT_CompareNameSpace(issuerCert, namesList, namesIndex, arena, handle);
	if (badCert != NULL) {
	    PORT_SetError(SEC_ERROR_CERT_NOT_IN_NAME_SPACE);
            LOG_ERROR_OR_EXIT(log, badCert, count + 1, 0);
	    goto loser;
	}
	/* make sure that the issuer is not self signed.  If it is, then
	 * stop here to prevent looping.
	 */
	rvCompare = SECITEM_CompareItem(&issuerCert->derSubject,
				 &issuerCert->derIssuer);
	if (rvCompare == SECEqual) {
	    PORT_SetError(SEC_ERROR_UNTRUSTED_ISSUER);
	    LOG_ERROR(log, issuerCert, count+1, 0);
	    goto loser;
	}

	CERT_DestroyCertificate(subjectCert);
	subjectCert = issuerCert;
    }

    subjectCert = NULL;
    PORT_SetError(SEC_ERROR_UNKNOWN_ISSUER);
    LOG_ERROR(log,issuerCert,count,0);
loser:
    rv = SECFailure;
done:
    if (namesIndex != NULL) {
	PORT_Free(namesIndex);
    }
    if ( issuerCert ) {
	CERT_DestroyCertificate(issuerCert);
    }
    
    if ( subjectCert ) {
	CERT_DestroyCertificate(subjectCert);
    }

    if ( arena != NULL ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return rv;
}
			
/*
 * verify a certificate by checking if its valid and that we
 * trust the issuer.
 * Note that this routine does not verify the signature of the certificate.
 */
SECStatus
CERT_VerifyCert(CERTCertDBHandle *handle, CERTCertificate *cert,
		PRBool checkSig, SECCertUsage certUsage, int64 t,
		void *wincx, CERTVerifyLog *log)
{
    SECStatus rv;
    unsigned int requiredKeyUsage;
    unsigned int requiredCertType;
    unsigned int flags;
    unsigned int certType;
    PRBool       allowOverride;
    SECCertTimeValidity validity;
    CERTStatusConfig *statusConfig;
    
    /* check if this cert is in the Evil list */
    rv = CERT_CheckForEvilCert(cert);
    if ( rv != SECSuccess ) {
	PORT_SetError(SEC_ERROR_REVOKED_CERTIFICATE);
	LOG_ERROR_OR_EXIT(log,cert,0,0);
    }
    
    /* make sure that the cert is valid at time t */
    allowOverride = (PRBool)((certUsage == certUsageSSLServer) ||
                             (certUsage == certUsageSSLServerWithStepUp));
    validity = CERT_CheckCertValidTimes(cert, t, allowOverride);
    if ( validity != secCertTimeValid ) {
	LOG_ERROR_OR_EXIT(log,cert,0,validity);
    }

    /* check key usage and netscape cert type */
    CERT_GetCertType(cert);
    certType = cert->nsCertType;
    switch ( certUsage ) {
      case certUsageSSLClient:
      case certUsageSSLServer:
      case certUsageSSLServerWithStepUp:
      case certUsageSSLCA:
      case certUsageEmailSigner:
      case certUsageEmailRecipient:
      case certUsageObjectSigner:
      case certUsageStatusResponder:
	rv = CERT_KeyUsageAndTypeForCertUsage(certUsage, PR_FALSE,
					      &requiredKeyUsage,
					      &requiredCertType);
	if ( rv != SECSuccess ) {
	    PORT_Assert(0);
	    EXIT_IF_NOT_LOGGING(log);
	    requiredKeyUsage = 0;
	    requiredCertType = 0;
	}
	break;
      case certUsageVerifyCA:
	requiredKeyUsage = KU_KEY_CERT_SIGN;
	requiredCertType = NS_CERT_TYPE_CA;
	if ( ! ( certType & NS_CERT_TYPE_CA ) ) {
	    certType |= NS_CERT_TYPE_CA;
	}
	break;
      default:
	PORT_Assert(0);
	EXIT_IF_NOT_LOGGING(log);
	requiredKeyUsage = 0;
	requiredCertType = 0;
    }
    if ( CERT_CheckKeyUsage(cert, requiredKeyUsage) != SECSuccess ) {
	PORT_SetError(SEC_ERROR_INADEQUATE_KEY_USAGE);
	LOG_ERROR_OR_EXIT(log,cert,0,requiredKeyUsage);
    }
    if ( !( certType & requiredCertType ) ) {
	PORT_SetError(SEC_ERROR_INADEQUATE_CERT_TYPE);
	LOG_ERROR_OR_EXIT(log,cert,0,requiredCertType);
    }

    /* check trust flags to see if this cert is directly trusted */
    if ( cert->trust ) { /* the cert is in the DB */
	switch ( certUsage ) {
	  case certUsageSSLClient:
	  case certUsageSSLServer:
	    flags = cert->trust->sslFlags;
	    
	    /* is the cert directly trusted or not trusted ? */
	    if ( flags & CERTDB_VALID_PEER ) {/*the trust record is valid*/
		if ( flags & CERTDB_TRUSTED ) {	/* trust this cert */
		    goto winner;
		} else { /* don't trust this cert */
		    PORT_SetError(SEC_ERROR_UNTRUSTED_CERT);
		    LOG_ERROR_OR_EXIT(log,cert,0,flags);
		}
	    }
	    break;
	  case certUsageSSLServerWithStepUp:
	    /* XXX - step up certs can't be directly trusted */
	    break;
	  case certUsageSSLCA:
	    break;
	  case certUsageEmailSigner:
	  case certUsageEmailRecipient:
	    flags = cert->trust->emailFlags;
	    
	    /* is the cert directly trusted or not trusted ? */
	    if ( ( flags & ( CERTDB_VALID_PEER | CERTDB_TRUSTED ) ) ==
		( CERTDB_VALID_PEER | CERTDB_TRUSTED ) ) {
		goto winner;
	    }
	    break;
	  case certUsageObjectSigner:
	    flags = cert->trust->objectSigningFlags;

	    /* is the cert directly trusted or not trusted ? */
	    if ( flags & CERTDB_VALID_PEER ) {/*the trust record is valid*/
		if ( flags & CERTDB_TRUSTED ) {	/* trust this cert */
		    goto winner;
		} else { /* don't trust this cert */
		    PORT_SetError(SEC_ERROR_UNTRUSTED_CERT);
		    LOG_ERROR_OR_EXIT(log,cert,0,flags);
		}
	    }
	    break;
	  case certUsageVerifyCA:
	  case certUsageStatusResponder:
	    flags = cert->trust->sslFlags;
	    /* is the cert directly trusted or not trusted ? */
	    if ( ( flags & ( CERTDB_VALID_CA | CERTDB_TRUSTED_CA ) ) ==
		( CERTDB_VALID_CA | CERTDB_TRUSTED_CA ) ) {
		goto winner;
	    }
	    flags = cert->trust->emailFlags;
	    /* is the cert directly trusted or not trusted ? */
	    if ( ( flags & ( CERTDB_VALID_CA | CERTDB_TRUSTED_CA ) ) ==
		( CERTDB_VALID_CA | CERTDB_TRUSTED_CA ) ) {
		goto winner;
	    }
	    flags = cert->trust->objectSigningFlags;
	    /* is the cert directly trusted or not trusted ? */
	    if ( ( flags & ( CERTDB_VALID_CA | CERTDB_TRUSTED_CA ) ) ==
		( CERTDB_VALID_CA | CERTDB_TRUSTED_CA ) ) {
		goto winner;
	    }
	    break;
	  case certUsageAnyCA:
	  case certUsageProtectedObjectSigner:
	  case certUsageUserCertImport:
	    /* XXX to make the compiler happy.  Should these be
	     * explicitly handled?
	     */
	    break;
	}
    }

    rv = CERT_VerifyCertChain(handle, cert, checkSig, certUsage,
			      t, wincx, log);
    if (rv != SECSuccess) {
	EXIT_IF_NOT_LOGGING(log);
    }

    /*
     * Check revocation status, but only if the cert we are checking
     * is not a status reponder itself.  We only do this in the case
     * where we checked the cert chain (above); explicit trust "wins"
     * (avoids status checking, just as it avoids CRL checking, which
     * is all done inside VerifyCertChain) by bypassing this code.
     */
    statusConfig = CERT_GetStatusConfig(handle);
    if (certUsage != certUsageStatusResponder && statusConfig != NULL) {
	if (statusConfig->statusChecker != NULL) {
	    rv = (* statusConfig->statusChecker)(handle, cert,
							 t, wincx);
	    if (rv != SECSuccess) {
		LOG_ERROR_OR_EXIT(log,cert,0,0);
	    }
	}
    }

winner:
    return(SECSuccess);

loser:
    rv = SECFailure;
    
    return(rv);
}

/*
 * verify a certificate by checking if its valid and that we
 * trust the issuer.  Verify time against now.
 */
SECStatus
CERT_VerifyCertNow(CERTCertDBHandle *handle, CERTCertificate *cert,
		   PRBool checkSig, SECCertUsage certUsage, void *wincx)
{
    return(CERT_VerifyCert(handle, cert, checkSig, 
		   certUsage, PR_Now(), wincx, NULL));
}

/* [ FROM pcertdb.c ] */
/*
 * Supported usage values and types:
 *	certUsageSSLClient
 *	certUsageSSLServer
 *	certUsageSSLServerWithStepUp
 *	certUsageEmailSigner
 *	certUsageEmailRecipient
 *	certUsageObjectSigner
 */

CERTCertificate *
CERT_FindMatchingCert(CERTCertDBHandle *handle, SECItem *derName,
		      CERTCertOwner owner, SECCertUsage usage,
		      PRBool preferTrusted, int64 validTime, PRBool validOnly)
{
    CERTCertList *certList = NULL;
    CERTCertificate *cert = NULL;
    unsigned int requiredTrustFlags;
    SECTrustType requiredTrustType;
    unsigned int flags;
    
    PRBool lookingForCA = PR_FALSE;
    SECStatus rv;
    CERTCertListNode *node;
    CERTCertificate *saveUntrustedCA = NULL;
    
    /* if preferTrusted is set, must be a CA cert */
    PORT_Assert( ! ( preferTrusted && ( owner != certOwnerCA ) ) );
    
    if ( owner == certOwnerCA ) {
	lookingForCA = PR_TRUE;
	if ( preferTrusted ) {
	    rv = CERT_TrustFlagsForCACertUsage(usage, &requiredTrustFlags,
					       &requiredTrustType);
	    if ( rv != SECSuccess ) {
		goto loser;
	    }
	    requiredTrustFlags |= CERTDB_VALID_CA;
	}
    }

    certList = CERT_CreateSubjectCertList(NULL, handle, derName, validTime,
					  validOnly);
    if ( certList != NULL ) {
	rv = CERT_FilterCertListByUsage(certList, usage, lookingForCA);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
	
	node = CERT_LIST_HEAD(certList);
	
	while ( !CERT_LIST_END(node, certList) ) {
	    cert = node->cert;

	    /* looking for a trusted CA cert */
	    if ( ( owner == certOwnerCA ) && preferTrusted &&
		( requiredTrustType != trustTypeNone ) ) {

		if ( cert->trust == NULL ) {
		    flags = 0;
		} else {
		    flags = SEC_GET_TRUST_FLAGS(cert->trust, requiredTrustType);
		}

		if ( ( flags & requiredTrustFlags ) != requiredTrustFlags ) {
		    /* cert is not trusted */
		    /* if this is the first cert to get this far, then save
		     * it, so we can use it if we can't find a trusted one
		     */
		    if ( saveUntrustedCA == NULL ) {
			saveUntrustedCA = cert;
		    }
		    goto endloop;
		}
	    }
	    /* if we got this far, then this cert meets all criteria */
	    break;
	    
endloop:
	    node = CERT_LIST_NEXT(node);
	    cert = NULL;
	}

	/* use the saved one if we have it */
	if ( cert == NULL ) {
	    cert = saveUntrustedCA;
	}

	/* if we found one then bump the ref count before freeing the list */
	if ( cert != NULL ) {
	    /* bump the ref count */
	    cert = CERT_DupCertificate(cert);
	}
	
	CERT_DestroyCertList(certList);
    }

    return(cert);

loser:
    if ( certList != NULL ) {
	CERT_DestroyCertList(certList);
    }

    return(NULL);
}


/* [ From certdb.c ] */
/*
 * Filter a list of certificates, removing those certs that do not have
 * one of the named CA certs somewhere in their cert chain.
 *
 *	"certList" - the list of certificates to filter
 *	"nCANames" - number of CA names
 *	"caNames" - array of CA names in string(rfc 1485) form
 *	"usage" - what use the certs are for, this is used when
 *		selecting CA certs
 */
SECStatus
CERT_FilterCertListByCANames(CERTCertList *certList, int nCANames,
			     char **caNames, SECCertUsage usage)
{
    CERTCertificate *issuerCert = NULL;
    CERTCertificate *subjectCert;
    CERTCertListNode *node, *freenode;
    CERTCertificate *cert;
    int n;
    char **names;
    PRBool found;
    int64 time;
    
    if ( nCANames <= 0 ) {
	return(SECSuccess);
    }

    time = PR_Now();
    
    node = CERT_LIST_HEAD(certList);
    
    while ( ! CERT_LIST_END(node, certList) ) {
	cert = node->cert;
	
	subjectCert = CERT_DupCertificate(cert);

	/* traverse the CA certs for this cert */
	found = PR_FALSE;
	while ( subjectCert != NULL ) {
	    n = nCANames;
	    names = caNames;
	   
            if (subjectCert->issuerName != NULL) { 
	        while ( n > 0 ) {
		    if ( PORT_Strcmp(*names, subjectCert->issuerName) == 0 ) {
		        found = PR_TRUE;
		        break;
		    }

		    n--;
		    names++;
                }
	    }

	    if ( found ) {
		break;
	    }
	    
	    issuerCert = CERT_FindCertIssuer(subjectCert, time, usage);
	    if ( issuerCert == subjectCert ) {
		CERT_DestroyCertificate(issuerCert);
		issuerCert = NULL;
		break;
	    }
	    CERT_DestroyCertificate(subjectCert);
	    subjectCert = issuerCert;

	}
	CERT_DestroyCertificate(subjectCert);
	if ( !found ) {
	    /* CA was not found, so remove this cert from the list */
	    freenode = node;
	    node = CERT_LIST_NEXT(node);
	    CERT_RemoveCertListNode(freenode);
	} else {
	    /* CA was found, so leave it in the list */
	    node = CERT_LIST_NEXT(node);
	}
    }
    
    return(SECSuccess);
}

/*
 * Given a certificate, return a string containing the nickname, and possibly
 * one of the validity strings, based on the current validity state of the
 * certificate.
 *
 * "arena" - arena to allocate returned string from.  If NULL, then heap
 *	is used.
 * "cert" - the cert to get nickname from
 * "expiredString" - the string to append to the nickname if the cert is
 *		expired.
 * "notYetGoodString" - the string to append to the nickname if the cert is
 *		not yet good.
 */
char *
CERT_GetCertNicknameWithValidity(PRArenaPool *arena, CERTCertificate *cert,
				 char *expiredString, char *notYetGoodString)
{
    SECCertTimeValidity validity;
    char *nickname, *tmpstr;
    
    validity = CERT_CheckCertValidTimes(cert, PR_Now(), PR_FALSE);

    /* if the cert is good, then just use the nickname directly */
    if ( validity == secCertTimeValid ) {
	if ( arena == NULL ) {
	    nickname = PORT_Strdup(cert->nickname);
	} else {
	    nickname = PORT_ArenaStrdup(arena, cert->nickname);
	}
	
	if ( nickname == NULL ) {
	    goto loser;
	}
    } else {
	    
	/* if the cert is not valid, then tack one of the strings on the
	 * end
	 */
	if ( validity == secCertTimeExpired ) {
	    tmpstr = PR_smprintf("%s%s", cert->nickname,
				 expiredString);
	} else {
	    /* not yet valid */
	    tmpstr = PR_smprintf("%s%s", cert->nickname,
				 notYetGoodString);
	}
	if ( tmpstr == NULL ) {
	    goto loser;
	}

	if ( arena ) {
	    /* copy the string into the arena and free the malloc'd one */
	    nickname = PORT_ArenaStrdup(arena, tmpstr);
	    PORT_Free(tmpstr);
	} else {
	    nickname = tmpstr;
	}
	if ( nickname == NULL ) {
	    goto loser;
	}
    }    
    return(nickname);

loser:
    return(NULL);
}

/*
 * Collect the nicknames from all certs in a CertList.  If the cert is not
 * valid, append a string to that nickname.
 *
 * "certList" - the list of certificates
 * "expiredString" - the string to append to the nickname of any expired cert
 * "notYetGoodString" - the string to append to the nickname of any cert
 *		that is not yet valid
 */
CERTCertNicknames *
CERT_NicknameStringsFromCertList(CERTCertList *certList, char *expiredString,
				 char *notYetGoodString)
{
    CERTCertNicknames *names;
    PRArenaPool *arena;
    CERTCertListNode *node;
    char **nn;
    
    /* allocate an arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	return(NULL);
    }
    
    /* allocate the structure */
    names = PORT_ArenaAlloc(arena, sizeof(CERTCertNicknames));
    if ( names == NULL ) {
	goto loser;
    }

    /* init the structure */
    names->arena = arena;
    names->head = NULL;
    names->numnicknames = 0;
    names->nicknames = NULL;
    names->totallen = 0;

    /* count the certs in the list */
    node = CERT_LIST_HEAD(certList);
    while ( ! CERT_LIST_END(node, certList) ) {
	names->numnicknames++;
	node = CERT_LIST_NEXT(node);
    }
    
    /* allocate nicknames array */
    names->nicknames = PORT_ArenaAlloc(arena,
				       sizeof(char *) * names->numnicknames);
    if ( names->nicknames == NULL ) {
	goto loser;
    }

    /* just in case printf can't deal with null strings */
    if (expiredString == NULL ) {
	expiredString = "";
    }

    if ( notYetGoodString == NULL ) {
	notYetGoodString = "";
    }
    
    /* traverse the list of certs and collect the nicknames */
    nn = names->nicknames;
    node = CERT_LIST_HEAD(certList);
    while ( ! CERT_LIST_END(node, certList) ) {
	*nn = CERT_GetCertNicknameWithValidity(arena, node->cert,
					       expiredString,
					       notYetGoodString);
	if ( *nn == NULL ) {
	    goto loser;
	}

	names->totallen += PORT_Strlen(*nn);
	
	nn++;
	node = CERT_LIST_NEXT(node);
    }

    return(names);

loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(NULL);
}

/*
 * Extract the nickname from a nickmake string that may have either
 * expiredString or notYetGoodString appended.
 *
 * Args:
 *	"namestring" - the string containing the nickname, and possibly
 *		one of the validity label strings
 *	"expiredString" - the expired validity label string
 *	"notYetGoodString" - the not yet good validity label string
 *
 * Returns the raw nickname
 */
char *
CERT_ExtractNicknameString(char *namestring, char *expiredString,
			   char *notYetGoodString)
{
    int explen, nyglen, namelen;
    int retlen;
    char *retstr;
    
    namelen = PORT_Strlen(namestring);
    explen = PORT_Strlen(expiredString);
    nyglen = PORT_Strlen(notYetGoodString);
    
    if ( namelen > explen ) {
	if ( PORT_Strcmp(expiredString, &namestring[namelen-explen]) == 0 ) {
	    retlen = namelen - explen;
	    retstr = (char *)PORT_Alloc(retlen+1);
	    if ( retstr == NULL ) {
		goto loser;
	    }
	    
	    PORT_Memcpy(retstr, namestring, retlen);
	    retstr[retlen] = '\0';
	    goto done;
	}
    }

    if ( namelen > nyglen ) {
	if ( PORT_Strcmp(notYetGoodString, &namestring[namelen-nyglen]) == 0) {
	    retlen = namelen - nyglen;
	    retstr = (char *)PORT_Alloc(retlen+1);
	    if ( retstr == NULL ) {
		goto loser;
	    }
	    
	    PORT_Memcpy(retstr, namestring, retlen);
	    retstr[retlen] = '\0';
	    goto done;
	}
    }

    /* if name string is shorter than either invalid string, then it must
     * be a raw nickname
     */
    retstr = PORT_Strdup(namestring);
    
done:
    return(retstr);

loser:
    return(NULL);
}

CERTCertList *
CERT_GetCertChainFromCert(CERTCertificate *cert, int64 time, SECCertUsage usage)
{
    CERTCertList *chain = NULL;

    if (NULL == cert) {
        return NULL;
    }
    
    cert = CERT_DupCertificate(cert);
    if (NULL == cert) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    chain = CERT_NewCertList();
    if (NULL == chain) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    while (cert != NULL) {
	if (SECSuccess != CERT_AddCertToListTail(chain, cert)) {
            /* return partial chain */
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            return chain;
        }

	if (SECITEM_CompareItem(&cert->derIssuer, &cert->derSubject)
	    == SECEqual) {
            /* return complete chain */
	    return chain;
	}

	cert = CERT_FindCertIssuer(cert, time, usage);
    }

    /* return partial chain */
    PORT_SetError(SEC_ERROR_UNKNOWN_ISSUER);
    return chain;
}
