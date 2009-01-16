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
 * nss_pkix_proxy.h
 *
 * PKIX - NSS proxy functions
 *
 */
#include "cert.h"
#include "pkix_pl_common.h"

#ifdef DEBUG

char *
pkix_Error2ASCII(PKIX_Error *error, void *plContext)
{
        PKIX_UInt32 length;
        char *asciiString = NULL;
        PKIX_PL_String *pkixString = NULL;
        PKIX_Error *errorResult = NULL;

        errorResult = PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)error, &pkixString, plContext);
        if (errorResult) goto cleanup;

        errorResult = PKIX_PL_String_GetEncoded
                (pkixString,
                PKIX_ESCASCII,
                (void **)&asciiString,
                &length,
                plContext);

cleanup:

        if (pkixString){
                if (PKIX_PL_Object_DecRef
                    ((PKIX_PL_Object*)pkixString, plContext)){
                        return (NULL);
                }
        }

        if (errorResult){
            PKIX_PL_Object_DecRef((PKIX_PL_Object*)errorResult, plContext);
            return (NULL);
        }

        return (asciiString);
}

char *
pkix_Object2ASCII(PKIX_PL_Object *object)
{
        PKIX_UInt32 length;
        char *asciiString = NULL;
        PKIX_PL_String *pkixString = NULL;
        PKIX_Error *errorResult = NULL;

        errorResult = PKIX_PL_Object_ToString
                (object, &pkixString, NULL);
        if (errorResult) goto cleanup;

        errorResult = PKIX_PL_String_GetEncoded
            (pkixString, PKIX_ESCASCII, (void **)&asciiString, &length, NULL);

cleanup:

        if (pkixString){
                if (PKIX_PL_Object_DecRef((PKIX_PL_Object*)pkixString, NULL)){
                        return (NULL);
                }
        }

        if (errorResult){
                return (NULL);
        }

        return (asciiString);
}

char *
pkix_Cert2ASCII(PKIX_PL_Cert *cert)
{
        PKIX_PL_X500Name *issuer = NULL;
        void *issuerAscii = NULL;
        PKIX_PL_X500Name *subject = NULL;
        void *subjectAscii = NULL;
        void *asciiString = NULL;
        PKIX_Error *errorResult = NULL;
        PKIX_UInt32 numChars;
        PKIX_UInt32 refCount = 0;

        /* Issuer */
        errorResult = PKIX_PL_Cert_GetIssuer(cert, &issuer, NULL);
        if (errorResult) goto cleanup;

        issuerAscii = pkix_Object2ASCII((PKIX_PL_Object*)issuer);

        /* Subject */
        errorResult = PKIX_PL_Cert_GetSubject(cert, &subject, NULL);
        if (errorResult) goto cleanup;

        if (subject){
                subjectAscii = pkix_Object2ASCII((PKIX_PL_Object*)subject);
        }

/*         errorResult = PKIX_PL_Object_GetRefCount((PKIX_PL_Object*)cert, &refCount, NULL); */
        if (errorResult) goto cleanup;

        errorResult = PKIX_PL_Malloc(200, &asciiString, NULL);
        if (errorResult) goto cleanup;

        numChars =
                PR_snprintf
                (asciiString,
                200,
                "Ref: %d   Issuer=%s\nSubject=%s\n",
                 refCount,
                issuerAscii,
                subjectAscii);

        if (!numChars) goto cleanup;

cleanup:

        if (issuer){
                if (PKIX_PL_Object_DecRef((PKIX_PL_Object*)issuer, NULL)){
                        return (NULL);
                }
        }

        if (subject){
                if (PKIX_PL_Object_DecRef((PKIX_PL_Object*)subject, NULL)){
                        return (NULL);
                }
        }

        if (PKIX_PL_Free((PKIX_PL_Object*)issuerAscii, NULL)){
                return (NULL);
        }

        if (PKIX_PL_Free((PKIX_PL_Object*)subjectAscii, NULL)){
                return (NULL);
        }

        if (errorResult){
                return (NULL);
        }

        return (asciiString);
}

PKIX_Error *
cert_PrintCertChain(
    PKIX_List *pkixCertChain,
    void *plContext)
{
    PKIX_PL_Cert *cert = NULL;
    PKIX_UInt32 numCerts = 0, i = 0;
    char *asciiResult = NULL;
    
    PKIX_ENTER(CERTVFYPKIX, "cert_PrintCertChain");

    PKIX_CHECK(
        PKIX_List_GetLength(pkixCertChain, &numCerts, plContext),
        PKIX_LISTGETLENGTHFAILED);
    
    fprintf(stderr, "\n");
    
    for (i = 0; i < numCerts; i++){
        PKIX_CHECK
            (PKIX_List_GetItem
             (pkixCertChain, i, (PKIX_PL_Object**)&cert, plContext),
             PKIX_LISTGETITEMFAILED);
        
        asciiResult = pkix_Cert2ASCII(cert);
        
        fprintf(stderr, "CERT[%d]:\n%s\n", i, asciiResult);
        
        PKIX_PL_Free(asciiResult, plContext);
        asciiResult = NULL;
        
        PKIX_DECREF(cert);
    }

cleanup:
    PKIX_DECREF(cert);

    PKIX_RETURN(CERTVFYPKIX);
}

void
cert_PrintCert(
    PKIX_PL_Cert *pkixCert,
    void *plContext)
{
    char *asciiResult = NULL;
    
    asciiResult = pkix_Cert2ASCII(pkixCert);
    
    fprintf(stderr, "CERT[0]:\n%s\n", asciiResult);
    
    PKIX_PL_Free(asciiResult, plContext);
}

#endif /* DEBUG */
