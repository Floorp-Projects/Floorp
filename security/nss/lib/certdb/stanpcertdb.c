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

#include "prtime.h"

#include "cert.h"
#include "mcom_db.h"
#include "certdb.h"
#include "secitem.h"
#include "secder.h"

/* Call to PK11_FreeSlot below */

#include "secasn1.h"
#include "secerr.h"
#include "nssilock.h"
#include "prmon.h"
#include "nsslocks.h"
#include "base64.h"
#include "sechash.h"
#include "plhash.h"
#include "pk11func.h" /* sigh */

#include "cdbhdl.h"

#ifndef NSS_3_4_CODE
#define NSS_3_4_CODE
#endif /* NSS_3_4_CODE */
#include "nsspki.h"
#include "pkit.h"
#include "pkim.h"
#include "pkinss3hack.h"
#include "ckhelper.h"
#include "base.h"

struct stan_cert_callback_str {
    CERTCertCallback callback;
    void *arg;
};

/* Translate from NSSCertificate to CERTCertificate, then pass the latter
 * to a callback.
 */
static PRStatus convert_cert(NSSCertificate *c, void *arg)
{
    CERTCertificate *nss3cert;
    SECStatus secrv;
    struct stan_cert_callback_str *scba = (struct stan_cert_callback_str *)arg;
    nss3cert = STAN_GetCERTCertificate(c);
    if (!nss3cert) return PR_FAILURE;
    secrv = (*scba->callback)(nss3cert, scba->arg);
    CERT_DestroyCertificate(nss3cert);
    return (secrv) ? PR_FAILURE : PR_SUCCESS;
}

struct stan_cert_der_callback_str {
    SECStatus (* callback)(CERTCertificate *cert, SECItem *k, void *pdata);
    void *arg;
};

/* Translate from NSSCertificate to CERTCertificate, then pass the latter
 * to a callback.
 */
static PRStatus convert_cert_der(NSSCertificate *c, void *arg)
{
    CERTCertificate *nss3cert;
    SECStatus secrv;
    SECItem certKey;
    struct stan_cert_der_callback_str *scdba = 
                                     (struct stan_cert_der_callback_str *)arg;
    nss3cert = STAN_GetCERTCertificate(c);
    if (!nss3cert) return PR_FAILURE;
    SECITEM_FROM_NSSITEM(&certKey, &c->encoding);
    secrv = (*scdba->callback)(nss3cert, &certKey, scdba->arg);
    CERT_DestroyCertificate(nss3cert);
    return (secrv) ? PR_FAILURE : PR_SUCCESS;
}


SECStatus
__CERT_TraversePermCertsForSubject(CERTCertDBHandle *handle, 
                                   SECItem *derSubject,
                                   CERTCertCallback cb, void *cbarg)
{
    struct stan_cert_callback_str scba;
    PRStatus nssrv;
    NSSDER subject;
    scba.callback = cb;
    scba.arg = cbarg;
    NSSITEM_FROM_SECITEM(&subject, derSubject);
    nssrv = nssTrustDomain_TraverseCertificatesBySubject(handle,
                                                         &subject,
                                                         convert_cert,
                                                         &scba);
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
}

SECStatus
CERT_TraversePermCertsForSubject(CERTCertDBHandle *handle, SECItem *derSubject,
				 CERTCertCallback cb, void *cbarg)
{
    return(__CERT_TraversePermCertsForSubject(handle, derSubject, cb, cbarg));
}

SECStatus
__CERT_TraversePermCertsForNickname(CERTCertDBHandle *handle, char *nickname,
				    CERTCertCallback cb, void *cbarg)
{
    struct stan_cert_callback_str scba;
    PRStatus nssrv;
    scba.callback = cb;
    scba.arg = cbarg;
    nssrv = nssTrustDomain_TraverseCertificatesByNickname(handle,
                                                          (NSSUTF8 *)nickname,
                                                          convert_cert,
                                                          &scba);
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
}

SECStatus
CERT_TraversePermCertsForNickname(CERTCertDBHandle *handle, char *nickname,
				  CERTCertCallback cb, void *cbarg)
{
    return(__CERT_TraversePermCertsForNickname(handle, nickname, cb, cbarg));
}

PRBool
SEC_CertNicknameConflict(char *nickname, SECItem *derSubject,
			 CERTCertDBHandle *handle)
{
    /* XXX still an issue? */
	return PR_FALSE;
}

SECStatus
SEC_DeletePermCertificate(CERTCertificate *cert)
{
    PRStatus nssrv;
    NSSCertificate *c = STAN_GetNSSCertificate(cert);
    nssrv = NSSCertificate_DeleteStoredObject(c, NULL);
    return SECFailure;
}

SECStatus
SEC_TraversePermCerts(CERTCertDBHandle *handle,
		      SECStatus (* certfunc)(CERTCertificate *cert, SECItem *k,
					    void *pdata),
		      void *udata )
{
    struct stan_cert_der_callback_str scdba;
    PRStatus nssrv;
    scdba.callback = certfunc;
    scdba.arg = udata;
    nssrv = nssTrustDomain_TraverseCertificates(handle, 
                                                convert_cert_der, &scdba);
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
}

void
__CERT_ClosePermCertDB(CERTCertDBHandle *handle)
{
    /* XXX do anything? */
}

void
CERT_ClosePermCertDB(CERTCertDBHandle *handle)
{
    __CERT_ClosePermCertDB(handle);
}

SECStatus
CERT_GetCertTrust(CERTCertificate *cert, CERTCertTrust *trust)
{
    SECStatus rv;
    CERT_LockCertTrust(cert);
    if ( cert->trust == NULL ) {
	rv = SECFailure;
    } else {
	*trust = *cert->trust;
	rv = SECSuccess;
    }
    CERT_UnlockCertTrust(cert);
    return(rv);
}

static char *
cert_parseNickname(char *nickname)
{
    char *cp;
    for (cp=nickname; *cp && *cp != ':'; cp++);
    if (*cp == ':') return cp+1;
    return nickname;
}

/* XXX needs work */
SECStatus
CERT_ChangeCertTrust(CERTCertDBHandle *handle, CERTCertificate *cert,
		    CERTCertTrust *trust)
{
    SECStatus ret;
    CERT_LockDB(handle);
    CERT_LockCertTrust(cert);
    /* only set the trust on permanent certs */
    if ( cert->trust == NULL ) {
	ret = SECFailure;
	goto done;
    }
    if (PK11_IsReadOnly(cert->slot)) {
	char *nickname = cert_parseNickname(cert->nickname);
	/* XXX store it on a writeable token */
	goto done;
    } else {
	STAN_ChangeCertTrust(cert->nssCertificate, trust);
    }
    ret = SECSuccess;
done:
    CERT_UnlockCertTrust(cert);
    CERT_UnlockDB(handle);
    return(ret);
}

SECStatus
CERT_AddTempCertToPerm(CERTCertificate *cert, char *nickname,
		       CERTCertTrust *trust)
{
    NSSCertificate *c = STAN_GetNSSCertificate(cert);
    /* might as well keep these */
    PORT_Assert(cert->istemp);
    PORT_Assert(!cert->isperm);
    if (SEC_CertNicknameConflict(nickname, &cert->derSubject, cert->dbhandle)){
	return SECFailure;
    }
    if (c->nickname && strcmp(nickname, c->nickname) != 0) {
	nss_ZFreeIf(c->nickname);
	c->nickname = nssUTF8_Duplicate((NSSUTF8 *)nickname, c->arena);
	PORT_Free(cert->nickname);
	cert->nickname = PORT_Strdup(nickname);
    }
    /*
    nssTrustDomain_AddTempCertToPerm(c);
    if (memcmp(cert->trust, trust, sizeof (*trust)) != 0) {
    */
	return STAN_ChangeCertTrust(c, trust);
	/*
    }
    */
    return SECFailure;
}

SECStatus
CERT_OpenCertDBFilename(CERTCertDBHandle *handle, char *certdbname,
			PRBool readOnly)
{
    /* XXX what to do here? */
}

/* only for jarver.c */
SECStatus
SEC_AddTempNickname(CERTCertDBHandle *handle, char *nickname,
		    SECItem *subjectName)
{
}

CERTCertificate *
__CERT_NewTempCertificate(CERTCertDBHandle *handle, SECItem *derCert,
			  char *nickname, PRBool isperm, PRBool copyDER)
{
    NSSCertificate *c;
    nssDecodedCert *dc;
    NSSArena *arena;
    CERTCertificate *cc;
    arena = NSSArena_Create();
    if (!arena) {
	return NULL;
    }
    c = NSSCertificate_Create(arena);
    if (!c) {
	goto loser;
    }
    NSSITEM_FROM_SECITEM(&c->encoding, derCert);
    cc = STAN_GetCERTCertificate(c);
    c->arena = arena;
    nssItem_Create(arena, 
                   &c->issuer, cc->derIssuer.len, cc->derIssuer.data);
    nssItem_Create(arena, 
                   &c->subject, cc->derSubject.len, cc->derSubject.data);
    nssItem_Create(arena, 
                   &c->serial, cc->serialNumber.len, cc->serialNumber.data);
    if (nickname) {
	c->nickname = nssUTF8_Create(arena, 
                                     nssStringType_UTF8String, 
                                     (NSSUTF8 *)nickname, 
                                     PORT_Strlen(nickname));
    }
    if (cc->emailAddr) {
	c->email = nssUTF8_Create(arena, 
	                          nssStringType_PrintableString, 
	                          (NSSUTF8 *)cc->emailAddr, 
	                          PORT_Strlen(cc->emailAddr));
    }
    c->trustDomain = handle;
    nssTrustDomain_AddCertsToCache(handle, &c, 1);
    cc->istemp = 1;
    cc->isperm = 0;
    /* XXX if !copyDER destroy it? */
    return cc;
loser:
    nssArena_Destroy(arena);
    return NULL;
}

CERTCertificate *
CERT_NewTempCertificate(CERTCertDBHandle *handle, SECItem *derCert,
			char *nickname, PRBool isperm, PRBool copyDER)
{
    return( __CERT_NewTempCertificate(handle, derCert, nickname,
                                      isperm, copyDER) );
}

/* maybe all the wincx's should be some const for internal token login? */

CERTCertificate *
CERT_FindCertByKey(CERTCertDBHandle *handle, SECItem *certKey)
{
}

/*
 * Lookup a certificate in the databases without locking
 */
CERTCertificate *
CERT_FindCertByKeyNoLocking(CERTCertDBHandle *handle, SECItem *certKey)
{
    return(CERT_FindCertByKey(handle, certKey));
}

CERTCertificate *
CERT_FindCertByIssuerAndSN(CERTCertDBHandle *handle, CERTIssuerAndSN *issuerAndSN)
{
    PK11SlotInfo *slot;
    return PK11_FindCertByIssuerAndSN(&slot, issuerAndSN, NULL);
}

CERTCertificate *
CERT_FindCertByName(CERTCertDBHandle *handle, SECItem *name)
{
    NSSCertificate *c;
    NSSDER subject;
    NSSUsage usage;
    NSSITEM_FROM_SECITEM(&subject, name);
    usage.anyUsage = PR_TRUE;
    c = NSSTrustDomain_FindBestCertificateBySubject(handle, &subject, 
                                                    NULL, &usage, NULL);
    return STAN_GetCERTCertificate(c);
}

/* this one is gonna be tough ... looks like traversal */
CERTCertificate *
CERT_FindCertByKeyID(CERTCertDBHandle *handle, SECItem *name, SECItem *keyID)
{
}

CERTCertificate *
CERT_FindCertByNickname(CERTCertDBHandle *handle, char *nickname)
{
    return PK11_FindCertFromNickname(nickname, NULL);
}

CERTCertificate *
CERT_FindCertByDERCert(CERTCertDBHandle *handle, SECItem *derCert)
{
    NSSCertificate *c;
    NSSDER encoding;
    NSSITEM_FROM_SECITEM(&encoding, derCert);
    c = NSSTrustDomain_FindCertificateByEncodedCertificate(handle, &encoding);
    return STAN_GetCERTCertificate(c);
}

CERTCertificate *
CERT_FindCertByNicknameOrEmailAddr(CERTCertDBHandle *handle, char *name)
{
    CERTCertificate *cc = PK11_FindCertFromNickname(name, NULL);
    if (!cc) {
	NSSCertificate *c;
	NSSUsage usage;
	usage.anyUsage = PR_TRUE;
	c = NSSTrustDomain_FindCertificateByEmail(handle, name,
	                                          NULL, &usage, NULL);
	if (c) {
	    cc = STAN_GetCERTCertificate(c);
	}
    }
    return cc;
}

CERTCertList *
CERT_CreateSubjectCertList(CERTCertList *certList, CERTCertDBHandle *handle,
			   SECItem *name, int64 sorttime, PRBool validOnly)
{
    NSSCertificate **subjectCerts;
    NSSCertificate *c;
    NSSDER subject;
    PRUint32 i;
    SECStatus secrv;
    NSSITEM_FROM_SECITEM(&subject, name);
    subjectCerts = NSSTrustDomain_FindCertificatesBySubject(handle,
                                                            &subject,
                                                            NULL,
                                                            0,
                                                            NULL);
    i = 0;
    if (certList == NULL && subjectCerts) {
	certList = CERT_NewCertList();
	if (!certList) goto loser;
    }
    while ((c = subjectCerts[i++])) {
	CERTCertificate *cc = STAN_GetCERTCertificate(c);
	if (!validOnly ||
	     CERT_CheckCertValidTimes(cc, sorttime, PR_FALSE)
                  == secCertTimeValid) {
	    secrv = CERT_AddCertToListSorted(certList, cc, 
	                                     CERT_SortCBValidity, 
	                                     (void *)&sorttime);
	    if (secrv != SECSuccess) {
		CERT_DestroyCertificate(cc);
		goto loser;
	    }
	} else {
	    CERT_DestroyCertificate(cc);
	}
    }
    nss_ZFreeIf(subjectCerts);
    return certList;
loser:
    nss_ZFreeIf(subjectCerts);
    if (certList != NULL) {
	CERT_DestroyCertList(certList);
    }
    return NULL;
}

SECStatus
CERT_DeleteTempCertificate(CERTCertificate *cert)
{
    /* remove from cache */
}

void
CERT_DestroyCertificate(CERTCertificate *cert)
{
    int refCount;
    CERTCertDBHandle *handle;
    if ( cert ) {
	handle = cert->dbhandle;
        CERT_LockCertRefCount(cert);
	PORT_Assert(cert->referenceCount > 0);
	refCount = --cert->referenceCount;
        CERT_UnlockCertRefCount(cert);
	if ( ( refCount == 0 ) && !cert->keepSession ) {
	    PRArenaPool *arena  = cert->arena;
	    if ( cert->istemp ) {
		CERT_DeleteTempCertificate(cert);
	    }
	    /* delete the NSSCertificate */
	    /* zero cert before freeing. Any stale references to this cert
	     * after this point will probably cause an exception.  */
	    PORT_Memset(cert, 0, sizeof *cert);
	    cert = NULL;
	    /* free the arena that contains the cert. */
	    PORT_FreeArena(arena, PR_FALSE);
        }
    }
    return;
}

SECStatus
CERT_SaveImportedCert(CERTCertificate *cert, SECCertUsage usage,
		      PRBool caOnly, char *nickname)
{
    SECStatus rv;
    PRBool saveit;
    CERTCertTrust trust;
    CERTCertTrust tmptrust;
    PRBool isCA;
    unsigned int certtype;
    PRBool freeNickname = PR_FALSE;
    
    isCA = CERT_IsCACert(cert, NULL);
    if ( caOnly && ( !isCA ) ) {
	return(SECSuccess);
    }
    
    saveit = PR_TRUE;
    
    PORT_Memset((void *)&trust, 0, sizeof(trust));

    certtype = cert->nsCertType;

    /* if no CA bits in cert type, then set all CA bits */
    if ( isCA && ( ! ( certtype & NS_CERT_TYPE_CA ) ) ) {
	certtype |= NS_CERT_TYPE_CA;
    }

    /* if no app bits in cert type, then set all app bits */
    if ( ( !isCA ) && ( ! ( certtype & NS_CERT_TYPE_APP ) ) ) {
	certtype |= NS_CERT_TYPE_APP;
    }

    if ( isCA && !nickname ) {
	nickname = CERT_MakeCANickname(cert);
	if ( nickname != NULL ) {
	    freeNickname = PR_TRUE;
	}
    }
    
    switch ( usage ) {
      case certUsageEmailSigner:
      case certUsageEmailRecipient:
	if ( isCA ) {
	    if ( certtype & NS_CERT_TYPE_EMAIL_CA ) {
		trust.emailFlags = CERTDB_VALID_CA;
	    }
	} else {
	    PORT_Assert(nickname == NULL);

	    if ( cert->emailAddr == NULL ) {
		saveit = PR_FALSE;
	    }
	    
	    if ( certtype & NS_CERT_TYPE_EMAIL ) {
		trust.emailFlags = CERTDB_VALID_PEER;
		if ( ! ( cert->rawKeyUsage & KU_KEY_ENCIPHERMENT ) ) {
		    /* don't save it if KeyEncipherment is not allowed */
		    saveit = PR_FALSE;
		}
	    }
	}
	break;
      case certUsageUserCertImport:
	if ( isCA ) {
	    if ( certtype & NS_CERT_TYPE_SSL_CA ) {
		trust.sslFlags = CERTDB_VALID_CA;
	    }
	    
	    if ( certtype & NS_CERT_TYPE_EMAIL_CA ) {
		trust.emailFlags = CERTDB_VALID_CA;
	    }
	    
	    if ( certtype & NS_CERT_TYPE_OBJECT_SIGNING_CA ) {
		trust.objectSigningFlags = CERTDB_VALID_CA;
	    }
	    
	} else {
	    if ( certtype & NS_CERT_TYPE_SSL_CLIENT ) {
		trust.sslFlags = CERTDB_VALID_PEER;
	    }
	    
	    if ( certtype & NS_CERT_TYPE_EMAIL ) {
		trust.emailFlags = CERTDB_VALID_PEER;
	    }
	    
	    if ( certtype & NS_CERT_TYPE_OBJECT_SIGNING ) {
		trust.objectSigningFlags = CERTDB_VALID_PEER;
	    }
	}
	break;
      case certUsageAnyCA:
	trust.sslFlags = CERTDB_VALID_CA;
	break;
      case certUsageSSLCA:
	trust.sslFlags = CERTDB_VALID_CA | 
			CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA;
	break;
      default:	/* XXX added to quiet warnings; no other cases needed? */
	break;
    }

    if ( saveit ) {
	if ( cert->isperm ) {
	    /* Cert already in the DB.  Just adjust flags */
	    tmptrust = *cert->trust;
	    tmptrust.sslFlags |= trust.sslFlags;
	    tmptrust.emailFlags |= trust.emailFlags;
	    tmptrust.objectSigningFlags |= trust.objectSigningFlags;
	    
	    rv = CERT_ChangeCertTrust(cert->dbhandle, cert,
				      &tmptrust);
	    if ( rv != SECSuccess ) {
		goto loser;
	    }
	} else {
	    /* Cert not already in the DB.  Add it */
	    rv = CERT_AddTempCertToPerm(cert, nickname, &trust);
	    if ( rv != SECSuccess ) {
		goto loser;
	    }
	}
    }

    rv = SECSuccess;
    goto done;

loser:
    rv = SECFailure;
done:

    if ( freeNickname ) {
	PORT_Free(nickname);
    }
    
    return(rv);
}

SECStatus
CERT_ChangeCertTrustByUsage(CERTCertDBHandle *certdb,
			    CERTCertificate *cert, SECCertUsage usage)
{
    SECStatus rv;
    CERTCertTrust trust;
    CERTCertTrust tmptrust;
    unsigned int certtype;
    PRBool saveit;
    
    saveit = PR_TRUE;
    
    PORT_Memset((void *)&trust, 0, sizeof(trust));

    certtype = cert->nsCertType;

    /* if no app bits in cert type, then set all app bits */
    if ( ! ( certtype & NS_CERT_TYPE_APP ) ) {
	certtype |= NS_CERT_TYPE_APP;
    }

    switch ( usage ) {
      case certUsageEmailSigner:
      case certUsageEmailRecipient:
	if ( certtype & NS_CERT_TYPE_EMAIL ) {
	     trust.emailFlags = CERTDB_VALID_PEER;
	     if ( ! ( cert->rawKeyUsage & KU_KEY_ENCIPHERMENT ) ) {
		/* don't save it if KeyEncipherment is not allowed */
		saveit = PR_FALSE;
	    }
	}
	break;
      case certUsageUserCertImport:
	if ( certtype & NS_CERT_TYPE_EMAIL ) {
	    trust.emailFlags = CERTDB_VALID_PEER;
	}
	/* VALID_USER is already set if the cert was imported, 
	 * in the case that the cert was already in the database
	 * through SMIME or other means, we should set the USER
	 * flags, if they are not already set.
	 */
	if( cert->isperm ) {
	    if ( certtype & NS_CERT_TYPE_SSL_CLIENT ) {
		if( !(cert->trust->sslFlags & CERTDB_USER) ) {
		    trust.sslFlags |= CERTDB_USER;
		}
	    }
	    
	    if ( certtype & NS_CERT_TYPE_EMAIL ) {
		if( !(cert->trust->emailFlags & CERTDB_USER) ) {
		    trust.emailFlags |= CERTDB_USER;
		}
	    }
	    
	    if ( certtype & NS_CERT_TYPE_OBJECT_SIGNING ) {
		if( !(cert->trust->objectSigningFlags & CERTDB_USER) ) {
		    trust.objectSigningFlags |= CERTDB_USER;
		}
	    }
	}
	break;
      default:	/* XXX added to quiet warnings; no other cases needed? */
	break;
    }

    if ( (trust.sslFlags | trust.emailFlags | trust.objectSigningFlags) == 0 ){
	saveit = PR_FALSE;
    }

    if ( saveit && cert->isperm ) {
	/* Cert already in the DB.  Just adjust flags */
	tmptrust = *cert->trust;
	tmptrust.sslFlags |= trust.sslFlags;
	tmptrust.emailFlags |= trust.emailFlags;
	tmptrust.objectSigningFlags |= trust.objectSigningFlags;
	    
	rv = CERT_ChangeCertTrust(cert->dbhandle, cert,
				  &tmptrust);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
    }

    rv = SECSuccess;
    goto done;

loser:
    rv = SECFailure;
done:

    return(rv);
}

int
CERT_GetDBContentVersion(CERTCertDBHandle *handle)
{
    /* do anything? */
}

/*
 *
 * Manage S/MIME profiles
 *
 */

SECStatus
CERT_SaveSMimeProfile(CERTCertificate *cert, SECItem *emailProfile,
		      SECItem *profileTime)
{
}

SECItem *
CERT_FindSMimeProfile(CERTCertificate *cert)
{
}

/*
 *
 * Manage CRL's
 *
 */

/* only for crlutil.c */
CERTSignedCrl *
SEC_FindCrlByKey(CERTCertDBHandle *handle, SECItem *crlKey, int type)
{
}

CERTSignedCrl *
SEC_FindCrlByName(CERTCertDBHandle *handle, SECItem *crlKey, int type)
{
}

SECStatus
SEC_DestroyCrl(CERTSignedCrl *crl)
{
}

CERTSignedCrl *
cert_DBInsertCRL (CERTCertDBHandle *handle, char *url,
		  CERTSignedCrl *newCrl, SECItem *derCrl, int type)
{
    CERTSignedCrl *oldCrl = NULL, *crl = NULL;
    PRArenaPool *arena = NULL;
    SECCertTimeValidity validity;
    /*
    certDBEntryType crlType = (type == SEC_CRL_TYPE) ?
	 certDBEntryTypeRevocation : certDBEntryTypeKeyRevocation;
	 */

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) goto done;

    validity = SEC_CheckCrlTimes(&newCrl->crl,PR_Now());
    if ( validity == secCertTimeExpired) {
	if (type == SEC_CRL_TYPE) {
	    PORT_SetError(SEC_ERROR_CRL_EXPIRED);
	} else {
	    PORT_SetError(SEC_ERROR_KRL_EXPIRED);
	}
	goto done;
    } else if (validity == secCertTimeNotValidYet) {
	if (type == SEC_CRL_TYPE) {
	    PORT_SetError(SEC_ERROR_CRL_NOT_YET_VALID);
	} else {
	    PORT_SetError(SEC_ERROR_KRL_NOT_YET_VALID);
	}
	goto done;
    }

    oldCrl = SEC_FindCrlByKey(handle, &newCrl->crl.derName, type);

    /* if there is an old crl, make sure the one we are installing
     * is newer. If not, exit out, otherwise delete the old crl.
     */
    if (oldCrl != NULL) {
	if (!SEC_CrlIsNewer(&newCrl->crl,&oldCrl->crl)) {

	    if (type == SEC_CRL_TYPE) {
		PORT_SetError(SEC_ERROR_OLD_CRL);
	    } else {
		PORT_SetError(SEC_ERROR_OLD_KRL);
	    }

	    goto done;
	}

	if ((SECITEM_CompareItem(&newCrl->crl.derName, 
	        &oldCrl->crl.derName) != SECEqual) &&
	    (type == SEC_KRL_TYPE) ) {
	    
	    PORT_SetError(SEC_ERROR_CKL_CONFLICT);
	    goto done;
	}

	/* if we have a url in the database, use that one */
	if (oldCrl->url) {
	    int nnlen = PORT_Strlen(oldCrl->url) + 1;
	    url  = (char *)PORT_ArenaAlloc(arena, nnlen);
	    if ( url != NULL ) {
	        PORT_Memcpy(url, oldCrl->url, nnlen);
	    }
	}


	/* really destroy this crl */
	/* first drum it out of the permanment Data base */
	SEC_DeletePermCRL(oldCrl);
	/* then get rid of our reference to it... */
	SEC_DestroyCrl(oldCrl);
	oldCrl = NULL;

    }

    /* Write the new entry into the data base */
#ifdef KNOW_HOW_TO_WRITE_CRL_TO_TOKEN
    entry = NewDBCrlEntry(derCrl, url, crlType, 0);
    if (entry == NULL) goto done;

    rv = WriteDBCrlEntry(handle, entry);
    if (rv != SECSuccess) goto done;

    crl = SEC_AddPermCrlToTemp(handle, entry);
    if (crl) entry = NULL; /*crl->dbEntry now points to entry data */
    crl->isperm = PR_TRUE;
#endif /* KNOW_HOW_TO_WRITE_CRL_TO_TOKEN */

done:
    if (arena) PORT_FreeArena(arena, PR_FALSE);
    if (oldCrl) SEC_DestroyCrl(oldCrl);

    return crl;
}


/*
 * create a new CRL from DER material.
 *
 * The signature on this CRL must be checked before you
 * load it. ???
 */
CERTSignedCrl *
SEC_NewCrl(CERTCertDBHandle *handle, char *url, SECItem *derCrl, int type)
{
    CERTSignedCrl *newCrl = NULL, *crl = NULL;

    /* make this decode dates! */
    newCrl = CERT_DecodeDERCrl(NULL, derCrl, type);
    if (newCrl == NULL) {
	if (type == SEC_CRL_TYPE) {
	    PORT_SetError(SEC_ERROR_CRL_INVALID);
	} else {
	    PORT_SetError(SEC_ERROR_KRL_INVALID);
	}
	goto done;
    }

    crl = cert_DBInsertCRL (handle, url, newCrl, derCrl, type);


done:
    if (newCrl) PORT_FreeArena(newCrl->arena, PR_FALSE);

    return crl;
}

SECStatus
SEC_LookupCrls(CERTCertDBHandle *handle, CERTCrlHeadNode **nodes, int type)
{
}

SECStatus
SEC_DeletePermCRL(CERTSignedCrl *crl)
{
}

/*
 *
 * SPK Digest code, unmodified from pcertdb.c
 *
 */

/*
 * The following is bunch of types and code to allow looking up a certificate
 * by a hash of its subject public key.  Because the words "hash" and "key"
 * are overloaded and thus terribly confusing, I tried to disambiguate things.
 * - Where I could, I used "digest" instead of "hash" when referring to
 *   hashing of the subject public key.  The PLHashTable interfaces and
 *   our own HASH_Foo interfaces had to be left as is, obviously.  The latter
 *   should be thought of as "digest" in this case.
 * - There are three keys in use here -- the subject public key, the key
 *   used to do a lookup in the PLHashTable, and the key used to do a lookup
 *   in the cert database.  As the latter is a fairly pervasive interface,
 *   I left it alone.  The other two uses I changed to "spk" or "SPK" when
 *   referring to the subject public key, and "index" when referring to the
 *   key into the PLHashTable.
 */

typedef struct SPKDigestInfoStr {
    PLHashTable *table;
    PRBool permPopulated;
} SPKDigestInfo;

/*
 * Since the key hash information is "hidden" (in a void pointer in the handle)
 * these macros with the appropriate casts make it easy to get at the parts.
 */
#define SPK_DIGEST_TABLE(handle)	\
	(((SPKDigestInfo *)(handle->spkDigestInfo))->table)

/*
** Hash allocator ops for the SPKDigest hash table.  The rules are:
**   + The index and value fields are "owned" by the hash table, and are
**     freed when the table entry is deleted.
**   + Replacing a value in the table is not allowed, since the caller can't
**     tell whether the index field was used or not, resulting in a memory
**     leak.  (This is a bug in the PL_Hash routines.
*/
static void * PR_CALLBACK
spkAllocTable(void *pool, PRSize size)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif
    return PR_MALLOC(size);
}

static void PR_CALLBACK
spkFreeTable(void *pool, void *item)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif
    PR_Free(item);
}

/* NOTE: the key argument here appears to be useless, since the RawAdd
 * routine in PL_Hash just uses the original anyway.
 */
static PLHashEntry * PR_CALLBACK
spkAllocEntry(void *pool, const void *key)
{
#if defined(XP_MAC)
#pragma unused (pool,key)
#endif
    return PR_NEW(PLHashEntry);
}

static void PR_CALLBACK
spkFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif
    SECItem *value = (SECItem *)he->value;
    /* The flag should always be to free the whole entry.  Otherwise the
     * index field gets leaked because the caller can't tell whether
     * the "new" value (which is the same as the old) was used or not.
     */
    PORT_Assert(flag == HT_FREE_ENTRY);
    /* We always free the value */
    SECITEM_FreeItem(value, PR_TRUE);
    if (flag == HT_FREE_ENTRY)
    {
        /* Comes from BTOA, is this the right free call? */
        PORT_Free((char *)he->key);
        PR_Free(he);
    }
}

static PLHashAllocOps spkHashAllocOps = {
    spkAllocTable, spkFreeTable,
    spkAllocEntry, spkFreeEntry
};

/*
 * Create the key hash lookup table.  Note that the table, and the
 * structure which holds it and a little more information, is never freed.
 * This is because the temporary database is never actually closed out,
 * so there is no safe/obvious place to free the whole thing.
 *
 * The database must be locked already.
 */
static SECStatus
InitDBspkDigestInfo(CERTCertDBHandle *handle)
{
    SPKDigestInfo *spkDigestInfo;
    PLHashTable *table;
    PORT_Assert(handle != NULL);
    PORT_Assert(handle->spkDigestInfo == NULL);
    spkDigestInfo = PORT_ZAlloc(sizeof(SPKDigestInfo));
    if ( spkDigestInfo == NULL ) {
	return(SECFailure);
    }
    table = PL_NewHashTable(128, PL_HashString, PL_CompareStrings,
			    (PLHashComparator) SECITEM_ItemsAreEqual,
			    &spkHashAllocOps, NULL);
    if ( table == NULL ) {
	PORT_Free(spkDigestInfo);
	return(SECFailure);
    }
    spkDigestInfo->table = table;
    handle->spkDigestInfo = spkDigestInfo;
    return(SECSuccess);
}

static const SECHashObject *
OidTagToRawDigestObject(SECOidTag digestAlg)
{
    const SECHashObject *rawDigestObject;
    switch (digestAlg) {
      case SEC_OID_MD2:
	rawDigestObject = &SECRawHashObjects[HASH_AlgMD2];
	break;
      case SEC_OID_MD5:
	rawDigestObject = &SECRawHashObjects[HASH_AlgMD5];
	break;
      case SEC_OID_SHA1:
	rawDigestObject = &SECRawHashObjects[HASH_AlgSHA1];
	break;
      default:
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	rawDigestObject = NULL;
	break;
    }
    return(rawDigestObject);
}

/*
 * Digest the cert's subject public key using the specified algorithm.
 * The necessary storage for the digest data is allocated.  If "fill" is
 * non-null, the data is put there, otherwise a SECItem is allocated.
 * Allocation from "arena" if it is non-null, heap otherwise.  Any problem
 * results in a NULL being returned (and an appropriate error set).
 */ 
SECItem *
CERT_SPKDigestValueForCert(PRArenaPool *arena, CERTCertificate *cert,
			   SECOidTag digestAlg, SECItem *fill)
{
    const SECHashObject *digestObject;
    void *digestContext;
    SECItem *result = NULL;
    void *mark = NULL;
    SECItem spk;
    if ( arena != NULL ) {
	mark = PORT_ArenaMark(arena);
    }
    /*
     * This can end up being called before PKCS #11 is initialized,
     * so we have to use the raw digest functions.
     */
    digestObject = OidTagToRawDigestObject(digestAlg);
    if ( digestObject == NULL ) {
	goto loser;
    }
    result = SECITEM_AllocItem(arena, fill, digestObject->length);
    if ( result == NULL ) {
	goto loser;
    }
    /*
     * Copy just the length and data pointer (nothing needs to be freed)
     * of the subject public key so we can convert the length from bits
     * to bytes, which is what the digest function expects.
     */
    spk = cert->subjectPublicKeyInfo.subjectPublicKey;
    DER_ConvertBitString(&spk);
    /*
     * Now digest the value, using the specified algorithm.
     */
    digestContext = digestObject->create();
    if ( digestContext == NULL ) {
	goto loser;
    }
    digestObject->begin(digestContext);
    digestObject->update(digestContext, spk.data, spk.len);
    digestObject->end(digestContext, result->data, &(result->len), result->len);
    digestObject->destroy(digestContext, PR_TRUE);
    if ( arena != NULL ) {
	PORT_ArenaUnmark(arena, mark);
    }
    return(result);
loser:
    if ( arena != NULL ) {
	PORT_ArenaRelease(arena, mark);
    } else {
	if ( result != NULL ) {
	    SECITEM_FreeItem(result, (fill == NULL) ? PR_TRUE : PR_FALSE);
	}
    }
    return(NULL);
}

/*
 * Return the index for the spk digest lookup table for "spkDigest".
 *
 * Caller is responsible for freeing the returned string.
 */
static char *
spkDigestIndexFromDigest(SECItem *spkDigest)
{
    return BTOA_ConvertItemToAscii(spkDigest);
}

/*
 * Return the index for the spk digest lookup table for this certificate,
 * based on the specified digest algorithm.
 *
 * Caller is responsible for freeing the returned string.
 */
static char *
spkDigestIndexFromCert(CERTCertificate *cert, SECOidTag digestAlg)
{
    SECItem *spkDigest;
    char *index;
    spkDigest = CERT_SPKDigestValueForCert(NULL, cert, digestAlg, NULL);
    if ( spkDigest == NULL )
	return(NULL);
    index = spkDigestIndexFromDigest(spkDigest);
    SECITEM_FreeItem(spkDigest, PR_TRUE);
    return(index);
}

/*
 * Add the spk digest for the given cert to the spk digest table,
 * based on the given digest algorithm.
 *
 * If a cert for the same spk digest is already in the table, choose whichever
 * cert is "newer".  (The other cert cannot be found via spk digest.)
 *
 * The database must be locked already.
 * 
 * XXX Note that this implementation results in leaking the index value.
 * Fixing that did not seem worth the trouble, given we will only leak
 * once per cert.  This whole thing should be done differently in the
 * new rewrite (Stan), and then the problem will go away.
 */
static SECStatus
AddCertToSPKDigestTableForAlg(CERTCertDBHandle *handle, CERTCertificate *cert,
			      SECItem *certDBKey, SECOidTag digestAlg)
{
    SECStatus rv = SECFailure;
    SECItem *oldCertDBKey;
    PRBool addit = PR_TRUE;
    CERTCertificate *oldCert = NULL;
    char *index = NULL;
    PLHashTable *table;
    /*
     * After running some testing doing key hash lookups (like using OCSP),
     * if these are never hit, they can probably be removed.
     */
    PORT_Assert(handle != NULL);
    PORT_Assert(handle == cert->dbhandle);
    PORT_Assert(handle->spkDigestInfo != NULL);
    PORT_Assert((certDBKey == &cert->certKey)
		|| (SECITEM_CompareItem(certDBKey,
					&cert->certKey) == SECEqual));
    table = SPK_DIGEST_TABLE(handle);
    PORT_Assert(table != NULL);
    index = spkDigestIndexFromCert(cert, digestAlg);
    if ( index == NULL ) {
	goto loser;
    }
    /*
     * See if this cert's spk digest is already in the table.
     */
    oldCertDBKey = PL_HashTableLookup(table, index);
    if ( oldCertDBKey != NULL ) {
	/*
	 * The spk digest *is* already in the table.  We need to find that
	 * cert and see -- if it is the same, then we can just leave as is.
	 * Otherwise we have to choose which cert we want represented;
	 * in that case the best plan I can think of is to hang onto the
	 * most recent one.
	 */
	oldCert = CERT_FindCertByKey(handle, oldCertDBKey);
	if ( oldCert != NULL ) {
	    if ( cert == oldCert ) {
		/* They are the same cert, so we are done. */
		addit = PR_FALSE;
	    } else if ( CERT_IsNewer(cert, oldCert) ) {
		if ( PL_HashTableRemove(table, index) != PR_TRUE ) {
		    goto loser;
		}
	    } else {
		/* oldCert is "newer", so we are done. */
		addit = PR_FALSE;
	    }
	}
    }
    if ( addit ) {
	certDBKey = SECITEM_DupItem(certDBKey);
	if ( certDBKey == NULL ) {
	    goto loser;
	}
	if ( PL_HashTableAdd(table, index, certDBKey) == NULL ) {
	    SECITEM_FreeItem(certDBKey, PR_TRUE);
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	index = NULL;				/* don't want to free it */
    }
    rv = SECSuccess;
loser:
    if ( index != NULL ) {
	PORT_Free(index);
    }
    if ( oldCert != NULL ) {
	CERT_DestroyCertificate(oldCert);
    }
    return(rv);
}

/*
 * Add the spk digest for the given cert to the spk digest table,
 * for all known digest algorithms.
 *
 * The database must be locked already, and the digest table already created.
 */
static SECStatus
AddCertToSPKDigestTableForAllAlgs(CERTCertDBHandle *handle,
				  CERTCertificate *cert, SECItem *certDBKey)
{
    (void) AddCertToSPKDigestTableForAlg(handle, cert, certDBKey, SEC_OID_MD2);
    (void) AddCertToSPKDigestTableForAlg(handle, cert, certDBKey, SEC_OID_MD5);
    return AddCertToSPKDigestTableForAlg(handle, cert, certDBKey, SEC_OID_SHA1);
}

/*
 * Add the spk digest for the given cert to the spk digest table,
 * for all known digest algorithms.  This function is called while
 * traversing all of the certs in the permanent database -- since
 * that imposes some constraints on its arguments this routine is a
 * simple cover for the "real" interface.
 *
 * The database must be locked already, and the digest table already created.
 */
static SECStatus
AddCertToSPKDigestTableInTraversal(CERTCertificate *cert, SECItem *certDBKey,
				   void *data)
{
    CERTCertDBHandle *handle = data;
    return AddCertToSPKDigestTableForAllAlgs(handle, cert, certDBKey);
}

/*
 * Add the spk digests for the all permanent certs to the spk digest table,
 * for all known digest algorithms.
 *
 * This locks the database, and then checks to make sure that the work
 * actually needs to get done.
 *
 * If the spk digest table does not yet exist, it is created.
 */
static SECStatus
PopulateSPKDigestTable(CERTCertDBHandle *handle)
{
    SPKDigestInfo *spkDigestInfo;
    SECStatus rv = SECSuccess;
    CERT_LockDB(handle);
    spkDigestInfo = handle->spkDigestInfo;
    if ( spkDigestInfo == NULL ) {
	rv = InitDBspkDigestInfo(handle);
	if ( rv != SECSuccess ) {
	    return(rv);
	}
	spkDigestInfo = handle->spkDigestInfo;
	PORT_Assert(spkDigestInfo != NULL);
    } else {
	/*
	 * Check to see if someone already did it; it is important to do
	 * this after getting the lock.
	 */
	if ( spkDigestInfo->permPopulated == PR_TRUE ) {
	    goto done;
	}
    }
    rv = SEC_TraversePermCerts(handle, AddCertToSPKDigestTableInTraversal,
				    handle);
    if ( rv != SECSuccess ) {
	goto done;
    }
    spkDigestInfo->permPopulated = PR_TRUE;
done:
    CERT_UnlockDB(handle);
    return(rv);
}

/*
 * Lookup a certificate by a digest of a subject public key.  If it is
 * found, it is returned (and must then be destroyed by the caller).
 * NULL is returned otherwise -- if there was a problem performing the
 * lookup, an appropriate error is set (e.g. SEC_ERROR_NO_MEMORY);
 * if the cert simply was not found, the error is SEC_ERROR_UNKNOWN_CERT.
 *
 * If the lookup table has not yet been created or populated, do that first.
 */
CERTCertificate *
CERT_FindCertBySPKDigest(CERTCertDBHandle *handle, SECItem *spkDigest)
{
    SPKDigestInfo *spkDigestInfo;
    char *index = NULL;
    SECItem *certDBKey;
    CERTCertificate *cert = NULL;
    PORT_Assert(handle != NULL);
    spkDigestInfo = handle->spkDigestInfo;
    if ( spkDigestInfo == NULL || spkDigestInfo->permPopulated != PR_TRUE ) {
	if ( PopulateSPKDigestTable(handle) != SECSuccess ) {
	    goto loser;
	}
    }
    index = spkDigestIndexFromDigest(spkDigest);
    if ( index == NULL ) {
	goto loser;
    }
    certDBKey = PL_HashTableLookup(SPK_DIGEST_TABLE(handle), index);
    if ( certDBKey != NULL ) {
	cert = CERT_FindCertByKey(handle, certDBKey);
    }
    if ( cert == NULL ) {
	PORT_SetError(SEC_ERROR_UNKNOWN_CERT);
    }
loser:
    if ( index != NULL ) {
	PORT_Free(index);
    }
    return(cert);
}

/* XXX
 * XXX
 *
 * These are included for now to allow this to build, but will not be needed
 * once the softoken is below PKCS#11.
 */
SECStatus
CERT_AddPermNickname(CERTCertificate *cert, char *nickname)
{
}

int
CERT_NumPermCertsForSubject(CERTCertDBHandle *handle, SECItem *derSubject)
{
}

int
CERT_NumPermCertsForNickname(CERTCertDBHandle *handle, char *nickname)
{
}

SECStatus
CERT_OpenCertDB(CERTCertDBHandle *handle, PRBool readOnly,
		CERTDBNameFunc namecb, void *cbarg)
{
    return SECSuccess;
}
