/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

secuPWData  pwdata          = { PW_NONE, 0 };

static void
Usage(const char *progName)
{
    fprintf(stderr, 
	"Usage: %s [options] [revocation options] certfile "
            "[[options] certfile] ...\n"
	"\tWhere options are:\n"
	"\t-a\t\t Following certfile is base64 encoded\n"
	"\t-b YYMMDDHHMMZ\t Validate date (default: now)\n"
	"\t-d directory\t Database directory\n"
	"\t-i number of consecutive verifications\n"
	"\t-f \t\t Enable cert fetching from AIA URL\n"
	"\t-o oid\t\t Set policy OID for cert validation(Format OID.1.2.3)\n"
	"\t-p \t\t Use PKIX Library to validate certificate by calling:\n"
	"\t\t\t   * CERT_VerifyCertificate if specified once,\n"
	"\t\t\t   * CERT_PKIXVerifyCert if specified twice and more.\n"
	"\t-r\t\t Following certfile is raw binary DER (default)\n"
        "\t-t\t\t Following cert is explicitly trusted (overrides db trust).\n"
	"\t-u usage \t 0=SSL client, 1=SSL server, 2=SSL StepUp, 3=SSL CA,\n"
	"\t\t\t 4=Email signer, 5=Email recipient, 6=Object signer,\n"
	"\t\t\t 9=ProtectedObjectSigner, 10=OCSP responder, 11=Any CA\n"
	"\t-T\t\t Trust both explicit trust anchors (-t) and the database.\n"
	"\t\t\t (Default is to only trust certificates marked -t, if there are any,\n"
	"\t\t\t or to trust the database if there are certificates marked -t.)\n"
	"\t-v\t\t Verbose mode. Prints root cert subject(double the\n"
	"\t\t\t argument for whole root cert info)\n"
	"\t-w password\t Database password.\n"
	"\t-W pwfile\t Password file.\n\n"
        "\tRevocation options for PKIX API(invoked with -pp options) is a\n"
        "\tcollection of the following flags:\n"
        "\t\t[-g type [-h flags] [-m type [-s flags]] ...] ...\n"
        "\tWhere:\n"
        "\t-g test type\t Sets status checking test type. Possible values\n"
        "\t\t\tare \"leaf\" or \"chain\"\n"
        "\t-h test flags\t Sets revocation flags for the test type it\n"
        "\t\t\tfollows. Possible flags: \"testLocalInfoFirst\" and\n"
        "\t\t\t\"requireFreshInfo\".\n"
        "\t-m method type\t Sets method type for the test type it follows.\n"
        "\t\t\tPossible types are \"crl\" and \"ocsp\".\n"
        "\t-s method flags\t Sets revocation flags for the method it follows.\n"
        "\t\t\tPossible types are \"doNotUse\", \"forbidFetching\",\n"
        "\t\t\t\"ignoreDefaultSrc\", \"requireInfo\" and \"failIfNoInfo\".\n",
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
    fprintf(stderr, "Error in function %s: %s\n",
		    function, SECU_Strerror(PR_GetError()));
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
getCert(const char *name, PRBool isAscii, const char * progName)
{
    CERTCertificate * cert;
    CERTCertDBHandle *defaultDB;
    PRFileDesc*     fd;
    SECStatus       rv;
    SECItem         item        = {0, NULL, 0};

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
	PRErrorCode err = PR_GetError();
    	fprintf(stderr, "open of %s failed, %d = %s\n", 
	        name, err, SECU_Strerror(err));
	return cert;
    }

    rv = SECU_ReadDERFromFile(&item, fd, isAscii, PR_FALSE);
    PR_Close(fd);
    if (rv != SECSuccess) {
	fprintf(stderr, "%s: SECU_ReadDERFromFile failed\n", progName);
	return cert;
    }

    if (!item.len) { /* file was empty */
	fprintf(stderr, "cert file %s was empty.\n", name);
	return cert;
    }

    cert = CERT_NewTempCertificate(defaultDB, &item, 
                                   NULL     /* nickname */, 
                                   PR_FALSE /* isPerm */, 
				   PR_TRUE  /* copyDER */);
    if (!cert) {
	PRErrorCode err = PR_GetError();
	fprintf(stderr, "couldn't import %s, %d = %s\n",
	        name, err, SECU_Strerror(err));
    }
    PORT_Free(item.data);
    return cert;
}


#define REVCONFIG_TEST_UNDEFINED      0
#define REVCONFIG_TEST_LEAF           1
#define REVCONFIG_TEST_CHAIN          2
#define REVCONFIG_METHOD_CRL          1
#define REVCONFIG_METHOD_OCSP         2

#define REVCONFIG_TEST_LEAF_STR       "leaf"
#define REVCONFIG_TEST_CHAIN_STR      "chain"
#define REVCONFIG_METHOD_CRL_STR      "crl"
#define REVCONFIG_METHOD_OCSP_STR     "ocsp"

#define REVCONFIG_TEST_TESTLOCALINFOFIRST_STR     "testLocalInfoFirst"
#define REVCONFIG_TEST_REQUIREFRESHINFO_STR       "requireFreshInfo"
#define REVCONFIG_METHOD_DONOTUSEMETHOD_STR       "doNotUse"
#define REVCONFIG_METHOD_FORBIDNETWORKFETCHIN_STR "forbidFetching"
#define REVCONFIG_METHOD_IGNOREDEFAULTSRC_STR     "ignoreDefaultSrc"
#define REVCONFIG_METHOD_REQUIREINFO_STR          "requireInfo"
#define REVCONFIG_METHOD_FAILIFNOINFO_STR         "failIfNoInfo" 

#define REV_METHOD_INDEX_MAX  4

typedef struct RevMethodsStruct {
    unsigned int testType;
    char *testTypeStr;
    unsigned int testFlags;
    char *testFlagsStr;
    unsigned int methodType;
    char *methodTypeStr;
    unsigned int methodFlags;
    char *methodFlagsStr;
} RevMethods;

RevMethods revMethodsData[REV_METHOD_INDEX_MAX];

SECStatus
parseRevMethodsAndFlags()
{
    int i;
    unsigned int testType = 0;

    for(i = 0;i < REV_METHOD_INDEX_MAX;i++) {
        /* testType */
        if (revMethodsData[i].testTypeStr) {
            char *typeStr = revMethodsData[i].testTypeStr;

            testType = 0;
            if (!PORT_Strcmp(typeStr, REVCONFIG_TEST_LEAF_STR)) {
                testType = REVCONFIG_TEST_LEAF;
            } else if (!PORT_Strcmp(typeStr, REVCONFIG_TEST_CHAIN_STR)) {
                testType = REVCONFIG_TEST_CHAIN;
            }
        }
        if (!testType) {
            return SECFailure;
        }
        revMethodsData[i].testType = testType;
        /* testFlags */
        if (revMethodsData[i].testFlagsStr) {
            char *flagStr = revMethodsData[i].testFlagsStr;
            unsigned int testFlags = 0;

            if (PORT_Strstr(flagStr, REVCONFIG_TEST_TESTLOCALINFOFIRST_STR)) {
                testFlags |= CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST;
            } 
            if (PORT_Strstr(flagStr, REVCONFIG_TEST_REQUIREFRESHINFO_STR)) {
                testFlags |= CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE;
            }
            revMethodsData[i].testFlags = testFlags;
        }
        /* method type */
        if (revMethodsData[i].methodTypeStr) {
            char *methodStr = revMethodsData[i].methodTypeStr;
            unsigned int methodType = 0;
            
            if (!PORT_Strcmp(methodStr, REVCONFIG_METHOD_CRL_STR)) {
                methodType = REVCONFIG_METHOD_CRL;
            } else if (!PORT_Strcmp(methodStr, REVCONFIG_METHOD_OCSP_STR)) {
                methodType = REVCONFIG_METHOD_OCSP;
            }
            if (!methodType) {
                return SECFailure;
            }
            revMethodsData[i].methodType = methodType;
        }
        if (!revMethodsData[i].methodType) {
            revMethodsData[i].testType = REVCONFIG_TEST_UNDEFINED;
            continue;
        }
        /* method flags */
        if (revMethodsData[i].methodFlagsStr) {
            char *flagStr = revMethodsData[i].methodFlagsStr;
            unsigned int methodFlags = 0;

            if (!PORT_Strstr(flagStr, REVCONFIG_METHOD_DONOTUSEMETHOD_STR)) {
                methodFlags |= CERT_REV_M_TEST_USING_THIS_METHOD;
            } 
            if (PORT_Strstr(flagStr,
                            REVCONFIG_METHOD_FORBIDNETWORKFETCHIN_STR)) {
                methodFlags |= CERT_REV_M_FORBID_NETWORK_FETCHING;
            }
            if (PORT_Strstr(flagStr, REVCONFIG_METHOD_IGNOREDEFAULTSRC_STR)) {
                methodFlags |= CERT_REV_M_IGNORE_IMPLICIT_DEFAULT_SOURCE;
            }
            if (PORT_Strstr(flagStr, REVCONFIG_METHOD_REQUIREINFO_STR)) {
                methodFlags |= CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE;
            }
            if (PORT_Strstr(flagStr, REVCONFIG_METHOD_FAILIFNOINFO_STR)) {
                methodFlags |= CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO;
            }
            revMethodsData[i].methodFlags = methodFlags;
        } else {
            revMethodsData[i].methodFlags |= CERT_REV_M_TEST_USING_THIS_METHOD;
        }
    }
    return SECSuccess;
}

SECStatus
configureRevocationParams(CERTRevocationFlags *flags)
{
   int i;
   unsigned int testType = REVCONFIG_TEST_UNDEFINED;
   static CERTRevocationTests *revTests = NULL;
   PRUint64 *revFlags;

   for(i = 0;i < REV_METHOD_INDEX_MAX;i++) {
       if (revMethodsData[i].testType == REVCONFIG_TEST_UNDEFINED) {
           continue;
       }
       if (revMethodsData[i].testType != testType) {
           testType = revMethodsData[i].testType;
           if (testType == REVCONFIG_TEST_CHAIN) {
               revTests = &flags->chainTests;
           } else {
               revTests = &flags->leafTests;
           }
           revTests->number_of_preferred_methods = 0;
           revTests->preferred_methods = 0;
           revFlags = revTests->cert_rev_flags_per_method;
       }
       /* Set the number of the methods independently to the max number of
        * methods. If method flags are not set it will be ignored due to
        * default DO_NOT_USE flag. */
       revTests->number_of_defined_methods = cert_revocation_method_count;
       revTests->cert_rev_method_independent_flags |=
           revMethodsData[i].testFlags;
       if (revMethodsData[i].methodType == REVCONFIG_METHOD_CRL) {
           revFlags[cert_revocation_method_crl] =
               revMethodsData[i].methodFlags;
       } else if (revMethodsData[i].methodType == REVCONFIG_METHOD_OCSP) {
           revFlags[cert_revocation_method_ocsp] =
               revMethodsData[i].methodFlags;
       }
   }
   return SECSuccess;
}

void
freeRevocationMethodData()
{
    int i = 0;
    for(;i < REV_METHOD_INDEX_MAX;i++) {
        if (revMethodsData[i].testTypeStr) {
            PORT_Free(revMethodsData[i].testTypeStr);
        }
        if (revMethodsData[i].testFlagsStr) {
            PORT_Free(revMethodsData[i].testFlagsStr);
        }
        if (revMethodsData[i].methodTypeStr) {
            PORT_Free(revMethodsData[i].methodTypeStr);
        }
        if (revMethodsData[i].methodFlagsStr) {
            PORT_Free(revMethodsData[i].methodFlagsStr);
        }
    }
}

PRBool
isOCSPEnabled()
{
    int i;

    for(i = 0;i < REV_METHOD_INDEX_MAX;i++) {
        if (revMethodsData[i].methodType == REVCONFIG_METHOD_OCSP) {
            return PR_TRUE;
        }
    }
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
    PRBool               certFetching = PR_FALSE;
    int                  revDataIndex = 0;
    PRBool               ocsp_fetchingFailureIsAFailure = PR_TRUE;
    PRBool               useDefaultRevFlags = PR_TRUE;
    PRBool               onlyTrustAnchors = PR_TRUE;
    int                  vfyCounts = 1;

    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    progName = PL_strdup(argv[0]);

    optstate = PL_CreateOptState(argc, argv, "ab:c:d:efg:h:i:m:o:prs:tTu:vw:W:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch(optstate->option) {
	case  0  : /* positional parameter */  goto breakout;
	case 'a' : isAscii  = PR_TRUE;                        break;
	case 'b' : secStatus = DER_AsciiToTime(&time, optstate->value);
	           if (secStatus != SECSuccess) Usage(progName); break;
	case 'd' : certDir  = PL_strdup(optstate->value);     break;
	case 'e' : ocsp_fetchingFailureIsAFailure = PR_FALSE;  break;
	case 'f' : certFetching = PR_TRUE;                    break;
	case 'g' : 
                   if (revMethodsData[revDataIndex].testTypeStr ||
                       revMethodsData[revDataIndex].methodTypeStr) {
                       revDataIndex += 1;
                       if (revDataIndex == REV_METHOD_INDEX_MAX) {
                           fprintf(stderr, "Invalid revocation configuration"
                                   "specified.\n");
                           secStatus = SECFailure;
                           break;
                       }
                   }
                   useDefaultRevFlags = PR_FALSE;
                   revMethodsData[revDataIndex].
                       testTypeStr = PL_strdup(optstate->value); break;
	case 'h' : 
                   revMethodsData[revDataIndex].
                       testFlagsStr = PL_strdup(optstate->value);break;
        case 'i' : vfyCounts = PORT_Atoi(optstate->value);       break;
                   break;
	case 'm' : 
                   if (revMethodsData[revDataIndex].methodTypeStr) {
                       revDataIndex += 1;
                       if (revDataIndex == REV_METHOD_INDEX_MAX) {
                           fprintf(stderr, "Invalid revocation configuration"
                                   "specified.\n");
                           secStatus = SECFailure;
                           break;
                       }
                   }
                   useDefaultRevFlags = PR_FALSE;
                   revMethodsData[revDataIndex].
                       methodTypeStr = PL_strdup(optstate->value); break;
	case 'o' : oidStr = PL_strdup(optstate->value);       break;
	case 'p' : usePkix += 1;                              break;
	case 'r' : isAscii  = PR_FALSE;                       break;
	case 's' : 
                   revMethodsData[revDataIndex].
                       methodFlagsStr = PL_strdup(optstate->value); break;
	case 't' : trusted  = PR_TRUE;                        break;
	case 'T' : onlyTrustAnchors = PR_FALSE;               break;
	case 'u' : usage    = PORT_Atoi(optstate->value);
	           if (usage < 0 || usage > 62) Usage(progName);
		   certUsage = ((SECCertificateUsage)1) << usage; 
		   if (certUsage > certificateUsageHighest) Usage(progName);
		   break;
        case 'w':
                  pwdata.source = PW_PLAINTEXT;
                  pwdata.data = PORT_Strdup(optstate->value);
                  break;

        case 'W':
                  pwdata.source = PW_FROMFILE;
                  pwdata.data = PORT_Strdup(optstate->value);
                  break;
	case 'v' : verbose++;                                 break;
	default  : Usage(progName);                           break;
	}
    }
breakout:
    if (status != PL_OPT_OK)
	Usage(progName);

    if (usePkix < 2) {
        if (oidStr) {
            fprintf(stderr, "Policy oid(-o) can be used only with"
                    " CERT_PKIXVerifyCert(-pp) function.\n");
            Usage(progName);
        }
        if (trusted) {
            fprintf(stderr, "Cert trust flag can be used only with"
                    " CERT_PKIXVerifyCert(-pp) function.\n");
            Usage(progName);
        }
        if (!onlyTrustAnchors) {
            fprintf(stderr, "Cert trust anchor exclusiveness can be"
                    " used only with CERT_PKIXVerifyCert(-pp)"
                    " function.\n");
        }
    }

    if (!useDefaultRevFlags && parseRevMethodsAndFlags()) {
        fprintf(stderr, "Invalid revocation configuration specified.\n");
        goto punt;
    }

    /* Set our password function callback. */
    PK11_SetPasswordFunc(SECU_GetModulePassword);

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
    if (isOCSPEnabled()) {
        CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
        CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
        if (!ocsp_fetchingFailureIsAFailure) {
            CERT_SetOCSPFailureMode(ocspMode_FailureIsNotAVerificationFailure);
        }
    }

    while (status == PL_OPT_OK) {
	switch(optstate->option) {
	default  : Usage(progName);                           break;
	case 'a' : isAscii  = PR_TRUE;                        break;
	case 'r' : isAscii  = PR_FALSE;                       break;
	case 't' : trusted  = PR_TRUE;                       break;
	case  0  : /* positional parameter */
            if (usePkix < 2 && trusted) {
                fprintf(stderr, "Cert trust flag can be used only with"
                        " CERT_PKIXVerifyCert(-pp) function.\n");
                Usage(progName);
            }
	    cert = getCert(optstate->value, isAscii, progName);
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

    /* Initialize log structure */
    log.arena = PORT_NewArena(512);
    log.head = log.tail = NULL;
    log.count = 0;

    do {
        if (usePkix < 2) {
            /* NOW, verify the cert chain. */
            if (usePkix) {
                /* Use old API with libpkix validation lib */
                CERT_SetUsePKIXForValidation(PR_TRUE);
            }
            if (!time)
                time = PR_Now();

            defaultDB = CERT_GetDefaultCertDB();
            secStatus = CERT_VerifyCertificate(defaultDB, firstCert, 
                                               PR_TRUE /* check sig */,
                                               certUsage, 
                                               time,
                                               &pwdata, /* wincx  */
                                               &log, /* error log */
                                           NULL);/* returned usages */
        } else do {
                static CERTValOutParam cvout[4];
                static CERTValInParam cvin[7];
                SECOidTag oidTag;
                int inParamIndex = 0;
                static PRUint64 revFlagsLeaf[2];
                static PRUint64 revFlagsChain[2];
                static CERTRevocationFlags rev;
                
                if (oidStr) {
                    PLArenaPool *arena;
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
                
                cvin[inParamIndex].type = cert_pi_useAIACertFetch;
                cvin[inParamIndex].value.scalar.b = certFetching;
                inParamIndex++;
                
                rev.leafTests.cert_rev_flags_per_method = revFlagsLeaf;
                rev.chainTests.cert_rev_flags_per_method = revFlagsChain;
                secStatus = configureRevocationParams(&rev);
                if (secStatus) {
                    fprintf(stderr, "Can not config revocation parameters ");
                    break;
                }
                
                cvin[inParamIndex].type = cert_pi_revocationFlags;
                cvin[inParamIndex].value.pointer.revocation = &rev;
                inParamIndex++;
                
                if (time) {
                    cvin[inParamIndex].type = cert_pi_date;
                    cvin[inParamIndex].value.scalar.time = time;
                    inParamIndex++;
                }

                if (!onlyTrustAnchors) {
                    cvin[inParamIndex].type = cert_pi_useOnlyTrustAnchors;
                    cvin[inParamIndex].value.scalar.b = onlyTrustAnchors;
                    inParamIndex++;
                }
                
                cvin[inParamIndex].type = cert_pi_end;
                
                cvout[0].type = cert_po_trustAnchor;
                cvout[0].value.pointer.cert = NULL;
                cvout[1].type = cert_po_certList;
                cvout[1].value.pointer.chain = NULL;
                
                /* setting pointer to CERTVerifyLog. Initialized structure
                 * will be used CERT_PKIXVerifyCert */
                cvout[2].type = cert_po_errorLog;
                cvout[2].value.pointer.log = &log;
                
                cvout[3].type = cert_po_end;
                
                secStatus = CERT_PKIXVerifyCert(firstCert, certUsage,
                                                cvin, cvout, &pwdata);
                if (secStatus != SECSuccess) {
                    break;
                }
                issuerCert = cvout[0].value.pointer.cert;
                builtChain = cvout[1].value.pointer.chain;
            } while (0);
        
        /* Display validation results */
        if (secStatus != SECSuccess || log.count > 0) {
            CERTVerifyLogNode *node = NULL;
            fprintf(stderr, "Chain is bad!\n");
            
            SECU_displayVerifyLog(stderr, &log, verbose); 
            /* Have cert refs in the log only in case of failure.
             * Destroy them. */
            for (node = log.head; node; node = node->next) {
                if (node->cert)
                    CERT_DestroyCertificate(node->cert);
            }
            log.head = log.tail = NULL;
            log.count = 0;
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
    } while (--vfyCounts > 0);

    /* Need to destroy CERTVerifyLog arena at the end */
    PORT_FreeArena(log.arena, PR_FALSE);

punt:
    forgetCerts();
    if (NSS_Shutdown() != SECSuccess) {
	SECU_PrintError(progName, "NSS_Shutdown");
	rv = 1;
    }
    PORT_Free(progName);
    PORT_Free(certDir);
    PORT_Free(oidStr);
    freeRevocationMethodData();
    if (pwdata.data) {
        PORT_Free(pwdata.data);
    }
    PL_ArenaFinish();
    PR_Cleanup();
    return rv;
}
