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
** certutil.c
**
** utility for managing certificates and the cert database
**
*/
#include <stdio.h>
#include <string.h>

#if defined(WIN32)
#include "fcntl.h"
#include "io.h"
#endif

#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"

#include "pk11func.h"
#include "secasn1.h"
#include "cert.h"
#include "cryptohi.h"
#include "secoid.h"
#include "certdb.h"
#include "nss.h"

#define MIN_KEY_BITS		512
#define MAX_KEY_BITS		2048
#define DEFAULT_KEY_BITS	1024

#define GEN_BREAK(e) rv=e; break;


extern SECKEYPrivateKey *CERTUTIL_GeneratePrivateKey(KeyType keytype,
						     PK11SlotInfo *slot, 
                                                     int rsasize,
						     int publicExponent,
						     char *noise,
						     SECKEYPublicKey **pubkeyp,
						     char *pqgFile,
                                                     secuPWData *pwdata);

static char *progName;

static CERTGeneralName *
GetGeneralName (PRArenaPool *arena)
{
    CERTGeneralName *namesList = NULL;
    CERTGeneralName *current;
    CERTGeneralName *tail = NULL;
    SECStatus rv = SECSuccess;
    int intValue;
    char buffer[512];
    void *mark;

    PORT_Assert (arena);
    mark = PORT_ArenaMark (arena);
    do {
	puts ("\nSelect one of the following general name type: \n");
	puts ("\t1 - instance of other name\n\t2 - rfc822Name\n\t3 - dnsName\n");
	puts ("\t4 - x400Address\n\t5 - directoryName\n\t6 - ediPartyName\n");
	puts ("\t7 - uniformResourceidentifier\n\t8 - ipAddress\n\t9 - registerID\n");
	puts ("\tOther - omit\n\t\tChoice:");
	scanf ("%d", &intValue);
	if (intValue >= certOtherName || intValue <= certRegisterID) {
	    if (namesList == NULL) {
		namesList = current = tail = (CERTGeneralName *) PORT_ArenaAlloc 
		                                  (arena, sizeof (CERTGeneralName));
	    } else {
		current = (CERTGeneralName *) PORT_ArenaAlloc(arena, 
							      sizeof (CERTGeneralName));
	    }
	    if (current == NULL) {
		GEN_BREAK (SECFailure);
	    }	
	} else {
	    break;
	}
	current->type = intValue;
	puts ("\nEnter data:");
	fflush (stdout);
	gets (buffer);
	switch (current->type) {
	    case certURI:
	    case certDNSName:
	    case certRFC822Name:
		current->name.other.data = PORT_ArenaAlloc (arena, strlen (buffer));
		if (current->name.other.data == NULL) {
		    GEN_BREAK (SECFailure);
		}
		PORT_Memcpy
		  (current->name.other.data, buffer, current->name.other.len = strlen(buffer));
		break;

	    case certEDIPartyName:
	    case certIPAddress:
	    case certOtherName:
	    case certRegisterID:
	    case certX400Address: {

		current->name.other.data = PORT_ArenaAlloc (arena, strlen (buffer) + 2);
		if (current->name.other.data == NULL) {
		    GEN_BREAK (SECFailure);
		}

		PORT_Memcpy (current->name.other.data + 2, buffer, strlen (buffer));
		/* This may not be accurate for all cases.  For now, use this tag type */
		current->name.other.data[0] = (char)(((current->type - 1) & 0x1f)| 0x80);
		current->name.other.data[1] = (char)strlen (buffer);
		current->name.other.len = strlen (buffer) + 2;
		break;
	    }

	    case certDirectoryName: {
		CERTName *directoryName = NULL;

		directoryName = CERT_AsciiToName (buffer);
		if (!directoryName) {
		    fprintf(stderr, "certutil: improperly formatted name: \"%s\"\n", buffer);
		    break;
		}
			    
		rv = CERT_CopyName (arena, &current->name.directoryName, directoryName);
		CERT_DestroyName (directoryName);
		
		break;
	    }
	}
	if (rv != SECSuccess)
	    break;
	current->l.next = &(namesList->l);
	current->l.prev = &(tail->l);
	tail->l.next = &(current->l);
	tail = current;

    }while (1);

    if (rv != SECSuccess) {
	PORT_SetError (rv);
	PORT_ArenaRelease (arena, mark);
	namesList = NULL;
    }
    return (namesList);
}

static SECStatus 
GetString(PRArenaPool *arena, char *prompt, SECItem *value)
{
    char buffer[251];

    value->data = NULL;
    value->len = 0;
    
    puts (prompt);
    gets (buffer);
    if (strlen (buffer) > 0) {
	value->data = PORT_ArenaAlloc (arena, strlen (buffer));
	if (value->data == NULL) {
	    PORT_SetError (SEC_ERROR_NO_MEMORY);
	    return (SECFailure);
	}
	PORT_Memcpy (value->data, buffer, value->len = strlen(buffer));
    }
    return (SECSuccess);
}

static CERTCertificateRequest *
GetCertRequest(PRFileDesc *inFile, PRBool ascii)
{
    CERTCertificateRequest *certReq = NULL;
    CERTSignedData signedData;
    PRArenaPool *arena = NULL;
    SECItem reqDER;
    SECStatus rv;

    reqDER.data = NULL;
    do {
	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if (arena == NULL) {
	    GEN_BREAK (SECFailure);
	}
	
 	rv = SECU_ReadDERFromFile(&reqDER, inFile, ascii);
	if (rv) 
	    break;
        certReq = (CERTCertificateRequest*) PORT_ArenaZAlloc
		  (arena, sizeof(CERTCertificateRequest));
        if (!certReq) 
	    break;
	certReq->arena = arena;

	/* Since cert request is a signed data, must decode to get the inner
	   data
	 */
	PORT_Memset(&signedData, 0, sizeof(signedData));
	rv = SEC_ASN1DecodeItem(arena, &signedData, CERT_SignedDataTemplate, 
				&reqDER);
	if (rv)
	    break;
	
        rv = SEC_ASN1DecodeItem(arena, certReq, CERT_CertificateRequestTemplate,
				&signedData.data);
   } while (0);

   if (rv) {
       PRErrorCode  perr = PR_GetError();
       fprintf(stderr, "%s: unable to decode DER cert request (%s)\n", progName,
               SECU_Strerror(perr));
   }
   return (certReq);
}

static PRBool 
GetYesNo(char *prompt) 
{
    char buf[3];

    PR_Sync(PR_STDIN);
    PR_Write(PR_STDOUT, prompt, strlen(prompt)+1);
    PR_Read(PR_STDIN, buf, sizeof(buf));
    return (buf[0] == 'y' || buf[0] == 'Y') ? PR_TRUE : PR_FALSE;
#if 0
    char charValue;

    puts (prompt);
    scanf ("%c", &charValue);
    if (charValue != 'y' && charValue != 'Y')
	return (0);
    return (1);
#endif
}

static SECStatus
AddCert(PK11SlotInfo *slot, CERTCertDBHandle *handle, char *name, char *trusts, 
        PRFileDesc *inFile, PRBool ascii, PRBool emailcert, void *pwdata)
{
    CERTCertTrust *trust = NULL;
    CERTCertificate *cert = NULL, *tempCert = NULL;
    SECItem certDER;
    SECStatus rv;

    certDER.data = NULL;
    do {
	/* Read in the entire file specified with the -i argument */
	rv = SECU_ReadDERFromFile(&certDER, inFile, ascii);
	if (rv != SECSuccess) {
	    SECU_PrintError(progName, "unable to read input file");
	    break;
	}

	/* Read in an ASCII cert and return a CERTCertificate */
	cert = CERT_DecodeCertFromPackage((char *)certDER.data, certDER.len);
	if (!cert) {
	    SECU_PrintError(progName, "could not obtain certificate from file"); 
	    GEN_BREAK(SECFailure);
	}

	/* Create a cert trust to pass to SEC_AddPermCertificate */
	trust = (CERTCertTrust *)PORT_ZAlloc(sizeof(CERTCertTrust));
	if (!trust) {
	    SECU_PrintError(progName, "unable to allocate cert trust");
	    GEN_BREAK(SECFailure);
	}

	rv = CERT_DecodeTrustString(trust, trusts);
	if (rv) {
	    SECU_PrintError(progName, "unable to decode trust string");
	    GEN_BREAK(SECFailure);
	}

#ifdef notdef
	/* CERT_ImportCert only collects certificates and returns the
	* first certficate.  It does not insert these certificates into
	* the dbase.  For now, just call CERT_NewTempCertificate.
	* This will result in decoding the der twice.  This have to
	* be handle properly.
	*/

	tempCert = CERT_NewTempCertificate(handle, &cert->derCert, NULL,
	                                   PR_FALSE, PR_TRUE);

	if (!PK11_IsInternal(slot)) {
	    tempCert->trust = trust;

	    rv = PK11_ImportCertForKeyToSlot(slot, tempCert, name,
	                                     PR_FALSE, NULL);
	}

	if (tempCert == NULL) {
	    SECU_PrintError(progName,"unable to add cert to the temp database");
	    GEN_BREAK(SECFailure);
	}

	rv = CERT_AddTempCertToPerm(tempCert, name, trust);
	if (rv) {
	    SECU_PrintError(progName, "could not add certificate to database");
	    GEN_BREAK(SECFailure);
	}

	if ( emailcert )
	    CERT_SaveSMimeProfile(tempCert, NULL, NULL);
#else
	cert->trust = trust;
	rv = PK11_Authenticate(slot, PR_TRUE, pwdata);
	if (rv != SECSuccess) {
	    SECU_PrintError(progName, "could authenticate to token or database");
	    GEN_BREAK(SECFailure);
	}

	rv =  PK11_ImportCert(slot, cert, CK_INVALID_HANDLE, name, PR_FALSE);
	if (rv != SECSuccess) {
	    SECU_PrintError(progName, "could not add certificate to token or database");
	    GEN_BREAK(SECFailure);
	}

	rv = CERT_ChangeCertTrust(handle, cert, trust);
	if (rv != SECSuccess) {
	    SECU_PrintError(progName, "could not change trust on certificate");
	    GEN_BREAK(SECFailure);
	}

	if ( emailcert ) {
	    CERT_SaveSMimeProfile(cert, NULL, pwdata);
	}

#endif
    } while (0);

#ifdef notdef
    CERT_DestroyCertificate (tempCert);
#endif
    CERT_DestroyCertificate (cert);
    PORT_Free(trust);
    PORT_Free(certDER.data);

    return rv;
}

static SECStatus
CertReq(SECKEYPrivateKey *privk, SECKEYPublicKey *pubk, KeyType keyType,
	CERTName *subject, char *phone, int ascii, PRFileDesc *outFile)
{
    CERTSubjectPublicKeyInfo *spki;
    CERTCertificateRequest *cr;
    SECItem *encoding;
    SECItem result;
    SECStatus rv;
    PRArenaPool *arena;
    PRInt32 numBytes;

    /* Create info about public key */
    spki = SECKEY_CreateSubjectPublicKeyInfo(pubk);
    if (!spki) {
	SECU_PrintError(progName, "unable to create subject public key");
	return SECFailure;
    }

    /* Generate certificate request */
    cr = CERT_CreateCertificateRequest(subject, spki, 0);
    if (!cr) {
	SECU_PrintError(progName, "unable to make certificate request");
	return SECFailure;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena ) {
	SECU_PrintError(progName, "out of memory");
	return SECFailure;
    }
    
    /* Der encode the request */
    encoding = SEC_ASN1EncodeItem(arena, NULL, cr,
				  CERT_CertificateRequestTemplate);
    if (encoding == NULL) {
	SECU_PrintError(progName, "der encoding of request failed");
	return SECFailure;
    }

    /* Sign the request */
    switch (keyType) {
    case rsaKey:
	rv = SEC_DerSignData(arena, &result, encoding->data, encoding->len, 
	                     privk, SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION);
	break;
    case dsaKey:
	rv = SEC_DerSignData(arena, &result, encoding->data, encoding->len, 
	                 privk, SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST);
	break;
    default:
	SECU_PrintError(progName, "Must use rsa or dsa key type");
	return SECFailure;
    }

    if (rv) {
	SECU_PrintError(progName, "signing of data failed");
	return SECFailure;
    }

    /* Encode request in specified format */
    if (ascii) {
	char *obuf;
	char *name, *email, *org, *state, *country;
	SECItem *it;
	int total;

	it = &result;

	obuf = BTOA_ConvertItemToAscii(it);
	total = PL_strlen(obuf);

	name = CERT_GetCommonName(subject);
	if (!name) {
	    fprintf(stderr, "You must specify a common name\n");
	    return SECFailure;
	}

	if (!phone)
	    phone = strdup("(not specified)");

	email = CERT_GetCertEmailAddress(subject);
	if (!email)
	    email = strdup("(not specified)");

	org = CERT_GetOrgName(subject);
	if (!org)
	    org = strdup("(not specified)");

	state = CERT_GetStateName(subject);
	if (!state)
	    state = strdup("(not specified)");

	country = CERT_GetCountryName(subject);
	if (!country)
	    country = strdup("(not specified)");

	PR_fprintf(outFile, 
	           "\nCertificate request generated by Netscape certutil\n");
	PR_fprintf(outFile, "Phone: %s\n\n", phone);
	PR_fprintf(outFile, "Common Name: %s\n", name);
	PR_fprintf(outFile, "Email: %s\n", email);
	PR_fprintf(outFile, "Organization: %s\n", org);
	PR_fprintf(outFile, "State: %s\n", state);
	PR_fprintf(outFile, "Country: %s\n\n", country);

	PR_fprintf(outFile, "%s\n", NS_CERTREQ_HEADER);
	numBytes = PR_Write(outFile, obuf, total);
	if (numBytes != total) {
	    SECU_PrintSystemError(progName, "write error");
	    return SECFailure;
	}
	PR_fprintf(outFile, "%s\n", NS_CERTREQ_TRAILER);
    } else {
	numBytes = PR_Write(outFile, result.data, result.len);
	if (numBytes != (int)result.len) {
	    SECU_PrintSystemError(progName, "write error");
	    return SECFailure;
	}
    }
    return SECSuccess;
}

static SECStatus 
ChangeTrustAttributes(CERTCertDBHandle *handle, char *name, char *trusts)
{
    SECStatus rv;
    CERTCertificate *cert;
    CERTCertTrust *trust;
    
    cert = CERT_FindCertByNicknameOrEmailAddr(handle, name);
    if (!cert) {
	SECU_PrintError(progName, "could not find certificate named \"%s\"",
			name);
	return SECFailure;
    }

    trust = (CERTCertTrust *)PORT_ZAlloc(sizeof(CERTCertTrust));
    if (!trust) {
	SECU_PrintError(progName, "unable to allocate cert trust");
	return SECFailure;
    }

    /* This function only decodes these characters: pPwcTCu, */
    rv = CERT_DecodeTrustString(trust, trusts);
    if (rv) {
	SECU_PrintError(progName, "unable to decode trust string");
	return SECFailure;
    }

    rv = CERT_ChangeCertTrust(handle, cert, trust);
    if (rv) {
	SECU_PrintError(progName, "unable to modify trust attributes");
	return SECFailure;
    }

    return SECSuccess;
}

static SECStatus
printCertCB(CERTCertificate *cert, void *arg)
{
    SECStatus rv;
    SECItem data;
    CERTCertTrust *trust = (CERTCertTrust *)arg;
    
    data.data = cert->derCert.data;
    data.len = cert->derCert.len;

    rv = SECU_PrintSignedData(stdout, &data, "Certificate", 0,
			      SECU_PrintCertificate);
    if (rv) {
	SECU_PrintError(progName, "problem printing certificate");
	return(SECFailure);
    }
    if (trust) {
	SECU_PrintTrustFlags(stdout, trust,
	                     "Certificate Trust Flags", 1);
    } else if (cert->trust) {
	SECU_PrintTrustFlags(stdout, cert->trust,
	                     "Certificate Trust Flags", 1);
    }

    printf("\n");

    return(SECSuccess);
}

static SECStatus
listCerts(CERTCertDBHandle *handle, char *name, PK11SlotInfo *slot,
          PRBool raw, PRBool ascii, PRFileDesc *outfile, void *pwarg)
{
    CERTCertificate *cert;
    SECItem data;
    PRInt32 numBytes;
    SECStatus rv;

#ifdef nodef
    /* For now, split handling of slot to internal vs. other.  slot should
     * probably be allowed to be NULL so that all slots can be listed.
     * In that case, need to add a call to PK11_TraverseSlotCerts().
     */
    if (PK11_IsInternal(slot)) {
	if (name == NULL) {
	    /* Print all certs in internal slot db. */
	    rv = SECU_PrintCertificateNames(handle, PR_STDOUT, 
	                                    PR_FALSE, PR_TRUE);
	    if (rv) {
		SECU_PrintError(progName, 
		                "problem printing certificate nicknames");
		return SECFailure;
	    }
	} else if (raw || ascii) {
	    /* Dump binary or ascii DER for the cert to stdout. */
	    cert = CERT_FindCertByNicknameOrEmailAddr(handle, name);
	    if (!cert) {
		SECU_PrintError(progName,
		               "could not find certificate named \"%s\"", name);
		return SECFailure;
	    }
	    data.data = cert->derCert.data;
	    data.len = cert->derCert.len;
	    if (ascii) {
		PR_fprintf(outfile, "%s\n%s\n%s\n", NS_CERT_HEADER, 
		        BTOA_DataToAscii(data.data, data.len), NS_CERT_TRAILER);
	    } else if (raw) {
	        numBytes = PR_Write(outfile, data.data, data.len);
	        if (numBytes != data.len) {
		    SECU_PrintSystemError(progName, "error writing raw cert");
		    return SECFailure;
		}
	    }
	} else {
	    /* Pretty-print cert. */
	    rv = CERT_TraversePermCertsForNickname(handle, name, printCertCB,
	                                           NULL);
	}
    } else {
#endif
	/* List certs on a non-internal slot. */
	if ( PK11_IsFIPS() || 
	     (!PK11_IsFriendly(slot) && PK11_NeedLogin(slot)) )
	    PK11_Authenticate(slot, PR_TRUE, pwarg);
	if (name) {
	    CERTCertificate *the_cert;
	    the_cert = PK11_FindCertFromNickname(name, NULL);
	    if (!the_cert) {
		SECU_PrintError(progName, "Could not find: %s\n", name);
		return SECFailure;
	    }
	    data.data = the_cert->derCert.data;
	    data.len = the_cert->derCert.len;
	    if (ascii) {
		PR_fprintf(outfile, "%s\n%s\n%s\n", NS_CERT_HEADER, 
		        BTOA_DataToAscii(data.data, data.len), NS_CERT_TRAILER);
		rv = SECSuccess;
	    } else if (raw) {
		numBytes = PR_Write(outfile, data.data, data.len);
	        if (numBytes != data.len) {
		    SECU_PrintSystemError(progName, "error writing raw cert");
		    rv = SECFailure;
		}
		rv = SECSuccess;
	    } else {
	        rv = printCertCB(the_cert, the_cert->trust);
	    }
	} else {
	    rv = PK11_TraverseCertsInSlot(slot, SECU_PrintCertNickname, stdout);
	}
	if (rv) {
	    SECU_PrintError(progName, "problem printing certificate nicknames");
	    return SECFailure;
	}
#ifdef notdef
    }
#endif

    return SECSuccess;	/* not rv ?? */
}

static SECStatus
ListCerts(CERTCertDBHandle *handle, char *name, PK11SlotInfo *slot,
          PRBool raw, PRBool ascii, PRFileDesc *outfile, secuPWData *pwdata)
{
    SECStatus rv;

    if (slot == NULL) {
	PK11SlotList *list;
	PK11SlotListElement *le;

	list= PK11_GetAllTokens(CKM_INVALID_MECHANISM,
						PR_FALSE,PR_FALSE,pwdata);
	if (list) for (le = list->head; le; le = le->next) {
	    rv = listCerts(handle,name,le->slot,raw,ascii,outfile,pwdata);
	}
    } else {
	rv = listCerts(handle,name,slot,raw,ascii,outfile,pwdata);
    }
    return rv;
}


static SECStatus 
DeleteCert(CERTCertDBHandle *handle, char *name)
{
    SECStatus rv;
    CERTCertificate *cert;

    cert = CERT_FindCertByNicknameOrEmailAddr(handle, name);
    if (!cert) {
	SECU_PrintError(progName, "could not find certificate named \"%s\"",
			name);
	return SECFailure;
    }

    rv = SEC_DeletePermCertificate(cert);
    if (rv) {
	SECU_PrintError(progName, "unable to delete certificate");
	return SECFailure;
    }

    return SECSuccess;
}

static SECStatus
ValidateCert(CERTCertDBHandle *handle, char *name, char *date,
	     char *certUsage, PRBool checkSig, PRBool logit, secuPWData *pwdata)
{
    SECStatus rv;
    CERTCertificate *cert;
    int64 timeBoundary;
    SECCertUsage usage;
    CERTVerifyLog reallog;
    CERTVerifyLog *log = NULL;
    
    switch (*certUsage) {
	case 'C':
	    usage = certUsageSSLClient;
	    break;
	case 'V':
	    usage = certUsageSSLServer;
	    break;
	case 'S':
	    usage = certUsageEmailSigner;
	    break;
	case 'R':
	    usage = certUsageEmailRecipient;
	    break;
	default:
	    PORT_SetError (SEC_ERROR_INVALID_ARGS);
	    return (SECFailure);
    }
    do {
	cert = CERT_FindCertByNicknameOrEmailAddr(handle, name);
	if (!cert) {
	    SECU_PrintError(progName, "could not find certificate named \"%s\"",
			    name);
	    GEN_BREAK (SECFailure)
	}

	if (date != NULL) {
	    rv = DER_AsciiToTime(&timeBoundary, date);
	    if (rv) {
		SECU_PrintError(progName, "invalid input date");
		GEN_BREAK (SECFailure)
	    }
	} else {
	    timeBoundary = PR_Now();
	}

	if ( logit ) {
	    log = &reallog;
	    
	    log->count = 0;
	    log->head = NULL;
	    log->tail = NULL;
	    log->arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	    if ( log->arena == NULL ) {
		SECU_PrintError(progName, "out of memory");
		GEN_BREAK (SECFailure)
	    }
	}
 
	rv = CERT_VerifyCert(handle, cert, checkSig, usage,
			     timeBoundary, pwdata, log);
	if ( log ) {
	    if ( log->head == NULL ) {
		fprintf(stdout, "%s: certificate is valid\n", progName);
		GEN_BREAK (SECSuccess)
	    } else {
		char *name;
		CERTVerifyLogNode *node;
		
		node = log->head;
		while ( node ) {
		    if ( node->cert->nickname != NULL ) {
			name = node->cert->nickname;
		    } else {
			name = node->cert->subjectName;
		    }
		    fprintf(stderr, "%s : %s\n", name, 
		    	SECU_Strerror(node->error));
		    CERT_DestroyCertificate(node->cert);
		    node = node->next;
		}
	    }
	} else {
	    if (rv != SECSuccess) {
		PRErrorCode perr = PORT_GetError();
		fprintf(stdout, "%s: certificate is invalid: %s\n",
			progName, SECU_Strerror(perr));
		GEN_BREAK (SECFailure)
	    }
	    fprintf(stdout, "%s: certificate is valid\n", progName);
	    GEN_BREAK (SECSuccess)
	}
    } while (0);

    return (rv);
}

#ifdef notdef
static SECStatus
DumpPublicKey(int dbindex, char *nickname, FILE *out)
{
    SECKEYLowPrivateKey *privKey;
    SECKEYLowPublicKey *publicKey;

    if (dbindex) {
	/*privKey = secu_GetPrivKeyFromIndex(dbindex);*/
    } else {
	privKey = GetPrivKeyFromNickname(nickname);
    }
    publicKey = SECKEY_LowConvertToPublicKey(privKey);

    /* Output public key (in the clear) */
    switch(publicKey->keyType) {
      case rsaKey:
	fprintf(out, "RSA Public-Key:\n");
	SECU_PrintInteger(out, &publicKey->u.rsa.modulus, "modulus", 1);
	SECU_PrintInteger(out, &publicKey->u.rsa.publicExponent,
			  "publicExponent", 1);
	break;
      case dsaKey:
	fprintf(out, "DSA Public-Key:\n");
	SECU_PrintInteger(out, &publicKey->u.dsa.params.prime, "prime", 1);
	SECU_PrintInteger(out, &publicKey->u.dsa.params.subPrime,
			  "subPrime", 1);
	SECU_PrintInteger(out, &publicKey->u.dsa.params.base, "base", 1);
	SECU_PrintInteger(out, &publicKey->u.dsa.publicValue, "publicValue", 1);
	break;
      default:
	fprintf(out, "unknown key type\n");
	break;
    }
    return SECSuccess;
}

static SECStatus
DumpPrivateKey(int dbindex, char *nickname, FILE *out)
{
    SECKEYLowPrivateKey *key;
    if (dbindex) {
	/*key = secu_GetPrivKeyFromIndex(dbindex);*/
    } else {
	key = GetPrivKeyFromNickname(nickname);
    }

    switch(key->keyType) {
      case rsaKey:
	fprintf(out, "RSA Private-Key:\n");
	SECU_PrintInteger(out, &key->u.rsa.modulus, "modulus", 1);
	SECU_PrintInteger(out, &key->u.rsa.publicExponent, "publicExponent", 1);
	SECU_PrintInteger(out, &key->u.rsa.privateExponent,
			  "privateExponent", 1);
	SECU_PrintInteger(out, &key->u.rsa.prime1, "prime1", 1);
	SECU_PrintInteger(out, &key->u.rsa.prime2, "prime2", 1);
	SECU_PrintInteger(out, &key->u.rsa.exponent1, "exponent2", 1);
	SECU_PrintInteger(out, &key->u.rsa.exponent2, "exponent2", 1);
	SECU_PrintInteger(out, &key->u.rsa.coefficient, "coefficient", 1);
	break;
      case dsaKey:
	fprintf(out, "DSA Private-Key:\n");
	SECU_PrintInteger(out, &key->u.dsa.params.prime, "prime", 1);
	SECU_PrintInteger(out, &key->u.dsa.params.subPrime, "subPrime", 1);
	SECU_PrintInteger(out, &key->u.dsa.params.base, "base", 1);
	SECU_PrintInteger(out, &key->u.dsa.publicValue, "publicValue", 1);
	SECU_PrintInteger(out, &key->u.dsa.privateValue, "privateValue", 1);
	break;
      default:
	fprintf(out, "unknown key type\n");
	break;
    }
    return SECSuccess;
}
#endif

static SECStatus
printKeyCB(SECKEYPublicKey *key, SECItem *data, void *arg)
{
    if (key->keyType == rsaKey) {
	fprintf(stdout, "RSA Public-Key:\n");
	SECU_PrintInteger(stdout, &key->u.rsa.modulus, "modulus", 1);
    } else {
	fprintf(stdout, "DSA Public-Key:\n");
	SECU_PrintInteger(stdout, &key->u.dsa.publicValue, "publicValue", 1);
    }
    return SECSuccess;
}

/* callback for listing certs through pkcs11 */
SECStatus
secu_PrintKeyFromCert(CERTCertificate *cert, void *data)
{
    FILE *out;
    SECKEYPrivateKey *key;

    out = (FILE *)data;
    key = PK11_FindPrivateKeyFromCert(PK11_GetInternalKeySlot(), cert, NULL);
    if (!key) {
	fprintf(out, "XXX could not extract key for %s.\n", cert->nickname);
	return SECFailure;
    }
    /* XXX should have a type field also */
    fprintf(out, "<%d> %s\n", 0, cert->nickname);

    return SECSuccess;
}

static SECStatus
listKeys(PK11SlotInfo *slot, KeyType keyType, void *pwarg)
{
    SECStatus rv = SECSuccess;

#ifdef notdef
    if (PK11_IsInternal(slot)) {
	/* Print all certs in internal slot db. */
	rv = SECU_PrintKeyNames(SECKEY_GetDefaultKeyDB(), stdout);
	if (rv) {
	    SECU_PrintError(progName, "problem listing keys");
	    return SECFailure;
	}
    } else {
#endif
	/* XXX need a function as below */
	/* could iterate over certs on slot and print keys */
	/* this would miss stranded keys */
    /*rv = PK11_TraverseSlotKeys(slotname, keyType, printKeyCB, NULL, NULL);*/
	if (PK11_NeedLogin(slot))
	    PK11_Authenticate(slot, PR_TRUE, pwarg);
	rv = PK11_TraverseCertsInSlot(slot, secu_PrintKeyFromCert, stdout);
	if (rv) {
	    SECU_PrintError(progName, "problem listing keys");
	    return SECFailure;
	}
	return SECSuccess;
#ifdef notdef
    }
    return rv;
#endif
}

static SECStatus
ListKeys(PK11SlotInfo *slot, char *keyname, int index, 
         KeyType keyType, PRBool dopriv, secuPWData *pwdata)
{
    SECStatus rv = SECSuccess;

#ifdef notdef
    if (keyname) {
	if (dopriv) {
	    return DumpPrivateKey(index, keyname, stdout);
	} else {
	    return DumpPublicKey(index, keyname, stdout);
	}
    }
#endif
    /* For now, split handling of slot to internal vs. other.  slot should
     * probably be allowed to be NULL so that all slots can be listed.
     * In that case, need to add a call to PK11_TraverseSlotCerts().
     */
    if (slot == NULL) {
	PK11SlotList *list;
	PK11SlotListElement *le;

	list= PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_FALSE,pwdata);
	if (list) for (le = list->head; le; le = le->next) {
	    rv = listKeys(le->slot,keyType,pwdata);
	}
    } else {
	rv = listKeys(slot,keyType,pwdata);
    }
    return rv;
}

static SECStatus
DeleteKey(char *nickname, secuPWData *pwdata)
{
    SECStatus rv;
    CERTCertificate *cert;
    PK11SlotInfo *slot;

    slot = PK11_GetInternalKeySlot();
    if (PK11_NeedLogin(slot))
	PK11_Authenticate(slot, PR_TRUE, pwdata);
    cert = PK11_FindCertFromNickname(nickname, pwdata);
    if (!cert) return SECFailure;
    rv = PK11_DeleteTokenCertAndKey(cert, pwdata);
    if (rv != SECSuccess) {
	SECU_PrintError("problem deleting private key \"%s\"\n", nickname);
    }
    return rv;
}


/*
 *  L i s t M o d u l e s
 *
 *  Print a list of the PKCS11 modules that are
 *  available. This is useful for smartcard people to
 *  make sure they have the drivers loaded.
 *
 */
static SECStatus
ListModules(void)
{
    PK11SlotList *list;
    PK11SlotListElement *le;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_FALSE,NULL);
    if (list == NULL) return SECFailure;

    /* look at each slot*/
    for (le = list->head ; le; le = le->next) {
      printf ("\n");
      printf ("    slot: %s\n", PK11_GetSlotName(le->slot));
      printf ("   token: %s\n", PK11_GetTokenName(le->slot));
    }
    PK11_FreeSlotList(list);

    return SECSuccess;
}

static void 
Usage(char *progName)
{
#define FPS fprintf(stderr, 
    FPS "Type %s -H for more detailed descriptions\n", progName);
    FPS "Usage:  %s -N [-d certdir] [-P dbprefix] [-f pwfile]\n", progName);
    FPS "Usage:  %s -T [-d certdir] [-P dbprefix] [-h token-name] [-f pwfile]\n", progName);
    FPS "\t%s -A -n cert-name -t trustargs [-d certdir] [-P dbprefix] [-a] [-i input]\n", 
    	progName);
    FPS "\t%s -C [-c issuer-name | -x] -i cert-request-file -o cert-file\n"
	"\t\t [-m serial-number] [-w warp-months] [-v months-valid]\n"
        "\t\t [-f pwfile] [-d certdir] [-P dbprefix] [-1] [-2] [-3] [-4] [-5] [-6]\n",
	progName);
    FPS "\t%s -D -n cert-name [-d certdir] [-P dbprefix]\n", progName);
    FPS "\t%s -E -n cert-name -t trustargs [-d certdir] [-P dbprefix] [-a] [-i input]\n", 
	progName);
    FPS "\t%s -G -n key-name [-h token-name] [-k rsa] [-g key-size] [-y exp]\n" 
	"\t\t [-f pwfile] [-z noisefile] [-d certdir] [-P dbprefix]\n", progName);
    FPS "\t%s -G [-h token-name] -k dsa [-q pqgfile -g key-size] [-f pwfile]\n"
	"\t\t [-z noisefile] [-d certdir] [-P dbprefix]\n", progName);
    FPS "\t%s -K [-n key-name] [-h token-name] [-k dsa|rsa|all]\n", 
	progName);
    FPS "\t\t [-f pwfile] [-d certdir] [-P dbprefix]\n");
    FPS "\t%s -L [-n cert-name] [-d certdir] [-P dbprefix] [-r] [-a]\n", progName);
    FPS "\t%s -M -n cert-name -t trustargs [-d certdir] [-P dbprefix]\n",
	progName);
    FPS "\t%s -R -s subj -o cert-request-file [-d certdir] [-P dbprefix] [-p phone] [-a]\n"
	"\t\t [-k key-type] [-h token-name] [-f pwfile] [-g key-size]\n",
	progName);
    FPS "\t%s -V -n cert-name -u usage [-b time] [-e] [-d certdir] [-P dbprefix]\n",
	progName);
    FPS "\t%s -S -n cert-name -s subj [-c issuer-name | -x]  -t trustargs\n"
	"\t\t [-k key-type] [-h token-name] [-g key-size]\n"
        "\t\t [-m serial-number] [-w warp-months] [-v months-valid]\n"
	"\t\t [-f pwfile] [-d certdir] [-P dbprefix]\n"
        "\t\t [-p phone] [-1] [-2] [-3] [-4] [-5] [-6]\n",
	progName);
    FPS "\t%s -U [-d certdir] [-P dbprefix]\n", progName);
    exit(1);
}

static void LongUsage(char *progName)
{

    FPS "%-15s Add a certificate to the database        (create if needed)\n",
	"-A");
    FPS "%-15s Add an Email certificate to the database (create if needed)\n",
	"-E");
    FPS "%-20s Specify the nickname of the certificate to add\n",
	"   -n cert-name");
    FPS "%-20s Set the certificate trust attributes:\n",
	"   -t trustargs");
    FPS "%-25s p \t valid peer\n", "");
    FPS "%-25s P \t trusted peer (implies p)\n", "");
    FPS "%-25s c \t valid CA\n", "");
    FPS "%-25s T \t trusted CA to issue client certs (implies c)\n", "");
    FPS "%-25s C \t trusted CA to issue server certs (implies c)\n", "");
    FPS "%-25s u \t user cert\n", "");
    FPS "%-25s w \t send warning\n", "");
#ifdef DEBUG_NSSTEAM_ONLY
    FPS "%-25s g \t make step-up cert\n", "");
#endif /* DEBUG_NSSTEAM_ONLY */
    FPS "%-20s Specify the password file\n",
	"   -f pwfile");
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "%-20s The input certificate is encoded in ASCII (RFC1113)\n",
	"   -a");
    FPS "%-20s Specify the certificate file (default is stdin)\n",
	"   -i input");
    FPS "\n");

    FPS "%-15s Create a new binary certificate from a BINARY cert request\n",
	"-C");
    FPS "%-20s The nickname of the issuer cert\n",
	"   -c issuer-name");
    FPS "%-20s The BINARY certificate request file\n",
	"   -i cert-request ");
    FPS "%-20s Output binary cert to this file (default is stdout)\n",
	"   -o output-cert");
    FPS "%-20s Self sign\n",
	"   -x");
    FPS "%-20s Cert serial number\n",
	"   -m serial-number");
    FPS "%-20s Time Warp\n",
	"   -w warp-months");
    FPS "%-20s Months valid (default is 3)\n",
        "   -v months-valid");
    FPS "%-20s Specify the password file\n",
	"   -f pwfile");
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "%-20s Create key usage extension\n",
	"   -1 ");
    FPS "%-20s Create basic constraint extension\n",
	"   -2 ");
    FPS "%-20s Create authority key ID extension\n",
	"   -3 ");
    FPS "%-20s Create crl distribution point extension\n",
	"   -4 ");
    FPS "%-20s Create netscape cert type extension\n",
	"   -5 ");
    FPS "%-20s Create extended key usage extension\n",
	"   -6 ");
    FPS "\n");

    FPS "%-15s Generate a new key pair\n",
	"-G");
    FPS "%-20s Name of token in which to generate key (default is internal)\n",
	"   -h token-name");
    FPS "%-20s Type of key pair to generate (\"dsa\", \"rsa\" (default))\n",
	"   -k key-type");
    FPS "%-20s Key size in bits, (min %d, max %d, default %d)\n",
	"   -g key-size", MIN_KEY_BITS, MAX_KEY_BITS, DEFAULT_KEY_BITS);
    FPS "%-20s Set the public exponent value (3, 17, 65537) (rsa only)\n",
	"   -y exp");
    FPS "%-20s Specify the password file\n",
        "   -f password-file");
    FPS "%-20s Specify the noise file to be used\n",
	"   -z noisefile");
    FPS "%-20s read PQG value from pqgfile (dsa only)\n",
	"   -q pqgfile");
    FPS "%-20s Key database directory (default is ~/.netscape)\n",
	"   -d keydir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "\n");

    FPS "%-15s Delete a certificate from the database\n",
	"-D");
    FPS "%-20s The nickname of the cert to delete\n",
	"   -n cert-name");
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "\n");

    FPS "%-15s List all modules\n", /*, or print out a single named module\n",*/
        "-U");
    FPS "%-20s Module database directory (default is '~/.netscape')\n",
        "   -d moddir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");

    FPS "%-15s List all keys\n", /*, or print out a single named key\n",*/
        "-K");
    FPS "%-20s Name of token in which to look for keys (default is internal,"
	" use \"all\" to list keys on all tokens)\n",
	"   -h token-name ");
    FPS "%-20s Type of key pair to list (\"all\", \"dsa\", \"rsa\" (default))\n",
	"   -k key-type");
    FPS "%-20s Specify the password file\n",
        "   -f password-file");
    FPS "%-20s Key database directory (default is ~/.netscape)\n",
	"   -d keydir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "\n");

    FPS "%-15s List all certs, or print out a single named cert\n",
	"-L");
    FPS "%-20s Pretty print named cert (list all if unspecified)\n",
	"   -n cert-name");
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "%-20s For single cert, print binary DER encoding\n",
	"   -r");
    FPS "%-20s For single cert, print ASCII encoding (RFC1113)\n",
	"   -a");
    FPS "\n");

    FPS "%-15s Modify trust attributes of certificate\n",
	"-M");
    FPS "%-20s The nickname of the cert to modify\n",
	"   -n cert-name");
    FPS "%-20s Set the certificate trust attributes (see -A above)\n",
	"   -t trustargs");
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "\n");

    FPS "%-15s Create a new certificate database\n",
	"-N");
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "\n");
    FPS "%-15s Reset the Key database or token\n",
	"-T");
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "%-20s Token to reset (default is internal)\n"
	"   -h token-name");
    FPS "\n");

    FPS "%-15s Generate a certificate request (stdout)\n",
	"-R");
    FPS "%-20s Specify the subject name (using RFC1485)\n",
	"   -s subject");
    FPS "%-20s Output the cert request to this file\n",
	"   -o output-req");
    FPS "%-20s Type of key pair to generate (\"dsa\", \"rsa\" (default))\n",
	"   -k key-type");
    FPS "%-20s Name of token in which to generate key (default is internal)\n",
	"   -h token-name");
    FPS "%-20s Key size in bits, RSA keys only (min %d, max %d, default %d)\n",
	"   -g key-size", MIN_KEY_BITS, MAX_KEY_BITS, DEFAULT_KEY_BITS);
    FPS "%-20s Specify the password file\n",
	"   -f pwfile");
    FPS "%-20s Key database directory (default is ~/.netscape)\n",
	"   -d keydir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "%-20s Specify the contact phone number (\"123-456-7890\")\n",
	"   -p phone");
    FPS "%-20s Output the cert request in ASCII (RFC1113); default is binary\n",
	"   -a");
    FPS "\n");

    FPS "%-15s Validate a certificate\n",
	"-V");
    FPS "%-20s The nickname of the cert to Validate\n",
	"   -n cert-name");
    FPS "%-20s validity time (\"YYMMDDHHMMSS[+HHMM|-HHMM|Z]\")\n",
	"   -b time");
    FPS "%-20s Check certificate signature \n",
	"   -e ");   
    FPS "%-20s Specify certificate usage:\n", "   -u certusage");
    FPS "%-25s C \t SSL Client\n", "");
    FPS "%-25s V \t SSL Server\n", "");
    FPS "%-25s S \t Email signer\n", "");
    FPS "%-25s R \t Email Recipient\n", "");   
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "\n");

    FPS "%-15s Make a certificate and add to database\n",
        "-S");
    FPS "%-20s Specify the nickname of the cert\n",
        "   -n key-name");
    FPS "%-20s Specify the subject name (using RFC1485)\n",
        "   -s subject");
    FPS "%-20s The nickname of the issuer cert\n",
	"   -c issuer-name");
    FPS "%-20s Set the certificate trust attributes (see -A above)\n",
	"   -t trustargs");
    FPS "%-20s Type of key pair to generate (\"dsa\", \"rsa\" (default))\n",
	"   -k key-type");
    FPS "%-20s Name of token in which to generate key (default is internal)\n",
	"   -h token-name");
    FPS "%-20s Key size in bits, RSA keys only (min %d, max %d, default %d)\n",
	"   -g key-size", MIN_KEY_BITS, MAX_KEY_BITS, DEFAULT_KEY_BITS);
    FPS "%-20s Self sign\n",
	"   -x");
    FPS "%-20s Cert serial number\n",
	"   -m serial-number");
    FPS "%-20s Time Warp\n",
	"   -w warp-months");
    FPS "%-20s Months valid (default is 3)\n",
        "   -v months-valid");
    FPS "%-20s Specify the password file\n",
	"   -f pwfile");
    FPS "%-20s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-20s Cert & Key database prefix\n",
	"   -P dbprefix");
    FPS "%-20s Specify the contact phone number (\"123-456-7890\")\n",
	"   -p phone");
    FPS "%-20s Create key usage extension\n",
	"   -1 ");
    FPS "%-20s Create basic constraint extension\n",
	"   -2 ");
    FPS "%-20s Create authority key ID extension\n",
	"   -3 ");
    FPS "%-20s Create crl distribution point extension\n",
	"   -4 ");
    FPS "%-20s Create netscape cert type extension\n",
	"   -5 ");
    FPS "%-20s Create extended key usage extension\n",
	"   -6 ");
    FPS "\n");

    exit(1);
#undef FPS
}


static CERTCertificate *
MakeV1Cert(	CERTCertDBHandle *	handle, 
		CERTCertificateRequest *req,
	    	char *			issuerNickName, 
		PRBool 			selfsign, 
		int 			serialNumber,
		int 			warpmonths,
                int                     validitylength)
{
    CERTCertificate *issuerCert = NULL;
    CERTValidity *validity;
    CERTCertificate *cert = NULL;
#ifndef NSPR20    
    PRTime printableTime;
    int64 now, after;
#else
    PRExplodedTime printableTime;
    PRTime now, after;
#endif    
   
    

    if ( !selfsign ) {
	issuerCert = CERT_FindCertByNicknameOrEmailAddr(handle, issuerNickName);
	if (!issuerCert) {
	    SECU_PrintError(progName, "could not find certificate named \"%s\"",
			    issuerNickName);
	    return NULL;
	}
    }

    now = PR_Now();
#ifndef NSPR20
    PR_ExplodeGMTTime (&printableTime, now);
#else    
    PR_ExplodeTime (now, PR_GMTParameters, &printableTime);
#endif
    if ( warpmonths ) {
#ifndef	NSPR20    
	printableTime.tm_mon += warpmonths;
	now = PR_ImplodeTime (&printableTime, 0, 0);
	PR_ExplodeGMTTime (&printableTime, now);
#else
	printableTime.tm_month += warpmonths;
	now = PR_ImplodeTime (&printableTime);
	PR_ExplodeTime (now, PR_GMTParameters, &printableTime);
#endif
    }
#ifndef	NSPR20  
    printableTime.tm_mon += validitylength;  
    printableTime.tm_mon += 3;
    after = PR_ImplodeTime (&printableTime, 0, 0);

#else
    printableTime.tm_month += validitylength;
    printableTime.tm_month += 3;
    after = PR_ImplodeTime (&printableTime);
#endif    

    /* note that the time is now in micro-second unit */
    validity = CERT_CreateValidity (now, after);

    if ( selfsign ) {
	cert = CERT_CreateCertificate
	    (serialNumber,&(req->subject), validity, req);
    } else {
	cert = CERT_CreateCertificate
	    (serialNumber,&(issuerCert->subject), validity, req);
    }
    
    CERT_DestroyValidity(validity);
    if ( issuerCert ) {
	CERT_DestroyCertificate (issuerCert);
    }
    
    return(cert);
}

static SECStatus 
AddKeyUsage (void *extHandle)
{
    SECItem bitStringValue;
    unsigned char keyUsage = 0x0;
    char buffer[5];
    int value;

    while (1) {
	fprintf(stdout, "%-25s 0 - Digital Signature\n", "");
	fprintf(stdout, "%-25s 1 - Non-repudiation\n", "");
	fprintf(stdout, "%-25s 2 - Key encipherment\n", "");
	fprintf(stdout, "%-25s 3 - Data encipherment\n", "");   
	fprintf(stdout, "%-25s 4 - Key agreement\n", "");
	fprintf(stdout, "%-25s 5 - Cert signning key\n", "");   
	fprintf(stdout, "%-25s 6 - CRL signning key\n", "");
	fprintf(stdout, "%-25s Other to finish\n", "");
	if (gets (buffer)) {
	    value = atoi (buffer);
	    if (value < 0 || value > 6)
	        break;
	    keyUsage |= (0x80 >> value);
	}
	else {		/* gets() returns NULL on EOF or error */
	    break;
	}
    }

    bitStringValue.data = &keyUsage;
    bitStringValue.len = 1;
    buffer[0] = 'n';
    puts ("Is this a critical extension [y/n]? ");
    gets (buffer);	

    return (CERT_EncodeAndAddBitStrExtension
	    (extHandle, SEC_OID_X509_KEY_USAGE, &bitStringValue,
	     (buffer[0] == 'y' || buffer[0] == 'Y') ? PR_TRUE : PR_FALSE));

}


static CERTOidSequence *
CreateOidSequence(void)
{
  CERTOidSequence *rv = (CERTOidSequence *)NULL;
  PRArenaPool *arena = (PRArenaPool *)NULL;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if( (PRArenaPool *)NULL == arena ) {
    goto loser;
  }

  rv = (CERTOidSequence *)PORT_ArenaZAlloc(arena, sizeof(CERTOidSequence));
  if( (CERTOidSequence *)NULL == rv ) {
    goto loser;
  }

  rv->oids = (SECItem **)PORT_ArenaZAlloc(arena, sizeof(SECItem *));
  if( (SECItem **)NULL == rv->oids ) {
    goto loser;
  }

  rv->arena = arena;
  return rv;

 loser:
  if( (PRArenaPool *)NULL != arena ) {
    PORT_FreeArena(arena, PR_FALSE);
  }

  return (CERTOidSequence *)NULL;
}

static SECStatus
AddOidToSequence(CERTOidSequence *os, SECOidTag oidTag)
{
  SECItem **oids;
  PRUint32 count = 0;
  SECOidData *od;

  od = SECOID_FindOIDByTag(oidTag);
  if( (SECOidData *)NULL == od ) {
    return SECFailure;
  }

  for( oids = os->oids; (SECItem *)NULL != *oids; oids++ ) {
    count++;
  }

  /* ArenaZRealloc */

  {
    PRUint32 i;

    oids = (SECItem **)PORT_ArenaZAlloc(os->arena, sizeof(SECItem *) * (count+2));
    if( (SECItem **)NULL == oids ) {
      return SECFailure;
    }
    
    for( i = 0; i < count; i++ ) {
      oids[i] = os->oids[i];
    }

    /* ArenaZFree(os->oids); */
  }

  os->oids = oids;
  os->oids[count] = &od->oid;

  return SECSuccess;
}

static SECItem *
EncodeOidSequence(CERTOidSequence *os)
{
  SECItem *rv;
  extern const SEC_ASN1Template CERT_OidSeqTemplate[];

  rv = (SECItem *)PORT_ArenaZAlloc(os->arena, sizeof(SECItem));
  if( (SECItem *)NULL == rv ) {
    goto loser;
  }

  if( !SEC_ASN1EncodeItem(os->arena, rv, os, CERT_OidSeqTemplate) ) {
    goto loser;
  }

  return rv;

 loser:
  return (SECItem *)NULL;
}

static SECStatus 
AddExtKeyUsage (void *extHandle)
{
  char buffer[5];
  int value;
  CERTOidSequence *os;
  SECStatus rv;
  SECItem *item;

  os = CreateOidSequence();
  if( (CERTOidSequence *)NULL == os ) {
    return SECFailure;
  }

  while (1) {
    fprintf(stdout, "%-25s 0 - Server Auth\n", "");
    fprintf(stdout, "%-25s 1 - Client Auth\n", "");
    fprintf(stdout, "%-25s 2 - Code Signing\n", "");
    fprintf(stdout, "%-25s 3 - Email Protection\n", "");
    fprintf(stdout, "%-25s 4 - Timestamp\n", "");
    fprintf(stdout, "%-25s 5 - OSCP Responder\n", "");
#ifdef DEBUG_NSSTEAM_ONLY
    fprintf(stdout, "%-25s 6 - Step-up\n", "");
#endif /* DEBUG_NSSTEAM_ONLY */
    fprintf(stdout, "%-25s Other to finish\n", "");

    gets(buffer);
    value = atoi(buffer);

    switch( value ) {
    case 0:
      rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_SERVER_AUTH);
      break;
    case 1:
      rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_CLIENT_AUTH);
      break;
    case 2:
      rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_CODE_SIGN);
      break;
    case 3:
      rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_EMAIL_PROTECT);
      break;
    case 4:
      rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_TIME_STAMP);
      break;
    case 5:
      rv = AddOidToSequence(os, SEC_OID_OCSP_RESPONDER);
      break;
#ifdef DEBUG_NSSTEAM_ONLY
    case 6:
      rv = AddOidToSequence(os, SEC_OID_NS_KEY_USAGE_GOVT_APPROVED);
      break;
#endif /* DEBUG_NSSTEAM_ONLY */
    default:
      goto endloop;
    }

    if( SECSuccess != rv ) goto loser;
  }

 endloop:;
  item = EncodeOidSequence(os);

  buffer[0] = 'n';
  puts ("Is this a critical extension [y/n]? ");
  gets (buffer);	

  rv = CERT_AddExtension(extHandle, SEC_OID_X509_EXT_KEY_USAGE, item,
                         ((buffer[0] == 'y' || buffer[0] == 'Y')
                          ? PR_TRUE : PR_FALSE), PR_TRUE);
  /*FALLTHROUGH*/
 loser:
  CERT_DestroyOidSequence(os);
  return rv;
}

static SECStatus 
AddNscpCertType (void *extHandle)
{
    SECItem bitStringValue;
    unsigned char keyUsage = 0x0;
    char buffer[5];
    int value;

    while (1) {
	fprintf(stdout, "%-25s 0 - SSL Client\n", "");
	fprintf(stdout, "%-25s 1 - SSL Server\n", "");
	fprintf(stdout, "%-25s 2 - S/MIME\n", "");
	fprintf(stdout, "%-25s 3 - Object Signing\n", "");   
	fprintf(stdout, "%-25s 4 - Reserved for futuer use\n", "");
	fprintf(stdout, "%-25s 5 - SSL CA\n", "");   
	fprintf(stdout, "%-25s 6 - S/MIME CA\n", "");
	fprintf(stdout, "%-25s 7 - Object Signing CA\n", "");
	fprintf(stdout, "%-25s Other to finish\n", "");
	gets (buffer);
	value = atoi (buffer);
	if (value < 0 || value > 7)
	    break;
	keyUsage |= (0x80 >> value);
    }

    bitStringValue.data = &keyUsage;
    bitStringValue.len = 1;
    buffer[0] = 'n';
    puts ("Is this a critical extension [y/n]? ");
    gets (buffer);	

    return (CERT_EncodeAndAddBitStrExtension
	    (extHandle, SEC_OID_NS_CERT_EXT_CERT_TYPE, &bitStringValue,
	     (buffer[0] == 'y' || buffer[0] == 'Y') ? PR_TRUE : PR_FALSE));

}

typedef SECStatus (* EXTEN_VALUE_ENCODER)
		(PRArenaPool *extHandle, void *value, SECItem *encodedValue);

static SECStatus 
EncodeAndAddExtensionValue(
	PRArenaPool *	arena, 
	void *		extHandle, 
	void *		value, 
	PRBool 		criticality,
	int 		extenType, 
	EXTEN_VALUE_ENCODER EncodeValueFn)
{
    SECItem encodedValue;
    SECStatus rv;
	

    encodedValue.data = NULL;
    encodedValue.len = 0;
    do {
	rv = (*EncodeValueFn)(arena, value, &encodedValue);
	if (rv != SECSuccess)
	break;

	rv = CERT_AddExtension
	     (extHandle, extenType, &encodedValue, criticality,PR_TRUE);
    } while (0);
	
    return (rv);
}

static SECStatus 
AddBasicConstraint(void *extHandle)
{
    CERTBasicConstraints basicConstraint;    
    SECItem encodedValue;
    SECStatus rv;
    char buffer[10];

    encodedValue.data = NULL;
    encodedValue.len = 0;
    do {
	basicConstraint.pathLenConstraint = CERT_UNLIMITED_PATH_CONSTRAINT;
	puts ("Is this a CA certificate [y/n]?");
	gets (buffer);
	basicConstraint.isCA = (buffer[0] == 'Y' || buffer[0] == 'y') ?
                                PR_TRUE : PR_FALSE;

	puts ("Enter the path length constraint, enter to skip [<0 for unlimited path]:");
	gets (buffer);
	if (PORT_Strlen (buffer) > 0)
	    basicConstraint.pathLenConstraint = atoi (buffer);
	
	rv = CERT_EncodeBasicConstraintValue (NULL, &basicConstraint, &encodedValue);
	if (rv)
	    return (rv);

	buffer[0] = 'n';
	puts ("Is this a critical extension [y/n]? ");
	gets (buffer);	
	rv = CERT_AddExtension
	     (extHandle, SEC_OID_X509_BASIC_CONSTRAINTS,
	      &encodedValue, (buffer[0] == 'y' || buffer[0] == 'Y') ?
              PR_TRUE : PR_FALSE ,PR_TRUE);
    } while (0);
    PORT_Free (encodedValue.data);
    return (rv);
}

static SECItem *
SignCert(CERTCertDBHandle *handle, 
CERTCertificate *cert, PRBool selfsign, 
SECKEYPrivateKey *selfsignprivkey, char *issuerNickName, void *pwarg)
{
    SECItem der;
    SECItem *result = NULL;
    SECKEYPrivateKey *caPrivateKey = NULL;    
    SECStatus rv;
    PRArenaPool *arena;
    SECOidTag algID;
    void *dummy;

    if( selfsign ) {
      caPrivateKey = selfsignprivkey;
    } else {
      /*CERTCertificate *issuer = CERT_FindCertByNickname(handle, issuerNickName);*/
      CERTCertificate *issuer = PK11_FindCertFromNickname(issuerNickName, pwarg);
      if( (CERTCertificate *)NULL == issuer ) {
        SECU_PrintError(progName, "unable to find issuer with nickname %s", 
	                issuerNickName);
        return (SECItem *)NULL;
      }

      caPrivateKey = PK11_FindKeyByAnyCert(issuer, pwarg);
      if (caPrivateKey == NULL) {
	SECU_PrintError(progName, "unable to retrieve key %s", issuerNickName);
	return NULL;
      }
    }
	
    arena = cert->arena;

    switch(caPrivateKey->keyType) {
      case rsaKey:
	algID = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
	break;
      case dsaKey:
	algID = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
	break;
      default:
	fprintf(stderr, "Unknown key type for issuer.");
	goto done;
	break;
    }

    rv = SECOID_SetAlgorithmID(arena, &cert->signature, algID, 0);
    if (rv != SECSuccess) {
	fprintf(stderr, "Could not set signature algorithm id.");
	goto done;
    }

    /* we only deal with cert v3 here */
    *(cert->version.data) = 2;
    cert->version.len = 1;

    der.len = 0;
    der.data = NULL;
    dummy = SEC_ASN1EncodeItem (arena, &der, cert, CERT_CertificateTemplate);
    if (!dummy) {
	fprintf (stderr, "Could not encode certificate.\n");
	goto done;
    }

    result = (SECItem *) PORT_ArenaZAlloc (arena, sizeof (SECItem));
    if (result == NULL) {
	fprintf (stderr, "Could not allocate item for certificate data.\n");
	goto done;
    }

    rv = SEC_DerSignData (arena, result, der.data, der.len, caPrivateKey,
			  algID);
    if (rv != SECSuccess) {
	fprintf (stderr, "Could not sign encoded certificate data.\n");
	PORT_Free(result);
	result = NULL;
	goto done;
    }
    cert->derCert = *result;
done:
    SECKEY_DestroyPrivateKey(caPrivateKey);
    return result;
}

static SECStatus 
AddAuthKeyID (void *extHandle)
{
    CERTAuthKeyID *authKeyID = NULL;    
    PRArenaPool *arena = NULL;
    SECStatus rv = SECSuccess;
    char buffer[512];

    do {
	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if ( !arena ) {
	    SECU_PrintError(progName, "out of memory");
	    GEN_BREAK (SECFailure);
	}

	if (GetYesNo ("Enter value for the authKeyID extension [y/n]?\n") == 0)
	    break;
	
	authKeyID = PORT_ArenaZAlloc (arena, sizeof (CERTAuthKeyID));
	if (authKeyID == NULL) {
	    GEN_BREAK (SECFailure);
	}

	rv = GetString (arena, "Enter value for the key identifier fields, enter to omit:",
			&authKeyID->keyID);
	if (rv != SECSuccess)
	    break;
	authKeyID->authCertIssuer = GetGeneralName (arena);
	if (authKeyID->authCertIssuer == NULL && SECFailure == PORT_GetError ())
		break;
	

	rv = GetString (arena, "Enter value for the authCertSerial field, enter to omit:",
			&authKeyID->authCertSerialNumber);

	buffer[0] = 'n';
	puts ("Is this a critical extension [y/n]? ");
	gets (buffer);	

	rv = EncodeAndAddExtensionValue
	    (arena, extHandle, authKeyID,
	     (buffer[0] == 'y' || buffer[0] == 'Y') ? PR_TRUE : PR_FALSE,
	     SEC_OID_X509_AUTH_KEY_ID, 
	     (EXTEN_VALUE_ENCODER) CERT_EncodeAuthKeyID);
	if (rv)
	    break;
	
    } while (0);
    if (arena)
	PORT_FreeArena (arena, PR_FALSE);
    return (rv);
}   
    
static SECStatus 
AddCrlDistPoint(void *extHandle)
{
    PRArenaPool *arena = NULL;
    CERTCrlDistributionPoints *crlDistPoints = NULL;
    CRLDistributionPoint *current;
    SECStatus rv = SECSuccess;
    int count = 0, intValue;
    char buffer[5];

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena )
	return (SECFailure);

    do {
	current = NULL;
	current = PORT_ArenaZAlloc (arena, sizeof (*current));
        if (current == NULL) {
	    GEN_BREAK (SECFailure);
	}   

	/* Get the distributionPointName fields - this field is optional */
	puts ("Enter the type of the distribution point name:\n");
	puts ("\t1 - Full Name\n\t2 - Relative Name\n\tOther - omit\n\t\tChoice: ");
	scanf ("%d", &intValue);
	switch (intValue) {
	    case generalName:
		current->distPointType = intValue;
		current->distPoint.fullName = GetGeneralName (arena);
		rv = PORT_GetError();
		break;
		
	    case relativeDistinguishedName: {
		CERTName *name;
		char buffer[512];

		current->distPointType = intValue;
		puts ("Enter the relative name: ");
		fflush (stdout);
		gets (buffer);
		/* For simplicity, use CERT_AsciiToName to converse from a string
		   to NAME, but we only interest in the first RDN */
		name = CERT_AsciiToName (buffer);
		if (!name) {
		    GEN_BREAK (SECFailure);
		}
		rv = CERT_CopyRDN (arena, &current->distPoint.relativeName, name->rdns[0]);
		CERT_DestroyName (name);
		break;
	    }
	}
	if (rv != SECSuccess)
	    break;

	/* Get the reason flags */
	puts ("\nSelect one of the following for the reason flags\n");
	puts ("\t0 - unused\n\t1 - keyCompromise\n\t2 - caCompromise\n\t3 - affiliationChanged\n");
	puts ("\t4 - superseded\n\t5 - cessationOfOperation\n\t6 - certificateHold\n");
	puts ("\tother - omit\t\tChoice: ");
	
	scanf ("%d", &intValue);
	if (intValue >= 0 && intValue <8) {
	    current->reasons.data = PORT_ArenaAlloc (arena, sizeof(char));
	    if (current->reasons.data == NULL) {
		GEN_BREAK (SECFailure);
	    }
	    *current->reasons.data = (char)(0x80 >> intValue);
	    current->reasons.len = 1;
	}
	puts ("Enter value for the CRL Issuer name:\n");
        current->crlIssuer = GetGeneralName (arena);
	if (current->crlIssuer == NULL && (rv = PORT_GetError()) == SECFailure)
	    break;

	if (crlDistPoints == NULL) {
	    crlDistPoints = PORT_ArenaZAlloc (arena, sizeof (*crlDistPoints));
	    if (crlDistPoints == NULL) {
		GEN_BREAK (SECFailure);
	    }
	}
	    
	crlDistPoints->distPoints = PORT_ArenaGrow (arena, 
	     crlDistPoints->distPoints,
	     sizeof (*crlDistPoints->distPoints) * count,
	     sizeof (*crlDistPoints->distPoints) *(count + 1));
	if (crlDistPoints->distPoints == NULL) {
	    GEN_BREAK (SECFailure);
	}
	
	crlDistPoints->distPoints[count] = current;
	++count;
	if (GetYesNo ("Enter more value for the CRL distribution point extension [y/n]\n") == 0) {
	    /* Add null to the end of the crlDistPoints to mark end of data */
	    crlDistPoints->distPoints = PORT_ArenaGrow(arena, 
		 crlDistPoints->distPoints,
		 sizeof (*crlDistPoints->distPoints) * count,
		 sizeof (*crlDistPoints->distPoints) *(count + 1));
	    crlDistPoints->distPoints[count] = NULL;	    
	    break;
	}
	

    } while (1);
    
    if (rv == SECSuccess) {
	buffer[0] = 'n';
	puts ("Is this a critical extension [y/n]? ");
	gets (buffer);	
	
	rv = EncodeAndAddExtensionValue(arena, extHandle, crlDistPoints,
	      (buffer[0] == 'Y' || buffer[0] == 'y') ? PR_TRUE : PR_FALSE,
	      SEC_OID_X509_CRL_DIST_POINTS,
	      (EXTEN_VALUE_ENCODER)  CERT_EncodeCRLDistributionPoints);
    }
    if (arena)
	PORT_FreeArena (arena, PR_FALSE);
    return (rv);
}

static SECStatus
CreateCert(
	CERTCertDBHandle *handle, 
	char *  issuerNickName, 
	PRFileDesc *inFile,
	PRFileDesc *outFile, 
	SECKEYPrivateKey *selfsignprivkey,
	void 	*pwarg,
	int     serialNumber, 
	int     warpmonths,
	int     validitylength,
	PRBool  ascii,
	PRBool  selfsign,
	PRBool	keyUsage, 
	PRBool  extKeyUsage,
	PRBool  basicConstraint, 
	PRBool  authKeyID,
	PRBool  crlDistPoints, 
	PRBool  nscpCertType)
{
    void *	extHandle;
    SECItem *	certDER;
    PRArenaPool *arena			= NULL;
    CERTCertificate *subjectCert 	= NULL;
    /*CERTCertificate *issuerCert 	= NULL;*/
    CERTCertificateRequest *certReq	= NULL;
    SECStatus 	rv 			= SECSuccess;
    SECItem 	reqDER;

    reqDER.data = NULL;
    do {
	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if (!arena) {
	    GEN_BREAK (SECFailure);
	}
	
	/* Create a certrequest object from the input cert request der */
	certReq = GetCertRequest(inFile, ascii);
	if (certReq == NULL) {
	    GEN_BREAK (SECFailure)
	}

	subjectCert = MakeV1Cert (handle, certReq, issuerNickName, selfsign,
				  serialNumber, warpmonths, validitylength);
	if (subjectCert == NULL) {
	    GEN_BREAK (SECFailure)
	}

	extHandle = CERT_StartCertExtensions (subjectCert);
	if (extHandle == NULL) {
	    GEN_BREAK (SECFailure)
	}

	/* Add key usage extension */
	if (keyUsage) {
	    rv = AddKeyUsage(extHandle);
	    if (rv)
		break;
	}

	/* Add extended key usage extension */
	if (extKeyUsage) {
	    rv = AddExtKeyUsage(extHandle);
	    if (rv)
		break;
	}

	/* Add basic constraint extension */
	if (basicConstraint) {
	    rv = AddBasicConstraint(extHandle);
	    if (rv)
		break;
	}

	if (authKeyID) {
	    rv = AddAuthKeyID (extHandle);
	    if (rv)
		break;
	}    


	if (crlDistPoints) {
	    rv = AddCrlDistPoint (extHandle);
	    if (rv)
		break;
	}
	
	if (nscpCertType) {
	    rv = AddNscpCertType(extHandle);
	    if (rv)
		break;
	}
       

	CERT_FinishExtensions(extHandle);

	certDER = SignCert (handle, subjectCert, selfsign, selfsignprivkey, issuerNickName,pwarg);

	if (certDER) {
	   if (ascii) {
		PR_fprintf(outFile, "%s\n%s\n%s\n", NS_CERT_HEADER, 
		        BTOA_DataToAscii(certDER->data, certDER->len), NS_CERT_TRAILER);
	   } else {
		PR_Write(outFile, certDER->data, certDER->len);
	   }
	}

    } while (0);
    CERT_DestroyCertificateRequest (certReq);
    CERT_DestroyCertificate (subjectCert);
    PORT_FreeArena (arena, PR_FALSE);
    if (rv != SECSuccess) {
	PRErrorCode  perr = PR_GetError();
        fprintf(stderr, "%s: unable to create cert (%s)\n", progName,
               SECU_Strerror(perr));
    }
    return (rv);
}

/*  Certutil commands  */
enum {
    cmd_AddCert = 0,
    cmd_CreateNewCert,
    cmd_DeleteCert,
    cmd_AddEmailCert,
    cmd_DeleteKey,
    cmd_GenKeyPair,
    cmd_PrintHelp,
    cmd_ListKeys,
    cmd_ListCerts,
    cmd_ModifyCertTrust,
    cmd_NewDBs,
    cmd_CertReq,
    cmd_CreateAndAddCert,
    cmd_TokenReset,
    cmd_ListModules,
    cmd_CheckCertValidity,
    cmd_ChangePassword,
    cmd_Version
};

/*  Certutil options */
enum {
    opt_SSOPass = 0,
    opt_AddKeyUsageExt,
    opt_AddBasicConstraintExt,
    opt_AddAuthorityKeyIDExt,
    opt_AddCRLDistPtsExt,
    opt_AddNSCertTypeExt,
    opt_AddExtKeyUsageExt,
    opt_ASCIIForIO,
    opt_ValidityTime,
    opt_IssuerName,
    opt_CertDir,
    opt_VerifySig,
    opt_PasswordFile,
    opt_KeySize,
    opt_TokenName,
    opt_InputFile,
    opt_KeyIndex,
    opt_KeyType,
    opt_DetailedInfo,
    opt_SerialNumber,
    opt_Nickname,
    opt_OutputFile,
    opt_PhoneNumber,
    opt_DBPrefix,
    opt_PQGFile,
    opt_BinaryDER,
    opt_Subject,
    opt_Trust,
    opt_Usage,
    opt_Validity,
    opt_OffsetMonths,
    opt_SelfSign,
    opt_Exponent,
    opt_NoiseFile
};

static secuCommandFlag certutil_commands[] =
{
	{ /* cmd_AddCert             */  'A', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_CreateNewCert       */  'C', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_DeleteCert          */  'D', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_AddEmailCert        */  'E', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_DeleteKey           */  'F', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_GenKeyPair          */  'G', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_PrintHelp           */  'H', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_ListKeys            */  'K', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_ListCerts           */  'L', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_ModifyCertTrust     */  'M', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_NewDBs              */  'N', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_CertReq             */  'R', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_CreateAndAddCert    */  'S', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_TokenReset          */  'T', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_ListModules         */  'U', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_CheckCertValidity   */  'V', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_ChangePassword      */  'W', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_Version             */  'Y', PR_FALSE, 0, PR_FALSE }
};

static secuCommandFlag certutil_options[] =
{
	{ /* opt_SSOPass             */  '0', PR_TRUE,  0, PR_FALSE },
	{ /* opt_AddKeyUsageExt      */  '1', PR_FALSE, 0, PR_FALSE },
	{ /* opt_AddBasicConstraintExt*/ '2', PR_FALSE, 0, PR_FALSE },
	{ /* opt_AddAuthorityKeyIDExt*/  '3', PR_FALSE, 0, PR_FALSE },
	{ /* opt_AddCRLDistPtsExt    */  '4', PR_FALSE, 0, PR_FALSE },
	{ /* opt_AddNSCertTypeExt    */  '5', PR_FALSE, 0, PR_FALSE },
	{ /* opt_AddExtKeyUsageExt   */  '6', PR_FALSE, 0, PR_FALSE },
	{ /* opt_ASCIIForIO          */  'a', PR_FALSE, 0, PR_FALSE },
	{ /* opt_ValidityTime        */  'b', PR_TRUE,  0, PR_FALSE },
	{ /* opt_IssuerName          */  'c', PR_TRUE,  0, PR_FALSE },
	{ /* opt_CertDir             */  'd', PR_TRUE,  0, PR_FALSE },
	{ /* opt_VerifySig           */  'e', PR_FALSE, 0, PR_FALSE },
	{ /* opt_PasswordFile        */  'f', PR_TRUE,  0, PR_FALSE },
	{ /* opt_KeySize             */  'g', PR_TRUE,  0, PR_FALSE },
	{ /* opt_TokenName           */  'h', PR_TRUE,  0, PR_FALSE },
	{ /* opt_InputFile           */  'i', PR_TRUE,  0, PR_FALSE },
	{ /* opt_KeyIndex            */  'j', PR_TRUE,  0, PR_FALSE },
	{ /* opt_KeyType             */  'k', PR_TRUE,  0, PR_FALSE },
	{ /* opt_DetailedInfo        */  'l', PR_FALSE, 0, PR_FALSE },
	{ /* opt_SerialNumber        */  'm', PR_TRUE,  0, PR_FALSE },
	{ /* opt_Nickname            */  'n', PR_TRUE,  0, PR_FALSE },
	{ /* opt_OutputFile          */  'o', PR_TRUE,  0, PR_FALSE },
	{ /* opt_PhoneNumber         */  'p', PR_TRUE,  0, PR_FALSE },
	{ /* opt_DBPrefix            */  'P', PR_TRUE,  0, PR_FALSE },
	{ /* opt_PQGFile             */  'q', PR_TRUE,  0, PR_FALSE },
	{ /* opt_BinaryDER           */  'r', PR_FALSE, 0, PR_FALSE },
	{ /* opt_Subject             */  's', PR_TRUE,  0, PR_FALSE },
	{ /* opt_Trust               */  't', PR_TRUE,  0, PR_FALSE },
	{ /* opt_Usage               */  'u', PR_TRUE,  0, PR_FALSE },
	{ /* opt_Validity            */  'v', PR_TRUE,  0, PR_FALSE },
	{ /* opt_OffsetMonths        */  'w', PR_TRUE,  0, PR_FALSE },
	{ /* opt_SelfSign            */  'x', PR_FALSE, 0, PR_FALSE },
	{ /* opt_Exponent            */  'y', PR_TRUE,  0, PR_FALSE },
	{ /* opt_NoiseFile           */  'z', PR_TRUE,  0, PR_FALSE }
};

int 
main(int argc, char **argv)
{
    CERTCertDBHandle *certHandle;
    PK11SlotInfo *slot = NULL;
    CERTName *  subject         = 0;
    PRFileDesc *inFile          = 0;
    PRFileDesc *outFile         = 0;
    char *      certfile        = "tempcert";
    char *      certreqfile     = "tempcertreq";
    char *      slotname        = "internal";
    char *      certPrefix      = "";
    KeyType     keytype         = rsaKey;
    /*char *	keyslot	        = NULL;*/
    /*char *      keynickname     = NULL;*/
    char *      name            = NULL;
    int	        keysize	        = DEFAULT_KEY_BITS;
    int         publicExponent  = 0x010001;
    int         serialNumber    = 0;
    int         warpmonths      = 0;
    int         validitylength  = 0;
    int         commandsEntered = 0;
    char        commandToRun    = '\0';
    secuPWData  pwdata          = { PW_NONE, 0 };

    SECKEYPrivateKey *privkey;
    SECKEYPublicKey *pubkey = NULL;

    int i;
    SECStatus rv;

    secuCommand certutil;
    certutil.numCommands = sizeof(certutil_commands) / sizeof(secuCommandFlag);
    certutil.numOptions = sizeof(certutil_options) / sizeof(secuCommandFlag);
    certutil.commands = certutil_commands;
    certutil.options = certutil_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &certutil);

    if (rv != SECSuccess)
	Usage(progName);

    if (certutil.commands[cmd_PrintHelp].activated)
	LongUsage(progName);

    if (certutil.options[opt_PasswordFile].arg) {
	pwdata.source = PW_FROMFILE;
	pwdata.data = certutil.options[opt_PasswordFile].arg;
    }

    if (certutil.options[opt_CertDir].activated)
	SECU_ConfigDirectory(certutil.options[opt_CertDir].arg);

    if (certutil.options[opt_KeySize].activated) {
	keysize = PORT_Atoi(certutil.options[opt_KeySize].arg);
	if ((keysize < MIN_KEY_BITS) || (keysize > MAX_KEY_BITS)) {
	    PR_fprintf(PR_STDERR, 
                       "%s -g:  Keysize must be between %d and %d.\n",
	               MIN_KEY_BITS, MAX_KEY_BITS);
	    return 255;
	}
    }

    /*  -h specify token name  */
    if (certutil.options[opt_TokenName].activated) {
	if (PL_strcmp(certutil.options[opt_TokenName].arg, "all") == 0)
	    slotname = NULL;
	else
	    slotname = PL_strdup(certutil.options[opt_TokenName].arg);
    }

    /*  -k key type  */
    if (certutil.options[opt_KeyType].activated) {
	if (PL_strcmp(certutil.options[opt_KeyType].arg, "rsa") == 0) {
	    keytype = rsaKey;
	} else if (PL_strcmp(certutil.options[opt_KeyType].arg, "dsa") == 0) {
	    keytype = dsaKey;
	} else if (PL_strcmp(certutil.options[opt_KeyType].arg, "all") == 0) {
	    keytype = nullKey;
	} else {
	    PR_fprintf(PR_STDERR, "%s -k:  %s is not a recognized type.\n",
	               progName, certutil.options[opt_KeyType].arg);
	    return 255;
	}
    }

    /*  -m serial number */
    if (certutil.options[opt_SerialNumber].activated) {
	serialNumber = PORT_Atoi(certutil.options[opt_SerialNumber].arg);
	if (serialNumber < 0) {
	    PR_fprintf(PR_STDERR, "%s -m:  %s is not a valid serial number.\n",
	               progName, certutil.options[opt_SerialNumber].arg);
	    return 255;
	}
    }

    /*  -P certdb name prefix */
    if (certutil.options[opt_DBPrefix].activated)
	certPrefix = strdup(certutil.options[opt_DBPrefix].arg);

    /*  -q PQG file  */
    if (certutil.options[opt_PQGFile].activated) {
	if (keytype != dsaKey) {
	    PR_fprintf(PR_STDERR, "%s -q: PQG file is for DSA key (-k dsa).\n)",
	               progName);
	    return 255;
	}
    }

    /*  -s subject name  */
    if (certutil.options[opt_Subject].activated) {
	subject = CERT_AsciiToName(certutil.options[opt_Subject].arg);
	if (!subject) {
	    PR_fprintf(PR_STDERR, "%s -s: improperly formatted name: \"%s\"\n",
	               progName, certutil.options[opt_Subject].arg);
	    return 255;
	}
    }

    /*  -v validity period  */
    if (certutil.options[opt_Validity].activated) {
	validitylength = PORT_Atoi(certutil.options[opt_Validity].arg);
	if (validitylength < 0) {
	    PR_fprintf(PR_STDERR, "%s -v: incorrect validity period: \"%s\"\n",
	               progName, certutil.options[opt_Validity].arg);
	    return 255;
	}
    }

    /*  -w warp months  */
    if (certutil.options[opt_OffsetMonths].activated)
	warpmonths = PORT_Atoi(certutil.options[opt_OffsetMonths].arg);

    /*  -y public exponent (for RSA)  */
    if (certutil.options[opt_Exponent].activated) {
	publicExponent = PORT_Atoi(certutil.options[opt_Exponent].arg);
	if ((publicExponent != 3) &&
	    (publicExponent != 17) &&
	    (publicExponent != 65537)) {
	    PR_fprintf(PR_STDERR, "%s -y: incorrect public exponent %d.", 
	                           progName, publicExponent);
	    PR_fprintf(PR_STDERR, "Must be 3, 17, or 65537.\n");
	    return 255;
	}
    }

    /*  Check number of commands entered.  */
    commandsEntered = 0;
    for (i=0; i< certutil.numCommands; i++) {
	if (certutil.commands[i].activated) {
	    commandToRun = certutil.commands[i].flag;
	    commandsEntered++;
	}
	if (commandsEntered > 1)
	    break;
    }
    if (commandsEntered > 1) {
	PR_fprintf(PR_STDERR, "%s: only one command at a time!\n", progName);
	PR_fprintf(PR_STDERR, "You entered: ");
	for (i=0; i< certutil.numCommands; i++) {
	    if (certutil.commands[i].activated)
		PR_fprintf(PR_STDERR, " -%c", certutil.commands[i].flag);
	}
	PR_fprintf(PR_STDERR, "\n");
	return 255;
    }
    if (commandsEntered == 0) {
	PR_fprintf(PR_STDERR, "%s: you must enter a command!\n", progName);
	Usage(progName);
    }

    /*  -A, -D, -F, -M, -S, -V, and all require -n  */
    if ((certutil.commands[cmd_AddCert].activated ||
         certutil.commands[cmd_DeleteCert].activated ||
         certutil.commands[cmd_DeleteKey].activated ||
         certutil.commands[cmd_ModifyCertTrust].activated ||
         certutil.commands[cmd_CreateAndAddCert].activated ||
         certutil.commands[cmd_CheckCertValidity].activated) &&
        !certutil.options[opt_Nickname].activated) {
	PR_fprintf(PR_STDERR, 
	          "%s -%c: nickname is required for this command (-n).\n",
	           progName, commandToRun);
	return 255;
    }

    /*  -A, -E, -M, -S require trust  */
    if ((certutil.commands[cmd_AddCert].activated ||
         certutil.commands[cmd_AddEmailCert].activated ||
         certutil.commands[cmd_ModifyCertTrust].activated ||
         certutil.commands[cmd_CreateAndAddCert].activated) &&
        !certutil.options[opt_Trust].activated) {
	PR_fprintf(PR_STDERR, 
	          "%s -%c: trust is required for this command (-t).\n",
	           progName, commandToRun);
	return 255;
    }

    /*  if -L is given raw or ascii mode, it must be for only one cert.  */
    if (certutil.commands[cmd_ListCerts].activated &&
        (certutil.options[opt_ASCIIForIO].activated ||
         certutil.options[opt_BinaryDER].activated) &&
        !certutil.options[opt_Nickname].activated) {
	PR_fprintf(PR_STDERR, 
	        "%s: nickname is required to dump cert in raw or ascii mode.\n",
	           progName);
	return 255;
    }
    
    /*  -L can only be in (raw || ascii).  */
    if (certutil.commands[cmd_ListCerts].activated &&
        certutil.options[opt_ASCIIForIO].activated &&
        certutil.options[opt_BinaryDER].activated) {
	PR_fprintf(PR_STDERR, 
	           "%s: cannot specify both -r and -a when dumping cert.\n",
	           progName);
	return 255;
    }

    /*  For now, deny -C -x combination */
    if (certutil.commands[cmd_CreateNewCert].activated &&
        certutil.options[opt_SelfSign].activated) {
	PR_fprintf(PR_STDERR,
	           "%s: self-signing a cert request is not supported.\n",
	           progName);
	return 255;
    }

    /*  If making a cert request, need a subject.  */
    if ((certutil.commands[cmd_CertReq].activated ||
         certutil.commands[cmd_CreateAndAddCert].activated) &&
        !certutil.options[opt_Subject].activated) {
	PR_fprintf(PR_STDERR, 
	           "%s -%c: subject is required to create a cert request.\n",
	           progName, commandToRun);
	return 255;
    }

    /*  If making a cert, need a serial number.  */
    if ((certutil.commands[cmd_CreateNewCert].activated ||
         certutil.commands[cmd_CreateAndAddCert].activated) &&
         !certutil.options[opt_SerialNumber].activated) {
	/*  Make a default serial number from the current time.  */
	PRTime now = PR_Now();
	serialNumber = LL_L2UI(serialNumber, now);
    }

    /*  Validation needs the usage to validate for.  */
    if (certutil.commands[cmd_CheckCertValidity].activated &&
        !certutil.options[opt_Usage].activated) {
	PR_fprintf(PR_STDERR, 
	           "%s -V: specify a usage to validate the cert for (-u).\n",
	           progName);
	return 255;
    }
    
    /*  To make a cert, need either a issuer or to self-sign it.  */
    if (certutil.commands[cmd_CreateAndAddCert].activated &&
	!(certutil.options[opt_IssuerName].activated ||
          certutil.options[opt_SelfSign].activated)) {
	PR_fprintf(PR_STDERR,
	           "%s -S: must specify issuer (-c) or self-sign (-x).\n",
	           progName);
	return 255;
    }

    /*  Using slotname == NULL for listing keys and certs on all slots, 
     *  but only that. */
    if (!(certutil.commands[cmd_ListKeys].activated ||
    	  certutil.commands[cmd_ListCerts].activated) && slotname == NULL) {
	PR_fprintf(PR_STDERR,
	           "%s -%c: cannot use \"-h all\" for this command.\n",
	           progName, commandToRun);
	return 255;
    }

    /*  Using keytype == nullKey for list all key types, but only that.  */
    if (!certutil.commands[cmd_ListKeys].activated && keytype == nullKey) {
	PR_fprintf(PR_STDERR,
	           "%s -%c: cannot use \"-k all\" for this command.\n",
	           progName, commandToRun);
	return 255;
    }

    /*  -S  open outFile, temporary file for cert request.  */
    if (certutil.commands[cmd_CreateAndAddCert].activated) {
	outFile = PR_Open(certreqfile, PR_RDWR | PR_CREATE_FILE, 00660);
	if (!outFile) {
	    PR_fprintf(PR_STDERR, 
		       "%s -o: unable to open \"%s\" for writing (%ld, %ld)\n",
		       progName, certreqfile,
		       PR_GetError(), PR_GetOSError());
	    return 255;
	}
    }

    /*  Open the input file.  */
    if (certutil.options[opt_InputFile].activated) {
	inFile = PR_Open(certutil.options[opt_InputFile].arg, PR_RDONLY, 0);
	if (!inFile) {
	    PR_fprintf(PR_STDERR,
	               "%s:  unable to open \"%s\" for reading (%ld, %ld).\n",
	               progName, certutil.options[opt_InputFile].arg,
	               PR_GetError(), PR_GetOSError());
	    return 255;
	}
    }

    /*  Open the output file.  */
    if (certutil.options[opt_OutputFile].activated && !outFile) {
	outFile = PR_Open(certutil.options[opt_OutputFile].arg, 
                          PR_CREATE_FILE | PR_RDWR, 00660);
	if (!outFile) {
	    PR_fprintf(PR_STDERR,
	               "%s:  unable to open \"%s\" for writing (%ld, %ld).\n",
	               progName, certutil.options[opt_OutputFile].arg,
	               PR_GetError(), PR_GetOSError());
	    return 255;
	}
    }

    name = SECU_GetOptionArg(&certutil, opt_Nickname);

    PK11_SetPasswordFunc(SECU_GetModulePassword);

    /*  Initialize NSPR and NSS.  */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_Initialize(SECU_ConfigDirectory(NULL), certPrefix, certPrefix,
                        "secmod.db", 0);
    if (rv != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return 255;
    }
    certHandle = CERT_GetDefaultCertDB();

    if (certutil.commands[cmd_Version].activated) {
	int version = CERT_GetDBContentVersion(certHandle);
	printf("Certificate database content version:  %d\n", version);
    }

    if (PL_strcmp(slotname, "internal") == 0)
	slot = PK11_GetInternalKeySlot();
    else if (slotname != NULL)
	slot = PK11_FindSlotByName(slotname);

    /*  If creating new database, initialize the password.  */
    if (certutil.commands[cmd_NewDBs].activated) {
	SECU_ChangePW(slot, 0, certutil.options[opt_PasswordFile].arg);
    }

    /* The following 8 options are mutually exclusive with all others. */

    /*  List certs (-L)  */
    if (certutil.commands[cmd_ListCerts].activated) {
	rv = ListCerts(certHandle, name, slot,
	               certutil.options[opt_BinaryDER].activated,
	               certutil.options[opt_ASCIIForIO].activated, 
                       (outFile) ? outFile : PR_STDOUT, &pwdata);
	return rv ? 255 : 0;
    }
    /*  XXX needs work  */
    /*  List keys (-K)  */
    if (certutil.commands[cmd_ListKeys].activated) {
	rv = ListKeys(slot, name, 0 /*keyindex*/, keytype, PR_FALSE /*dopriv*/,
	              &pwdata);
	return rv ? 255 : 0;
    }
    /*  List modules (-U)  */
    if (certutil.commands[cmd_ListModules].activated) {
	rv = ListModules();
	return rv ? 255 : 0;
    }
    /*  Delete cert (-D)  */
    if (certutil.commands[cmd_DeleteCert].activated) {
	rv = DeleteCert(certHandle, name);
	return rv ? 255 : 0;
    }
    /*  Delete key (-F)  */
    if (certutil.commands[cmd_DeleteKey].activated) {
	rv = DeleteKey(name, &pwdata);
	return rv ? 255 : 0;
    }
    /*  Modify trust attribute for cert (-M)  */
    if (certutil.commands[cmd_ModifyCertTrust].activated) {
	rv = ChangeTrustAttributes(certHandle, name, 
	                           certutil.options[opt_Trust].arg);
	return rv ? 255 : 0;
    }
    /*  Change key db password (-W) (future - change pw to slot?)  */
    if (certutil.commands[cmd_ChangePassword].activated) {
	rv = SECU_ChangePW(slot, 0, certutil.options[opt_PasswordFile].arg);
	return rv ? 255 : 0;
    }
    /*  Reset the a token */
    if (certutil.commands[cmd_TokenReset].activated) {
	char *sso_pass = "";

	if (certutil.options[opt_SSOPass].activated) {
	    sso_pass = certutil.options[opt_SSOPass].arg;
 	}
	rv = PK11_ResetToken(slot,sso_pass);

 	return !rv - 1;
    }
    /*  Check cert validity against current time (-V)  */
    if (certutil.commands[cmd_CheckCertValidity].activated) {
	/* XXX temporary hack for fips - must log in to get priv key */
	if (certutil.options[opt_VerifySig].activated) {
	    if (PK11_NeedLogin(slot))
		PK11_Authenticate(slot, PR_TRUE, &pwdata);
	}
	rv = ValidateCert(certHandle, name, 
	                  certutil.options[opt_ValidityTime].arg,
			  certutil.options[opt_Usage].arg,
			  certutil.options[opt_VerifySig].activated,
			  certutil.options[opt_DetailedInfo].activated,
	                  &pwdata);
	return rv ? 255 : 0;
    }

    /*
     *  Key generation
     */

    /*  These commands require keygen.  */
    if (certutil.commands[cmd_CertReq].activated ||
        certutil.commands[cmd_CreateAndAddCert].activated ||
	certutil.commands[cmd_GenKeyPair].activated) {
	/*  XXX Give it a nickname.  */
	privkey = 
	    CERTUTIL_GeneratePrivateKey(keytype, slot, keysize,
	                                publicExponent, 
	                                certutil.options[opt_NoiseFile].arg,
	                                &pubkey, 
	                                certutil.options[opt_PQGFile].arg,
	                                &pwdata);
	if (privkey == NULL) {
	    SECU_PrintError(progName, "unable to generate key(s)\n");
	    return 255;
	}
	privkey->wincx = &pwdata;
	PORT_Assert(pubkey != NULL);

	/*  If all that was needed was keygen, exit.  */
	if (certutil.commands[cmd_GenKeyPair].activated) {
	    return SECSuccess;
	}
    }

    /*
     *  Certificate request
     */

    /*  Make a cert request (-R or -S).  */
    if (certutil.commands[cmd_CreateAndAddCert].activated ||
         certutil.commands[cmd_CertReq].activated) {
	rv = CertReq(privkey, pubkey, keytype, subject,
	             certutil.options[opt_PhoneNumber].arg,
	             certutil.options[opt_ASCIIForIO].activated,
		     outFile ? outFile : PR_STDOUT);
	if (rv) 
	    return 255;
	privkey->wincx = &pwdata;
    }

    /*
     *  Certificate creation
     */

    /*  If making and adding a cert, load the cert request file
     *  and output the cert to another file.
     */
    if (certutil.commands[cmd_CreateAndAddCert].activated) {
	PR_Close(outFile);
	inFile  = PR_Open(certreqfile, PR_RDONLY, 0);
	if (!inFile) {
	    PR_fprintf(PR_STDERR, "Failed to open file \"%s\" (%ld, %ld).\n",
                       certreqfile, PR_GetError(), PR_GetOSError());
	    return 255;
	}
	outFile = PR_Open(certfile, PR_RDWR | PR_CREATE_FILE, 00660);
	if (!outFile) {
	    PR_fprintf(PR_STDERR, "Failed to open file \"%s\" (%ld, %ld).\n",
                       certfile, PR_GetError(), PR_GetOSError());
	    return 255;
	}
    }

    /*  Create a certificate (-C or -S).  */
    if (certutil.commands[cmd_CreateAndAddCert].activated ||
         certutil.commands[cmd_CreateNewCert].activated) {
	rv = CreateCert(certHandle, 
	                certutil.options[opt_IssuerName].arg,
	                inFile, outFile, privkey, &pwdata,
	                serialNumber, warpmonths, validitylength,
	                certutil.options[opt_ASCIIForIO].activated,
	                certutil.options[opt_SelfSign].activated,
	                certutil.options[opt_AddKeyUsageExt].activated,
	                certutil.options[opt_AddExtKeyUsageExt].activated,
	                certutil.options[opt_AddBasicConstraintExt].activated,
	                certutil.options[opt_AddAuthorityKeyIDExt].activated,
	                certutil.options[opt_AddCRLDistPtsExt].activated,
	                certutil.options[opt_AddNSCertTypeExt].activated);
	if (rv) 
	    return 255;
    }

    /* 
     * Adding a cert to the database (or slot)
     */
 
    if (certutil.commands[cmd_CreateAndAddCert].activated) { 
	PR_Close(inFile);
	PR_Close(outFile);
	inFile = PR_Open(certfile, PR_RDONLY, 0);
	if (!inFile) {
	    PR_fprintf(PR_STDERR, "Failed to open file \"%s\" (%ld, %ld).\n",
                       certfile, PR_GetError(), PR_GetOSError());
	    return 255;
	}
    }

    if (certutil.commands[cmd_CreateAndAddCert].activated ||
         certutil.commands[cmd_AddCert].activated ||
	 certutil.commands[cmd_AddEmailCert].activated) {
	rv = AddCert(slot, certHandle, name, 
	             certutil.options[opt_Trust].arg,
	             inFile, 
	             certutil.options[opt_ASCIIForIO].activated,
	             certutil.commands[cmd_AddEmailCert].activated,&pwdata);
	if (rv) 
	    return 255;
    }

    if (certutil.commands[cmd_CreateAndAddCert].activated) {
	PR_Close(inFile);
	PR_Delete(certfile);
	PR_Delete(certreqfile);
    }

#ifdef notdef
    if ( certHandle ) {
	CERT_ClosePermCertDB(certHandle);
    }
#else
    NSS_Shutdown();
#endif

    return rv;  
}
