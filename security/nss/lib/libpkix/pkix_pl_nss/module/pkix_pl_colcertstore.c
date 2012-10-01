/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_colcertstore.c
 *
 * CollectionCertStore Function Definitions
 *
 */

#include "pkix_pl_colcertstore.h"

/*
 * This Object is going to be taken out from libpkix SOON. The following
 * function is copied from nss/cmd library, but not supported by NSS as
 * public API. We use it since ColCertStore are read in Cert/Crl from
 * files and need this support.
 */

static SECStatus
SECU_FileToItem(SECItem *dst, PRFileDesc *src)
{
    PRFileInfo info;
    PRInt32 numBytes;
    PRStatus prStatus;

    prStatus = PR_GetOpenFileInfo(src, &info);

    if (prStatus != PR_SUCCESS) {
        PORT_SetError(SEC_ERROR_IO);
        return SECFailure;
    }

    /* XXX workaround for 3.1, not all utils zero dst before sending */
    dst->data = 0;
    if (!SECITEM_AllocItem(NULL, dst, info.size))
        goto loser;

    numBytes = PR_Read(src, dst->data, info.size);
    if (numBytes != info.size) {
        PORT_SetError(SEC_ERROR_IO);
        goto loser;
    }

    return SECSuccess;
loser:
    SECITEM_FreeItem(dst, PR_FALSE);
    return SECFailure;
}

static SECStatus
SECU_ReadDERFromFile(SECItem *der, PRFileDesc *inFile, PRBool ascii)
{
    SECStatus rv;
    if (ascii) {
        /* First convert ascii to binary */
        SECItem filedata;
        char *asc, *body;

        /* Read in ascii data */
        rv = SECU_FileToItem(&filedata, inFile);
        asc = (char *)filedata.data;
        if (!asc) {
            fprintf(stderr, "unable to read data from input file\n");
            return SECFailure;
        }

        /* check for headers and trailers and remove them */
        if ((body = strstr(asc, "-----BEGIN")) != NULL) {
            char *trailer = NULL;
            asc = body;
            body = PORT_Strchr(body, '\n');
            if (!body)
                body = PORT_Strchr(asc, '\r'); /* maybe this is a MAC file */
            if (body)
                trailer = strstr(++body, "-----END");
            if (trailer != NULL) {
                *trailer = '\0';
            } else {
                fprintf(stderr, "input has header but no trailer\n");
                PORT_Free(filedata.data);
                return SECFailure;
            }
        } else {
            body = asc;
        }
     
        /* Convert to binary */
        rv = ATOB_ConvertAsciiToItem(der, body);
        if (rv) {
            return SECFailure;
        }

        PORT_Free(filedata.data);
    } else {
        /* Read in binary der */
        rv = SECU_FileToItem(der, inFile);
        if (rv) {
            return SECFailure;
        }
    }
    return SECSuccess;
}

/*
 * FUNCTION: PKIX_PL_CollectionCertStoreContext_Create
 * DESCRIPTION:
 *
 *  Creates a new CollectionCertStoreContext using the String pointed to
 *  by "storeDir" and stores it at "pColCertStoreContext".
 *
 * PARAMETERS:
 *  "storeDir"
 *      The absolute path where *.crl and *.crt files are located.
 *  "pColCertStoreContext"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CollectionCertStoreContext Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_Create(
        PKIX_PL_String *storeDir,
        PKIX_PL_CollectionCertStoreContext **pColCertStoreContext,
        void *plContext)
{
        PKIX_PL_CollectionCertStoreContext *colCertStoreContext = NULL;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_Create");
        PKIX_NULLCHECK_TWO(storeDir, pColCertStoreContext);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_COLLECTIONCERTSTORECONTEXT_TYPE,
                    sizeof (PKIX_PL_CollectionCertStoreContext),
                    (PKIX_PL_Object **)&colCertStoreContext,
                    plContext),
                    PKIX_COULDNOTCREATECOLLECTIONCERTSTORECONTEXTOBJECT);

        PKIX_INCREF(storeDir);
        colCertStoreContext->storeDir = storeDir;

        colCertStoreContext->crlList = NULL;
        colCertStoreContext->certList = NULL;

        *pColCertStoreContext = colCertStoreContext;
        colCertStoreContext = NULL;

cleanup:

        PKIX_DECREF(colCertStoreContext);

        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_CollectionCertStoreContext *colCertStoreContext = NULL;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_COLLECTIONCERTSTORECONTEXT_TYPE, plContext),
                    PKIX_OBJECTNOTCOLLECTIONCERTSTORECONTEXT);

        colCertStoreContext = (PKIX_PL_CollectionCertStoreContext *)object;

        PKIX_DECREF(colCertStoreContext->storeDir);
        PKIX_DECREF(colCertStoreContext->crlList);
        PKIX_DECREF(colCertStoreContext->certList);

cleanup:
        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_CollectionCertStoreContext *collectionCSContext = NULL;
        PKIX_UInt32 tempHash = 0;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType
                    (object,
                    PKIX_COLLECTIONCERTSTORECONTEXT_TYPE,
                    plContext),
                    PKIX_OBJECTNOTCOLLECTIONCERTSTORECONTEXT);

        collectionCSContext = (PKIX_PL_CollectionCertStoreContext *)object;

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                    ((PKIX_PL_Object *) collectionCSContext->storeDir,
                    &tempHash,
                    plContext),
                   PKIX_STRINGHASHCODEFAILED);

        *pHashcode = tempHash << 7;

        /* should not hash on crlList and certList, values are dynamic */

cleanup:
        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Int32 *pResult,
        void *plContext)
{
        PKIX_PL_CollectionCertStoreContext *firstCCSContext = NULL;
        PKIX_PL_CollectionCertStoreContext *secondCCSContext = NULL;
        PKIX_Boolean cmpResult = 0;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        PKIX_CHECK(pkix_CheckTypes
                    (firstObject,
                    secondObject,
                    PKIX_COLLECTIONCERTSTORECONTEXT_TYPE,
                    plContext),
                    PKIX_OBJECTNOTCOLLECTIONCERTSTORECONTEXT);

        firstCCSContext = (PKIX_PL_CollectionCertStoreContext *)firstObject;
        secondCCSContext = (PKIX_PL_CollectionCertStoreContext *)secondObject;

        if (firstCCSContext->storeDir == secondCCSContext->storeDir) {

                cmpResult = PKIX_TRUE;

        } else {

                PKIX_CHECK(PKIX_PL_Object_Equals
                    ((PKIX_PL_Object *) firstCCSContext->storeDir,
                    (PKIX_PL_Object *) secondCCSContext->storeDir,
                    &cmpResult,
                    plContext),
                    PKIX_STRINGEQUALSFAILED);
        }

        *pResult = cmpResult;

        /* should not check equal on crlList and certList, data are dynamic */

cleanup:
        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStore_CheckTrust
 * DESCRIPTION:
 * This function checks the trust status of this "cert" that was retrieved
 * from the CertStore "store" and returns its trust status at "pTrusted".
 *
 * PARAMETERS:
 * "store"
 *      Address of the CertStore. Must be non-NULL.
 * "cert"
 *      Address of the Cert. Must be non-NULL.
 * "pTrusted"
 *      Address of PKIX_Boolean where the "cert" trust status is returned.
 *      Must be non-NULL.
 * "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CollectionCertStore_CheckTrust(
        PKIX_CertStore *store,
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pTrusted,
        void *plContext)
{
        PKIX_ENTER(CERTSTORE, "pkix_pl_CollectionCertStore_CheckTrust");
        PKIX_NULLCHECK_THREE(store, cert, pTrusted);

        *pTrusted = PKIX_TRUE;

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_CreateCert
 * DESCRIPTION:
 *
 *  Creates Cert using data file path name pointed to by "certFileName" and
 *  stores it at "pCert". If the Cert can not be decoded, NULL is stored
 *  at "pCert".
 *
 * PARAMETERS
 *  "certFileName" - Address of Cert data file path name. Must be non-NULL.
 *  "pCert" - Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CollectionCertStoreContext Error if the function fails in
 *              a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_CreateCert(
        const char *certFileName,
        PKIX_PL_Cert **pCert,
        void *plContext)
{
        PKIX_PL_ByteArray *byteArray = NULL;
        PKIX_PL_Cert *cert = NULL;
        PRFileDesc *inFile = NULL;
        SECItem certDER;
        void *buf = NULL;
        PKIX_UInt32 len;
        SECStatus rv;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_CreateCert");
        PKIX_NULLCHECK_TWO(certFileName, pCert);

        *pCert = NULL;
        certDER.data = NULL;

        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG("\t\t Calling PR_Open.\n");
        inFile = PR_Open(certFileName, PR_RDONLY, 0);

        if (!inFile){
                PKIX_ERROR(PKIX_UNABLETOOPENCERTFILE);
        } else {
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling SECU_ReadDerFromFile.\n");
                rv = SECU_ReadDERFromFile(&certDER, inFile, PR_FALSE);
                if (!rv){
                        buf = (void *)certDER.data;
                        len = certDER.len;

                        PKIX_CHECK(PKIX_PL_ByteArray_Create
                                    (buf, len, &byteArray, plContext),
                                    PKIX_BYTEARRAYCREATEFAILED);

                        PKIX_CHECK(PKIX_PL_Cert_Create
                                    (byteArray, &cert, plContext),
                                    PKIX_CERTCREATEFAILED);

                        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                                ("\t\t Calling SECITEM_FreeItem.\n");
                        SECITEM_FreeItem(&certDER, PR_FALSE);

                } else {
                        PKIX_ERROR(PKIX_UNABLETOREADDERFROMCERTFILE);
                }
        }

        *pCert = cert;

cleanup:
        if (inFile){
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_CloseDir.\n");
                PR_Close(inFile);
        }

        if (PKIX_ERROR_RECEIVED){
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling SECITEM_FreeItem).\n");
                SECITEM_FreeItem(&certDER, PR_FALSE);

                PKIX_DECREF(cert);
        }
        PKIX_DECREF(byteArray);
        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}


/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_CreateCRL
 * DESCRIPTION:
 *
 *  Creates CRL using data file path name pointed to by "crlFileName" and
 *  stores it at "pCrl". If the CRL can not be decoded, NULL is stored
 *  at "pCrl".
 *
 * PARAMETERS
 *  "crlFileName" - Address of CRL data file path name. Must be non-NULL.
 *  "pCrl" - Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CollectionCertStoreContext Error if the function fails in
 *              a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_CreateCRL(
        const char *crlFileName,
        PKIX_PL_CRL **pCrl,
        void *plContext)
{
        PKIX_PL_ByteArray *byteArray = NULL;
        PKIX_PL_CRL *crl = NULL;
        PRFileDesc *inFile = NULL;
        SECItem crlDER;
        void *buf = NULL;
        PKIX_UInt32 len;
        SECStatus rv;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_CreateCRL");
        PKIX_NULLCHECK_TWO(crlFileName, pCrl);

        *pCrl = NULL;
        crlDER.data = NULL;

        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG("\t\t Calling PR_Open.\n");
        inFile = PR_Open(crlFileName, PR_RDONLY, 0);

        if (!inFile){
                PKIX_ERROR(PKIX_UNABLETOOPENCRLFILE);
        } else {
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling SECU_ReadDerFromFile.\n");
                rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE);
                if (!rv){
                        buf = (void *)crlDER.data;
                        len = crlDER.len;

                        PKIX_CHECK(PKIX_PL_ByteArray_Create
                                (buf, len, &byteArray, plContext),
                                PKIX_BYTEARRAYCREATEFAILED);

                        PKIX_CHECK(PKIX_PL_CRL_Create
                                (byteArray, &crl, plContext),
                                PKIX_CRLCREATEFAILED);

                        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                                ("\t\t Calling SECITEM_FreeItem.\n");
                        SECITEM_FreeItem(&crlDER, PR_FALSE);

                } else {
                        PKIX_ERROR(PKIX_UNABLETOREADDERFROMCRLFILE);
                }
        }

        *pCrl = crl;

cleanup:
        if (inFile){
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_CloseDir.\n");
                PR_Close(inFile);
        }

        if (PKIX_ERROR_RECEIVED){
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling SECITEM_FreeItem).\n");
                SECITEM_FreeItem(&crlDER, PR_FALSE);

                PKIX_DECREF(crl);
                if (crlDER.data != NULL) {
                        SECITEM_FreeItem(&crlDER, PR_FALSE);
                }
        }

        PKIX_DECREF(byteArray);

        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_PopulateCert
 * DESCRIPTION:
 *
 *  Create list of Certs from *.crt files at directory specified in dirName,
 *  Not recursive to sub-directory. Also assume the directory contents are
 *  not changed dynamically.
 *
 * PARAMETERS
 *  "colCertStoreContext" - Address of CollectionCertStoreContext
 *              where the dirName is specified and where the return
 *              Certs are stored as a list. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Not Thread Safe - A lock at top level is required.
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CollectionCertStoreContext Error if the function fails in
 *              a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_PopulateCert(
        PKIX_PL_CollectionCertStoreContext *colCertStoreContext,
        void *plContext)
{
        PKIX_List *certList = NULL;
        PKIX_PL_Cert *certItem = NULL;
        char *dirName = NULL;
        char *pathName = NULL;
        PKIX_UInt32 dirNameLen = 0;
        PRErrorCode prError = 0;
        PRDir *dir = NULL;
        PRDirEntry *dirEntry = NULL;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_PopulateCert");
        PKIX_NULLCHECK_ONE(colCertStoreContext);

        /* convert directory to ascii */

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                    (colCertStoreContext->storeDir,
                    PKIX_ESCASCII,
                    (void **)&dirName,
                    &dirNameLen,
                    plContext),
                    PKIX_STRINGGETENCODEDFAILED);

        /* create cert list, if no cert file, should return an empty list */

        PKIX_CHECK(PKIX_List_Create(&certList, plContext),
                    PKIX_LISTCREATEFAILED);

        /* open directory and read in .crt files */

        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG("\t\t Calling PR_OpenDir.\n");
        dir = PR_OpenDir(dirName);

        if (!dir) {
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG_ARG
                        ("\t\t Directory Name:%s\n", dirName);
                PKIX_ERROR(PKIX_CANNOTOPENCOLLECTIONCERTSTORECONTEXTDIRECTORY);
        }

        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG("\t\t Calling PR_ReadDir.\n");
        dirEntry = PR_ReadDir(dir, PR_SKIP_HIDDEN | PR_SKIP_BOTH);

        if (!dirEntry) {
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Empty directory.\n");
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_GetError.\n");
                prError = PR_GetError();
        }

        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG("\t\t Calling PR_SetError.\n");
        PR_SetError(0, 0);

        while (dirEntry != NULL && prError == 0) {
                if (PL_strrstr(dirEntry->name, ".crt") ==
                    dirEntry->name + PL_strlen(dirEntry->name) - 4) {

                        PKIX_CHECK_ONLY_FATAL
                                (PKIX_PL_Malloc
                                (dirNameLen + PL_strlen(dirEntry->name) + 2,
                                (void **)&pathName,
                                plContext),
                                PKIX_MALLOCFAILED);

                        if ((!PKIX_ERROR_RECEIVED) && (pathName != NULL)){

                                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                                    ("\t\t Calling PL_strcpy for dirName.\n");
                                PL_strcpy(pathName, dirName);
                                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                                    ("\t\t Calling PL_strcat for dirName.\n");
                                PL_strcat(pathName, "/");
                                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                                        ("\t\t Calling PL_strcat for /.\n");
                                PL_strcat(pathName, dirEntry->name);

                                PKIX_CHECK_ONLY_FATAL
                                (pkix_pl_CollectionCertStoreContext_CreateCert
                                    (pathName, &certItem, plContext),
                              PKIX_COLLECTIONCERTSTORECONTEXTCREATECERTFAILED);

                                if (!PKIX_ERROR_RECEIVED){
                                        PKIX_CHECK_ONLY_FATAL
                                                (PKIX_List_AppendItem
                                                (certList,
                                                (PKIX_PL_Object *)certItem,
                                                plContext),
                                                PKIX_LISTAPPENDITEMFAILED);
                                }
                        }

                        PKIX_DECREF(certItem);
                        PKIX_FREE(pathName);
                }

                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_SetError.\n");
                PR_SetError(0, 0);

                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_ReadDir.\n");
                dirEntry = PR_ReadDir(dir, PR_SKIP_HIDDEN | PR_SKIP_BOTH);

                if (!dirEntry) {
                    PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_GetError.\n");
                    prError = PR_GetError();
                }
        }

        if ((prError != 0) && (prError != PR_NO_MORE_FILES_ERROR)) {
                PKIX_ERROR(PKIX_COLLECTIONCERTSTOREPOPULATECERTFAILED);
        }

        PKIX_CHECK(PKIX_List_SetImmutable(certList, plContext),
                    PKIX_LISTSETIMMUTABLEFAILED);

        PKIX_INCREF(certList);
        colCertStoreContext->certList = certList;

cleanup:
        if (dir) {
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_CloseDir.\n");
                PR_CloseDir(dir);
        }

        PKIX_FREE(pathName);
        PKIX_FREE(dirName);

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(certList);
        }

        PKIX_DECREF(certItem);
        PKIX_DECREF(certList);

        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_PopulateCRL
 * DESCRIPTION:
 *
 *  Create list of CRLs from *.crl files at directory specified in dirName,
 *  Not recursive to sub-dirctory. Also assume the directory contents are
 *  not changed dynamically.
 *
 * PARAMETERS
 *  "colCertStoreContext" - Address of CollectionCertStoreContext
 *              where the dirName is specified and where the return
 *              CRLs are stored as a list. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Not Thread Safe - A lock at top level is required.
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CollectionCertStoreContext Error if the function fails in
 *              a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_PopulateCRL(
        PKIX_PL_CollectionCertStoreContext *colCertStoreContext,
        void *plContext)
{
        PKIX_List *crlList = NULL;
        PKIX_PL_CRL *crlItem = NULL;
        char *dirName = NULL;
        char *pathName = NULL;
        PKIX_UInt32 dirNameLen = 0;
        PRErrorCode prError = 0;
        PRDir *dir = NULL;
        PRDirEntry *dirEntry = NULL;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_PopulateCRL");
        PKIX_NULLCHECK_ONE(colCertStoreContext);

        /* convert directory to ascii */

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                    (colCertStoreContext->storeDir,
                    PKIX_ESCASCII,
                    (void **)&dirName,
                    &dirNameLen,
                    plContext),
                    PKIX_STRINGGETENCODEDFAILED);

        /* create CRL list, if no CRL file, should return an empty list */

        PKIX_CHECK(PKIX_List_Create(&crlList, plContext),
                    PKIX_LISTCREATEFAILED);

        /* open directory and read in .crl files */

        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG("\t\t Calling PR_OpenDir.\n");
        dir = PR_OpenDir(dirName);

        if (!dir) {
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG_ARG
                        ("\t\t Directory Name:%s\n", dirName);
                PKIX_ERROR(PKIX_CANNOTOPENCOLLECTIONCERTSTORECONTEXTDIRECTORY);
        }

        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG("\t\t Calling PR_ReadDir.\n");
        dirEntry = PR_ReadDir(dir, PR_SKIP_HIDDEN | PR_SKIP_BOTH);

        if (!dirEntry) {
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Empty directory.\n");
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_GetError.\n");
                prError = PR_GetError();
        }

        PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG("\t\t Calling PR_SetError.\n");
        PR_SetError(0, 0);

        while (dirEntry != NULL && prError == 0) {
                if (PL_strrstr(dirEntry->name, ".crl") ==
                    dirEntry->name + PL_strlen(dirEntry->name) - 4) {

                        PKIX_CHECK_ONLY_FATAL
                                (PKIX_PL_Malloc
                                (dirNameLen + PL_strlen(dirEntry->name) + 2,
                                (void **)&pathName,
                                plContext),
                                PKIX_MALLOCFAILED);

                        if ((!PKIX_ERROR_RECEIVED) && (pathName != NULL)){

                                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                                    ("\t\t Calling PL_strcpy for dirName.\n");
                                PL_strcpy(pathName, dirName);
                                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                                    ("\t\t Calling PL_strcat for dirName.\n");
                                PL_strcat(pathName, "/");
                                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                                        ("\t\t Calling PL_strcat for /.\n");
                                PL_strcat(pathName, dirEntry->name);

                        PKIX_CHECK_ONLY_FATAL
                                (pkix_pl_CollectionCertStoreContext_CreateCRL
                                (pathName, &crlItem, plContext),
                                PKIX_COLLECTIONCERTSTORECONTEXTCREATECRLFAILED);

                                if (!PKIX_ERROR_RECEIVED){
                                        PKIX_CHECK_ONLY_FATAL
                                                (PKIX_List_AppendItem
                                                (crlList,
                                                (PKIX_PL_Object *)crlItem,
                                                plContext),
                                                PKIX_LISTAPPENDITEMFAILED);
                                }
                        }

                        PKIX_DECREF(crlItem);
                        PKIX_FREE(pathName);
                }

                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_SetError.\n");
                PR_SetError(0, 0);

                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_ReadDir.\n");
                dirEntry = PR_ReadDir(dir, PR_SKIP_HIDDEN | PR_SKIP_BOTH);

                if (!dirEntry) {
                    PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_GetError.\n");
                    prError = PR_GetError();
                }
        }

        if ((prError != 0) && (prError != PR_NO_MORE_FILES_ERROR)) {
                PKIX_ERROR(PKIX_COLLECTIONCERTSTORECONTEXTGETSELECTCRLFAILED);
        }

        PKIX_CHECK(PKIX_List_SetImmutable(crlList, plContext),
                    PKIX_LISTSETIMMUTABLEFAILED);

        PKIX_INCREF(crlList);
        colCertStoreContext->crlList = crlList;

cleanup:
        if (dir) {
                PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG
                        ("\t\t Calling PR_CloseDir.\n");
                PR_CloseDir(dir);
        }

        PKIX_FREE(pathName);
        PKIX_FREE(dirName);

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(crlList);
        }

        PKIX_DECREF(crlItem);
        PKIX_DECREF(crlList);

        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_GetSelectedCert
 * DESCRIPTION:
 *
 *  Finds the Certs that match the criterion of the CertSelector pointed
 *  to by "selector" using the List of Certs pointed to by "certList" and
 *  stores the matching Certs at "pSelectedCertList".
 *
 *  Not recursive to sub-directory.
 *
 * PARAMETERS
 *  "certList" - Address of List of Certs to be searched. Must be non-NULL.
 *  "colCertStoreContext" - Address of CollectionCertStoreContext
 *              where the cached Certs are stored.
 *  "selector" - CertSelector for chosing Cert based on Params set
 *  "pSelectedCertList" - Certs that qualified by selector.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Not Thread Safe - A lock at top level is required.
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CollectionCertStoreContext Error if the function fails in
 *              a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_GetSelectedCert(
        PKIX_List *certList,
        PKIX_CertSelector *selector,
        PKIX_List **pSelectedCertList,
        void *plContext)
{
        PKIX_List *selectCertList = NULL;
        PKIX_PL_Cert *certItem = NULL;
        PKIX_CertSelector_MatchCallback certSelectorMatch = NULL;
        PKIX_UInt32 numCerts = 0;
        PKIX_UInt32 i = 0;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_GetSelectedCert");
        PKIX_NULLCHECK_THREE(certList, selector, pSelectedCertList);

        PKIX_CHECK(PKIX_CertSelector_GetMatchCallback
                    (selector, &certSelectorMatch, plContext),
                    PKIX_CERTSELECTORGETMATCHCALLBACKFAILED);

        PKIX_CHECK(PKIX_List_GetLength(certList, &numCerts, plContext),
                    PKIX_LISTGETLENGTHFAILED);

        if (certSelectorMatch) {

                PKIX_CHECK(PKIX_List_Create(&selectCertList, plContext),
                            PKIX_LISTCREATEFAILED);

                for (i = 0; i < numCerts; i++) {
                        PKIX_CHECK_ONLY_FATAL
                                (PKIX_List_GetItem
                                (certList,
                                i,
                                (PKIX_PL_Object **) &certItem,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                        if (!PKIX_ERROR_RECEIVED){
                                PKIX_CHECK_ONLY_FATAL
                                        (certSelectorMatch
                                        (selector, certItem, plContext),
                                        PKIX_CERTSELECTORMATCHFAILED);

                                if (!PKIX_ERROR_RECEIVED){
                                        PKIX_CHECK_ONLY_FATAL
                                                (PKIX_List_AppendItem
                                                (selectCertList,
                                                (PKIX_PL_Object *)certItem,
                                                plContext),
                                                PKIX_LISTAPPENDITEMFAILED);
                                }
                        }

                        PKIX_DECREF(certItem);
                }

        } else {

                PKIX_INCREF(certList);

                selectCertList = certList;
        }

        *pSelectedCertList = selectCertList;

cleanup:
        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_GetSelectedCRL
 * DESCRIPTION:
 *
 *  Finds the CRLs that match the criterion of the CRLSelector pointed
 *  to by "selector" using the List of CRLs pointed to by "crlList" and
 *  stores the matching CRLs at "pSelectedCrlList".
 *
 *  Not recursive to sub-directory.
 *
 * PARAMETERS
 *  "crlList" - Address of List of CRLs to be searched. Must be non-NULL
 *  "selector" - CRLSelector for chosing CRL based on Params set
 *  "pSelectedCrlList" - CRLs that qualified by selector.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Not Thread Safe - A lock at top level is required.
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CollectionCertStoreContext Error if the function fails in
 *              a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CollectionCertStoreContext_GetSelectedCRL(
        PKIX_List *crlList,
        PKIX_CRLSelector *selector,
        PKIX_List **pSelectedCrlList,
        void *plContext)
{
        PKIX_List *selectCrlList = NULL;
        PKIX_PL_CRL *crlItem = NULL;
        PKIX_CRLSelector_MatchCallback crlSelectorMatch = NULL;
        PKIX_UInt32 numCrls = 0;
        PKIX_UInt32 i = 0;
        PKIX_Boolean match = PKIX_FALSE;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_GetSelectedCRL");
        PKIX_NULLCHECK_THREE(crlList, selector, pSelectedCrlList);

        PKIX_CHECK(PKIX_CRLSelector_GetMatchCallback
                    (selector, &crlSelectorMatch, plContext),
                    PKIX_CRLSELECTORGETMATCHCALLBACKFAILED);

        PKIX_CHECK(PKIX_List_GetLength(crlList, &numCrls, plContext),
                    PKIX_LISTGETLENGTHFAILED);

        if (crlSelectorMatch) {

                PKIX_CHECK(PKIX_List_Create(&selectCrlList, plContext),
                            PKIX_LISTCREATEFAILED);

                for (i = 0; i < numCrls; i++) {
                        PKIX_CHECK_ONLY_FATAL(PKIX_List_GetItem
                                (crlList,
                                i,
                                (PKIX_PL_Object **) &crlItem,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                        if (!PKIX_ERROR_RECEIVED){
                                PKIX_CHECK_ONLY_FATAL
                                        (crlSelectorMatch
                                        (selector, crlItem, &match, plContext),
                                        PKIX_CRLSELECTORMATCHFAILED);

                                if (!(PKIX_ERROR_RECEIVED) && match) {
                                        PKIX_CHECK_ONLY_FATAL
                                                (PKIX_List_AppendItem
                                                (selectCrlList,
                                                (PKIX_PL_Object *)crlItem,
                                                plContext),
                                                PKIX_LISTAPPENDITEMFAILED);
                                }
                        }

                        PKIX_DECREF(crlItem);
                }
        } else {

                PKIX_INCREF(crlList);

                selectCrlList = crlList;
        }

        /* Don't throw away the list if one CRL was bad! */
        pkixTempErrorReceived = PKIX_FALSE;

        *pSelectedCrlList = selectCrlList;

cleanup:
        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStore_GetCert
 * DESCRIPTION:
 *
 *  Retrieve Certs in a list of PKIX_PL_Cert object.
 *
 * PARAMETERS:
 *  "colCertStoreContext"
 *      The object CertStore is the object passed in by checker call.
 *  "crlSelector"
 *      CRLSelector specifies criteria for chosing CRL's
 *  "pNBIOContext"
 *      Address where platform-dependent information is returned for CertStores
 *      that use non-blocking I/O. Must be non-NULL.
 *  "pCertList"
 *      Address where object pointer will be returned. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CollectionCertStore_GetCert(
        PKIX_CertStore *certStore,
        PKIX_CertSelector *selector,
        PKIX_VerifyNode *verifyNode,
        void **pNBIOContext,
        PKIX_List **pCerts,
        void *plContext)
{
        PKIX_PL_CollectionCertStoreContext *colCertStoreContext = NULL;
        PKIX_List *selectedCerts = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_CollectionCertStore_GetCert");
        PKIX_NULLCHECK_FOUR(certStore, selector, pNBIOContext, pCerts);

        *pNBIOContext = NULL;   /* We don't use non-blocking I/O */

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                    (certStore,
                    (PKIX_PL_Object **) &colCertStoreContext,
                    plContext),
                    PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        if (colCertStoreContext->certList == NULL) {

                PKIX_OBJECT_LOCK(colCertStoreContext);

                /*
                 * Certs in the directory are cached based on the
                 * assumption that the directory contents won't be
                 * changed dynamically.
                 */
                if (colCertStoreContext->certList == NULL){
                    PKIX_CHECK(pkix_pl_CollectionCertStoreContext_PopulateCert
                            (colCertStoreContext, plContext),
                            PKIX_COLLECTIONCERTSTORECONTEXTPOPULATECERTFAILED);
                }

                PKIX_OBJECT_UNLOCK(colCertStoreContext);
        }

        PKIX_CHECK(pkix_pl_CollectionCertStoreContext_GetSelectedCert
                    (colCertStoreContext->certList,
                    selector,
                    &selectedCerts,
                    plContext),
                    PKIX_COLLECTIONCERTSTORECONTEXTGETSELECTCERTFAILED);

        *pCerts = selectedCerts;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_DECREF(colCertStoreContext);
        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStore_GetCRL
 * DESCRIPTION:
 *
 *  Retrieve CRL's in a list of PKIX_PL_CRL object.
 *
 * PARAMETERS:
 *  "colCertStoreContext"
 *      The object CertStore is passed in by checker call.
 *  "crlSelector"
 *      CRLSelector specifies criteria for chosing CRL's
 *  "pNBIOContext"
 *      Address where platform-dependent information is returned for CertStores
 *      that use non-blocking I/O. Must be non-NULL.
 *  "pCrlList"
 *      Address where object pointer will be returned. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CollectionCertStore_GetCRL(
        PKIX_CertStore *certStore,
        PKIX_CRLSelector *selector,
        void **pNBIOContext,
        PKIX_List **pCrlList,
        void *plContext)
{
        PKIX_PL_CollectionCertStoreContext *colCertStoreContext = NULL;
        PKIX_List *selectCrl = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_CollectionCertStore_GetCRL");
        PKIX_NULLCHECK_FOUR(certStore, selector, pNBIOContext, pCrlList);

        *pNBIOContext = NULL;   /* We don't use non-blocking I/O */

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                    (certStore,
                    (PKIX_PL_Object **) &colCertStoreContext,
                    plContext),
                    PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        if (colCertStoreContext->crlList == NULL) {

                PKIX_OBJECT_LOCK(colCertStoreContext);

                /*
                 * CRLs in the directory are cached based on the
                 * assumption that the directory contents won't be
                 * changed dynamically.
                 */
                if (colCertStoreContext->crlList == NULL){
                    PKIX_CHECK(pkix_pl_CollectionCertStoreContext_PopulateCRL
                            (colCertStoreContext, plContext),
                            PKIX_COLLECTIONCERTSTORECONTEXTPOPULATECRLFAILED);
                }

                PKIX_OBJECT_UNLOCK(colCertStoreContext);

        }

        PKIX_CHECK(pkix_pl_CollectionCertStoreContext_GetSelectedCRL
                    (colCertStoreContext->crlList,
                    selector,
                    &selectCrl,
                    plContext),
                    PKIX_COLLECTIONCERTSTORECONTEXTGETSELECTCRLFAILED);

        *pCrlList = selectCrl;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_DECREF(colCertStoreContext);
        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_CollectionCertStoreContext_RegisterSelf
 * DESCRIPTION:
 *
 *  Registers PKIX_PL_COLLECTIONCERTSTORECONTEXT_TYPE and its related
 *  functions with systemClasses[]
 *
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_CollectionCertStoreContext_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(COLLECTIONCERTSTORECONTEXT,
                    "pkix_pl_CollectionCertStoreContext_RegisterSelf");

        entry.description = "CollectionCertStoreContext";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_CollectionCertStoreContext);
        entry.destructor = pkix_pl_CollectionCertStoreContext_Destroy;
        entry.equalsFunction = pkix_pl_CollectionCertStoreContext_Equals;
        entry.hashcodeFunction = pkix_pl_CollectionCertStoreContext_Hashcode;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_COLLECTIONCERTSTORECONTEXT_TYPE] = entry;

        PKIX_RETURN(COLLECTIONCERTSTORECONTEXT);
}

/* --Public-CollectionCertStoreContext-Functions--------------------------- */

/*
 * FUNCTION: PKIX_PL_CollectionCertStore_Create
 * (see comments in pkix_samples_modules.h)
 */
PKIX_Error *
PKIX_PL_CollectionCertStore_Create(
        PKIX_PL_String *storeDir,
        PKIX_CertStore **pCertStore,
        void *plContext)
{
        PKIX_PL_CollectionCertStoreContext *colCertStoreContext = NULL;
        PKIX_CertStore *certStore = NULL;

        PKIX_ENTER(CERTSTORE, "PKIX_PL_CollectionCertStore_Create");
        PKIX_NULLCHECK_TWO(storeDir, pCertStore);

        PKIX_CHECK(pkix_pl_CollectionCertStoreContext_Create
                    (storeDir, &colCertStoreContext, plContext),
                    PKIX_COULDNOTCREATECOLLECTIONCERTSTORECONTEXTOBJECT);

        PKIX_CHECK(PKIX_CertStore_Create
                    (pkix_pl_CollectionCertStore_GetCert,
                    pkix_pl_CollectionCertStore_GetCRL,
                    NULL, /* GetCertContinue */
                    NULL, /* GetCRLContinue */
                    pkix_pl_CollectionCertStore_CheckTrust,
                    NULL,      /* can not store crls */
                    NULL,      /* can not do revocation check */
                    (PKIX_PL_Object *)colCertStoreContext,
                    PKIX_TRUE, /* cache flag */
                    PKIX_TRUE, /* local - no network I/O */
                    &certStore,
                    plContext),
                    PKIX_CERTSTORECREATEFAILED);

        PKIX_DECREF(colCertStoreContext);

        *pCertStore = certStore;

cleanup:
        PKIX_RETURN(CERTSTORE);
}
