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
 * $Id: certdb.c,v 1.3 2000/09/06 22:10:06 relyea%netscape.com Exp $
 */

#include "prlock.h"
#include "prmon.h"
#include "prtime.h"
#include "cert.h"
#include "secder.h"
#include "secoid.h"
#include "secasn1.h"
#include "blapi.h"		/* for SHA1_HashBuf */
#include "genname.h"
#include "keyhi.h"
#include "secitem.h"
#include "mcom_db.h"
#include "certdb.h"
#include "prprf.h"
#include "sechash.h"
#include "prlong.h"
#include "certxutl.h"
#include "portreg.h"
#include "secerr.h"
#include "sslerr.h"
#include "nsslocks.h"
#include "cdbhdl.h"

/*
 * Certificate database handling code
 */


const SEC_ASN1Template CERT_CertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCertExtension) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTCertExtension,id) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,		/* XXX DER_DEFAULT */
	  offsetof(CERTCertExtension,critical) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(CERTCertExtension,value) },
    { 0, }
};

const SEC_ASN1Template CERT_SequenceOfCertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, CERT_CertExtensionTemplate }
};

const SEC_ASN1Template CERT_CertificateTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(CERTCertificate) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0, 		/* XXX DER_DEFAULT */ 
	  offsetof(CERTCertificate,version),
	  SEC_IntegerTemplate },
    { SEC_ASN1_INTEGER,
	  offsetof(CERTCertificate,serialNumber) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCertificate,signature),
	  SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_SAVE, 
	  offsetof(CERTCertificate,derIssuer) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCertificate,issuer),
	  CERT_NameTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCertificate,validity),
	  CERT_ValidityTemplate },
    { SEC_ASN1_SAVE,
	  offsetof(CERTCertificate,derSubject) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCertificate,subject),
	  CERT_NameTemplate },
    { SEC_ASN1_SAVE,
	  offsetof(CERTCertificate,derPublicKey) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCertificate,subjectPublicKeyInfo),
	  CERT_SubjectPublicKeyInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(CERTCertificate,issuerID),
	  SEC_ObjectIDTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(CERTCertificate,subjectID),
	  SEC_ObjectIDTemplate },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 3,
	  offsetof(CERTCertificate,extensions),
	  CERT_SequenceOfCertExtensionTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_SignedCertificateTemplate[] =
{
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCertificate) },
    { SEC_ASN1_SAVE, 
	  offsetof(CERTCertificate,signatureWrap.data) },
    { SEC_ASN1_INLINE, 
	  0, CERT_CertificateTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCertificate,signatureWrap.signatureAlgorithm),
	  SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(CERTCertificate,signatureWrap.signature) },
    { 0 }
};

/*
 * Find the subjectName in a DER encoded certificate
 */
const SEC_ASN1Template SEC_CertSubjectTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SECItem) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  0, SEC_SkipTemplate },	/* version */
    { SEC_ASN1_SKIP },		/* serial number */
    { SEC_ASN1_SKIP },		/* signature algorithm */
    { SEC_ASN1_SKIP },		/* issuer */
    { SEC_ASN1_SKIP },		/* validity */
    { SEC_ASN1_ANY, 0, NULL },		/* subject */
    { SEC_ASN1_SKIP_REST },
    { 0 }
};

/*
 * Find the issuerName in a DER encoded certificate
 */
const SEC_ASN1Template SEC_CertIssuerTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SECItem) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  0, SEC_SkipTemplate },	/* version */
    { SEC_ASN1_SKIP },		/* serial number */
    { SEC_ASN1_SKIP },		/* signature algorithm */
    { SEC_ASN1_ANY, 0, NULL },		/* issuer */
    { SEC_ASN1_SKIP_REST },
    { 0 }
};
/*
 * Find the subjectName in a DER encoded certificate
 */
const SEC_ASN1Template SEC_CertSerialNumberTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SECItem) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  0, SEC_SkipTemplate },	/* version */
    { SEC_ASN1_ANY, 0, NULL }, /* serial number */
    { SEC_ASN1_SKIP_REST },
    { 0 }
};

/*
 * Find the issuer and serialNumber in a DER encoded certificate.
 * This data is used as the database lookup key since its the unique
 * identifier of a certificate.
 */
const SEC_ASN1Template CERT_CertKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCertKey) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  0, SEC_SkipTemplate },	/* version */ 
    { SEC_ASN1_INTEGER,
	  offsetof(CERTCertKey,serialNumber) },
    { SEC_ASN1_SKIP },		/* signature algorithm */
    { SEC_ASN1_ANY,
	  offsetof(CERTCertKey,derIssuer) },
    { SEC_ASN1_SKIP_REST },
    { 0 }
};



SECStatus
CERT_KeyFromIssuerAndSN(PRArenaPool *arena, SECItem *issuer, SECItem *sn,
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
 * Extract the subject name from a DER certificate
 */
SECStatus
CERT_NameFromDERCert(SECItem *derCert, SECItem *derName)
{
    int rv;
    PRArenaPool *arena;
    CERTSignedData sd;
    void *tmpptr;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( ! arena ) {
	return(SECFailure);
    }
   
    PORT_Memset(&sd, 0, sizeof(CERTSignedData));
    rv = SEC_ASN1DecodeItem(arena, &sd, CERT_SignedDataTemplate, derCert);
    
    if ( rv ) {
	goto loser;
    }
    
    PORT_Memset(derName, 0, sizeof(SECItem));
    rv = SEC_ASN1DecodeItem(arena, derName, SEC_CertSubjectTemplate, &sd.data);

    if ( rv ) {
	goto loser;
    }

    tmpptr = derName->data;
    derName->data = (unsigned char*)PORT_Alloc(derName->len);
    if ( derName->data == NULL ) {
	goto loser;
    }
    
    PORT_Memcpy(derName->data, tmpptr, derName->len);
    
    PORT_FreeArena(arena, PR_FALSE);
    return(SECSuccess);

loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(SECFailure);
}

SECStatus
CERT_IssuerNameFromDERCert(SECItem *derCert, SECItem *derName)
{
    int rv;
    PRArenaPool *arena;
    CERTSignedData sd;
    void *tmpptr;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( ! arena ) {
	return(SECFailure);
    }
   
    PORT_Memset(&sd, 0, sizeof(CERTSignedData));
    rv = SEC_ASN1DecodeItem(arena, &sd, CERT_SignedDataTemplate, derCert);
    
    if ( rv ) {
	goto loser;
    }
    
    PORT_Memset(derName, 0, sizeof(SECItem));
    rv = SEC_ASN1DecodeItem(arena, derName, SEC_CertIssuerTemplate, &sd.data);

    if ( rv ) {
	goto loser;
    }

    tmpptr = derName->data;
    derName->data = (unsigned char*)PORT_Alloc(derName->len);
    if ( derName->data == NULL ) {
	goto loser;
    }
    
    PORT_Memcpy(derName->data, tmpptr, derName->len);
    
    PORT_FreeArena(arena, PR_FALSE);
    return(SECSuccess);

loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(SECFailure);
}

SECStatus
CERT_SerialNumberFromDERCert(SECItem *derCert, SECItem *derName)
{
    int rv;
    PRArenaPool *arena;
    CERTSignedData sd;
    void *tmpptr;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( ! arena ) {
	return(SECFailure);
    }
   
    PORT_Memset(&sd, 0, sizeof(CERTSignedData));
    rv = SEC_ASN1DecodeItem(arena, &sd, CERT_SignedDataTemplate, derCert);
    
    if ( rv ) {
	goto loser;
    }
    
    PORT_Memset(derName, 0, sizeof(SECItem));
    rv = SEC_ASN1DecodeItem(arena, derName, SEC_CertSerialNumberTemplate, &sd.data);

    if ( rv ) {
	goto loser;
    }

    tmpptr = derName->data;
    derName->data = (unsigned char*)PORT_Alloc(derName->len);
    if ( derName->data == NULL ) {
	goto loser;
    }
    
    PORT_Memcpy(derName->data, tmpptr, derName->len);
    
    PORT_FreeArena(arena, PR_FALSE);
    return(SECSuccess);

loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(SECFailure);
}

/*
 * Generate a database key, based on serial number and issuer, from a
 * DER certificate.
 */
SECStatus
CERT_KeyFromDERCert(PRArenaPool *arena, SECItem *derCert, SECItem *key)
{
    int rv;
    CERTSignedData sd;
    CERTCertKey certkey;

    PORT_Memset(&sd, 0, sizeof(CERTSignedData));    
    PORT_Memset(&certkey, 0, sizeof(CERTCertKey));    

    PORT_Memset(&sd, 0, sizeof(CERTSignedData));
    rv = SEC_ASN1DecodeItem(arena, &sd, CERT_SignedDataTemplate, derCert);
    
    if ( rv ) {
	goto loser;
    }
    
    PORT_Memset(&certkey, 0, sizeof(CERTCertKey));
    rv = SEC_ASN1DecodeItem(arena, &certkey, CERT_CertKeyTemplate, &sd.data);

    if ( rv ) {
	goto loser;
    }

    return(CERT_KeyFromIssuerAndSN(arena, &certkey.derIssuer,
				   &certkey.serialNumber, key));
loser:
    return(SECFailure);
}

/*
 * fill in keyUsage field of the cert based on the cert extension
 * if the extension is not critical, then we allow all uses
 */
static SECStatus
GetKeyUsage(CERTCertificate *cert)
{
    SECStatus rv;
    SECItem tmpitem;
    
    rv = CERT_FindKeyUsageExtension(cert, &tmpitem);
    if ( rv == SECSuccess ) {
	/* remember the actual value of the extension */
	cert->rawKeyUsage = tmpitem.data[0];
	cert->keyUsagePresent = PR_TRUE;
	cert->keyUsage = tmpitem.data[0];

	PORT_Free(tmpitem.data);
	tmpitem.data = NULL;
	
    } else {
	/* if the extension is not present, then we allow all uses */
	cert->keyUsage = KU_ALL;
	cert->rawKeyUsage = KU_ALL;
	cert->keyUsagePresent = PR_FALSE;
    }

    if ( CERT_GovtApprovedBitSet(cert) ) {
	cert->keyUsage |= KU_NS_GOVT_APPROVED;
	cert->rawKeyUsage |= KU_NS_GOVT_APPROVED;
    }
    
    return(SECSuccess);
}


/*
 * determine if a fortezza V1 Cert is a CA or not.
 */
static PRBool
fortezzaIsCA( CERTCertificate *cert) {
    PRBool isCA = PR_FALSE;
    CERTSubjectPublicKeyInfo *spki = &cert->subjectPublicKeyInfo;
    int tag;

    tag = SECOID_GetAlgorithmTag(&spki->algorithm);
    if ((tag == SEC_OID_MISSI_KEA_DSS_OLD) ||
       (tag == SEC_OID_MISSI_KEA_DSS) ||
       (tag == SEC_OID_MISSI_DSS_OLD) ||
       (tag == SEC_OID_MISSI_DSS) ) {
	SECItem rawkey;
	unsigned char *rawptr;
	unsigned char *end;
	int len;

        rawkey = spki->subjectPublicKey;
	DER_ConvertBitString(&rawkey);
	rawptr = rawkey.data;
	end = rawkey.data + rawkey.len;

	/* version */	
	rawptr += sizeof(((SECKEYPublicKey*)0)->u.fortezza.KMID)+2;

	/* clearance (the string up to the first byte with the hi-bit on */
	while ((rawptr < end) && (*rawptr++ & 0x80));
	if (rawptr >= end) { return PR_FALSE; }

	/* KEAPrivilege (the string up to the first byte with the hi-bit on */
	while ((rawptr < end) && (*rawptr++ & 0x80));
	if (rawptr >= end) { return PR_FALSE; }

	/* skip the key */
	len = (*rawptr << 8) | rawptr[1];
	rawptr += 2 + len;

	/* shared key */
	if (rawptr >= end) { return PR_FALSE; }
	/* DSS Version is next */
	rawptr += 2;

	/* DSSPrivilege (the string up to the first byte with the hi-bit on */
	if (*rawptr & 0x30) isCA = PR_TRUE;
        
   }
   return isCA;
}

static SECStatus
findOIDinOIDSeqByTagNum(CERTOidSequence *seq, SECOidTag tagnum)
{
    SECItem **oids;
    SECItem *oid;
    SECStatus rv = SECFailure;
    
    if (seq != NULL) {
	oids = seq->oids;
	while (oids != NULL && *oids != NULL) {
	    oid = *oids;
	    if (SECOID_FindOIDTag(oid) == tagnum) {
		rv = SECSuccess;
		break;
	    }
	    oids++;
	}
    }
    return rv;
}

/*
 * fill in nsCertType field of the cert based on the cert extension
 */
SECStatus
CERT_GetCertType(CERTCertificate *cert)
{
    SECStatus rv;
    SECItem tmpitem;
    SECItem encodedExtKeyUsage;
    CERTOidSequence *extKeyUsage = NULL;
    PRBool basicConstraintPresent = PR_FALSE;
    CERTBasicConstraints basicConstraint;

    tmpitem.data = NULL;
    CERT_FindNSCertTypeExtension(cert, &tmpitem);
    rv = CERT_FindCertExtension(cert, SEC_OID_X509_EXT_KEY_USAGE, 
				&encodedExtKeyUsage);
    if (rv == SECSuccess) {
	extKeyUsage = CERT_DecodeOidSequence(&encodedExtKeyUsage);
    }
    rv = CERT_FindBasicConstraintExten(cert, &basicConstraint);
    if (rv == SECSuccess) {
	basicConstraintPresent = PR_TRUE;
    }
    if (tmpitem.data != NULL || extKeyUsage != NULL) {
	if (tmpitem.data == NULL) {
	    cert->nsCertType = 0;
	} else {
	    cert->nsCertType = tmpitem.data[0];
	}

	/* free tmpitem data pointer to avoid memory leak */
	PORT_Free(tmpitem.data);
	tmpitem.data = NULL;
	
	/*
	 * for this release, we will allow SSL certs with an email address
	 * to be used for email
	 */
	if ( ( cert->nsCertType & NS_CERT_TYPE_SSL_CLIENT ) &&
	    cert->emailAddr ) {
	    cert->nsCertType |= NS_CERT_TYPE_EMAIL;
	}
	/*
	 * for this release, we will allow SSL intermediate CAs to be
	 * email intermediate CAs too.
	 */
	if ( cert->nsCertType & NS_CERT_TYPE_SSL_CA ) {
	    cert->nsCertType |= NS_CERT_TYPE_EMAIL_CA;
	}
	/*
	 * allow a cert with the extended key usage of EMail Protect
	 * to be used for email or as an email CA, if basic constraints
	 * indicates that it is a CA.
	 */
	if (findOIDinOIDSeqByTagNum(extKeyUsage, 
				    SEC_OID_EXT_KEY_USAGE_EMAIL_PROTECT) ==
	    SECSuccess) {
	    if (basicConstraintPresent == PR_TRUE &&
		(basicConstraint.isCA)) {
		cert->nsCertType |= NS_CERT_TYPE_EMAIL_CA;
	    } else {
		cert->nsCertType |= NS_CERT_TYPE_EMAIL;
	    }
	}
	if (findOIDinOIDSeqByTagNum(extKeyUsage, 
				    SEC_OID_EXT_KEY_USAGE_SERVER_AUTH) ==
	    SECSuccess){
	    if (basicConstraintPresent == PR_TRUE &&
		(basicConstraint.isCA)) {
		cert->nsCertType |= NS_CERT_TYPE_SSL_CA;
	    } else {
		cert->nsCertType |= NS_CERT_TYPE_SSL_SERVER;
	    }
	}
	if (findOIDinOIDSeqByTagNum(extKeyUsage,
				    SEC_OID_EXT_KEY_USAGE_CLIENT_AUTH) ==
	    SECSuccess){
	    if (basicConstraintPresent == PR_TRUE &&
		(basicConstraint.isCA)) {
		cert->nsCertType |= NS_CERT_TYPE_SSL_CA;
	    } else {
		cert->nsCertType |= NS_CERT_TYPE_SSL_CLIENT;
	    }
	}
	if (findOIDinOIDSeqByTagNum(extKeyUsage,
				    SEC_OID_EXT_KEY_USAGE_CODE_SIGN) ==
	    SECSuccess) {
	    if (basicConstraintPresent == PR_TRUE &&
		(basicConstraint.isCA)) {
		cert->nsCertType |= NS_CERT_TYPE_OBJECT_SIGNING_CA;
	    } else {
		cert->nsCertType |= NS_CERT_TYPE_OBJECT_SIGNING;
	    }
	}
	if (findOIDinOIDSeqByTagNum(extKeyUsage,
				    SEC_OID_EXT_KEY_USAGE_TIME_STAMP) ==
	    SECSuccess) {
	    cert->nsCertType |= EXT_KEY_USAGE_TIME_STAMP;
	}
	if (findOIDinOIDSeqByTagNum(extKeyUsage,
				    SEC_OID_OCSP_RESPONDER) == 
	    SECSuccess) {
	    cert->nsCertType |= EXT_KEY_USAGE_STATUS_RESPONDER;
	}
    } else {
	/* if no extension, then allow any ssl or email (no ca or object
	 * signing)
	 */
	cert->nsCertType = NS_CERT_TYPE_SSL_CLIENT | NS_CERT_TYPE_SSL_SERVER |
	    NS_CERT_TYPE_EMAIL;

        /* if the basic constraint extension says the cert is a CA, then
	   allow SSL CA and EMAIL CA and Status Responder */
	if ((basicConstraintPresent == PR_TRUE)
	    && (basicConstraint.isCA)) {
	        cert->nsCertType |= NS_CERT_TYPE_SSL_CA;
	        cert->nsCertType |= NS_CERT_TYPE_EMAIL_CA;
		cert->nsCertType |= EXT_KEY_USAGE_STATUS_RESPONDER;
	} else if (CERT_IsCACert(cert, NULL) == PR_TRUE) {
		cert->nsCertType |= EXT_KEY_USAGE_STATUS_RESPONDER;
	}

	/* if the cert is a fortezza CA cert, then allow SSL CA and EMAIL CA */
	if (fortezzaIsCA(cert)) {
	        cert->nsCertType |= NS_CERT_TYPE_SSL_CA;
	        cert->nsCertType |= NS_CERT_TYPE_EMAIL_CA;
	}
    }

    if (extKeyUsage != NULL) {
	PORT_Free(encodedExtKeyUsage.data);
	CERT_DestroyOidSequence(extKeyUsage);
    }
    return(SECSuccess);
}

/*
 * cert_GetKeyID() - extract or generate the subjectKeyID from a certificate
 */
SECStatus
cert_GetKeyID(CERTCertificate *cert)
{
    SECItem tmpitem;
    SECStatus rv;
    SECKEYPublicKey *key;
    
    cert->subjectKeyID.len = 0;

    /* see of the cert has a key identifier extension */
    rv = CERT_FindSubjectKeyIDExten(cert, &tmpitem);
    if ( rv == SECSuccess ) {
	cert->subjectKeyID.data = (unsigned char*) PORT_ArenaAlloc(cert->arena, tmpitem.len);
	if ( cert->subjectKeyID.data != NULL ) {
	    PORT_Memcpy(cert->subjectKeyID.data, tmpitem.data, tmpitem.len);
	    cert->subjectKeyID.len = tmpitem.len;
	    cert->keyIDGenerated = PR_FALSE;
	}
	
	PORT_Free(tmpitem.data);
    }
    
    /* if the cert doesn't have a key identifier extension and the cert is
     * a V1 fortezza certificate, use the cert's 8 byte KMID as the
     * key identifier.  */
    key = CERT_KMIDPublicKey(cert);

    if (key != NULL) {
	
	if (key->keyType == fortezzaKey) {

	    cert->subjectKeyID.data = (unsigned char *)PORT_ArenaAlloc(cert->arena, 8);
	    if ( cert->subjectKeyID.data != NULL ) {
	        PORT_Memcpy(cert->subjectKeyID.data, key->u.fortezza.KMID, 8);
	        cert->subjectKeyID.len = 8;
	        cert->keyIDGenerated = PR_FALSE;
	    }
	}
		
	SECKEY_DestroyPublicKey(key);
    }

    /* if the cert doesn't have a key identifier extension, then generate one*/
    if ( cert->subjectKeyID.len == 0 ) {
	/*
	 * pkix says that if the subjectKeyID is not present, then we should
	 * use the SHA-1 hash of the DER-encoded publicKeyInfo from the cert
	 */
	cert->subjectKeyID.data = (unsigned char *)PORT_ArenaAlloc(cert->arena, SHA1_LENGTH);
	if ( cert->subjectKeyID.data != NULL ) {
	    rv = SHA1_HashBuf(cert->subjectKeyID.data,
			      cert->derPublicKey.data,
			      cert->derPublicKey.len);
	    if ( rv == SECSuccess ) {
		cert->subjectKeyID.len = SHA1_LENGTH;
	    }
	}
    }

    if ( cert->subjectKeyID.len == 0 ) {
	return(SECFailure);
    }
    return(SECSuccess);

}

/*
 * take a DER certificate and decode it into a certificate structure
 */
CERTCertificate *
CERT_DecodeDERCertificate(SECItem *derSignedCert, PRBool copyDER,
			 char *nickname)
{
    CERTCertificate *cert;
    PRArenaPool *arena;
    void *data;
    int rv;
    int len;
    char *tmpname;
    
    /* make a new arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( !arena ) {
	return 0;
    }

    /* allocate the certificate structure */
    cert = (CERTCertificate *)PORT_ArenaZAlloc(arena, sizeof(CERTCertificate));
    
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
    rv = SEC_ASN1DecodeItem(arena, cert, SEC_SignedCertificateTemplate,
		    &cert->derCert);

    if ( rv ) {
	goto loser;
    }

    if (cert_HasUnknownCriticalExten (cert->extensions) == PR_TRUE) {
	PORT_SetError(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
	goto loser;
    }

    /* generate and save the database key for the cert */
    rv = CERT_KeyFromDERCert(arena, &cert->derCert, &cert->certKey);
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

    /* set the email address */
    cert->emailAddr = CERT_GetCertificateEmailAddress(cert);
    
    /* initialize the subjectKeyID */
    rv = cert_GetKeyID(cert);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* initialize keyUsage */
    rv = GetKeyUsage(cert);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* initialize the certType */
    rv = CERT_GetCertType(cert);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    tmpname = CERT_NameToAscii(&cert->subject);
    if ( tmpname != NULL ) {
	cert->subjectName = PORT_ArenaStrdup(cert->arena, tmpname);
	PORT_Free(tmpname);
    }
    
    tmpname = CERT_NameToAscii(&cert->issuer);
    if ( tmpname != NULL ) {
	cert->issuerName = PORT_ArenaStrdup(cert->arena, tmpname);
	PORT_Free(tmpname);
    }
    
    cert->referenceCount = 1;
    cert->slot = NULL;
    cert->pkcs11ID = CK_INVALID_KEY;
    cert->dbnickname = NULL;
    
    return(cert);
    
loser:

    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(0);
}

/*
** Amount of time that a certifiate is allowed good before it is actually
** good. This is used for pending certificates, ones that are about to be
** valid. The slop is designed to allow for some variance in the clocks
** of the machine checking the certificate.
*/
#define PENDING_SLOP (24L*60L*60L)

SECStatus
CERT_GetCertTimes(CERTCertificate *c, int64 *notBefore, int64 *notAfter)
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
 * Check the validity times of a certificate
 */
SECCertTimeValidity
CERT_CheckCertValidTimes(CERTCertificate *c, int64 t, PRBool allowOverride)
{
    int64 notBefore, notAfter, pendingSlop;
    SECStatus rv;

    /* if cert is already marked OK, then don't bother to check */
    if ( allowOverride && c->timeOK ) {
        return(secCertTimeValid);
    }

    rv = CERT_GetCertTimes(c, &notBefore, &notAfter);
    
    if (rv) {
	return(secCertTimeExpired); /*XXX is this the right thing to do here?*/
    }
    
    LL_I2L(pendingSlop, PENDING_SLOP);
    LL_SUB(notBefore, notBefore, pendingSlop);
    if ( LL_CMP( t, <, notBefore ) ) {
	PORT_SetError(SEC_ERROR_EXPIRED_CERTIFICATE);
	return(secCertTimeNotValidYet);
    }
    if ( LL_CMP( t, >, notAfter) ) {
	PORT_SetError(SEC_ERROR_EXPIRED_CERTIFICATE);
	return(secCertTimeExpired);
    }

    return(secCertTimeValid);
}

SECStatus
SEC_GetCrlTimes(CERTCrl *date, int64 *notBefore, int64 *notAfter)
{
    int rv;
    
    /* convert DER not-before time */
    rv = DER_UTCTimeToTime(notBefore, &date->lastUpdate);
    if (rv) {
	return(SECFailure);
    }
    
    /* convert DER not-after time */
    if (date->nextUpdate.data) {
	rv = DER_UTCTimeToTime(notAfter, &date->nextUpdate);
	if (rv) {
	    return(SECFailure);
	}
    }
    else {
	LL_I2L(*notAfter, 0L);
    }
    return(SECSuccess);
}

/* These routines should probably be combined with the cert
 * routines using an common extraction routine.
 */
SECCertTimeValidity
SEC_CheckCrlTimes(CERTCrl *crl, int64 t) {
    int64 notBefore, notAfter, pendingSlop;
    SECStatus rv;

    rv = SEC_GetCrlTimes(crl, &notBefore, &notAfter);
    
    if (rv) {
	return(secCertTimeExpired); 
    }

    LL_I2L(pendingSlop, PENDING_SLOP);
    LL_SUB(notBefore, notBefore, pendingSlop);
    if ( LL_CMP( t, <, notBefore ) ) {
	return(secCertTimeNotValidYet);
    }

    /* If next update is omitted and the test for notBefore passes, then
       we assume that the crl is up to date.
     */
    if ( LL_IS_ZERO(notAfter) ) {
	return(secCertTimeValid);
    }

    if ( LL_CMP( t, >, notAfter) ) {
	return(secCertTimeExpired);
    }

    return(secCertTimeValid);
}

PRBool
SEC_CrlIsNewer(CERTCrl *inNew, CERTCrl *old) {
    int64 newNotBefore, newNotAfter;
    int64 oldNotBefore, oldNotAfter;
    SECStatus rv;

    /* problems with the new CRL? reject it */
    rv = SEC_GetCrlTimes(inNew, &newNotBefore, &newNotAfter);
    if (rv) return PR_FALSE;

    /* problems with the old CRL? replace it */
    rv = SEC_GetCrlTimes(old, &oldNotBefore, &oldNotAfter);
    if (rv) return PR_TRUE;

    /* Question: what about the notAfter's? */
    return ((PRBool)LL_CMP(oldNotBefore, <, newNotBefore));
}
   
/*
 * return required key usage and cert type based on cert usage 
 */
SECStatus
CERT_KeyUsageAndTypeForCertUsage(SECCertUsage usage,
				 PRBool ca,
				 unsigned int *retKeyUsage,
				 unsigned int *retCertType)
{
    unsigned int requiredKeyUsage = 0;
    unsigned int requiredCertType = 0;
    
    if ( ca ) {
	switch ( usage ) {
	  case certUsageSSLServerWithStepUp:
	    requiredKeyUsage = KU_NS_GOVT_APPROVED | KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_SSL_CA;
	    break;
	  case certUsageSSLClient:
	    requiredKeyUsage = KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_SSL_CA;
	    break;
	  case certUsageSSLServer:
	    requiredKeyUsage = KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_SSL_CA;
	    break;
	  case certUsageSSLCA:
	    requiredKeyUsage = KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_SSL_CA;
	    break;
	  case certUsageEmailSigner:
	    requiredKeyUsage = KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_EMAIL_CA;
	    break;
	  case certUsageEmailRecipient:
	    requiredKeyUsage = KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_EMAIL_CA;
	    break;
	  case certUsageObjectSigner:
	    requiredKeyUsage = KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_OBJECT_SIGNING_CA;
	    break;
	  case certUsageAnyCA:
	  case certUsageStatusResponder:
	    requiredKeyUsage = KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_OBJECT_SIGNING_CA |
		NS_CERT_TYPE_EMAIL_CA |
		    NS_CERT_TYPE_SSL_CA;
	    break;
	  default:
	    PORT_Assert(0);
	    goto loser;
	}
    } else {
	switch ( usage ) {
	  case certUsageSSLClient:
	    requiredKeyUsage = KU_DIGITAL_SIGNATURE;
	    requiredCertType = NS_CERT_TYPE_SSL_CLIENT;
	    break;
	  case certUsageSSLServer:
	    requiredKeyUsage = KU_KEY_AGREEMENT_OR_ENCIPHERMENT;
	    requiredCertType = NS_CERT_TYPE_SSL_SERVER;
	    break;
	  case certUsageSSLServerWithStepUp:
	    requiredKeyUsage = KU_KEY_AGREEMENT_OR_ENCIPHERMENT |
		KU_NS_GOVT_APPROVED;
	    requiredCertType = NS_CERT_TYPE_SSL_SERVER;
	    break;
	  case certUsageSSLCA:
	    requiredKeyUsage = KU_KEY_CERT_SIGN;
	    requiredCertType = NS_CERT_TYPE_SSL_CA;
	    break;
	  case certUsageEmailSigner:
	    requiredKeyUsage = KU_DIGITAL_SIGNATURE;
	    requiredCertType = NS_CERT_TYPE_EMAIL;
	    break;
	  case certUsageEmailRecipient:
	    requiredKeyUsage = KU_KEY_AGREEMENT_OR_ENCIPHERMENT;
	    requiredCertType = NS_CERT_TYPE_EMAIL;
	    break;
	  case certUsageObjectSigner:
	    requiredKeyUsage = KU_DIGITAL_SIGNATURE;
	    requiredCertType = NS_CERT_TYPE_OBJECT_SIGNING;
	    break;
	  case certUsageStatusResponder:
	    requiredKeyUsage = KU_DIGITAL_SIGNATURE;
	    requiredCertType = EXT_KEY_USAGE_STATUS_RESPONDER;
	    break;
	  default:
	    PORT_Assert(0);
	    goto loser;
	}
    }

    if ( retKeyUsage != NULL ) {
	*retKeyUsage = requiredKeyUsage;
    }
    if ( retCertType != NULL ) {
	*retCertType = requiredCertType;
    }

    return(SECSuccess);
loser:
    return(SECFailure);
}

/*
 * check the key usage of a cert against a set of required values
 */
SECStatus
CERT_CheckKeyUsage(CERTCertificate *cert, unsigned int requiredUsage)
{
    SECKEYPublicKey *key;
    
    /* choose between key agreement or key encipherment based on key
     * type in cert
     */
    if ( requiredUsage & KU_KEY_AGREEMENT_OR_ENCIPHERMENT ) {
	key = CERT_ExtractPublicKey(cert);
	if ( ( key->keyType == keaKey ) || ( key->keyType == fortezzaKey ) ||
	     ( key->keyType == dhKey ) ) {
	    requiredUsage |= KU_KEY_AGREEMENT;
	} else {
	    requiredUsage |= KU_KEY_ENCIPHERMENT;
	} 

	/* now turn off the special bit */
	requiredUsage &= (~KU_KEY_AGREEMENT_OR_ENCIPHERMENT);
	
	SECKEY_DestroyPublicKey(key);
    }

    if ( ( cert->keyUsage & requiredUsage ) != requiredUsage ) {
	return(SECFailure);
    }
    return(SECSuccess);
}


CERTCertificate *
CERT_DupCertificate(CERTCertificate *c)
{
    if (c) {
	CERT_LockCertRefCount(c);
	++c->referenceCount;
	CERT_UnlockCertRefCount(c);
    }
    return c;
}

/*
 * Allow use of default cert database, so that apps(such as mozilla) don't
 * have to pass the handle all over the place.
 */
static CERTCertDBHandle *default_cert_db_handle = 0;

void
CERT_SetDefaultCertDB(CERTCertDBHandle *handle)
{
    default_cert_db_handle = handle;
    
    return;
}

CERTCertDBHandle *
CERT_GetDefaultCertDB(void)
{
    return(default_cert_db_handle);
}

/*
 * Open volatile certificate database and index databases.  This is a
 * fallback if the real databases can't be opened or created.  It is only
 * resident in memory, so it will not be persistent.  We do this so that
 * we don't crash if the databases can't be created.
 */
SECStatus
CERT_OpenVolatileCertDB(CERTCertDBHandle *handle)
{
    /*
     * Open the memory resident perm cert database.
     */
    handle->permCertDB = dbopen( 0, O_RDWR | O_CREAT, 0600, DB_HASH, 0 );
    if ( !handle->permCertDB ) {
	goto loser;
    }

    /*
     * Open the memory resident decoded cert database.
     */
    handle->tempCertDB = dbopen( 0, O_RDWR | O_CREAT, 0600, DB_HASH, 0 );
    if ( !handle->tempCertDB ) {
	goto loser;
    }

    handle->dbMon = PR_NewMonitor();
    PORT_Assert(handle->dbMon != NULL);

    handle->spkDigestInfo = NULL;
    handle->statusConfig = NULL;

    /* initialize the cert database */
    (void) CERT_InitCertDB(handle);

    return (SECSuccess);
    
loser:

    PORT_SetError(SEC_ERROR_BAD_DATABASE);

    if ( handle->permCertDB ) {
	(* handle->permCertDB->close)(handle->permCertDB);
	handle->permCertDB = 0;
    }

    if ( handle->tempCertDB ) {
	(* handle->tempCertDB->close)(handle->tempCertDB);
	handle->tempCertDB = 0;
    }

    return(SECFailure);
}

/* XXX this would probably be okay/better as an xp routine? */
static void
sec_lower_string(char *s)
{
    if ( s == NULL ) {
	return;
    }
    
    while ( *s ) {
	*s = PORT_Tolower(*s);
	s++;
    }
    
    return;
}

/*
** Add a domain name to the list of names that the user has explicitly
** allowed (despite cert name mismatches) for use with a server cert.
*/
SECStatus
CERT_AddOKDomainName(CERTCertificate *cert, const char *hn)
{
    CERTOKDomainName *domainOK;
    int               newNameLen;

    if (!hn || !(newNameLen = strlen(hn))) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    domainOK = (CERTOKDomainName *)PORT_ArenaZAlloc(cert->arena, 
                                  (sizeof *domainOK) + newNameLen);
    if (!domainOK) 
    	return SECFailure;	/* error code is already set. */

    PORT_Strcpy(domainOK->name, hn);
    sec_lower_string(domainOK->name);

    /* put at head of list. */
    domainOK->next = cert->domainOK;
    cert->domainOK = domainOK;
    return SECSuccess;
}

/* Make sure that the name of the host we are connecting to matches the
 * name that is incoded in the common-name component of the certificate
 * that they are using.
 */
SECStatus
CERT_VerifyCertName(CERTCertificate *cert, const char *hn)
{
    char *    cn;
    char *    domain;
    char *    hndomain;
    char *    hostname;
    int       regvalid;
    int       match;
    SECStatus rv;
    CERTOKDomainName *domainOK;

    if (!hn || !strlen(hn)) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    hostname = PORT_Strdup(hn);
    if ( hostname == NULL ) {
	return(SECFailure);
    }
    sec_lower_string(hostname);

    /* if the name is one that the user has already approved, it's OK. */
    for (domainOK = cert->domainOK; domainOK; domainOK = domainOK->next) {
	if (0 == PORT_Strcmp(hostname, domainOK->name)) {
	    PORT_Free(hostname);
	    return SECSuccess;
    	}
    }

    /* try the cert extension first, then the common name */
    cn = CERT_FindNSStringExtension(cert, SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME);
    if ( cn == NULL ) {
	cn = CERT_GetCommonName(&cert->subject);
    }
    
    sec_lower_string(cn);

    if ( cn ) {
	if ( ( hndomain = PORT_Strchr(hostname, '.') ) == NULL ) {
	    /* No domain in server name */
	    if ( ( domain = PORT_Strchr(cn, '.') ) != NULL ) {
		/* there is a domain in the cn string, so chop it off */
		*domain = '\0';
	    }
	}

	regvalid = PORT_RegExpValid(cn);
	
	if ( regvalid == NON_SXP ) {
	    /* compare entire hostname with cert name */
	    if ( PORT_Strcmp(hostname, cn) == 0 ) {
		rv = SECSuccess;
		goto done;
	    }
	    
	    if ( hndomain ) {
		/* compare just domain name with cert name */
		if ( PORT_Strcmp(hndomain+1, cn) == 0 ) {
		    rv = SECSuccess;
		    goto done;
		}
	    }

	    PORT_SetError(SSL_ERROR_BAD_CERT_DOMAIN);
	    rv = SECFailure;
	    goto done;
	    
	} else {
	    /* try to match the shexp */
	    match = PORT_RegExpCaseSearch(hostname, cn);

	    if ( match == 0 ) {
		rv = SECSuccess;
	    } else {
		PORT_SetError(SSL_ERROR_BAD_CERT_DOMAIN);
		rv = SECFailure;
	    }
	    goto done;
	}
    }

    PORT_SetError(SEC_ERROR_NO_MEMORY);
    rv = SECFailure;

done:
    /* free the common name */
    if ( cn ) {
	PORT_Free(cn);
    }
    
    if ( hostname ) {
	PORT_Free(hostname);
    }
    
    return(rv);
}

PRBool
CERT_CompareCerts(CERTCertificate *c1, CERTCertificate *c2)
{
    SECComparison comp;
    
    comp = SECITEM_CompareItem(&c1->derCert, &c2->derCert);
    if ( comp == SECEqual ) { /* certs are the same */
	return(PR_TRUE);
    } else {
	return(PR_FALSE);
    }
}

static SECStatus
StringsEqual(char *s1, char *s2) {
    if ( ( s1 == NULL ) || ( s2 == NULL ) ) {
	if ( s1 != s2 ) { /* only one is null */
	    return(SECFailure);
	}
	return(SECSuccess); /* both are null */
    }
	
    if ( PORT_Strcmp( s1, s2 ) != 0 ) {
	return(SECFailure); /* not equal */
    }

    return(SECSuccess); /* strings are equal */
}


PRBool
CERT_CompareCertsForRedirection(CERTCertificate *c1, CERTCertificate *c2)
{
    SECComparison comp;
    char *c1str, *c2str;
    SECStatus eq;
    
    comp = SECITEM_CompareItem(&c1->derCert, &c2->derCert);
    if ( comp == SECEqual ) { /* certs are the same */
	return(PR_TRUE);
    }
	
    /* check if they are issued by the same CA */
    comp = SECITEM_CompareItem(&c1->derIssuer, &c2->derIssuer);
    if ( comp != SECEqual ) { /* different issuer */
	return(PR_FALSE);
    }

    /* check country name */
    c1str = CERT_GetCountryName(&c1->subject);
    c2str = CERT_GetCountryName(&c2->subject);
    eq = StringsEqual(c1str, c2str);
    PORT_Free(c1str);
    PORT_Free(c2str);
    if ( eq != SECSuccess ) {
	return(PR_FALSE);
    }

    /* check locality name */
    c1str = CERT_GetLocalityName(&c1->subject);
    c2str = CERT_GetLocalityName(&c2->subject);
    eq = StringsEqual(c1str, c2str);
    PORT_Free(c1str);
    PORT_Free(c2str);
    if ( eq != SECSuccess ) {
	return(PR_FALSE);
    }
	
    /* check state name */
    c1str = CERT_GetStateName(&c1->subject);
    c2str = CERT_GetStateName(&c2->subject);
    eq = StringsEqual(c1str, c2str);
    PORT_Free(c1str);
    PORT_Free(c2str);
    if ( eq != SECSuccess ) {
	return(PR_FALSE);
    }

    /* check org name */
    c1str = CERT_GetOrgName(&c1->subject);
    c2str = CERT_GetOrgName(&c2->subject);
    eq = StringsEqual(c1str, c2str);
    PORT_Free(c1str);
    PORT_Free(c2str);
    if ( eq != SECSuccess ) {
	return(PR_FALSE);
    }

#ifdef NOTDEF	
    /* check orgUnit name */
    /*
     * We need to revisit this and decide which fields should be allowed to be
     * different
     */
    c1str = CERT_GetOrgUnitName(&c1->subject);
    c2str = CERT_GetOrgUnitName(&c2->subject);
    eq = StringsEqual(c1str, c2str);
    PORT_Free(c1str);
    PORT_Free(c2str);
    if ( eq != SECSuccess ) {
	return(PR_FALSE);
    }
#endif

    return(PR_TRUE); /* all fields but common name are the same */
}


/* CERT_CertChainFromCert and CERT_DestroyCertificateList moved
   to certhigh.c */


CERTIssuerAndSN *
CERT_GetCertIssuerAndSN(PRArenaPool *arena, CERTCertificate *cert)
{
    CERTIssuerAndSN *result;
    SECStatus rv;

    if ( arena == NULL ) {
	arena = cert->arena;
    }
    
    result = (CERTIssuerAndSN*)PORT_ArenaZAlloc(arena, sizeof(*result));
    if (result == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    rv = SECITEM_CopyItem(arena, &result->derIssuer, &cert->derIssuer);
    if (rv != SECSuccess)
	return NULL;

    rv = CERT_CopyName(arena, &result->issuer, &cert->issuer);
    if (rv != SECSuccess)
	return NULL;

    rv = SECITEM_CopyItem(arena, &result->serialNumber, &cert->serialNumber);
    if (rv != SECSuccess)
	return NULL;

    return result;
}

char *
CERT_MakeCANickname(CERTCertificate *cert)
{
    char *firstname = NULL;
    char *org = NULL;
    char *nickname = NULL;
    int count;
    CERTCertificate *dummycert;
    CERTCertDBHandle *handle;
    
    handle = cert->dbhandle;
    
    nickname = CERT_GetNickName(cert, handle, cert->arena);
    if (nickname == NULL) {
	firstname = CERT_GetCommonName(&cert->subject);
	if ( firstname == NULL ) {
	    firstname = CERT_GetOrgUnitName(&cert->subject);
	}

	org = CERT_GetOrgName(&cert->issuer);
	if ( org == NULL ) {
	    goto loser;
	}
    
	count = 1;
	while ( 1 ) {

	    if ( firstname ) {
		if ( count == 1 ) {
		    nickname = PR_smprintf("%s - %s", firstname, org);
		} else {
		    nickname = PR_smprintf("%s - %s #%d", firstname, org, count);
		}
	    } else {
		if ( count == 1 ) {
		    nickname = PR_smprintf("%s", org);
		} else {
		    nickname = PR_smprintf("%s #%d", org, count);
		}
	    }
	    if ( nickname == NULL ) {
		goto loser;
	    }

	    /* look up the nickname to make sure it isn't in use already */
	    dummycert = CERT_FindCertByNickname(handle, nickname);

	    if ( dummycert == NULL ) {
		goto done;
	    }
	
	    /* found a cert, destroy it and loop */
	    CERT_DestroyCertificate(dummycert);

	    /* free the nickname */
	    PORT_Free(nickname);

	    count++;
	}
    }
loser:
    if ( nickname ) {
	PORT_Free(nickname);
    }

    nickname = "";
    
done:
    if ( firstname ) {
	PORT_Free(firstname);
    }
    if ( org ) {
	PORT_Free(org);
    }
    
    return(nickname);
}

/* CERT_Import_CAChain moved to certhigh.c */

void
CERT_DestroyCrl (CERTSignedCrl *crl)
{
    SEC_DestroyCrl (crl);
}



/*
 * Does a cert belong to a CA?  We decide based on perm database trust
 * flags, Netscape Cert Type Extension, and KeyUsage Extension.
 */
PRBool
CERT_IsCACert(CERTCertificate *cert, unsigned int *rettype)
{
    CERTCertTrust *trust;
    SECStatus rv;
    unsigned int type;
    PRBool ret;

    ret = PR_FALSE;
    type = 0;
    
    if ( cert->isperm ) {
	trust = cert->trust;
	if ( ( trust->sslFlags & CERTDB_VALID_CA ) == CERTDB_VALID_CA ) {
	    ret = PR_TRUE;
	    type |= NS_CERT_TYPE_SSL_CA;
	}
	
	if ( ( trust->emailFlags & CERTDB_VALID_CA ) == CERTDB_VALID_CA ) {
	    ret = PR_TRUE;
	    type |= NS_CERT_TYPE_EMAIL_CA;
	}
	
	if ( ( trust->objectSigningFlags & CERTDB_VALID_CA ) ==
	    CERTDB_VALID_CA ) {
	    ret = PR_TRUE;
	    type |= NS_CERT_TYPE_OBJECT_SIGNING_CA;
	}
    } else {
	if ( cert->nsCertType &
	    ( NS_CERT_TYPE_SSL_CA | NS_CERT_TYPE_EMAIL_CA |
	     NS_CERT_TYPE_OBJECT_SIGNING_CA ) ) {
	    ret = PR_TRUE;
	    type = (cert->nsCertType & NS_CERT_TYPE_CA);
	} else {
	    CERTBasicConstraints constraints;
	    rv = CERT_FindBasicConstraintExten(cert, &constraints);
	    if ( rv == SECSuccess ) {
		if ( constraints.isCA ) {
		    ret = PR_TRUE;
		    type = (NS_CERT_TYPE_SSL_CA | NS_CERT_TYPE_EMAIL_CA);
		}
	    } 
	} 

	/* finally check if it's a FORTEZZA V1 CA */
	if (ret == PR_FALSE) {
	    if (fortezzaIsCA(cert)) {
		ret = PR_TRUE;
		type = (NS_CERT_TYPE_SSL_CA | NS_CERT_TYPE_EMAIL_CA);
	    }
	}
    }
    if ( rettype != NULL ) {
	*rettype = type;
    }
    
    return(ret);
}

/*
 * is certa newer than certb?  If one is expired, pick the other one.
 */
PRBool
CERT_IsNewer(CERTCertificate *certa, CERTCertificate *certb)
{
    int64 notBeforeA, notAfterA, notBeforeB, notAfterB, now;
    SECStatus rv;
    PRBool newerbefore, newerafter;
    
    rv = CERT_GetCertTimes(certa, &notBeforeA, &notAfterA);
    if ( rv != SECSuccess ) {
	return(PR_FALSE);
    }
    
    rv = CERT_GetCertTimes(certb, &notBeforeB, &notAfterB);
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

void
CERT_DestroyCertArray(CERTCertificate **certs, unsigned int ncerts)
{
    unsigned int i;
    
    if ( certs ) {
	for ( i = 0; i < ncerts; i++ ) {
	    if ( certs[i] ) {
		CERT_DestroyCertificate(certs[i]);
	    }
	}

	PORT_Free(certs);
    }
    
    return;
}

char *
CERT_FixupEmailAddr(char *emailAddr)
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

/*
 * NOTE - don't allow encode of govt-approved or invisible bits
 */
SECStatus
CERT_DecodeTrustString(CERTCertTrust *trust, char *trusts)
{
    int i;
    unsigned int *pflags;
    
    trust->sslFlags = 0;
    trust->emailFlags = 0;
    trust->objectSigningFlags = 0;

    pflags = &trust->sslFlags;
    
    for (i=0; i < PORT_Strlen(trusts); i++) {
	switch (trusts[i]) {
	  case 'p':
	      *pflags = *pflags | CERTDB_VALID_PEER;
	      break;

	  case 'P':
	      *pflags = *pflags | CERTDB_TRUSTED | CERTDB_VALID_PEER;
	      break;

	  case 'w':
	      *pflags = *pflags | CERTDB_SEND_WARN;
	      break;

	  case 'c':
	      *pflags = *pflags | CERTDB_VALID_CA;
	      break;

	  case 'T':
	      *pflags = *pflags | CERTDB_TRUSTED_CLIENT_CA | CERTDB_VALID_CA;
	      break;

	  case 'C' :
	      *pflags = *pflags | CERTDB_TRUSTED_CA | CERTDB_VALID_CA;
	      break;

	  case 'u':
	      *pflags = *pflags | CERTDB_USER;
	      break;

#ifdef DEBUG_NSSTEAM_ONLY
	  case 'i':
	      *pflags = *pflags | CERTDB_INVISIBLE_CA;
	      break;
	  case 'g':
	      *pflags = *pflags | CERTDB_GOVT_APPROVED_CA;
	      break;
#endif /* DEBUG_NSSTEAM_ONLY */

	  case ',':
	      if ( pflags == &trust->sslFlags ) {
		  pflags = &trust->emailFlags;
	      } else {
		  pflags = &trust->objectSigningFlags;
	      }
	      break;
	  default:
	      return SECFailure;
	}
    }

    return SECSuccess;
}

static void
EncodeFlags(char *trusts, unsigned int flags)
{
    if (flags & CERTDB_VALID_CA)
	if (!(flags & CERTDB_TRUSTED_CA) &&
	    !(flags & CERTDB_TRUSTED_CLIENT_CA))
	    PORT_Strcat(trusts, "c");
    if (flags & CERTDB_VALID_PEER)
	if (!(flags & CERTDB_TRUSTED))
	    PORT_Strcat(trusts, "p");
    if (flags & CERTDB_TRUSTED_CA)
	PORT_Strcat(trusts, "C");
    if (flags & CERTDB_TRUSTED_CLIENT_CA)
	PORT_Strcat(trusts, "T");
    if (flags & CERTDB_TRUSTED)
	PORT_Strcat(trusts, "P");
    if (flags & CERTDB_USER)
	PORT_Strcat(trusts, "u");
    if (flags & CERTDB_SEND_WARN)
	PORT_Strcat(trusts, "w");
    if (flags & CERTDB_INVISIBLE_CA)
	PORT_Strcat(trusts, "I");
    if (flags & CERTDB_GOVT_APPROVED_CA)
	PORT_Strcat(trusts, "G");
    return;
}

char *
CERT_EncodeTrustString(CERTCertTrust *trust)
{
    char tmpTrustSSL[32];
    char tmpTrustEmail[32];
    char tmpTrustSigning[32];
    char *retstr = NULL;

    if ( trust ) {
	tmpTrustSSL[0] = '\0';
	tmpTrustEmail[0] = '\0';
	tmpTrustSigning[0] = '\0';
    
	EncodeFlags(tmpTrustSSL, trust->sslFlags);
	EncodeFlags(tmpTrustEmail, trust->emailFlags);
	EncodeFlags(tmpTrustSigning, trust->objectSigningFlags);
    
	retstr = PR_smprintf("%s,%s,%s", tmpTrustSSL, tmpTrustEmail,
			     tmpTrustSigning);
    }
    
    return(retstr);
}

SECStatus
CERT_ImportCerts(CERTCertDBHandle *certdb, SECCertUsage usage,
		 unsigned int ncerts, SECItem **derCerts,
		 CERTCertificate ***retCerts, PRBool keepCerts,
		 PRBool caOnly, char *nickname)
{
    int i;
    CERTCertificate **certs = NULL;
    SECStatus rv;
    int fcerts;

    if ( ncerts ) {
	certs = (CERTCertificate**)PORT_ZAlloc(sizeof(CERTCertificate *) * ncerts );
	if ( certs == NULL ) {
	    return(SECFailure);
	}
    
	/* decode all of the certs into the temporary DB */
	for ( i = 0, fcerts= 0; i < ncerts; i++) {
	    certs[fcerts] = CERT_NewTempCertificate(certdb, derCerts[i], NULL,
					       PR_FALSE, PR_TRUE);
	    if (certs[fcerts]) fcerts++;
	}

	if ( keepCerts ) {
	    for ( i = 0; i < fcerts; i++ ) {
		SECKEY_UpdateCertPQG(certs[i]);
		if(CERT_IsCACert(certs[i], NULL) && (fcerts > 1)) {
		    /* if we are importing only a single cert and specifying
		     * a nickname, we want to use that nickname if it a CA,
		     * otherwise if there are more than one cert, we don't
		     * know which cert it belongs to.
		     */
		    rv = CERT_SaveImportedCert(certs[i], usage, caOnly, NULL);
		} else {
		    rv = CERT_SaveImportedCert(certs[i], usage, caOnly, 
                			       nickname);
		}
		/* don't care if it fails - keep going */
	    }
	}
    }

    if ( retCerts ) {
	*retCerts = certs;
    } else {
	CERT_DestroyCertArray(certs, fcerts);
    }

    return(SECSuccess);
    
#if 0	/* dead code here - why ?? XXX */
loser:
    if ( retCerts ) {
	*retCerts = NULL;
    }
    if ( certs ) {
	CERT_DestroyCertArray(certs, ncerts);
    }    
    return(SECFailure);
#endif
}

/*
 * a real list of certificates - need to convert CERTCertificateList
 * stuff and ASN 1 encoder/decoder over to using this...
 */
CERTCertList *
CERT_NewCertList(void)
{
    PRArenaPool *arena = NULL;
    CERTCertList *ret = NULL;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }
    
    ret = (CERTCertList *)PORT_ArenaZAlloc(arena, sizeof(CERTCertList));
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
CERT_DestroyCertList(CERTCertList *certs)
{
    PRCList *node;

    while( !PR_CLIST_IS_EMPTY(&certs->list) ) {
	node = PR_LIST_HEAD(&certs->list);
	CERT_DestroyCertificate(((CERTCertListNode *)node)->cert);
	PR_REMOVE_LINK(node);
    }
    
    PORT_FreeArena(certs->arena, PR_FALSE);
    
    return;
}

void
CERT_RemoveCertListNode(CERTCertListNode *node)
{
    CERT_DestroyCertificate(node->cert);
    PR_REMOVE_LINK(&node->links);
    return;
}

SECStatus
CERT_AddCertToListTail(CERTCertList *certs, CERTCertificate *cert)
{
    CERTCertListNode *node;
    
    node = (CERTCertListNode *)PORT_ArenaZAlloc(certs->arena,
						sizeof(CERTCertListNode));
    if ( node == NULL ) {
	goto loser;
    }
    
    PR_INSERT_BEFORE(&node->links, &certs->list);
    /* certs->count++; */
    node->cert = cert;
    return(SECSuccess);
    
loser:
    return(SECFailure);
}

/*
 * Sort callback function to determine if cert a is newer than cert b.
 * Not valid certs are considered older than valid certs.
 */
PRBool
CERT_SortCBValidity(CERTCertificate *certa,
		    CERTCertificate *certb,
		    void *arg)
{
    int64 sorttime;
    int64 notBeforeA, notAfterA, notBeforeB, notAfterB;
    SECStatus rv;
    PRBool newerbefore, newerafter;
    PRBool aNotValid = PR_FALSE, bNotValid = PR_FALSE;

    sorttime = *(int64 *)arg;
    
    rv = CERT_GetCertTimes(certa, &notBeforeA, &notAfterA);
    if ( rv != SECSuccess ) {
	return(PR_FALSE);
    }
    
    rv = CERT_GetCertTimes(certb, &notBeforeB, &notAfterB);
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

    /* check if A is valid at sorttime */
    if ( CERT_CheckCertValidTimes(certa, sorttime, PR_FALSE)
	!= secCertTimeValid ) {
	aNotValid = PR_TRUE;
    }

    /* check if B is valid at sorttime */
    if ( CERT_CheckCertValidTimes(certb, sorttime, PR_FALSE)
	!= secCertTimeValid ) {
	bNotValid = PR_TRUE;
    }

    /* a is valid, b is not */
    if ( bNotValid && ( ! aNotValid ) ) {
	return(PR_TRUE);
    }

    /* b is valid, a is not */
    if ( aNotValid && ( ! bNotValid ) ) {
	return(PR_FALSE);
    }
    
    /* a and b are either valid or not valid */
    if ( newerbefore && newerafter ) {
	return(PR_TRUE);
    }
    
    if ( ( !newerbefore ) && ( !newerafter ) ) {
	return(PR_FALSE);
    }

    if ( newerbefore ) {
	/* cert A was issued after cert B, but expires sooner */
	return(PR_TRUE);
    } else {
	/* cert B was issued after cert A, but expires sooner */
	return(PR_FALSE);
    }
}


SECStatus
CERT_AddCertToListSorted(CERTCertList *certs,
			 CERTCertificate *cert,
			 CERTSortCallback f,
			 void *arg)
{
    CERTCertListNode *node;
    CERTCertListNode *head;
    PRBool ret;
    
    node = (CERTCertListNode *)PORT_ArenaZAlloc(certs->arena,
						sizeof(CERTCertListNode));
    if ( node == NULL ) {
	goto loser;
    }
    
    head = CERT_LIST_HEAD(certs);
    
    while ( !CERT_LIST_END(head, certs) ) {

	/* if cert is already in the list, then don't add it again */
	if ( cert == head->cert ) {
	    /*XXX*/
	    /* don't keep a reference */
	    CERT_DestroyCertificate(cert);
	    goto done;
	}
	
	ret = (* f)(cert, head->cert, arg);
	/* if sort function succeeds, then insert before current node */
	if ( ret ) {
	    PR_INSERT_BEFORE(&node->links, &head->links);
	    goto done;
	}

	head = CERT_LIST_NEXT(head);
    }
    /* if we get to the end, then just insert it at the tail */
    PR_INSERT_BEFORE(&node->links, &certs->list);

done:    
    /* certs->count++; */
    node->cert = cert;
    return(SECSuccess);
    
loser:
    return(SECFailure);
}

/* This routine is here because pcertdb.c still has a call to it.
 * The SMIME profile code in pcertdb.c should be split into high (find
 * the email cert) and low (store the profile) code.  At that point, we
 * can move this to certhigh.c where it belongs.
 *
 * remove certs from a list that don't have keyUsage and certType
 * that match the given usage.
 */
SECStatus
CERT_FilterCertListByUsage(CERTCertList *certList, SECCertUsage usage,
			   PRBool ca)
{
    unsigned int requiredKeyUsage;
    unsigned int requiredCertType;
    CERTCertListNode *node, *savenode;
    PRBool bad;
    SECStatus rv;
    unsigned int certType;
    PRBool dummyret;
    
    if (certList == NULL) goto loser;

    rv = CERT_KeyUsageAndTypeForCertUsage(usage, ca, &requiredKeyUsage,
					  &requiredCertType);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    node = CERT_LIST_HEAD(certList);
	
    while ( !CERT_LIST_END(node, certList) ) {

	bad = PR_FALSE;

	/* bad key usage */
	if ( CERT_CheckKeyUsage(node->cert, requiredKeyUsage )
	    != SECSuccess ) {
	    bad = PR_TRUE;
	}
	/* bad cert type */
	if ( ca ) {
	    /* This function returns a more comprehensive cert type that
	     * takes trust flags into consideration.  Should probably
	     * fix the cert decoding code to do this.
	     */
	    dummyret = CERT_IsCACert(node->cert, &certType);
	} else {
	    certType = node->cert->nsCertType;
	}
	
	if ( ! ( certType & requiredCertType ) ) {
	    bad = PR_TRUE;
	}

	if ( bad ) {
	    /* remove the node if it is bad */
	    savenode = CERT_LIST_NEXT(node);
	    CERT_RemoveCertListNode(node);
	    node = savenode;
	} else {
	    node = CERT_LIST_NEXT(node);
	}
    }
    return(SECSuccess);
    
loser:
    return(SECFailure);
}

/*
 * Acquire the global lock on the cert database.
 * This lock is currently used for the following operations:
 *	adding or deleting a cert to either the temp or perm databases
 *	converting a temp to perm or perm to temp
 *	changing(maybe just adding????) the trust of a cert
 *      chaning the DB status checking Configuration
 */
void
CERT_LockDB(CERTCertDBHandle *handle)
{
    PR_EnterMonitor(handle->dbMon);
    return;
}

/*
 * Free the global cert database lock.
 */
void
CERT_UnlockDB(CERTCertDBHandle *handle)
{
    PRStatus prstat;
    
    prstat = PR_ExitMonitor(handle->dbMon);
    
    PORT_Assert(prstat == PR_SUCCESS);
    
    return;
}

static PRLock *certRefCountLock = NULL;

/*
 * Acquire the cert reference count lock
 * There is currently one global lock for all certs, but I'm putting a cert
 * arg here so that it will be easy to make it per-cert in the future if
 * that turns out to be necessary.
 */
void
CERT_LockCertRefCount(CERTCertificate *cert)
{
    if ( certRefCountLock == NULL ) {
	nss_InitLock(&certRefCountLock);
	PORT_Assert(certRefCountLock != NULL);
    }
    
    PR_Lock(certRefCountLock);
    return;
}

/*
 * Free the cert reference count lock
 */
void
CERT_UnlockCertRefCount(CERTCertificate *cert)
{
    PRStatus prstat;

    PORT_Assert(certRefCountLock != NULL);
    
    prstat = PR_Unlock(certRefCountLock);
    
    PORT_Assert(prstat == PR_SUCCESS);

    return;
}

static PRLock *certTrustLock = NULL;

/*
 * Acquire the cert trust lock
 * There is currently one global lock for all certs, but I'm putting a cert
 * arg here so that it will be easy to make it per-cert in the future if
 * that turns out to be necessary.
 */
void
CERT_LockCertTrust(CERTCertificate *cert)
{
    if ( certTrustLock == NULL ) {
	nss_InitLock(&certTrustLock);
	PORT_Assert(certTrustLock != NULL);
    }
    
    PR_Lock(certTrustLock);
    return;
}

/*
 * Free the cert trust lock
 */
void
CERT_UnlockCertTrust(CERTCertificate *cert)
{
    PRStatus prstat;

    PORT_Assert(certTrustLock != NULL);
    
    prstat = PR_Unlock(certTrustLock);
    
    PORT_Assert(prstat == PR_SUCCESS);

    return;
}


/*
 * Get the StatusConfig data for this handle
 */
CERTStatusConfig *
CERT_GetStatusConfig(CERTCertDBHandle *handle)
{
  return handle->statusConfig;
}

/*
 * Set the StatusConfig data for this handle.  There
 * should not be another configuration set.
 */
void
CERT_SetStatusConfig(CERTCertDBHandle *handle, CERTStatusConfig *statusConfig)
{
  PORT_Assert(handle->statusConfig == NULL);

  handle->statusConfig = statusConfig;
}
