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

#include "secutil.h"
#include "secmod.h"
#include "cert.h"
#include "secoid.h"
#include "nss.h"

/* NSPR 2.0 header files */
#include "prinit.h"
#include "prprf.h"
#include "prsystem.h"
#include "prmem.h"
/* Portable layer header files */
#include "plstr.h"
#include "sechash.h"	/* for HASH_GetHashObject() */

static PRBool debugInfo;
static PRBool verbose;
static PRBool doVerify;
static PRBool displayAll;

static const char * const usageInfo[] = {
    "signver - verify a detached PKCS7 signature - Version " NSS_VERSION,
    "Commands:",
    " -A		    display all information from pkcs #7",
    " -V		    verify the signed object and display result",
    "Options:",
    " -a		    signature file is ASCII",
    " -d certdir	    directory containing cert database",
    " -i dataFileName	    input file containing signed data (default stdin)",
    " -o outputFileName     output file name, default stdout",
    " -s signatureFileName  input file for signature (default stdin)",
    " -v		    display verbose reason for failure"
};
static int nUsageInfo = sizeof(usageInfo)/sizeof(char *);

extern int SV_PrintPKCS7ContentInfo(FILE *, SECItem *);

static void Usage(char *progName, FILE *outFile)
{
    int i;
    fprintf(outFile, "Usage:  %s [ commands ] options\n", progName);
    for (i = 0; i < nUsageInfo; i++)
	fprintf(outFile, "%s\n", usageInfo[i]);
    exit(-1);
}

static HASH_HashType
AlgorithmToHashType(SECAlgorithmID *digestAlgorithms)
{
    SECOidTag	  tag  = SECOID_GetAlgorithmTag(digestAlgorithms);
    HASH_HashType hash = HASH_GetHashTypeByOidTag(tag);
    return hash;
}


static SECStatus
DigestContent (SECItem * digest, SECItem * content, HASH_HashType hashType)
{
    unsigned int maxLen = digest->len;
    unsigned int len	= HASH_ResultLen(hashType);
    SECStatus	 rv;

    if (len > maxLen) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }

    rv = HASH_HashBuf(hashType, digest->data, content->data, content->len);
    if (rv == SECSuccess)
	digest->len = len;
    return rv;
}

enum {
    cmd_DisplayAllPCKS7Info = 0,
    cmd_VerifySignedObj
};

enum {
    opt_ASCII,
    opt_CertDir,
    opt_InputDataFile,
    opt_ItemNumber,
    opt_OutputFile,
    opt_InputSigFile,
    opt_PrintWhyFailure,
    opt_DebugInfo
};

static secuCommandFlag signver_commands[] =
{
    { /* cmd_DisplayAllPCKS7Info*/  'A', PR_FALSE, 0, PR_FALSE },
    { /* cmd_VerifySignedObj	*/  'V', PR_FALSE, 0, PR_FALSE }
};

static secuCommandFlag signver_options[] =
{
    { /* opt_ASCII		*/  'a', PR_FALSE, 0, PR_FALSE },
    { /* opt_CertDir		*/  'd', PR_TRUE,  0, PR_FALSE },
    { /* opt_InputDataFile	*/  'i', PR_TRUE,  0, PR_FALSE },
    { /* opt_OutputFile 	*/  'o', PR_TRUE,  0, PR_FALSE },
    { /* opt_InputSigFile	*/  's', PR_TRUE,  0, PR_FALSE },
    { /* opt_PrintWhyFailure	*/  'v', PR_FALSE, 0, PR_FALSE },
    { /* opt_DebugInfo		*/    0, PR_FALSE, 0, PR_FALSE, "debug" }
};

int main(int argc, char **argv)
{
    PRFileDesc *contentFile = NULL;
    PRFileDesc *signFile = PR_STDIN;
    FILE *	outFile  = stdout;
    char *	progName;
    SECStatus	rv;
    int 	result	 = 1;
    SECItem	pkcs7der, content;
    secuCommand signver;

    pkcs7der.data  = NULL;
    content.data = NULL;

    signver.numCommands = sizeof(signver_commands) /sizeof(secuCommandFlag);
    signver.numOptions = sizeof(signver_options) / sizeof(secuCommandFlag);
    signver.commands = signver_commands;
    signver.options = signver_options;

#ifdef XP_PC
    progName = strrchr(argv[0], '\\');
#else
    progName = strrchr(argv[0], '/');
#endif
    progName = progName ? progName+1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &signver);
    if (SECSuccess != rv) {
	Usage(progName, outFile);
    }
    debugInfo = signver.options[opt_DebugInfo	    ].activated;
    verbose   = signver.options[opt_PrintWhyFailure ].activated;
    doVerify  = signver.commands[cmd_VerifySignedObj].activated;
    displayAll= signver.commands[cmd_DisplayAllPCKS7Info].activated;
    if (!doVerify && !displayAll)
	doVerify = PR_TRUE;

    /*	Set the certdb directory (default is ~/.netscape) */
    rv = NSS_Init(SECU_ConfigDirectory(signver.options[opt_CertDir].arg));
    if (rv != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return result;
    }
    /* below here, goto cleanup */
    SECU_RegisterDynamicOids();

    /*	Open the input content file. */
    if (signver.options[opt_InputDataFile].activated &&
	signver.options[opt_InputDataFile].arg) {
	if (PL_strcmp("-", signver.options[opt_InputDataFile].arg)) {
	    contentFile = PR_Open(signver.options[opt_InputDataFile].arg,
			       PR_RDONLY, 0);
	    if (!contentFile) {
		PR_fprintf(PR_STDERR,
			   "%s: unable to open \"%s\" for reading.\n",
			   progName, signver.options[opt_InputDataFile].arg);
		goto cleanup;
	    }
	} else
	    contentFile = PR_STDIN;
    }

    /*	Open the input signature file.	*/
    if (signver.options[opt_InputSigFile].activated &&
	signver.options[opt_InputSigFile].arg) {
	if (PL_strcmp("-", signver.options[opt_InputSigFile].arg)) {
	    signFile = PR_Open(signver.options[opt_InputSigFile].arg,
			       PR_RDONLY, 0);
	    if (!signFile) {
		PR_fprintf(PR_STDERR,
			   "%s: unable to open \"%s\" for reading.\n",
			   progName, signver.options[opt_InputSigFile].arg);
		goto cleanup;
	    }
	}
    }

    if (contentFile == PR_STDIN && signFile == PR_STDIN && doVerify) {
	PR_fprintf(PR_STDERR,
	    "%s: cannot read both content and signature from standard input\n",
		   progName);
	goto cleanup;
    }

    /*	Open|Create the output file.  */
    if (signver.options[opt_OutputFile].activated) {
	outFile = fopen(signver.options[opt_OutputFile].arg, "w");
	if (!outFile) {
	    PR_fprintf(PR_STDERR, "%s: unable to open \"%s\" for writing.\n",
		       progName, signver.options[opt_OutputFile].arg);
	    goto cleanup;
	}
    }

    /* read in the input files' contents */
    rv = SECU_ReadDERFromFile(&pkcs7der, signFile,
			      signver.options[opt_ASCII].activated);
    if (signFile != PR_STDIN)
	PR_Close(signFile);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "problem reading PKCS7 input");
	goto cleanup;
    }
    if (contentFile) {
	rv = SECU_FileToItem(&content, contentFile);
	if (contentFile != PR_STDIN)
	    PR_Close(contentFile);
	if (rv != SECSuccess)
	    content.data = NULL;
    }

    /* Signature Verification */
    if (doVerify) {
	SEC_PKCS7ContentInfo *cinfo;
	SEC_PKCS7SignedData *signedData;
	HASH_HashType digestType;
	PRBool contentIsSigned;

	cinfo = SEC_PKCS7DecodeItem(&pkcs7der, NULL, NULL, NULL, NULL,
				    NULL, NULL, NULL);
	if (cinfo == NULL) {
	    PR_fprintf(PR_STDERR, "Unable to decode PKCS7 data\n");
	    goto cleanup;
	}
	/* below here, goto done */

	contentIsSigned = SEC_PKCS7ContentIsSigned(cinfo);
	if (debugInfo) {
	    PR_fprintf(PR_STDERR, "Content is%s encrypted.\n",
		       SEC_PKCS7ContentIsEncrypted(cinfo) ? "" : " not");
	}
	if (debugInfo || !contentIsSigned) {
	    PR_fprintf(PR_STDERR, "Content is%s signed.\n",
		       contentIsSigned ? "" : " not");
	}

	if (!contentIsSigned)
	    goto done;

	signedData = cinfo->content.signedData;

	/* assume that there is only one digest algorithm for now */
	digestType = AlgorithmToHashType(signedData->digestAlgorithms[0]);
	if (digestType == HASH_AlgNULL) {
	    PR_fprintf(PR_STDERR, "Invalid hash algorithmID\n");
	    goto done;
	}
	if (content.data) {
	    SECCertUsage   usage = certUsageEmailSigner;
	    SECItem	   digest;
	    unsigned char  digestBuffer[HASH_LENGTH_MAX];

	    if (debugInfo)
		PR_fprintf(PR_STDERR, "contentToVerify=%s\n", content.data);

	    digest.data = digestBuffer;
	    digest.len	= sizeof digestBuffer;

	    if (DigestContent(&digest, &content, digestType)) {
		SECU_PrintError(progName, "Message digest computation failure");
		goto done;
	    }

	    if (debugInfo) {
		unsigned int i;
		PR_fprintf(PR_STDERR, "Data Digest=:");
		for (i = 0; i < digest.len; i++)
		    PR_fprintf(PR_STDERR, "%02x:", digest.data[i]);
		PR_fprintf(PR_STDERR, "\n");
	    }

	    fprintf(outFile, "signatureValid=");
	    PORT_SetError(0);
	    if (SEC_PKCS7VerifyDetachedSignature (cinfo, usage,
				   &digest, digestType, PR_FALSE)) {
		fprintf(outFile, "yes");
	    } else {
		fprintf(outFile, "no");
		if (verbose) {
		    fprintf(outFile, ":%s",
			    SECU_Strerror(PORT_GetError()));
		}
	    }
	    fprintf(outFile, "\n");
	    result = 0;
	}
done:
	SEC_PKCS7DestroyContentInfo(cinfo);
    }

    if (displayAll) {
	if (SV_PrintPKCS7ContentInfo(outFile, &pkcs7der))
	    result = 1;
    }

cleanup:
    SECITEM_FreeItem(&pkcs7der, PR_FALSE);
    SECITEM_FreeItem(&content, PR_FALSE);

    if (NSS_Shutdown() != SECSuccess) {
	result = 1;
    }

    return result;
}
