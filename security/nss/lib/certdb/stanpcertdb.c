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

#ifndef NSS_3_4_CODE
#define NSS_3_4_CODE
#endif /* NSS_3_4_CODE */
#include "nsspki.h"
#include "pkit.h"
#include "pkim.h"
#include "pki3hack.h"
#include "ckhelper.h"
#include "base.h"

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
    SECStatus rv = SECFailure;
    PRStatus ret;

    CERT_LockCertTrust(cert);
    if (PK11_IsReadOnly(cert->slot)) {
	char *nickname = cert_parseNickname(cert->nickname);
	/* XXX store it on a writeable token */
	goto done;
    } else {
	NSSCertificate *c = STAN_GetNSSCertificate(cert);
	ret = STAN_ChangeCertTrust(c, trust);
	rv = (ret == PR_SUCCESS) ? SECSuccess : SECFailure;
    }
done:
    CERT_UnlockCertTrust(cert);
    return rv;
}

SECStatus
CERT_AddTempCertToPerm(CERTCertificate *cert, char *nickname,
		       CERTCertTrust *trust)
{
    NSSCertificate *c = STAN_GetNSSCertificate(cert);
#ifdef notdef
    /* might as well keep these */
    /* actually we shouldn't keep these! rjr */
    PORT_Assert(cert->istemp);
    PORT_Assert(!cert->isperm);
#endif
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
    cc->dbhandle = handle;
    nssTrustDomain_AddCertsToCache(handle, &c, 1);
    cc->istemp = 1;
    cc->isperm = 0;

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
CERT_FindCertByIssuerAndSN(CERTCertDBHandle *handle, CERTIssuerAndSN *issuerAndSN)
{
    PK11SlotInfo *slot;
    CERTCertificate *cert;

    cert = PK11_FindCertByIssuerAndSN(&slot,issuerAndSN,NULL);
    if (slot) {
        PK11_FreeSlot(slot);
    }

    return cert;
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
    if (!c) return NULL;
    return STAN_GetCERTCertificate(c);
}

CERTCertificate *
CERT_FindCertByKeyID(CERTCertDBHandle *handle, SECItem *name, SECItem *keyID)
{
   CERTCertList *list =
                        CERT_CreateSubjectCertList(NULL,handle,name,0,PR_FALSE);
    CERTCertificate *cert = NULL;
    CERTCertListNode *node = CERT_LIST_HEAD(list);

    if (list == NULL) return NULL;

    for (node = CERT_LIST_HEAD(list); node ; node = CERT_LIST_NEXT(node)) {
        if (SECITEM_ItemsAreEqual(&cert->subjectKeyID, keyID) ) {
            cert = CERT_DupCertificate(node->cert);
            break;
        }
    }
    return cert;
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
    if (!c) {
	return NULL;
    }
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
		/* uncache the cert ? */
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

#ifdef notdef
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
#endif

int
CERT_GetDBContentVersion(CERTCertDBHandle *handle)
{
    /* should read the DB content version from the pkcs #11 device */
    return 0;
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
    int64 oldtime;
    int64 newtime;
    SECStatus rv = SECFailure;
    PRBool saveit;
    char *emailAddr;
    SECItem *oldProfile = NULL;
    SECItem *oldProfileTime = NULL;
    PK11SlotInfo *slot = NULL;
    
    emailAddr = cert->emailAddr;
    
    PORT_Assert(emailAddr);
    if ( emailAddr == NULL ) {
	goto loser;
    }

    saveit = PR_FALSE;
   
    oldProfile = PK11_FindSMimeProfile(&slot, emailAddr, &cert->derSubject, 
							&oldProfileTime); 
    
    /* both profileTime and emailProfile have to exist or not exist */
    if ( emailProfile == NULL ) {
	profileTime = NULL;
    } else if ( profileTime == NULL ) {
	emailProfile = NULL;
    }
   
    if ( oldProfileTime == NULL ) {
	saveit = PR_TRUE;
    } else {
	/* there was already a profile for this email addr */
	if ( profileTime ) {
	    /* we have an old and new profile - save whichever is more recent*/
	    if ( oldProfileTime->len == 0 ) {
		/* always replace if old entry doesn't have a time */
		oldtime = LL_MININT;
	    } else {
		rv = DER_UTCTimeToTime(&oldtime, oldProfileTime);
		if ( rv != SECSuccess ) {
		    goto loser;
		}
	    }
	    
	    rv = DER_UTCTimeToTime(&newtime, profileTime);
	    if ( rv != SECSuccess ) {
		goto loser;
	    }
	
	    if ( LL_CMP(newtime, >, oldtime ) ) {
		/* this is a newer profile, save it and cert */
		saveit = PR_TRUE;
	    }
	} else {
	    saveit = PR_TRUE;
	}
    }


    if (saveit) {
	rv = PK11_SaveSMimeProfile(slot, emailAddr, &cert->derSubject, 
						emailProfile, profileTime);
    } else {
	rv = SECSuccess;
    }

loser:
    if (oldProfile) {
    	SECITEM_FreeItem(oldProfile,PR_TRUE);
    }
    if (oldProfileTime) {
    	SECITEM_FreeItem(oldProfileTime,PR_TRUE);
    }
    
    return(rv);
}

SECItem *
CERT_FindSMimeProfile(CERTCertificate *cert)
{
    PK11SlotInfo *slot = NULL;
    return 
	PK11_FindSMimeProfile(&slot, cert->emailAddr, &cert->derSubject, NULL);
}

/*
 * depricated functions that are now just stubs.
 */
/*
 * Close the database
 */
void
__CERT_ClosePermCertDB(CERTCertDBHandle *handle)
{
    PORT_Assert("CERT_ClosePermCertDB is Depricated" == NULL);
    return;
}

SECStatus
CERT_OpenCertDBFilename(CERTCertDBHandle *handle, char *certdbname,
                        PRBool readOnly)
{
    PORT_Assert("CERT_OpenCertDBFilename is Depricated" == NULL);
    return SECFailure;
}

SECItem *
SECKEY_HashPassword(char *pw, SECItem *salt)
{
    PORT_Assert("SECKEY_HashPassword is Depricated" == NULL);
    return NULL;
}

SECStatus
__CERT_TraversePermCertsForSubject(CERTCertDBHandle *handle,
                                 SECItem *derSubject,
                                 void *cb, void *cbarg)
{
    PORT_Assert("CERT_TraversePermCertsForSubject is Depricated" == NULL);
    return SECFailure;
}


SECStatus
__CERT_TraversePermCertsForNickname(CERTCertDBHandle *handle, char *nickname,
                                  void *cb, void *cbarg)
{
    PORT_Assert("CERT_TraversePermCertsForNickname is Depricated" == NULL);
    return SECFailure;
}



