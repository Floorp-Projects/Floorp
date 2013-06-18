/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * dumpcrl.c
 *
 * dump CRL sample application
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

static 
void printUsage(void){
        (void) printf("\nUSAGE:\tdumpcrl <crlFile>\n");
        (void) printf("\tParses a CRL located at <crlFile> "
                "and displays it.\n");
}

static 
void printFailure(char *msg){
        (void) printf("FAILURE: %s\n", msg);
}

static PKIX_PL_CRL *
createCRL(char *inFileName)
{
        PKIX_PL_ByteArray *byteArray = NULL;
        PKIX_PL_CRL *crl = NULL;
        PKIX_Error *error = NULL;
        PRFileDesc *inFile = NULL;
        SECItem crlDER;
        void *buf = NULL;
        PKIX_UInt32 len;
        SECStatus rv;

        PKIX_TEST_STD_VARS();

        crlDER.data = NULL;

        inFile = PR_Open(inFileName, PR_RDONLY, 0);

        if (!inFile){
                printFailure("Unable to open crl file");
                goto cleanup;
        } else {
                rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE, PR_FALSE);
                if (!rv){
                        buf = (void *)crlDER.data;
                        len = crlDER.len;

                        error = PKIX_PL_ByteArray_Create
                                (buf, len, &byteArray, plContext);

                        if (error){
                                printFailure("PKIX_PL_ByteArray_Create failed");
                                goto cleanup;
                        }

                        error = PKIX_PL_CRL_Create(byteArray, &crl, plContext);
                        if (error){
                                printFailure("PKIX_PL_CRL_Create failed");
                                goto cleanup;
                        }

                        SECITEM_FreeItem(&crlDER, PR_FALSE);
                } else {
                        printFailure("Unable to read DER from crl file");
                        goto cleanup;
                }
        }

cleanup:

        if (inFile){
                PR_Close(inFile);
        }

        if (error){
                SECITEM_FreeItem(&crlDER, PR_FALSE);
        }

        if (byteArray){
                PKIX_PL_Object_DecRef((PKIX_PL_Object *)(byteArray), plContext);
        }

        PKIX_TEST_RETURN();

        return (crl);
}

int dumpcrl(int argc, char *argv[])
{

        PKIX_PL_String *string = NULL;
        PKIX_PL_CRL *crl = NULL;
        PKIX_Error *error = NULL;
        char *ascii = NULL;
        PKIX_UInt32 length;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        if (argc == 1){
                printUsage();
                return (0);
        }

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_Initialize
                (PKIX_TRUE, /* nssInitNeeded */
                useArenas,
                PKIX_MAJOR_VERSION,
                PKIX_MINOR_VERSION,
                PKIX_MINOR_VERSION,
                &actualMinorVersion,
                &plContext);

        crl = createCRL(argv[j+1]);

        if (crl){

                error = PKIX_PL_Object_ToString
                        ((PKIX_PL_Object *)crl, &string, plContext);

                if (error){
                        printFailure("Unable to get string representation "
                                    "of crl");
                        goto cleanup;
                }

                error = PKIX_PL_String_GetEncoded
                        (string,
                        PKIX_ESCASCII,
                        (void **)&ascii,
                        &length,
                        plContext);
                if (error || !ascii){
                        printFailure("Unable to get ASCII encoding of string");
                        goto cleanup;
                }

                (void) printf("OUTPUT:\n%s\n", ascii);

        } else {
                printFailure("Unable to create CRL");
                goto cleanup;
        }

cleanup:

        if (crl){
                PKIX_PL_Object_DecRef((PKIX_PL_Object *)(crl), plContext);
        }

        if (string){
                PKIX_PL_Object_DecRef((PKIX_PL_Object *)(string), plContext);
        }

        if (ascii){
                PKIX_PL_Free((PKIX_PL_Object *)(ascii), plContext);
        }

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("DUMPCRL");

        return (0);
}
