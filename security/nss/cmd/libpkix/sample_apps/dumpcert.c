/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * dumpcert.c
 *
 * dump certificate sample application
 *
 */

#include <stdio.h>

#include "pkix.h"
#include "testutil.h"
#include "prlong.h"
#include "plstr.h"
#include "prthread.h"
#include "plarena.h"
#include "seccomon.h"
#include "secdert.h"
#include "secasn1t.h"
#include "certt.h"

static void *plContext = NULL;

static void
printUsage(void)
{
    (void)printf("\nUSAGE:\tdumpcert <certFile>\n");
    (void)printf("\tParses a certificate located at <certFile> "
                 "and displays it.\n");
}

static void
printFailure(char *msg)
{
    (void)printf("FAILURE: %s\n", msg);
}

static PKIX_PL_Cert *
createCert(char *inFileName)
{
    PKIX_PL_ByteArray *byteArray = NULL;
    PKIX_PL_Cert *cert = NULL;
    PKIX_Error *error = NULL;
    PRFileDesc *inFile = NULL;
    SECItem certDER;
    void *buf = NULL;
    PKIX_UInt32 len;
    SECStatus rv = SECFailure;

    certDER.data = NULL;

    inFile = PR_Open(inFileName, PR_RDONLY, 0);

    if (!inFile) {
        printFailure("Unable to open cert file");
        goto cleanup;
    } else {
        rv = SECU_ReadDERFromFile(&certDER, inFile, PR_FALSE, PR_FALSE);
        if (!rv) {
            buf = (void *)certDER.data;
            len = certDER.len;

            error = PKIX_PL_ByteArray_Create(buf, len, &byteArray, plContext);

            if (error) {
                printFailure("PKIX_PL_ByteArray_Create failed");
                goto cleanup;
            }

            error = PKIX_PL_Cert_Create(byteArray, &cert, plContext);

            if (error) {
                printFailure("PKIX_PL_Cert_Create failed");
                goto cleanup;
            }
        } else {
            printFailure("Unable to read DER from cert file");
            goto cleanup;
        }
    }

cleanup:

    if (inFile) {
        PR_Close(inFile);
    }

    if (rv == SECSuccess) {
        SECITEM_FreeItem(&certDER, PR_FALSE);
    }

    if (byteArray) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)(byteArray), plContext);
    }

    return (cert);
}

int
dumpcert(int argc, char *argv[])
{

    PKIX_PL_String *string = NULL;
    PKIX_PL_Cert *cert = NULL;
    PKIX_Error *error = NULL;
    char *ascii = NULL;
    PKIX_UInt32 length = 0;
    PKIX_UInt32 j = 0;
    PKIX_Boolean useArenas = PKIX_FALSE;
    PKIX_UInt32 actualMinorVersion;

    PKIX_TEST_STD_VARS();

    if (argc == 1) {
        printUsage();
        return (0);
    }

    useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

    PKIX_Initialize(PKIX_TRUE, /* nssInitNeeded */
                    useArenas,
                    PKIX_MAJOR_VERSION,
                    PKIX_MINOR_VERSION,
                    PKIX_MINOR_VERSION,
                    &actualMinorVersion,
                    &plContext);

    cert = createCert(argv[1 + j]);

    if (cert) {

        error = PKIX_PL_Object_ToString((PKIX_PL_Object *)cert, &string, plContext);

        if (error) {
            printFailure("Unable to get string representation "
                         "of cert");
            goto cleanup;
        }

        error = PKIX_PL_String_GetEncoded(string,
                                          PKIX_ESCASCII,
                                          (void **)&ascii,
                                          &length,
                                          plContext);

        if (error || !ascii) {
            printFailure("Unable to get ASCII encoding of string");
            goto cleanup;
        }

        (void)printf("OUTPUT:\n%s\n", ascii);

    } else {
        printFailure("Unable to create certificate");
        goto cleanup;
    }

cleanup:

    if (cert) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)(cert), plContext);
    }

    if (string) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)(string), plContext);
    }

    if (ascii) {
        PKIX_PL_Free((PKIX_PL_Object *)(ascii), plContext);
    }

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("DUMPCERT");

    return (0);
}
