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

/*
 * Certificate handling code
 *
 * $Id: lowcert.c,v 1.2 2001/11/08 00:15:34 relyea%netscape.com Exp $
 */

#include "seccomon.h"
#include "secder.h"
#include "nssilock.h"
#include "prmon.h"
#include "prtime.h"
#include "lowkeyi.h"
#include "pcert.h"
#include "secasn1.h"
#include "secoid.h"

/* should have been in a 'util' header */
extern const SEC_ASN1Template CERT_ValidityTemplate[];

static const SEC_ASN1Template nsslowcert_CertKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSLOWCERTCertKey) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0, 0, SEC_SkipTemplate },	/* version */ 
    { SEC_ASN1_INTEGER, offsetof(NSSLOWCERTCertKey,serialNumber) },
    { SEC_ASN1_SKIP },		/* signature algorithm */
    { SEC_ASN1_ANY, offsetof(NSSLOWCERTCertKey,derIssuer) },
    { SEC_ASN1_SKIP_REST },
    { 0 }
};

const SEC_ASN1Template nsslowcert_SubjectPublicKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWCERTSubjectPublicKeyInfo) },
    { SEC_ASN1_INLINE, offsetof(NSSLOWCERTSubjectPublicKeyInfo,algorithm),
          SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
          offsetof(NSSLOWCERTSubjectPublicKeyInfo,subjectPublicKey), },
    { 0, }
};

const SEC_ASN1Template nsslowcert_CertificateTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSSLOWCERTCertificate) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_CONTEXT_SPECIFIC | 0, 0, SEC_SkipTemplate }, /* version */
    { SEC_ASN1_INTEGER, offsetof(NSSLOWCERTCertificate,serialNumber) },
    { SEC_ASN1_SKIP }, /* Signature algorithm */
    { SEC_ASN1_ANY, offsetof(NSSLOWCERTCertificate,derIssuer) },
    { SEC_ASN1_INLINE,
          offsetof(NSSLOWCERTCertificate,validity),
          CERT_ValidityTemplate },
    { SEC_ASN1_ANY, offsetof(NSSLOWCERTCertificate,derSubject) },
    { SEC_ASN1_INLINE,
          offsetof(NSSLOWCERTCertificate,subjectPublicKeyInfo),
          nsslowcert_SubjectPublicKeyInfoTemplate },
    { SEC_ASN1_SKIP_REST },
    { 0 }
};
const SEC_ASN1Template nsslowcert_SignedCertificateTemplate[] =
{
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWCERTCertificate) },
    { SEC_ASN1_INLINE, 0, nsslowcert_CertificateTemplate },
    { SEC_ASN1_SKIP_REST },
    { 0 }
};
const SEC_ASN1Template nsslowcert_SignedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWCERTSignedData) },
    { SEC_ASN1_ANY, offsetof(NSSLOWCERTSignedData,data), },
    { SEC_ASN1_SKIP_REST },
    { 0, }
};
const SEC_ASN1Template nsslowcert_RSAPublicKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPublicKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPublicKey,u.rsa.modulus), },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPublicKey,u.rsa.publicExponent), },
    { 0, }
};
const SEC_ASN1Template nsslowcert_DSAPublicKeyTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPublicKey,u.dsa.publicValue), },
    { 0, }
};
const SEC_ASN1Template nsslowcert_DHPublicKeyTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPublicKey,u.dh.publicValue), },
    { 0, }
};


static PZLock *pcertRefCountLock = NULL;

/*
 * Acquire the cert reference count lock
 * There is currently one global lock for all certs, but I'm putting a cert
 * arg here so that it will be easy to make it per-cert in the future if
 * that turns out to be necessary.
 */
void
nsslowcert_LockCertRefCount(NSSLOWCERTCertificate *cert)
{
    if ( pcertRefCountLock == NULL ) {
	nss_InitLock(&pcertRefCountLock, nssILockRefLock);
	PORT_Assert(pcertRefCountLock != NULL);
    }
    
    PZ_Lock(pcertRefCountLock);
    return;
}

/*
 * Free the cert reference count lock
 */
void
nsslowcert_UnlockCertRefCount(NSSLOWCERTCertificate *cert)
{
    PRStatus prstat;

    PORT_Assert(pcertRefCountLock != NULL);
    
    prstat = PZ_Unlock(pcertRefCountLock);
    
    PORT_Assert(prstat == PR_SUCCESS);

    return;
}


NSSLOWCERTCertificate *
nsslowcert_DupCertificate(NSSLOWCERTCertificate *c)
{
    if (c) {
	nsslowcert_LockCertRefCount(c);
	++c->referenceCount;
	nsslowcert_UnlockCertRefCount(c);
    }
    return c;
}

/*
 * Allow use of default cert database, so that apps(such as mozilla) don't
 * have to pass the handle all over the place.
 */
static NSSLOWCERTCertDBHandle *default_pcert_db_handle = 0;

void
nsslowcert_SetDefaultCertDB(NSSLOWCERTCertDBHandle *handle)
{
    default_pcert_db_handle = handle;
    
    return;
}

NSSLOWCERTCertDBHandle *
nsslowcert_GetDefaultCertDB(void)
{
    return(default_pcert_db_handle);
}


SECStatus
nsslowcert_GetCertTimes(NSSLOWCERTCertificate *c, PRTime *notBefore, PRTime *notAfter)
{
    int rv;
    
    /* convert DER not-before time */
    rv = DER_UTCTimeToTime(notBefore, &c->validity.notBefore);
    if (rv) {
	return(SECFailure);
    }
    
    /* convert DER not-after time */
    rv = DER_UTCTimeToTime(notAfter, &c->validity.notAfter);
    if (rv) {
	return(SECFailure);
    }

    return(SECSuccess);
}

/*
 * is certa newer than certb?  If one is expired, pick the other one.
 */
PRBool
nsslowcert_IsNewer(NSSLOWCERTCertificate *certa, NSSLOWCERTCertificate *certb)
{
    PRTime notBeforeA, notAfterA, notBeforeB, notAfterB, now;
    SECStatus rv;
    PRBool newerbefore, newerafter;
    
    rv = nsslowcert_GetCertTimes(certa, &notBeforeA, &notAfterA);
    if ( rv != SECSuccess ) {
	return(PR_FALSE);
    }
    
    rv = nsslowcert_GetCertTimes(certb, &notBeforeB, &notAfterB);
    if ( rv != SECSuccess ) {
	return(PR_TRUE);
    }

    newerbefore = PR_FALSE;
    if ( LL_CMP(notBeforeA, >, notBeforeB) ) {
	newerbefore = PR_TRUE;
    }

    newerafter = PR_FALSE;
    if ( LL_CMP(notAfterA, >, notAfterB) ) {
	newerafter = PR_TRUE;
    }
    
    if ( newerbefore && newerafter ) {
	return(PR_TRUE);
    }
    
    if ( ( !newerbefore ) && ( !newerafter ) ) {
	return(PR_FALSE);
    }

    /* get current UTC time */
    now = PR_Now();

    if ( newerbefore ) {
	/* cert A was issued after cert B, but expires sooner */
	/* if A is expired, then pick B */
	if ( LL_CMP(notAfterA, <, now ) ) {
	    return(PR_FALSE);
	}
	return(PR_TRUE);
    } else {
	/* cert B was issued after cert A, but expires sooner */
	/* if B is expired, then pick A */
	if ( LL_CMP(notAfterB, <, now ) ) {
	    return(PR_TRUE);
	}
	return(PR_FALSE);
    }
}

#define SOFT_DEFAULT_CHUNKSIZE 2048

/*
 * take a DER certificate and decode it into a certificate structure
 */
NSSLOWCERTCertificate *
nsslowcert_DecodeDERCertificate(SECItem *derSignedCert, PRBool copyDER,
			 char *nickname)
{
    NSSLOWCERTCertificate *cert;
    PRArenaPool *arena;
    void *data;
    int rv;
    int len;
    
    /* make a new arena */
    arena = PORT_NewArena(SOFT_DEFAULT_CHUNKSIZE);
    
    if ( !arena ) {
	return 0;
    }

    /* allocate the certificate structure */
    cert = (NSSLOWCERTCertificate *)PORT_ArenaZAlloc(arena, sizeof(NSSLOWCERTCertificate));
    
    if ( !cert ) {
	goto loser;
    }
    
    cert->arena = arena;
    
    if ( copyDER ) {
	/* copy the DER data for the cert into this arena */
	data = (void *)PORT_ArenaAlloc(arena, derSignedCert->len);
	if ( !data ) {
	    goto loser;
	}
	cert->derCert.data = (unsigned char *)data;
	cert->derCert.len = derSignedCert->len;
	PORT_Memcpy(data, derSignedCert->data, derSignedCert->len);
    } else {
	/* point to passed in DER data */
	cert->derCert = *derSignedCert;
    }

    /* decode the certificate info */
    rv = SEC_ASN1DecodeItem(arena, cert, nsslowcert_SignedCertificateTemplate,
                    &cert->derCert);

    /* cert->subjectKeyID;	 x509v3 subject key identifier */
    cert->dbEntry = NULL;
    cert ->trust = NULL;

#ifdef notdef
    /* these fields are used by client GUI code to keep track of ssl sockets
     * that are blocked waiting on GUI feedback related to this cert.
     * XXX - these should be moved into some sort of application specific
     *       data structure.  They are only used by the browser right now.
     */
    struct SECSocketNode *socketlist;
    int socketcount;
    struct SECSocketNode *authsocketlist;
    int authsocketcount;

    /* This is PKCS #11 stuff. */
    PK11SlotInfo *slot;		/*if this cert came of a token, which is it*/
    CK_OBJECT_HANDLE pkcs11ID;	/*and which object on that token is it */
    PRBool ownSlot;		/*true if the cert owns the slot reference */
#endif

    /* generate and save the database key for the cert */
    rv = nsslowcert_KeyFromDERCert(arena, &cert->derCert, &cert->certKey);
    if ( rv ) {
	goto loser;
    }

    /* set the nickname */
    if ( nickname == NULL ) {
	cert->nickname = NULL;
    } else {
	/* copy and install the nickname */
	len = PORT_Strlen(nickname) + 1;
	cert->nickname = (char*)PORT_ArenaAlloc(arena, len);
	if ( cert->nickname == NULL ) {
	    goto loser;
	}

	PORT_Memcpy(cert->nickname, nickname, len);
    }

#ifdef FIXME
    /* initialize the subjectKeyID */
    rv = cert_GetKeyID(cert);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* set the email address */
    cert->emailAddr = CERT_GetCertificateEmailAddress(cert);
    
#endif
    
    cert->referenceCount = 1;
    
    return(cert);
    
loser:

    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(0);
}

char *
nsslowcert_FixupEmailAddr(char *emailAddr)
{
    char *retaddr;
    char *str;

    if ( emailAddr == NULL ) {
	return(NULL);
    }
    
    /* copy the string */
    str = retaddr = PORT_Strdup(emailAddr);
    if ( str == NULL ) {
	return(NULL);
    }
    
    /* make it lower case */
    while ( *str ) {
	*str = tolower( *str );
	str++;
    }
    
    return(retaddr);
}

static SECStatus
nsslowcert_KeyFromIssuerAndSN(PRArenaPool *arena, SECItem *issuer, SECItem *sn,
			SECItem *key)
{
    key->len = sn->len + issuer->len;
    
    key->data = (unsigned char*)PORT_ArenaAlloc(arena, key->len);
    if ( !key->data ) {
	goto loser;
    }

    /* copy the serialNumber */
    PORT_Memcpy(key->data, sn->data, sn->len);

    /* copy the issuer */
    PORT_Memcpy(&key->data[sn->len], issuer->data, issuer->len);

    return(SECSuccess);

loser:
    return(SECFailure);
}



/*
 * Generate a database key, based on serial number and issuer, from a
 * DER certificate.
 */
SECStatus
nsslowcert_KeyFromDERCert(PRArenaPool *arena, SECItem *derCert, SECItem *key)
{
    int rv;
    NSSLOWCERTSignedData sd;
    NSSLOWCERTCertKey certkey;

    PORT_Memset(&sd, 0, sizeof(NSSLOWCERTSignedData));    
    PORT_Memset(&certkey, 0, sizeof(NSSLOWCERTCertKey));    

    rv = SEC_ASN1DecodeItem(arena, &sd, nsslowcert_SignedDataTemplate, derCert);
    
    if ( rv ) {
	goto loser;
    }
    
    PORT_Memset(&certkey, 0, sizeof(NSSLOWCERTCertKey));
    rv = SEC_ASN1DecodeItem(arena, &certkey, 
					nsslowcert_CertKeyTemplate, &sd.data);

    if ( rv ) {
	goto loser;
    }

    return(nsslowcert_KeyFromIssuerAndSN(arena, &certkey.derIssuer,
				   &certkey.serialNumber, key));
loser:
    return(SECFailure);
}

NSSLOWKEYPublicKey *
nsslowcert_ExtractPublicKey(NSSLOWCERTCertificate *cert)
{
    NSSLOWCERTSubjectPublicKeyInfo *spki = &cert->subjectPublicKeyInfo;
    NSSLOWKEYPublicKey *pubk;
    SECItem os;
    SECStatus rv;
    PRArenaPool *arena;
    SECOidTag tag;

    arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
        return NULL;

    pubk = (NSSLOWKEYPublicKey *) 
		PORT_ArenaZAlloc(arena, sizeof(NSSLOWKEYPublicKey));
    if (pubk == NULL) {
        PORT_FreeArena (arena, PR_FALSE);
        return NULL;
    }

    pubk->arena = arena;

    /* Convert bit string length from bits to bytes */
    os = spki->subjectPublicKey;
    DER_ConvertBitString (&os);

    tag = SECOID_GetAlgorithmTag(&spki->algorithm);
    switch ( tag ) {
      case SEC_OID_X500_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_RSA_ENCRYPTION:
        pubk->keyType = NSSLOWKEYRSAKey;
        rv = SEC_ASN1DecodeItem(arena, pubk, 
				nsslowcert_RSAPublicKeyTemplate, &os);
        if (rv == SECSuccess)
            return pubk;
        break;
      case SEC_OID_ANSIX9_DSA_SIGNATURE:
        pubk->keyType = NSSLOWKEYDSAKey;
        rv = SEC_ASN1DecodeItem(arena, pubk,
				 nsslowcert_DSAPublicKeyTemplate, &os);
        if (rv == SECSuccess) return pubk;
        break;
      case SEC_OID_X942_DIFFIE_HELMAN_KEY:
        pubk->keyType = NSSLOWKEYDHKey;
        rv = SEC_ASN1DecodeItem(arena, pubk,
				 nsslowcert_DHPublicKeyTemplate, &os);
        if (rv == SECSuccess) return pubk;
        break;
      default:
        rv = SECFailure;
        break;
    }

    nsslowkey_DestroyPublicKey (pubk);
    return NULL;
}

