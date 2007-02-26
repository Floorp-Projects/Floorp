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
 *   Sun Microsystems
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

void *plContext = NULL;

void printUsage(void){
        (void) printf("\nUSAGE:\tdumpcrl <crlFile>\n");
        (void) printf("\tParses a CRL located at <crlFile> "
                "and displays it.\n");
}

void printFailure(char *msg){
        (void) printf("FAILURE: %s\n", msg);
}

PKIX_PL_CRL *
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
                rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE);
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

int main(int argc, char *argv[])
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
