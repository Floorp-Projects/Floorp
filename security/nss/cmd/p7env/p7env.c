/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * p7env -- A command to create a pkcs7 enveloped data.
 */

#include "nspr.h"
#include "secutil.h"
#include "plgetopt.h"
#include "secpkcs7.h"
#include "cert.h"
#include "certdb.h"
#include "nss.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

#if (defined(XP_WIN) && !defined(WIN32)) || (defined(__sun) && !defined(SVR4))
extern int fread(char *, size_t, size_t, FILE*);
extern int fwrite(char *, size_t, size_t, FILE*);
extern int fprintf(FILE *, char *, ...);
#endif


static void
Usage(char *progName)
{
    fprintf(stderr,
	    "Usage:  %s -r recipient [-d dbdir] [-i input] [-o output]\n",
	    progName);
    fprintf(stderr, "%-20s Nickname of cert to use for encryption\n",
	    "-r recipient");
    fprintf(stderr, "%-20s Cert database directory (default is ~/.netscape)\n",
	    "-d dbdir");
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
	    "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
	    "-o output");
    exit(-1);
}

struct recipient {
    struct recipient *next;
    char *nickname;
    CERTCertificate *cert;
};

static void
EncryptOut(void *arg, const char *buf, unsigned long len)
{
   FILE *out;

   out = arg; 
   fwrite (buf, len, 1, out);
}

static int
EncryptFile(FILE *outFile, FILE *inFile, struct recipient *recipients,
	    char *progName)
{
    SEC_PKCS7ContentInfo *cinfo;
    SEC_PKCS7EncoderContext *ecx;
    struct recipient *rcpt;
    SECStatus rv;

    if (outFile == NULL || inFile == NULL || recipients == NULL)
	return -1;

    /* XXX Need a better way to handle that certUsage stuff! */
    /* XXX keysize? */
    cinfo = SEC_PKCS7CreateEnvelopedData (recipients->cert,
					  certUsageEmailRecipient,
					  NULL, SEC_OID_DES_EDE3_CBC, 0, 
					  NULL, NULL);
    if (cinfo == NULL)
	return -1;

    for (rcpt = recipients->next; rcpt != NULL; rcpt = rcpt->next) {
	rv = SEC_PKCS7AddRecipient (cinfo, rcpt->cert, certUsageEmailRecipient,
				    NULL);
	if (rv != SECSuccess) {
	    SECU_PrintError(progName, "error adding recipient \"%s\"",
			    rcpt->nickname);
	    return -1;
	}
    }

    ecx = SEC_PKCS7EncoderStart (cinfo, EncryptOut, outFile, NULL);
    if (ecx == NULL)
	return -1;

    for (;;) {
	char ibuf[1024];
	int nb;
 
	if (feof(inFile))
	    break;
	nb = fread(ibuf, 1, sizeof(ibuf), inFile);
	if (nb == 0) {
	    if (ferror(inFile)) {
		PORT_SetError(SEC_ERROR_IO);
		rv = SECFailure;
	    }
	    break;
	}
	rv = SEC_PKCS7EncoderUpdate(ecx, ibuf, nb);
	if (rv != SECSuccess)
	    break;
    }

    if (SEC_PKCS7EncoderFinish(ecx, NULL, NULL) != SECSuccess)
	rv = SECFailure;

    SEC_PKCS7DestroyContentInfo (cinfo);

    if (rv != SECSuccess)
	return -1;

    return 0;
}

int
main(int argc, char **argv)
{
    char *progName;
    FILE *inFile, *outFile;
    char *certName;
    CERTCertDBHandle *certHandle;
    struct recipient *recipients, *rcpt;
    PLOptState *optstate;
    PLOptStatus status;
    SECStatus rv;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    inFile = NULL;
    outFile = NULL;
    certName = NULL;
    recipients = NULL;
    rcpt = NULL;

    /*
     * Parse command line arguments
     * XXX This needs to be enhanced to allow selection of algorithms
     * and key sizes (or to look up algorithms and key sizes for each
     * recipient in the magic database).
     */
    optstate = PL_CreateOptState(argc, argv, "d:i:o:r:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case '?':
	    Usage(progName);
	    break;

	  case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

	  case 'i':
	    inFile = fopen(optstate->value, "r");
	    if (!inFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'o':
	    outFile = fopen(optstate->value, "wb");
	    if (!outFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'r':
	    if (rcpt == NULL) {
		recipients = rcpt = PORT_Alloc (sizeof(struct recipient));
	    } else {
		rcpt->next = PORT_Alloc (sizeof(struct recipient));
		rcpt = rcpt->next;
	    }
	    if (rcpt == NULL) {
		fprintf(stderr, "%s: unable to allocate recipient struct\n",
			progName);
		return -1;
	    }
	    rcpt->nickname = strdup(optstate->value);
	    rcpt->cert = NULL;
	    rcpt->next = NULL;
	    break;
	}
    }

    if (!recipients) Usage(progName);

    if (!inFile) inFile = stdin;
    if (!outFile) outFile = stdout;

    /* Call the NSS initialization routines */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_Init(SECU_ConfigDirectory(NULL));
    if (rv != SECSuccess) {
    	SECU_PrintPRandOSError(progName);
	return -1;
    }

    /* open cert database */
    certHandle = CERT_GetDefaultCertDB();
    if (certHandle == NULL) {
	return -1;
    }

    /* find certs */
    for (rcpt = recipients; rcpt != NULL; rcpt = rcpt->next) {
	rcpt->cert = CERT_FindCertByNickname(certHandle, rcpt->nickname);
	if (rcpt->cert == NULL) {
	    SECU_PrintError(progName,
			    "the cert for name \"%s\" not found in database",
			    rcpt->nickname);
	    return -1;
	}
    }

    if (EncryptFile(outFile, inFile, recipients, progName)) {
	SECU_PrintError(progName, "problem encrypting data");
	return -1;
    }

    return 0;
}
