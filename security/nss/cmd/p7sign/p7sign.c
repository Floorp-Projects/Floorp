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
 * p7sign -- A command to create a *detached* pkcs7 signature (over a given
 * input file).
 *
 * $Id: p7sign.c,v 1.1 2000/03/31 20:12:30 relyea%netscape.com Exp $
 */

#include "nspr.h"
#include "plgetopt.h"
#include "secutil.h"
#include "secpkcs7.h"
#include "cert.h"
#include "certdb.h"
#include "cdbhdl.h"

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

extern void SEC_Init(void);		/* XXX */


static void
Usage(char *progName)
{
    fprintf(stderr,
	    "Usage:  %s -k keyname [-d keydir] [-i input] [-o output]\n",
	    progName);
    fprintf(stderr, "%-20s Nickname of key to use for signature\n",
	    "-k keyname");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
	    "-d keydir");
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
	    "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
	    "-o output");
    fprintf(stderr, "%-20s Encapsulate content in signature message\n",
	    "-e");
    exit(-1);
}

static SECKEYKeyDBHandle *
OpenKeyDB(char *progName)
{
    SECKEYKeyDBHandle *keyHandle;

    keyHandle = SECU_OpenKeyDB(PR_FALSE);
    if (keyHandle == NULL) {
        SECU_PrintError(progName, "could not open key database");
	return NULL;
    }

    return(keyHandle);
}

static CERTCertDBHandle certHandleStatic;	/* avoid having to allocate */

static CERTCertDBHandle *
OpenCertDB(char *progName)
{
    CERTCertDBHandle *certHandle;
    SECStatus rv;

    certHandle = &certHandleStatic;
    rv = CERT_OpenCertDB(certHandle, PR_FALSE, SECU_CertDBNameCallback, NULL);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "could not open cert database");
	return NULL;
    } else {
	CERT_SetDefaultCertDB(certHandle);
    }

    return certHandle;
}

static void
SignOut(void *arg, const char *buf, unsigned long len)
{
   FILE *out;

   out = arg; 
   fwrite (buf, len, 1, out);
}

static int
CreateDigest(SECItem *data, char *digestdata, unsigned int *len, unsigned int maxlen)
{
    SECHashObject *hashObj;
    void *hashcx;

    /* XXX probably want to extend interface to allow other hash algorithms */
    hashObj = &SECHashObjects[HASH_AlgSHA1];

    hashcx = (* hashObj->create)();
    if (hashcx == NULL)
	return -1;

    (* hashObj->begin)(hashcx);
    (* hashObj->update)(hashcx, data->data, data->len);
    (* hashObj->end)(hashcx, (unsigned char *)digestdata, len, maxlen);
    (* hashObj->destroy)(hashcx, PR_TRUE);
    return 0;
}

static int
SignFile(FILE *outFile, PRFileDesc *inFile, CERTCertificate *cert, 
         PRBool encapsulated)
{
    int nb;
    char digestdata[32];
    unsigned int len;
    SECItem digest, data2sign;
    SEC_PKCS7ContentInfo *cinfo;
    SECStatus rv;

    if (outFile == NULL || inFile == NULL || cert == NULL)
	return -1;

    /* suck the file in */
	if (SECU_ReadDERFromFile(&data2sign, inFile, PR_FALSE) != SECSuccess)
	return -1;

    if (!encapsulated) {
	/* unfortunately, we must create the digest ourselves */
	/* SEC_PKCS7CreateSignedData should have a flag to not include */
	/* the content for non-encapsulated content at encode time, but */
	/* should always compute the hash itself */
	if (CreateDigest(&data2sign, digestdata, &len, 32) < 0)
	    return -1;
	digest.data = (unsigned char *)digestdata;
	digest.len = len;
    }

    /* XXX Need a better way to handle that usage stuff! */
    cinfo = SEC_PKCS7CreateSignedData (cert, certUsageEmailSigner, NULL,
				       SEC_OID_SHA1,
				       encapsulated ? NULL : &digest,
				       NULL, NULL);
    if (cinfo == NULL)
	return -1;

    if (encapsulated) {
	SEC_PKCS7SetContent(cinfo, (char *)data2sign.data, data2sign.len);
    }

    rv = SEC_PKCS7IncludeCertChain (cinfo, NULL);
    if (rv != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return -1;
    }

    rv = SEC_PKCS7Encode (cinfo, SignOut, outFile, NULL,
			  SECU_GetPassword, NULL);

    SEC_PKCS7DestroyContentInfo (cinfo);

    if (rv != SECSuccess)
	return -1;

    return 0;
}

int
main(int argc, char **argv)
{
    char *progName;
    FILE *outFile;
    PRFileDesc *inFile;
    char *keyName;
    SECKEYKeyDBHandle *keyHandle;
    CERTCertDBHandle *certHandle;
    CERTCertificate *cert;
    PRBool encapsulated = PR_FALSE;
    PLOptState *optstate;
    PLOptStatus status;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    inFile = NULL;
    outFile = NULL;
    keyName = NULL;

    /*
     * Parse command line arguments
     */
    optstate = PL_CreateOptState(argc, argv, "ed:k:i:o:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case '?':
	    Usage(progName);
	    break;

	  case 'e':
	    /* create a message with the signed content encapsulated */
	    encapsulated = PR_TRUE;
	    break;

	  case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

	  case 'i':
	    inFile = PR_Open(optstate->value, PR_RDONLY, 0);
	    if (!inFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'k':
	    keyName = strdup(optstate->value);
	    break;

	  case 'o':
	    outFile = fopen(optstate->value, "w");
	    if (!outFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		return -1;
	    }
	    break;
	}
    }

    if (!keyName) Usage(progName);

    if (!inFile) inFile = PR_STDIN;
    if (!outFile) outFile = stdout;

    /* Call the initialization routines */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    SECU_PKCS11Init(PR_FALSE);
    SEC_Init();

    /* open key database */
    keyHandle = OpenKeyDB(progName);
    if (keyHandle == NULL) {
	return -1;
    }

#if 0
    /* check if key actually exists */
    if (! SECU_CheckKeyNameExists(keyHandle, keyName)) {
	SECU_PrintError(progName, "the key \"%s\" does not exist", keyName);
	return -1;
    }
#endif

    SECKEY_SetDefaultKeyDB(keyHandle);

    /* open cert database */
    certHandle = OpenCertDB(progName);
    if (certHandle == NULL) {
	return -1;
    }

    /* find cert */
    cert = CERT_FindCertByNickname(certHandle, keyName);
    if (cert == NULL) {
	SECU_PrintError(progName,
		        "the corresponding cert for key \"%s\" does not exist",
			keyName);
	return -1;
    }

    CERT_SetDefaultCertDB(certHandle);

    if (SignFile(outFile, inFile, cert, encapsulated)) {
	SECU_PrintError(progName, "problem signing data");
	return -1;
    }

    return 0;
}
