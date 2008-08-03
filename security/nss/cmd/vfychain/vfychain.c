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

/****************************************************************************
 *  Read in a cert chain from one or more files, and verify the chain for
 *  some usage.
 *                                                                          *
 *  This code was modified from other code also kept in the NSS directory.
 ****************************************************************************/ 

#include <stdio.h>
#include <string.h>

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include "prerror.h"

#include "pk11func.h"
#include "seccomon.h"
#include "secutil.h"
#include "secmod.h"
#include "secitem.h"
#include "cert.h"
#include "ocsp.h"


/* #include <stdlib.h> */
/* #include <errno.h> */
/* #include <fcntl.h> */
/* #include <stdarg.h> */

#include "nspr.h"
#include "plgetopt.h"
#include "prio.h"
#include "nss.h"

/* #include "vfyutil.h" */

#define RD_BUF_SIZE (60 * 1024)

int verbose;

char *password = NULL;

/* Function: char * myPasswd()
 * 
 * Purpose: This function is our custom password handler that is called by
 * SSL when retreiving private certs and keys from the database. Returns a
 * pointer to a string that with a password for the database. Password pointer
 * should point to dynamically allocated memory that will be freed later.
 */
char *
myPasswd(PK11SlotInfo *info, PRBool retry, void *arg)
{
    char * passwd = NULL;

    if ( (!retry) && arg ) {
	passwd = PORT_Strdup((char *)arg);
    }
    return passwd;
}

static void
Usage(const char *progName)
{
    fprintf(stderr, 
	"Usage: %s [options] certfile [[options] certfile] ...\n"
	"\twhere options are:\n"
	"\t-a\t\t Following certfile is base64 encoded\n"
	"\t-b YYMMDDHHMMZ\t Validate date (default: now)\n"
	"\t-d directory\t Database directory\n"
	"\t-o oid\t\t Set policy OID for cert validation(Format OID.1.2.3)\n"
	"\t-p \t\t Use PKIX Library to validate certificate by calling:\n"
	"\t\t\t   * CERT_VerifyCertificate if specified once,\n"
	"\t\t\t   * CERT_PKIXVerifyCert if specified twice and more.\n"
	"\t-r\t\t Following certfile is raw binary DER (default)\n"
        "\t-s\t\t Status checking, following a configuration description.\n"
        "\t\t\t Implemented as of today are:\n"
        "\t\t\t   * allow-crl (default)\n"
        "\t\t\t   * allow-crl-and-ocsp\n"
	"\t-u usage \t 0=SSL client, 1=SSL server, 2=SSL StepUp, 3=SSL CA,\n"
	"\t\t\t 4=Email signer, 5=Email recipient, 6=Object signer,\n"
	"\t\t\t 9=ProtectedObjectSigner, 10=OCSP responder, 11=Any CA\n"
	"\t-v\t\t Verbose mode. Prints root cert subject(double the\n"
         "\t\t\t argument for whole root cert info)\n"
	"\t-w password\t Database password\n",
	progName);
    exit(1);
}

/**************************************************************************
** 
** Error and information routines.
**
**************************************************************************/

void
errWarn(char *function)
{
    PRErrorCode  errorNumber = PR_GetError();
    const char * errorString = SECU_Strerror(errorNumber);

    fprintf(stderr, "Error in function %s: %d\n - %s\n",
		    function, errorNumber, errorString);
}

void
exitErr(char *function)
{
    errWarn(function);
    /* Exit gracefully. */
    /* ignoring return value of NSS_Shutdown as code exits with 1 anyway*/
    (void) NSS_Shutdown();
    PR_Cleanup();
    exit(1);
}

typedef struct certMemStr {
    struct certMemStr * next;
    CERTCertificate * cert;
} certMem;

certMem * theCerts;
CERTCertList *trustedCertList;

void
rememberCert(CERTCertificate * cert, PRBool trusted)
{
    if (trusted) {
        if (!trustedCertList) {
            trustedCertList = CERT_NewCertList();
        }
        CERT_AddCertToListTail(trustedCertList, cert);
    } else {
        certMem * newCertMem = PORT_ZNew(certMem);
        if (newCertMem) {
            newCertMem->next = theCerts;
            newCertMem->cert = cert;
            theCerts = newCertMem;
        }
    }
}

void
forgetCerts(void)
{
    certMem * oldCertMem;
    while (theCerts) {
	oldCertMem = theCerts;
    	theCerts = theCerts->next;
	CERT_DestroyCertificate(oldCertMem->cert);
	PORT_Free(oldCertMem);
    }
    if (trustedCertList) {
        CERT_DestroyCertList(trustedCertList);
    }
}


CERTCertificate *
getCert(const char *name, PRBool isAscii)
{
    unsigned char * pb;
    CERTCertificate * cert  = NULL;
    CERTCertDBHandle *defaultDB = NULL;
    PRFileDesc*     fd;
    PRInt32         cc      = -1;
    PRInt32         total;
    PRInt32         remaining;
    SECItem         item;
    static unsigned char certBuf[RD_BUF_SIZE];

    defaultDB = CERT_GetDefaultCertDB();

    /* First, let's try to find the cert in existing DB. */
    cert = CERT_FindCertByNicknameOrEmailAddr(defaultDB, name);
    if (cert) {
        return cert;
    }

    /* Don't have a cert with name "name" in the DB. Try to
     * open a file with such name and get the cert from there.*/
    fd = PR_Open(name, PR_RDONLY, 0777); 
    if (!fd) {
	PRIntn err = PR_GetError();
    	fprintf(stderr, "open of %s failed, %d = %s\n", 
	        name, err, SECU_Strerror(err));
	return cert;
    }
    /* read until EOF or buffer is full */
    pb = certBuf;
    while (0 < (remaining = (sizeof certBuf) - (pb - certBuf))) {
	cc = PR_Read(fd, pb, remaining);
	if (cc == 0) 
	    break;
	if (cc < 0) {
	    PRIntn err = PR_GetError();
	    fprintf(stderr, "read of %s failed, %d = %s\n", 
	        name, err, SECU_Strerror(err));
	    break;
	}
	/* cc > 0 */
	pb += cc;
    }
    PR_Close(fd);
    if (cc < 0)
    	return cert;
    if (!remaining || cc > 0) { /* file was too big. */
	fprintf(stderr, "cert file %s was too big.\n", name);
	return cert;
    }
    total = pb - certBuf;
    if (!total) { /* file was empty */
	fprintf(stderr, "cert file %s was empty.\n", name);
	return cert;
    }
    if (isAscii) {
    	/* convert from Base64 to binary here ... someday */
    }
    item.type = siBuffer;
    item.data = certBuf;
    item.len  = total;
    cert = CERT_NewTempCertificate(defaultDB, &item, 
                                   NULL     /* nickname */, 
                                   PR_FALSE /* isPerm */, 
				   PR_TRUE  /* copyDER */);
    if (!cert) {
	PRIntn err = PR_GetError();
	fprintf(stderr, "couldn't import %s, %d = %s\n",
	        name, err, SECU_Strerror(err));
    }
    return cert;
}

#define REVCONFIG_ALLOW_CRL "allow-crl"
#define REVCONFIG_ALLOW_CRL_OCSP "allow-crl-and-ocsp"

PRBool
isAllowedRevConfig(const char *name)
{
    if (strcmp(REVCONFIG_ALLOW_CRL, name) == 0)
        return PR_TRUE;

    if (strcmp(REVCONFIG_ALLOW_CRL_OCSP, name) == 0)
        return PR_TRUE;

    return PR_FALSE;
}

int
main(int argc, char *argv[], char *envp[])
{
    char *               certDir      = NULL;
    char *               progName     = NULL;
    char *               oidStr       = NULL;
    CERTCertificate *    cert;
    CERTCertificate *    firstCert    = NULL;
    CERTCertificate *    issuerCert   = NULL;
    CERTCertDBHandle *   defaultDB    = NULL;
    PRBool               isAscii      = PR_FALSE;
    PRBool               trusted      = PR_FALSE;
    SECStatus            secStatus;
    SECCertificateUsage  certUsage    = certificateUsageSSLServer;
    PLOptState *         optstate;
    PRTime               time         = 0;
    PLOptStatus          status;
    int                  usePkix      = 0;
    int                  rv           = 1;
    int                  usage;
    CERTVerifyLog        log;
    CERTCertList        *builtChain = NULL;
    char *               revConfig    = NULL;

    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    progName = PL_strdup(argv[0]);

    optstate = PL_CreateOptState(argc, argv, "ab:d:o:prs:tu:w:v");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch(optstate->option) {
	case  0  : /* positional parameter */  goto breakout;
	case 'a' : isAscii  = PR_TRUE;                        break;
	case 'b' : secStatus = DER_AsciiToTime(&time, optstate->value);
	           if (secStatus != SECSuccess) Usage(progName); break;
	case 'd' : certDir  = PL_strdup(optstate->value);     break;
	case 'o' : oidStr = PL_strdup(optstate->value);       break;
	case 'p' : usePkix += 1;                              break;
	case 'r' : isAscii  = PR_FALSE;                       break;
        case 's' : revConfig  = PL_strdup(optstate->value);   break;
	case 'u' : usage    = PORT_Atoi(optstate->value);
	           if (usage < 0 || usage > 62) Usage(progName);
		   certUsage = ((SECCertificateUsage)1) << usage; 
		   if (certUsage > certificateUsageHighest) Usage(progName);
		   break;
	case 'w' : password = PL_strdup(optstate->value);     break;
	case 'v' : verbose++;                                 break;
	default  : Usage(progName);                           break;
	}
    }
breakout:
    if (status != PL_OPT_OK)
	Usage(progName);

    if (revConfig && !isAllowedRevConfig(revConfig)) {
        fprintf(stderr, "Invalid revocation configuration specified.\n");
        goto punt;
    }

    /* Set our password function callback. */
    PK11_SetPasswordFunc(myPasswd);

    /* Initialize the NSS libraries. */
    if (certDir) {
	secStatus = NSS_Init(certDir);
    } else {
	secStatus = NSS_NoDB_Init(NULL);

	/* load the builtins */
	SECMOD_AddNewModule("Builtins", DLL_PREFIX"nssckbi."DLL_SUFFIX, 0, 0);
    }
    if (secStatus != SECSuccess) {
	exitErr("NSS_Init");
    }
    SECU_RegisterDynamicOids();
    if (revConfig && strcmp(REVCONFIG_ALLOW_CRL_OCSP, revConfig) == 0) {
        CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
        CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
    }

    while (status == PL_OPT_OK) {
	switch(optstate->option) {
	default  : Usage(progName);                           break;
	case 'a' : isAscii  = PR_TRUE;                        break;
	case 'r' : isAscii  = PR_FALSE;                       break;
	case 't' : trusted  = PR_TRUE;                       break;
	case  0  : /* positional parameter */
	    cert = getCert(optstate->value, isAscii);
	    if (!cert) 
	        goto punt;
	    rememberCert(cert, trusted);
	    if (!firstCert)
	        firstCert = cert;
            trusted = PR_FALSE;
	}
        status = PL_GetNextOpt(optstate);
    }
    PL_DestroyOptState(optstate);
    if (status == PL_OPT_BAD || !firstCert)
	Usage(progName);

    if (!time)
    	time = PR_Now();

    /* Initialize log structure */
    log.arena = PORT_NewArena(512);
    log.head = log.tail = NULL;
    log.count = 0;

    if (usePkix < 2) {
        /* NOW, verify the cert chain. */
        if (usePkix) {
            /* Use old API with libpkix validation lib */
            CERT_SetUsePKIXForValidation(PR_TRUE);
        }
        defaultDB = CERT_GetDefaultCertDB();
        secStatus = CERT_VerifyCertificate(defaultDB, firstCert, 
                                           PR_TRUE /* check sig */,
                                           certUsage, 
                                           time, 
                                           NULL, /* wincx  */
                                           &log, /* error log */
                                           NULL);/* returned usages */
    } else do {
        CERTValOutParam cvout[4];
        CERTValInParam cvin[5];
        SECOidTag oidTag;
        int inParamIndex = 0;
        CERTRevocationFlags rev;
        PRUint64 revFlags[2];

        if (oidStr) {
            PRArenaPool *arena;
            SECOidData od;
            memset(&od, 0, sizeof od);
            od.offset = SEC_OID_UNKNOWN;
            od.desc = "User Defined Policy OID";
            od.mechanism = CKM_INVALID_MECHANISM;
            od.supportedExtension = INVALID_CERT_EXTENSION;

            arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
            if ( !arena ) {
                fprintf(stderr, "out of memory");
                goto punt;
            }
            
            secStatus = SEC_StringToOID(arena, &od.oid, oidStr, 0);
            if (secStatus != SECSuccess) {
                PORT_FreeArena(arena, PR_FALSE);
                fprintf(stderr, "Can not encode oid: %s(%s)\n", oidStr,
                        SECU_Strerror(PORT_GetError()));
                break;
            }
            
            oidTag = SECOID_AddEntry(&od);
            PORT_FreeArena(arena, PR_FALSE);
            if (oidTag == SEC_OID_UNKNOWN) {
                fprintf(stderr, "Can not add new oid to the dynamic "
                        "table: %s\n", oidStr);
                secStatus = SECFailure;
                break;
            }

            cvin[inParamIndex].type = cert_pi_policyOID;
            cvin[inParamIndex].value.arraySize = 1;
            cvin[inParamIndex].value.array.oids = &oidTag;

            inParamIndex++;
        }

        if (trustedCertList) {
            cvin[inParamIndex].type = cert_pi_trustAnchors;
            cvin[inParamIndex].value.pointer.chain = trustedCertList;
            
            inParamIndex++;
        }

	cvin[inParamIndex].type = cert_pi_date;
	cvin[inParamIndex].value.scalar.time = time;
	inParamIndex++;

        revFlags[cert_revocation_method_crl] = 
            CERT_REV_M_TEST_USING_THIS_METHOD;
        rev.leafTests.number_of_defined_methods = 
            cert_revocation_method_crl +1;
        rev.chainTests.number_of_defined_methods = 
            cert_revocation_method_crl +1;

        if (revConfig && strcmp(REVCONFIG_ALLOW_CRL_OCSP, revConfig) == 0) {
            revFlags[cert_revocation_method_ocsp] = 
                CERT_REV_M_TEST_USING_THIS_METHOD;
            rev.leafTests.number_of_defined_methods = 
                cert_revocation_method_ocsp +1;
            rev.chainTests.number_of_defined_methods = 
                cert_revocation_method_ocsp +1;
        }

        rev.leafTests.cert_rev_flags_per_method = revFlags;
        rev.leafTests.number_of_preferred_methods = 0;
        rev.leafTests.preferred_methods = 0;
        rev.leafTests.cert_rev_method_independent_flags = 0;
      
        rev.chainTests.cert_rev_flags_per_method = revFlags;
        rev.chainTests.number_of_preferred_methods = 0;
        rev.chainTests.preferred_methods = 0;
        rev.chainTests.cert_rev_method_independent_flags = 0;

        cvin[inParamIndex].type = cert_pi_revocationFlags;
        cvin[inParamIndex].value.pointer.revocation = &rev;
	inParamIndex++;

        cvin[inParamIndex].type = cert_pi_end;
        
        cvout[0].type = cert_po_trustAnchor;
        cvout[1].type = cert_po_certList;

        /* setting pointer to CERTVerifyLog. Initialized structure
         * will be used CERT_PKIXVerifyCert */
        cvout[2].type = cert_po_errorLog;
        cvout[2].value.pointer.log = &log;

        cvout[3].type = cert_po_end;
        
        secStatus = CERT_PKIXVerifyCert(firstCert, certUsage,
                                        cvin, cvout, NULL);
        if (secStatus != SECSuccess) {
            break;
        }
        issuerCert = cvout[0].value.pointer.cert;
        builtChain = cvout[1].value.pointer.chain;
    } while (0);

    /* Display validation results */
    if (secStatus != SECSuccess || log.count > 0) {
	CERTVerifyLogNode *node = NULL;
	PRIntn err = PR_GetError();
	fprintf(stderr, "Chain is bad, %d = %s\n", err, SECU_Strerror(err));

	SECU_displayVerifyLog(stderr, &log, verbose); 
	/* Have cert refs in the log only in case of failure.
	 * Destroy them. */
	for (node = log.head; node; node = node->next) {
	    if (node->cert)
	        CERT_DestroyCertificate(node->cert);
	}
    	rv = 1;
    } else {
    	fprintf(stderr, "Chain is good!\n");
    	if (issuerCert) {
    	   if (verbose > 1) {
               rv = SEC_PrintCertificateAndTrust(issuerCert, "Root Certificate",
                                                 NULL);
               if (rv != SECSuccess) {
		 SECU_PrintError(progName, "problem printing certificate");
               }
    	   } else if (verbose > 0) {
    	      SECU_PrintName(stdout, &issuerCert->subject, "Root "
                             "Certificate Subject:", 0);
    	   }
    	   CERT_DestroyCertificate(issuerCert);
    	}
         if (builtChain) {
            CERTCertListNode *node;
            int count = 0;
            char buff[256];

            if (verbose) { 
                for(node = CERT_LIST_HEAD(builtChain); !CERT_LIST_END(node, builtChain);
                    node = CERT_LIST_NEXT(node), count++ ) {
                    sprintf(buff, "Certificate %d Subject", count + 1);
                    SECU_PrintName(stdout, &node->cert->subject, buff, 0);
                }
            }
            CERT_DestroyCertList(builtChain);
         }
	rv = 0;
    }

    /* Need to destroy CERTVerifyLog arena at the end */
    PORT_FreeArena(log.arena, PR_FALSE);

punt:
    forgetCerts();
    if (NSS_Shutdown() != SECSuccess) {
	SECU_PrintError(progName, "NSS_Shutdown");
	rv = 1;
    }
    PR_Cleanup();
    return rv;
}
