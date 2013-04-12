/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * p7verify -- A command to do a verification of a *detached* pkcs7 signature.
 *
 * $Id$
 */

#include "nspr.h"
#include "secutil.h"
#include "plgetopt.h"
#include "secpkcs7.h"
#include "cert.h"
#include "certdb.h"
#include "secoid.h"
#include "sechash.h"	/* for HASH_GetHashObject() */
#include "nss.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

#if (defined(XP_WIN) && !defined(WIN32)) || (defined(__sun) && !defined(SVR4))
extern int fread(char *, size_t, size_t, FILE*);
extern int fprintf(FILE *, char *, ...);
#endif


static HASH_HashType
AlgorithmToHashType(SECAlgorithmID *digestAlgorithms)
{

    SECOidTag tag;

    tag = SECOID_GetAlgorithmTag(digestAlgorithms);
    
    switch (tag) {
      case SEC_OID_MD2:
	return HASH_AlgMD2;
      case SEC_OID_MD5:
	return HASH_AlgMD5;
      case SEC_OID_SHA1:
	return HASH_AlgSHA1;
      default:
	fprintf(stderr, "should never get here\n");
	return HASH_AlgNULL;
    }
}

static int
DigestFile(unsigned char *digest, unsigned int *len, unsigned int maxLen,
	   FILE *inFile, HASH_HashType hashType)
{
    int nb;
    unsigned char ibuf[4096];
    const SECHashObject *hashObj;
    void *hashcx;

    hashObj = HASH_GetHashObject(hashType);

    hashcx = (* hashObj->create)();
    if (hashcx == NULL)
	return -1;

    (* hashObj->begin)(hashcx);

    for (;;) {
	if (feof(inFile)) break;
	nb = fread(ibuf, 1, sizeof(ibuf), inFile);
	if (nb != sizeof(ibuf)) {
	    if (nb == 0) {
		if (ferror(inFile)) {
		    PORT_SetError(SEC_ERROR_IO);
		    (* hashObj->destroy)(hashcx, PR_TRUE);
		    return -1;
		}
		/* eof */
		break;
	    }
	}
	(* hashObj->update)(hashcx, ibuf, nb);
    }

    (* hashObj->end)(hashcx, digest, len, maxLen);
    (* hashObj->destroy)(hashcx, PR_TRUE);

    return 0;
}


static void
Usage(char *progName)
{
    fprintf(stderr,
	    "Usage:  %s -c content -s signature [-d dbdir] [-u certusage]\n",
	    progName);
    fprintf(stderr, "%-20s content file that was signed\n",
	    "-c content");
    fprintf(stderr, "%-20s file containing signature for that content\n",
	    "-s signature");
    fprintf(stderr,
	    "%-20s Key/Cert database directory (default is ~/.netscape)\n",
	    "-d dbdir");
    fprintf(stderr, "%-20s Define the type of certificate usage (default is certUsageEmailSigner)\n",
	    "-u certusage");
    fprintf(stderr, "%-25s  0 - certUsageSSLClient\n", " ");
    fprintf(stderr, "%-25s  1 - certUsageSSLServer\n", " ");
    fprintf(stderr, "%-25s  2 - certUsageSSLServerWithStepUp\n", " ");
    fprintf(stderr, "%-25s  3 - certUsageSSLCA\n", " ");
    fprintf(stderr, "%-25s  4 - certUsageEmailSigner\n", " ");
    fprintf(stderr, "%-25s  5 - certUsageEmailRecipient\n", " ");
    fprintf(stderr, "%-25s  6 - certUsageObjectSigner\n", " ");
    fprintf(stderr, "%-25s  7 - certUsageUserCertImport\n", " ");
    fprintf(stderr, "%-25s  8 - certUsageVerifyCA\n", " ");
    fprintf(stderr, "%-25s  9 - certUsageProtectedObjectSigner\n", " ");
    fprintf(stderr, "%-25s 10 - certUsageStatusResponder\n", " ");
    fprintf(stderr, "%-25s 11 - certUsageAnyCA\n", " ");

    exit(-1);
}

static int
HashDecodeAndVerify(FILE *out, FILE *content, PRFileDesc *signature,
		    SECCertUsage usage, char *progName)
{
    SECItem derdata;
    SEC_PKCS7ContentInfo *cinfo;
    SEC_PKCS7SignedData *signedData;
    HASH_HashType digestType;
    SECItem digest;
    unsigned char buffer[32];

    if (SECU_ReadDERFromFile(&derdata, signature, PR_FALSE) != SECSuccess) {
	SECU_PrintError(progName, "error reading signature file");
	return -1;
    }

    cinfo = SEC_PKCS7DecodeItem(&derdata, NULL, NULL, NULL, NULL,
				NULL, NULL, NULL);
    if (cinfo == NULL)
	return -1;

    if (! SEC_PKCS7ContentIsSigned(cinfo)) {
	fprintf (out, "Signature file is pkcs7 data, but not signed.\n");
	return -1;
    }

    signedData = cinfo->content.signedData;

    /* assume that there is only one digest algorithm for now */
    digestType = AlgorithmToHashType(signedData->digestAlgorithms[0]);
    if (digestType == HASH_AlgNULL) {
	fprintf (out, "Invalid hash algorithmID\n");
	return -1;
    }

    digest.data = buffer;
    if (DigestFile (digest.data, &digest.len, 32, content, digestType)) {
	SECU_PrintError (progName, "problem computing message digest");
	return -1;
    }

    fprintf(out, "Signature is ");
    if (SEC_PKCS7VerifyDetachedSignature (cinfo, usage, &digest, digestType,
					  PR_FALSE))
	fprintf(out, "valid.\n");
    else
	fprintf(out, "invalid (Reason: %s).\n",
		SECU_Strerror(PORT_GetError()));

    SEC_PKCS7DestroyContentInfo(cinfo);
    return 0;
}


int
main(int argc, char **argv)
{
    char *progName;
    FILE *contentFile, *outFile;
    PRFileDesc *signatureFile;
    SECCertUsage certUsage = certUsageEmailSigner;
    PLOptState *optstate;
    PLOptStatus status;
    SECStatus rv;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    contentFile = NULL;
    signatureFile = NULL;
    outFile = NULL;

    /*
     * Parse command line arguments
     */
    optstate = PL_CreateOptState(argc, argv, "c:d:o:s:u:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case '?':
	    Usage(progName);
	    break;

	  case 'c':
	    contentFile = fopen(optstate->value, "r");
	    if (!contentFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

	  case 'o':
	    outFile = fopen(optstate->value, "w");
	    if (!outFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 's':
	    signatureFile = PR_Open(optstate->value, PR_RDONLY, 0);
	    if (!signatureFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'u': {
	    int usageType;

	    usageType = atoi (strdup(optstate->value));
	    if (usageType < certUsageSSLClient || usageType > certUsageAnyCA)
		return -1;
	    certUsage = (SECCertUsage)usageType;
	    break;
	  }
	      
	}
    }

    if (!contentFile) Usage (progName);
    if (!signatureFile) Usage (progName);
    if (!outFile) outFile = stdout;

    /* Call the NSS initialization routines */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_Init(SECU_ConfigDirectory(NULL));
    if (rv != SECSuccess) {
    	SECU_PrintPRandOSError(progName);
	return -1;
    }

    if (HashDecodeAndVerify(outFile, contentFile, signatureFile,
			    certUsage, progName)) {
	SECU_PrintError(progName, "problem decoding/verifying signature");
	return -1;
    }

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

    return 0;
}
