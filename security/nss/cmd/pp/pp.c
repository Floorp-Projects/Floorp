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

/*
 * Pretty-print some well-known BER or DER encoded data (e.g. certificates,
 * keys, pkcs7)
 *
 * $Id: pp.c,v 1.7 2004/09/30 00:49:36 wchang0222%aol.com Exp $
 */

#include "secutil.h"

#if defined(__sun) && !defined(SVR4)
extern int fprintf(FILE *, char *, ...);
#endif

#include "plgetopt.h"

#include "pk11func.h"
#include "nspr.h"
#include "nss.h"

static void Usage(char *progName)
{
    fprintf(stderr,
	    "Usage:  %s -t type [-a] [-i input] [-o output]\n",
	    progName);
    fprintf(stderr, "%-20s Specify the input type (must be one of %s,\n",
	    "-t type", SEC_CT_PRIVATE_KEY);
    fprintf(stderr, "%-20s %s, %s, %s,\n", "", SEC_CT_PUBLIC_KEY,
	    SEC_CT_CERTIFICATE, SEC_CT_CERTIFICATE_REQUEST);
    fprintf(stderr, "%-20s %s or %s)\n", "", SEC_CT_PKCS7, SEC_CT_CRL);    
    fprintf(stderr, "%-20s Input is in ascii encoded form (RFC1113)\n",
	    "-a");
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
	    "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
	    "-o output");
    exit(-1);
}

int main(int argc, char **argv)
{
    int rv, ascii;
    char *progName;
    FILE *outFile;
    PRFileDesc *inFile;
    SECItem der, data;
    char *typeTag;
    PLOptState *optstate;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    ascii = 0;
    inFile = 0;
    outFile = 0;
    typeTag = 0;
    optstate = PL_CreateOptState(argc, argv, "at:i:o:");
    while ( PL_GetNextOpt(optstate) == PL_OPT_OK ) {
	switch (optstate->option) {
	  case '?':
	    Usage(progName);
	    break;

	  case 'a':
	    ascii = 1;
	    break;

	  case 'i':
	    inFile = PR_Open(optstate->value, PR_RDONLY, 0);
	    if (!inFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'o':
	    outFile = fopen(optstate->value, "w");
	    if (!outFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 't':
	    typeTag = strdup(optstate->value);
	    break;
	}
    }
    PL_DestroyOptState(optstate);
    if (!typeTag) Usage(progName);

    if (!inFile) inFile = PR_STDIN;
    if (!outFile) outFile = stdout;

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
	fprintf(stderr, "%s: NSS_NoDB_Init failed (%s)\n",
		progName, SECU_Strerror(PORT_GetError()));
	exit(1);
    }
    SECU_RegisterDynamicOids();

    rv = SECU_ReadDERFromFile(&der, inFile, ascii);
    if (rv != SECSuccess) {
	fprintf(stderr, "%s: SECU_ReadDERFromFile failed\n", progName);
	exit(1);
    }

    /* Data is untyped, using the specified type */
    data.data = der.data;
    data.len = der.len;

    /* Pretty print it */
    if (PORT_Strcmp(typeTag, SEC_CT_CERTIFICATE) == 0) {
	rv = SECU_PrintSignedData(outFile, &data, "Certificate", 0,
			     SECU_PrintCertificate);
    } else if (PORT_Strcmp(typeTag, SEC_CT_CERTIFICATE_REQUEST) == 0) {
	rv = SECU_PrintSignedData(outFile, &data, "Certificate Request", 0,
			     SECU_PrintCertificateRequest);
    } else if (PORT_Strcmp (typeTag, SEC_CT_CRL) == 0) {
	rv = SECU_PrintSignedData (outFile, &data, "CRL", 0, SECU_PrintCrl);
#ifdef HAVE_EPV_TEMPLATE
    } else if (PORT_Strcmp(typeTag, SEC_CT_PRIVATE_KEY) == 0) {
	rv = SECU_PrintPrivateKey(outFile, &data, "Private Key", 0);
#endif
    } else if (PORT_Strcmp(typeTag, SEC_CT_PUBLIC_KEY) == 0) {
	rv = SECU_PrintPublicKey(outFile, &data, "Public Key", 0);
    } else if (PORT_Strcmp(typeTag, SEC_CT_PKCS7) == 0) {
	rv = SECU_PrintPKCS7ContentInfo(outFile, &data,
					"PKCS #7 Content Info", 0);
    } else {
	fprintf(stderr, "%s: don't know how to print out '%s' files\n",
		progName, typeTag);
	return -1;
    }

    if (inFile != PR_STDIN)
	PR_Close(inFile);
    PORT_Free(der.data);
    if (rv) {
	fprintf(stderr, "%s: problem converting data (%s)\n",
		progName, SECU_Strerror(PORT_GetError()));
    }
    if (NSS_Shutdown() != SECSuccess) {
	fprintf(stderr, "%s: NSS_Shutdown failed (%s)\n",
		progName, SECU_Strerror(PORT_GetError()));
	rv = SECFailure;
    }
    PR_Cleanup();
    return rv;
}
