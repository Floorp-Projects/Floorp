/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cert.h"
#include "secpkcs7.h"
#include "base64.h"
#include "secitem.h"
#include "secder.h"
#include "secasn1.h"
#include "secoid.h"
#include "secerr.h"

SEC_ASN1_MKSUB(SEC_AnyTemplate)

SECStatus
SEC_ReadPKCS7Certs(SECItem *pkcs7Item, CERTImportCertificateFunc f, void *arg)
{
    SEC_PKCS7ContentInfo *contentInfo = NULL;
    SECStatus rv;
    SECItem **certs;
    int count;

    contentInfo = SEC_PKCS7DecodeItem(pkcs7Item, NULL, NULL, NULL, NULL, NULL, 
				      NULL, NULL);
    if ( contentInfo == NULL ) {
	goto loser;
    }

    if ( SEC_PKCS7ContentType (contentInfo) != SEC_OID_PKCS7_SIGNED_DATA ) {
	goto loser;
    }

    certs = contentInfo->content.signedData->rawCerts;
    if ( certs ) {
	count = 0;
	
	while ( *certs ) {
	    count++;
	    certs++;
	}
	rv = (* f)(arg, contentInfo->content.signedData->rawCerts, count);
    }
    
    rv = SECSuccess;
    
    goto done;
loser:
    rv = SECFailure;
    
done:
    if ( contentInfo ) {
	SEC_PKCS7DestroyContentInfo(contentInfo);
    }

    return(rv);
}

const SEC_ASN1Template SEC_CertSequenceTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_XTRN, 0, SEC_ASN1_SUB(SEC_AnyTemplate) }
};

SECStatus
SEC_ReadCertSequence(SECItem *certsItem, CERTImportCertificateFunc f, void *arg)
{
    SECStatus rv;
    SECItem **certs;
    int count;
    SECItem **rawCerts = NULL;
    PRArenaPool *arena;
    SEC_PKCS7ContentInfo *contentInfo = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	return SECFailure;
    }

    contentInfo = SEC_PKCS7DecodeItem(certsItem, NULL, NULL, NULL, NULL, NULL, 
				      NULL, NULL);
    if ( contentInfo == NULL ) {
	goto loser;
    }

    if ( SEC_PKCS7ContentType (contentInfo) != SEC_OID_NS_TYPE_CERT_SEQUENCE ) {
	goto loser;
    }


    rv = SEC_QuickDERDecodeItem(arena, &rawCerts, SEC_CertSequenceTemplate,
		    contentInfo->content.data);

    if (rv != SECSuccess) {
	goto loser;
    }

    certs = rawCerts;
    if ( certs ) {
	count = 0;
	
	while ( *certs ) {
	    count++;
	    certs++;
	}
	rv = (* f)(arg, rawCerts, count);
    }
    
    rv = SECSuccess;
    
    goto done;
loser:
    rv = SECFailure;
    
done:
    if ( contentInfo ) {
	SEC_PKCS7DestroyContentInfo(contentInfo);
    }

    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(rv);
}

CERTCertificate *
CERT_ConvertAndDecodeCertificate(char *certstr)
{
    CERTCertificate *cert;
    SECStatus rv;
    SECItem der;

    rv = ATOB_ConvertAsciiToItem(&der, certstr);
    if (rv != SECSuccess)
	return NULL;

    cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), 
                                   &der, NULL, PR_FALSE, PR_TRUE);

    PORT_Free(der.data);
    return cert;
}

static const char NS_CERT_HEADER[]  = "-----BEGIN CERTIFICATE-----";
static const char NS_CERT_TRAILER[] = "-----END CERTIFICATE-----";
#define NS_CERT_HEADER_LEN  ((sizeof NS_CERT_HEADER) - 1)
#define NS_CERT_TRAILER_LEN ((sizeof NS_CERT_TRAILER) - 1)

/*
 * read an old style ascii or binary certificate chain
 */
SECStatus
CERT_DecodeCertPackage(char *certbuf,
		       int certlen,
		       CERTImportCertificateFunc f,
		       void *arg)
{
    unsigned char *cp;
    unsigned char *bincert = NULL;
    char *         ascCert = NULL;
    SECStatus      rv;
    
    if ( certbuf == NULL ) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return(SECFailure);
    }
    /*
     * Make sure certlen is long enough to handle the longest possible
     * reference in the code below:
     * 0x30 0x84 l1 l2 l3 l4  +
     *                       tag 9 o1 o2 o3 o4 o5 o6 o7 o8 o9
     * where 9 is the longest length of the expected oids we are testing.
     *   6 + 11 = 17. 17 bytes is clearly too small to code any kind of
     *  certificate (a 128 bit ECC certificate contains at least an 8 byte
     * key and a 16 byte signature, plus coding overhead). Typically a cert
     * is much larger. So it's safe to require certlen to be at least 17
     * bytes.
     */
    if (certlen < 17) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return(SECFailure);
    }
    
    cp = (unsigned char *)certbuf;

    /* is a DER encoded certificate of some type? */
    if ( ( *cp  & 0x1f ) == SEC_ASN1_SEQUENCE ) {
	SECItem certitem;
	SECItem *pcertitem = &certitem;
	int seqLen, seqLenLen;

	cp++;
	
	if ( *cp & 0x80) {
	    /* Multibyte length */
	    seqLenLen = cp[0] & 0x7f;
	    
	    switch (seqLenLen) {
	      case 4:
		seqLen = ((unsigned long)cp[1]<<24) |
		    ((unsigned long)cp[2]<<16) | (cp[3]<<8) | cp[4];
		break;
	      case 3:
		seqLen = ((unsigned long)cp[1]<<16) | (cp[2]<<8) | cp[3];
		break;
	      case 2:
		seqLen = (cp[1]<<8) | cp[2];
		break;
	      case 1:
		seqLen = cp[1];
		break;
	      case 0:
		/* indefinite length */
		seqLen = 0;
		break;
	      default:
		goto notder;
	    }
	    cp += ( seqLenLen + 1 );

	} else {
	    seqLenLen = 0;
	    seqLen = *cp;
	    cp++;
	}

	/* check entire length if definite length */
	if ( seqLen || seqLenLen ) {
	    if ( certlen != ( seqLen + seqLenLen + 2 ) ) {
		if (certlen > ( seqLen + seqLenLen + 2 ))
		    PORT_SetError(SEC_ERROR_EXTRA_INPUT);
		else 
		    PORT_SetError(SEC_ERROR_INPUT_LEN);
		goto notder;
	    }
	}
	
	/* check the type oid */
	if ( cp[0] == SEC_ASN1_OBJECT_ID ) {
	    SECOidData *oiddata;
	    SECItem oiditem;
	    /* XXX - assume DER encoding of OID len!! */
	    oiditem.len = cp[1];
	    /* if we add an oid below that is longer than 9 bytes, then we
	     * need to change the certlen check at the top of the function
	     * to prevent a buffer overflow
	     */
	    if ( oiditem.len > 9 ) {
		PORT_SetError(SEC_ERROR_UNRECOGNIZED_OID);
		return(SECFailure);
	    }
	    oiditem.data = (unsigned char *)&cp[2];
	    oiddata = SECOID_FindOID(&oiditem);
	    if ( oiddata == NULL ) {
		return(SECFailure);
	    }

	    certitem.data = (unsigned char*)certbuf;
	    certitem.len = certlen;
	    
	    switch ( oiddata->offset ) {
	      case SEC_OID_PKCS7_SIGNED_DATA:
		/* oid: 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02 */
		return(SEC_ReadPKCS7Certs(&certitem, f, arg));
		break;
	      case SEC_OID_NS_TYPE_CERT_SEQUENCE:
		/* oid: 0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42, 0x02, 0x05 */
		return(SEC_ReadCertSequence(&certitem, f, arg));
		break;
	      default:
		break;
	    }
	    
	} else {
	    /* it had better be a certificate by now!! */
	    certitem.data = (unsigned char*)certbuf;
	    certitem.len = certlen;
	    
	    rv = (* f)(arg, &pcertitem, 1);
	    return(rv);
	}
    }

    /* now look for a netscape base64 ascii encoded cert */
notder:
  {
    unsigned char *certbegin = NULL; 
    unsigned char *certend   = NULL;
    char          *pc;
    int cl;

    /* Convert the ASCII data into a nul-terminated string */
    ascCert = (char *)PORT_Alloc(certlen + 1);
    if (!ascCert) {
        rv = SECFailure;
	goto loser;
    }

    PORT_Memcpy(ascCert, certbuf, certlen);
    ascCert[certlen] = '\0';

    pc = PORT_Strchr(ascCert, '\n');  /* find an EOL */
    if (!pc) { /* maybe this is a MAC file */
	pc = ascCert;
	while (*pc && NULL != (pc = PORT_Strchr(pc, '\r'))) {
	    *pc++ = '\n';
	}
    }

    cp = (unsigned char *)ascCert;
    cl = certlen;

    /* find the beginning marker */
    while ( cl > NS_CERT_HEADER_LEN ) {
	int found = 0;
	if ( !PORT_Strncasecmp((char *)cp, NS_CERT_HEADER,
			        NS_CERT_HEADER_LEN) ) {
	    cl -= NS_CERT_HEADER_LEN;
	    cp += NS_CERT_HEADER_LEN;
	    found = 1;
	}
	
	/* skip to next eol */
	while ( cl && ( *cp != '\n' )) {
	    cp++;
	    cl--;
	} 

	/* skip all blank lines */
	while ( cl && ( *cp == '\n' || *cp == '\r' )) {
	    cp++;
	    cl--;
	}
	if (cl && found) {
	    certbegin = cp;
	    break;
    	}
    }

    if ( certbegin ) {
	/* find the ending marker */
	while ( cl >= NS_CERT_TRAILER_LEN ) {
	    if ( !PORT_Strncasecmp((char *)cp, NS_CERT_TRAILER,
				   NS_CERT_TRAILER_LEN) ) {
		certend = cp;
		break;
	    }

	    /* skip to next eol */
	    while ( cl && ( *cp != '\n' )) {
		cp++;
		cl--;
	    }

	    /* skip all blank lines */
	    while ( cl && ( *cp == '\n' || *cp == '\r' )) {
		cp++;
		cl--;
	    }
	}
    }

    if ( certbegin && certend ) {
	unsigned int binLen;

	*certend = 0;
	/* convert to binary */
	bincert = ATOB_AsciiToData((char *)certbegin, &binLen);
	if (!bincert) {
	    rv = SECFailure;
	    goto loser;
	}

	/* now recurse to decode the binary */
	rv = CERT_DecodeCertPackage((char *)bincert, binLen, f, arg);
	
    } else {
	PORT_SetError(SEC_ERROR_BAD_DER);
	rv = SECFailure;
    }
  }

loser:

    if ( bincert ) {
	PORT_Free(bincert);
    }

    if ( ascCert ) {
	PORT_Free(ascCert);
    }

    return(rv);
}

typedef struct {
    PRArenaPool *arena;
    SECItem cert;
} collect_args;

static SECStatus
collect_certs(void *arg, SECItem **certs, int numcerts)
{
    SECStatus rv;
    collect_args *collectArgs;
    
    collectArgs = (collect_args *)arg;
    
    rv = SECITEM_CopyItem(collectArgs->arena, &collectArgs->cert, *certs);

    return(rv);
}


/*
 * read an old style ascii or binary certificate
 */
CERTCertificate *
CERT_DecodeCertFromPackage(char *certbuf, int certlen)
{
    collect_args collectArgs;
    SECStatus rv;
    CERTCertificate *cert = NULL;
    
    collectArgs.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    rv = CERT_DecodeCertPackage(certbuf, certlen, collect_certs,
				(void *)&collectArgs);
    if ( rv == SECSuccess ) {
	cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
	                               &collectArgs.cert, NULL, 
	                               PR_FALSE, PR_TRUE);
    }
    
    PORT_FreeArena(collectArgs.arena, PR_FALSE);
    
    return(cert);
}
