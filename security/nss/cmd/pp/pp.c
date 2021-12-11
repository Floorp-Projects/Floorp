/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Pretty-print some well-known BER or DER encoded data (e.g. certificates,
 * keys, pkcs7)
 */

#include "secutil.h"

#if defined(__sun) && !defined(SVR4)
extern int fprintf(FILE *, char *, ...);
#endif

#include "plgetopt.h"

#include "pk11func.h"
#include "nspr.h"
#include "nss.h"

static void
Usage(char *progName)
{
    fprintf(stderr,
            "Usage:  %s [-t type] [-a] [-i input] [-o output] [-w] [-u]\n",
            progName);
    fprintf(stderr, "Pretty prints a file containing ASN.1 data in DER or ascii format.\n");
    fprintf(stderr, "%-14s Specify input and display type:", "-t type");
    fprintf(stderr, " %s (sk),", SEC_CT_PRIVATE_KEY);
    fprintf(stderr, "\n");
    fprintf(stderr, "%-14s %s (pk), %s (c), %s (cr),\n", "", SEC_CT_PUBLIC_KEY,
            SEC_CT_CERTIFICATE, SEC_CT_CERTIFICATE_REQUEST);
    fprintf(stderr, "%-14s %s (ci), %s (p7), %s (p12), %s or %s (n).\n", "",
            SEC_CT_CERTIFICATE_ID, SEC_CT_PKCS7, SEC_CT_PKCS12,
            SEC_CT_CRL, SEC_CT_NAME);
    fprintf(stderr, "%-14s (Use either the long type name or the shortcut.)\n", "");
    fprintf(stderr, "%-14s Input is in ascii encoded form (RFC1113)\n",
            "-a");
    fprintf(stderr, "%-14s Define an input file to use (default is stdin)\n",
            "-i input");
    fprintf(stderr, "%-14s Define an output file to use (default is stdout)\n",
            "-o output");
    fprintf(stderr, "%-14s Don't wrap long output lines\n",
            "-w");
    fprintf(stderr, "%-14s Use UTF-8 (default is to show non-ascii as .)\n",
            "-u");
    exit(-1);
}

int
main(int argc, char **argv)
{
    int rv, ascii;
    char *progName;
    FILE *outFile;
    PRFileDesc *inFile;
    SECItem der, data;
    char *typeTag;
    PLOptState *optstate;
    PRBool wrap = PR_TRUE;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    ascii = 0;
    inFile = 0;
    outFile = 0;
    typeTag = 0;
    optstate = PL_CreateOptState(argc, argv, "at:i:o:uw");
    while (PL_GetNextOpt(optstate) == PL_OPT_OK) {
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
                    PORT_Free(typeTag);
                    PL_DestroyOptState(optstate);
                    return -1;
                }
                break;

            case 'o':
                outFile = fopen(optstate->value, "w");
                if (!outFile) {
                    fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
                            progName, optstate->value);
                    PORT_Free(typeTag);
                    PL_DestroyOptState(optstate);
                    return -1;
                }
                break;

            case 't':
                typeTag = strdup(optstate->value);
                break;

            case 'u':
                SECU_EnableUtf8Display(PR_TRUE);
                break;

            case 'w':
                wrap = PR_FALSE;
                break;
        }
    }
    PL_DestroyOptState(optstate);
    if (!typeTag)
        Usage(progName);

    if (!inFile)
        inFile = PR_STDIN;
    if (!outFile)
        outFile = stdout;

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
        fprintf(stderr, "%s: NSS_NoDB_Init failed (%s)\n",
                progName, SECU_Strerror(PORT_GetError()));
        exit(1);
    }
    SECU_RegisterDynamicOids();

    rv = SECU_ReadDERFromFile(&der, inFile, ascii, PR_FALSE);
    if (rv != SECSuccess) {
        fprintf(stderr, "%s: SECU_ReadDERFromFile failed\n", progName);
        exit(1);
    }

    /* Data is untyped, using the specified type */
    data.data = der.data;
    data.len = der.len;

    SECU_EnableWrap(wrap);

    /* Pretty print it */
    if (PORT_Strcmp(typeTag, SEC_CT_CERTIFICATE) == 0 ||
        PORT_Strcmp(typeTag, "c") == 0) {
        rv = SECU_PrintSignedData(outFile, &data, "Certificate", 0,
                                  (SECU_PPFunc)SECU_PrintCertificate);
    } else if (PORT_Strcmp(typeTag, SEC_CT_CERTIFICATE_ID) == 0 ||
               PORT_Strcmp(typeTag, "ci") == 0) {
        rv = SECU_PrintSignedContent(outFile, &data, 0, 0,
                                     SECU_PrintDumpDerIssuerAndSerial);
    } else if (PORT_Strcmp(typeTag, SEC_CT_CERTIFICATE_REQUEST) == 0 ||
               PORT_Strcmp(typeTag, "cr") == 0) {
        rv = SECU_PrintSignedData(outFile, &data, "Certificate Request", 0,
                                  SECU_PrintCertificateRequest);
    } else if (PORT_Strcmp(typeTag, SEC_CT_CRL) == 0) {
        rv = SECU_PrintSignedData(outFile, &data, "CRL", 0, SECU_PrintCrl);
    } else if (PORT_Strcmp(typeTag, SEC_CT_PRIVATE_KEY) == 0 ||
               PORT_Strcmp(typeTag, "sk") == 0) {
        rv = SECU_PrintPrivateKey(outFile, &data, "Private Key", 0);
    } else if (PORT_Strcmp(typeTag, SEC_CT_PUBLIC_KEY) == 0 ||
               PORT_Strcmp(typeTag, "pk") == 0) {
        rv = SECU_PrintSubjectPublicKeyInfo(outFile, &data, "Public Key", 0);
    } else if (PORT_Strcmp(typeTag, SEC_CT_PKCS7) == 0 ||
               PORT_Strcmp(typeTag, "p7") == 0) {
        rv = SECU_PrintPKCS7ContentInfo(outFile, &data,
                                        "PKCS #7 Content Info", 0);
    } else if (PORT_Strcmp(typeTag, SEC_CT_NAME) == 0 ||
               PORT_Strcmp(typeTag, "n") == 0) {
        rv = SECU_PrintDERName(outFile, &data, "Name", 0);
    } else if (PORT_Strcmp(typeTag, SEC_CT_PKCS12) == 0 ||
               PORT_Strcmp(typeTag, "p12") == 0) {
        rv = SECU_PrintPKCS12(outFile, &data, "PKCS #12 File", 0);
    } else {
        fprintf(stderr, "%s: don't know how to print out '%s' files\n",
                progName, typeTag);
        SECU_PrintAny(outFile, &data, "File contains", 0);
        return -1;
    }

    PORT_Free(typeTag);

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
