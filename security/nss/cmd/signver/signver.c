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
 * Communications Corporation.	Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

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

static int debugInfo = 0;

static char *usageInfo[] = {
	"Options:",
	" -a                           signature file is ASCII",
	" -d certdir                   directory containing cert database",
	" -i dataFileName              input file name containing data,",
	"                              leave filename blank to use stdin",
	" -s signatureFileName         input file name containing signature,",
	"                              leave filename blank to use stdin",
	" -o outputFileName            output file name, default stdout",
	" -A                           display all information from pkcs #7",
	" -C                           display all information from all certs",
	" -C -n                        display number of certificates",
	" -C -n certNum                display all information from the certNum",
	"                              certificate in pkcs #7 signed object",
	" -C -n certNum f1 ... fN      display information about specified", 
	"                              fields from the certNum certificate in",
	"                              the pkcs #7 signed object",
	" -S                           display signer information list",
	" -S -n                        display number of signer information blocks",
	" -S -n signerNum              display all information from the signerNum",
	"                              signer information block in pkcs #7",
	" -S -n signerNum f1 ... fN    display information about specified", 
	"                              fields from the signerNum signer",
	"                              information block in pkcs #7 object",
	" -V                           verify the signed object and display result",
	" -V -v                        verify the signed object and display",
	"                              result and reason for failure",
	"Version 2.0"
};
static int nUsageInfo = sizeof(usageInfo)/sizeof(char *);

extern int SV_PrintPKCS7ContentInfo(FILE *, SECItem *);

static void Usage(char *progName, FILE *outFile)
{
	int i;
	fprintf(outFile, "Usage:  %s  options\n", progName);
	for (i = 0; i < nUsageInfo; i++)
		fprintf(outFile, "%s\n", usageInfo[i]);
	exit(-1);
}

static HASH_HashType
AlgorithmToHashType(SECAlgorithmID *digestAlgorithms)
{
	SECOidTag tag;

	tag = SECOID_GetAlgorithmTag(digestAlgorithms);

	switch (tag) {
		case SEC_OID_MD2:
			if (debugInfo) PR_fprintf(PR_STDERR, "Hash algorithm: HASH_AlgMD2 SEC_OID_MD2\n");
			return HASH_AlgMD2;
		case SEC_OID_MD5:
			if (debugInfo) PR_fprintf(PR_STDERR, "Hash algorithm: HASH_AlgMD5 SEC_OID_MD5\n");
			return HASH_AlgMD5;
		case SEC_OID_SHA1:
			if (debugInfo) PR_fprintf(PR_STDERR, "Hash algorithm: HASH_AlgSHA1 SEC_OID_SHA1\n");
			return HASH_AlgSHA1;
		default:
			if (debugInfo) PR_fprintf(PR_STDERR, "should never get here\n");
			return HASH_AlgNULL;
	}
}


static int
DigestData (unsigned char *digest, unsigned char *data,
			unsigned int *len, unsigned int maxLen,
			HASH_HashType hashType)
{
	const SECHashObject *hashObj;
	void *hashcx;

	hashObj = HASH_GetHashObject(hashType);
	hashcx = (* hashObj->create)();
	if (hashcx == NULL)
	return -1;

	(* hashObj->begin)(hashcx);
	(* hashObj->update)(hashcx, data, PORT_Strlen((char *)data));
	(* hashObj->end)(hashcx, digest, len, maxLen);
	(* hashObj->destroy)(hashcx, PR_TRUE);

	return 0;
}

enum {
	cmd_DisplayAllPCKS7Info = 0,
	cmd_DisplayCertInfo,
	cmd_DisplaySignerInfo,
	cmd_VerifySignedObj
};

enum {
	opt_ASCII,
	opt_CertDir,
	opt_InputDataFile,
	opt_ItemNumber,
	opt_OutputFile,
	opt_InputSigFile,
	opt_TypeTag,
	opt_PrintWhyFailure
};

static secuCommandFlag signver_commands[] =
{
	{ /* cmd_DisplayAllPCKS7Info*/  'A', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_DisplayCertInfo    */  'C', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_DisplaySignerInfo  */  'S', PR_FALSE, 0, PR_FALSE },
	{ /* cmd_VerifySignedObj    */  'V', PR_FALSE, 0, PR_FALSE }
};

static secuCommandFlag signver_options[] =
{
	{ /* opt_ASCII              */  'a', PR_FALSE, 0, PR_FALSE },
	{ /* opt_CertDir            */  'd', PR_TRUE,  0, PR_FALSE },
	{ /* opt_InputDataFile      */  'i', PR_TRUE,  0, PR_FALSE },
	{ /* opt_ItemNumber         */  'n', PR_FALSE, 0, PR_FALSE },
	{ /* opt_OutputFile         */  'o', PR_TRUE,  0, PR_FALSE },
	{ /* opt_InputSigFile       */  's', PR_TRUE,  0, PR_FALSE },
	{ /* opt_TypeTag            */  't', PR_TRUE,  0, PR_FALSE },
	{ /* opt_PrintWhyFailure    */  'v', PR_FALSE, 0, PR_FALSE }
};

int main(int argc, char **argv)
{
	PRExplodedTime explodedCurrent;
	SECItem der, data;
	char *progName;
	int rv;
	PRFileDesc *dataFile = 0;
	PRFileDesc *signFile = 0;
	FILE *outFile = stdout;
	char *typeTag = 0;
	PRBool displayAllCerts = PR_FALSE;
	PRBool displayAllSigners = PR_FALSE;
	PRFileInfo info;
	PRInt32 nb;
	SECStatus secstatus;

	secuCommand signver;
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

	/*_asm int 3*/

	PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &explodedCurrent);
#if 0
	if (explodedCurrent.tm_year >= 1998
		&& explodedCurrent.tm_month >= 5 /* months past tm_year (0-11, Jan = 0) */
		&& explodedCurrent.tm_mday >= 1) {
		PR_fprintf(PR_STDERR, "%s: expired\n", progName);
		return -1;
	}
#endif

	SECU_ParseCommandLine(argc, argv, progName, &signver);

	/*  Set the certdb directory (default is ~/.{browser}) */
	SECU_ConfigDirectory(signver.options[opt_CertDir].arg);

	/*  Set the certificate type.  */
	typeTag = SECU_GetOptionArg(&signver, opt_TypeTag);

	/*  -i and -s without filenames  */
	if (signver.options[opt_InputDataFile].activated &&
	    signver.options[opt_InputSigFile].activated &&
	    !signver.options[opt_InputDataFile].arg &&
	    !signver.options[opt_InputSigFile].arg)
		PR_fprintf(PR_STDERR, 
	              "%s: Only data or signature file can use stdin (not both).\n",
	              progName);

	/*  Open the input data file (no arg == use stdin). */
	if (signver.options[opt_InputDataFile].activated) {
		if (signver.options[opt_InputDataFile].arg)
			dataFile = PR_Open(signver.options[opt_InputDataFile].arg, 
			                   PR_RDONLY, 0);
		else
			dataFile = PR_STDIN;
		if (!dataFile) {
			PR_fprintf(PR_STDERR, "%s: unable to open \"%s\" for reading.\n",
			           progName, signver.options[opt_InputDataFile].arg);
			return -1;
		}
	}

	/*  Open the input signature file (no arg == use stdin).  */
	if (signver.options[opt_InputSigFile].activated) {
		if (signver.options[opt_InputSigFile].arg)
			signFile = PR_Open(signver.options[opt_InputSigFile].arg, 
			                   PR_RDONLY, 0);
		else
			signFile = PR_STDIN;
		if (!signFile) {
			PR_fprintf(PR_STDERR, "%s: unable to open \"%s\" for reading.\n",
			           progName, signver.options[opt_InputSigFile].arg);
			return -1;
		}
	}

#if 0
				if (!typeTag) ascii = 1;
#endif

	/*  Open|Create the output file.  */
	if (signver.options[opt_OutputFile].activated) {
		outFile = fopen(signver.options[opt_OutputFile].arg, "w");
			/*
		outFile = PR_Open(signver.options[opt_OutputFile].arg,
		                  PR_WRONLY|PR_CREATE_FILE|PR_TRUNCATE, 00660);
						  */
		if (!outFile) {
			PR_fprintf(PR_STDERR, "%s: unable to open \"%s\" for writing.\n",
			           progName, signver.options[opt_OutputFile].arg);
			return -1;
		}
	}

	if (signver.commands[cmd_DisplayCertInfo].activated &&
	    !signver.options[opt_ItemNumber].arg)
		displayAllCerts = PR_TRUE;

	if (signver.commands[cmd_DisplaySignerInfo].activated &&
	    !signver.options[opt_ItemNumber].arg)
		displayAllSigners = PR_TRUE;

#if 0
			case 'W':
				debugInfo = 1;
				break;
#endif

	if (!signFile && !dataFile && !typeTag) 
		Usage(progName, outFile);

	if (!signFile && !dataFile && 
	     signver.commands[cmd_VerifySignedObj].activated) {
		PR_fprintf(PR_STDERR, 
		           "%s: unable to read all data from standard input\n", 
		           progName);
		return -1;
	} 

	PR_SetError(0, 0); /* PR_Init("pp", 1, 1, 0);*/
	secstatus = NSS_Init(SECU_ConfigDirectory(NULL));
    	if (secstatus != SECSuccess) {
	    SECU_PrintPRandOSError(progName);
	    return -1;
	}

	rv = SECU_ReadDERFromFile(&der, signFile, 
	                          signver.options[opt_ASCII].activated);

	/* Data is untyped, using the specified type */
	data.data = der.data;
	data.len = der.len;


	/* Signature Verification */
	if (!signver.options[opt_TypeTag].activated) {
		if (signver.commands[cmd_VerifySignedObj].activated) {
			SEC_PKCS7ContentInfo *cinfo;

			cinfo = SEC_PKCS7DecodeItem(&data, NULL, NULL, NULL, NULL,
			                            NULL, NULL, NULL);
			if (cinfo != NULL) {
#if 0
				if (debugInfo) {
					PR_fprintf(PR_STDERR, "Content %s encrypted.\n",
							   SEC_PKCS7ContentIsEncrypted(cinfo) ? "was" : "was not");

					PR_fprintf(PR_STDERR, "Content %s signed.\n",
							   SEC_PKCS7ContentIsSigned(cinfo) ? "was" : "was not");
				}
#endif

				if (SEC_PKCS7ContentIsSigned(cinfo)) {
					SEC_PKCS7SignedData *signedData;
					HASH_HashType digestType;
					SECItem digest, data;
					unsigned char *dataToVerify, digestBuffer[32];

					signedData = cinfo->content.signedData;

					/* assume that there is only one digest algorithm for now */
					digestType = 
					  AlgorithmToHashType(signedData->digestAlgorithms[0]);
					if (digestType == HASH_AlgNULL) {
						PR_fprintf(PR_STDERR, "Invalid hash algorithmID\n");
						return (-1);
					}
					rv = SECU_FileToItem(&data, dataFile);
					dataToVerify = data.data;
					if (dataToVerify) {
						                   /*certUsageObjectSigner;*/
						SECCertUsage usage = certUsageEmailSigner; 
						

#if 0
						if (debugInfo) 
							PR_fprintf(PR_STDERR, "dataToVerify=%s\n", 
							           dataToVerify);
#endif
						digest.data = digestBuffer;
						if (DigestData (digest.data, dataToVerify, 
						                &digest.len, 32, digestType)) {
							PR_fprintf(PR_STDERR, "Fail to compute message digest for verification. Reason: %s\n",
									  SECU_ErrorString((int16)PORT_GetError()));
							return (-1);
						}
#if 0
						if (debugInfo) {
							PR_fprintf(PR_STDERR, "Data Digest=:");
							for (i = 0; i < digest.len; i++)
								PR_fprintf(PR_STDERR, "%02x:", digest.data[i]);
							PR_fprintf(PR_STDERR, "\n");
						}
#endif


						if (signver.commands[cmd_VerifySignedObj].activated)
							fprintf(outFile, "signatureValid=");
						PORT_SetError(0);
						if (SEC_PKCS7VerifyDetachedSignature (cinfo, usage, 
						                       &digest, digestType, PR_FALSE)) {
							if (signver.commands[cmd_VerifySignedObj].activated)
								fprintf(outFile, "yes");
						} else {
							if (signver.commands[cmd_VerifySignedObj].activated){
								fprintf(outFile, "no");
							if (signver.options[opt_PrintWhyFailure].activated)
							fprintf(outFile, ":%s", SECU_ErrorString((int16)PORT_GetError()));
							}
						}
						if (signver.commands[cmd_VerifySignedObj].activated)
							fprintf(outFile, "\n");

						SECITEM_FreeItem(&data, PR_FALSE);
					} else {
						PR_fprintf(PR_STDERR, "Cannot read data\n");
						return (-1);
					}
				}

				SEC_PKCS7DestroyContentInfo(cinfo);
			} else
				PR_fprintf(PR_STDERR, "Unable to decode PKCS7 data\n");
		}

		if (signver.commands[cmd_DisplayAllPCKS7Info].activated)
			SV_PrintPKCS7ContentInfo(outFile, &data);

		if (displayAllCerts)
			PR_fprintf(PR_STDERR, "-C option is not implemented in this version\n");

		if (displayAllSigners)
			PR_fprintf(PR_STDERR, "-S option is not implemented in this version\n");

	/* Pretty print it */
	} else if (PL_strcmp(typeTag, SEC_CT_CERTIFICATE) == 0) {
		rv = SECU_PrintSignedData(outFile, &data, "Certificate", 0,
								  SECU_PrintCertificate);
	} else if (PL_strcmp(typeTag, SEC_CT_CERTIFICATE_REQUEST) == 0) {
		rv = SECU_PrintSignedData(outFile, &data, "Certificate Request", 0,
								  SECU_PrintCertificateRequest);
	} else if (PL_strcmp (typeTag, SEC_CT_CRL) == 0) {
		rv = SECU_PrintSignedData (outFile, &data, "CRL", 0, SECU_PrintCrl);
#ifdef HAVE_EPK_TEMPLATE
	} else if (PL_strcmp(typeTag, SEC_CT_PRIVATE_KEY) == 0) {
		rv = SECU_PrintPrivateKey(outFile, &data, "Private Key", 0);
#endif
	} else if (PL_strcmp(typeTag, SEC_CT_PUBLIC_KEY) == 0) {
		rv = SECU_PrintPublicKey(outFile, &data, "Public Key", 0);
	} else if (PL_strcmp(typeTag, SEC_CT_PKCS7) == 0) {
		rv = SECU_PrintPKCS7ContentInfo(outFile, &data,
										"PKCS #7 Content Info", 0);
	} else {
		PR_fprintf(PR_STDERR, "%s: don't know how to print out '%s' files\n",
				   progName, typeTag);
		return -1;
	}

	if (rv) {
		PR_fprintf(PR_STDERR, "%s: problem converting data (%s)\n",
				   progName, SECU_ErrorString((int16)PORT_GetError()));
		return -1;
	}
	return 0;
}
