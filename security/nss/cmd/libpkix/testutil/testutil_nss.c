/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * testutil_nss.c
 *
 * NSS-specific utility functions for handling test errors
 *
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "pkix_pl_generalname.h"
#include "pkix_pl_cert.h"
#include "pkix.h"
#include "testutil.h"
#include "prlong.h"
#include "plstr.h"
#include "prthread.h"
#include "secutil.h"
#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "pk11func.h"
#include "secasn1.h"
#include "cert.h"
#include "cryptohi.h"
#include "secoid.h"
#include "certdb.h"
#include "secitem.h"
#include "keythi.h"
#include "nss.h"

static char *
catDirName(char *dir, char *name, void *plContext)
{
    char *pathName = NULL;
    PKIX_UInt32 nameLen;
    PKIX_UInt32 dirLen;

    PKIX_TEST_STD_VARS();

    nameLen = PL_strlen(name);
    dirLen = PL_strlen(dir);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Malloc(dirLen + nameLen + 2,
                                             (void **)&pathName,
                                             plContext));

    PL_strcpy(pathName, dir);
    PL_strcat(pathName, "/");
    PL_strcat(pathName, name);
    printf("pathName = %s\n", pathName);

cleanup:

    PKIX_TEST_RETURN();

    return (pathName);
}

PKIX_PL_Cert *
createCert(
    char *dirName,
    char *certFileName,
    void *plContext)
{
    PKIX_PL_ByteArray *byteArray = NULL;
    void *buf = NULL;
    PRFileDesc *certFile = NULL;
    PKIX_UInt32 len;
    SECItem certDER;
    SECStatus rv;
    /* default: NULL cert (failure case) */
    PKIX_PL_Cert *cert = NULL;
    char *pathName = NULL;

    PKIX_TEST_STD_VARS();

    certDER.data = NULL;

    pathName = catDirName(dirName, certFileName, plContext);
    certFile = PR_Open(pathName, PR_RDONLY, 0);

    if (!certFile) {
        pkixTestErrorMsg = "Unable to open cert file";
        goto cleanup;
    } else {
        rv = SECU_ReadDERFromFile(&certDER, certFile, PR_FALSE, PR_FALSE);
        if (!rv) {
            buf = (void *)certDER.data;
            len = certDER.len;

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_Create(buf, len, &byteArray, plContext));

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_Create(byteArray, &cert, plContext));

            SECITEM_FreeItem(&certDER, PR_FALSE);
        } else {
            pkixTestErrorMsg = "Unable to read DER from cert file";
            goto cleanup;
        }
    }

cleanup:

    pkixTestErrorResult = PKIX_PL_Free(pathName, plContext);

    if (certFile) {
        PR_Close(certFile);
    }

    if (PKIX_TEST_ERROR_RECEIVED) {
        SECITEM_FreeItem(&certDER, PR_FALSE);
    }

    PKIX_TEST_DECREF_AC(byteArray);

    PKIX_TEST_RETURN();

    return (cert);
}

PKIX_PL_CRL *
createCRL(
    char *dirName,
    char *crlFileName,
    void *plContext)
{
    PKIX_PL_ByteArray *byteArray = NULL;
    PKIX_PL_CRL *crl = NULL;
    PKIX_Error *error = NULL;
    PRFileDesc *inFile = NULL;
    SECItem crlDER;
    void *buf = NULL;
    PKIX_UInt32 len;
    SECStatus rv;
    char *pathName = NULL;

    PKIX_TEST_STD_VARS();

    crlDER.data = NULL;

    pathName = catDirName(dirName, crlFileName, plContext);
    inFile = PR_Open(pathName, PR_RDONLY, 0);

    if (!inFile) {
        pkixTestErrorMsg = "Unable to open crl file";
        goto cleanup;
    } else {
        rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE, PR_FALSE);
        if (!rv) {
            buf = (void *)crlDER.data;
            len = crlDER.len;

            error = PKIX_PL_ByteArray_Create(buf, len, &byteArray, plContext);

            if (error) {
                pkixTestErrorMsg =
                    "PKIX_PL_ByteArray_Create failed";
                goto cleanup;
            }

            error = PKIX_PL_CRL_Create(byteArray, &crl, plContext);
            if (error) {
                pkixTestErrorMsg = "PKIX_PL_Crl_Create failed";
                goto cleanup;
            }

            SECITEM_FreeItem(&crlDER, PR_FALSE);
        } else {
            pkixTestErrorMsg = "Unable to read DER from crl file";
            goto cleanup;
        }
    }

cleanup:

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(pathName, plContext));

    if (inFile) {
        PR_Close(inFile);
    }

    if (error) {
        SECITEM_FreeItem(&crlDER, PR_FALSE);
    }

    PKIX_TEST_DECREF_AC(byteArray);

    PKIX_TEST_RETURN();

    return (crl);
}

PKIX_TrustAnchor *
createTrustAnchor(
    char *dirName,
    char *certFileName,
    PKIX_Boolean useCert,
    void *plContext)
{
    PKIX_TrustAnchor *anchor = NULL;
    PKIX_PL_Cert *cert = NULL;
    PKIX_PL_X500Name *name = NULL;
    PKIX_PL_PublicKey *pubKey = NULL;
    PKIX_PL_CertNameConstraints *nameConstraints = NULL;

    PKIX_TEST_STD_VARS();

    cert = createCert(dirName, certFileName, plContext);

    if (useCert) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_CreateWithCert(cert, &anchor, plContext));
    } else {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubject(cert, &name, plContext));

        if (name == NULL) {
            goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey(cert, &pubKey, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints(cert, &nameConstraints, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_CreateWithNameKeyPair(name, pubKey, nameConstraints, &anchor, plContext));
    }

cleanup:

    if (PKIX_TEST_ERROR_RECEIVED) {
        PKIX_TEST_DECREF_AC(anchor);
    }

    PKIX_TEST_DECREF_AC(cert);
    PKIX_TEST_DECREF_AC(name);
    PKIX_TEST_DECREF_AC(pubKey);
    PKIX_TEST_DECREF_AC(nameConstraints);

    PKIX_TEST_RETURN();

    return (anchor);
}

PKIX_List *
createCertChain(
    char *dirName,
    char *firstCertFileName,
    char *secondCertFileName,
    void *plContext)
{
    PKIX_PL_Cert *firstCert = NULL;
    PKIX_PL_Cert *secondCert = NULL;
    PKIX_List *certList = NULL;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&certList, plContext));

    firstCert = createCert(dirName, firstCertFileName, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(certList, (PKIX_PL_Object *)firstCert, plContext));

    if (secondCertFileName) {
        secondCert = createCert(dirName, secondCertFileName, plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(certList, (PKIX_PL_Object *)secondCert, plContext));
    }

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetImmutable(certList, plContext));

cleanup:

    if (PKIX_TEST_ERROR_RECEIVED) {
        PKIX_TEST_DECREF_AC(certList);
    }

    PKIX_TEST_DECREF_AC(firstCert);
    PKIX_TEST_DECREF_AC(secondCert);

    PKIX_TEST_RETURN();

    return (certList);
}

PKIX_List *
createCertChainPlus(
    char *dirName,
    char *certNames[],
    PKIX_PL_Cert *certs[],
    PKIX_UInt32 numCerts,
    void *plContext)
{
    PKIX_List *certList = NULL;
    PKIX_UInt32 i;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&certList, plContext));

    for (i = 0; i < numCerts; i++) {

        certs[i] = createCert(dirName, certNames[i], plContext);

        /* Create Cert may fail */
        if (certs[i] == NULL) {
            PKIX_TEST_DECREF_BC(certList);
            goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(certList,
                                                       (PKIX_PL_Object *)certs[i],
                                                       plContext));
    }

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetImmutable(certList, plContext));

cleanup:

    if (PKIX_TEST_ERROR_RECEIVED) {
        PKIX_TEST_DECREF_AC(certList);
    }

    for (i = 0; i < numCerts; i++) {
        PKIX_TEST_DECREF_AC(certs[i]);
    }

    PKIX_TEST_RETURN();

    return (certList);
}

PKIX_PL_Date *
createDate(
    char *asciiDate,
    void *plContext)
{
    PKIX_PL_Date *date = NULL;
    PKIX_PL_String *plString = NULL;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII, asciiDate, 0, &plString, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Date_Create_UTCTime(plString, &date, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(plString);

    PKIX_TEST_RETURN();

    return (date);
}

PKIX_ProcessingParams *
createProcessingParams(
    char *dirName,
    char *firstAnchorFileName,
    char *secondAnchorFileName,
    char *dateAscii,
    PKIX_List *initialPolicies, /* List of PKIX_PL_OID */
    PKIX_Boolean isCrlEnabled,
    void *plContext)
{

    PKIX_TrustAnchor *firstAnchor = NULL;
    PKIX_TrustAnchor *secondAnchor = NULL;
    PKIX_List *anchorsList = NULL;
    PKIX_ProcessingParams *procParams = NULL;
    PKIX_PL_String *dateString = NULL;
    PKIX_PL_Date *testDate = NULL;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&anchorsList, plContext));

    firstAnchor = createTrustAnchor(dirName, firstAnchorFileName, PKIX_FALSE, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(anchorsList,
                                                   (PKIX_PL_Object *)firstAnchor,
                                                   plContext));

    if (secondAnchorFileName) {
        secondAnchor =
            createTrustAnchor(dirName, secondAnchorFileName, PKIX_FALSE, plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(anchorsList,
                                                       (PKIX_PL_Object *)secondAnchor,
                                                       plContext));
    }

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_Create(anchorsList, &procParams, plContext));

    if (dateAscii) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                        dateAscii,
                                                        0,
                                                        &dateString,
                                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Date_Create_UTCTime(dateString, &testDate, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetDate(procParams, testDate, plContext));
    }

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetInitialPolicies(procParams, initialPolicies, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetRevocationEnabled(procParams, isCrlEnabled, plContext));

cleanup:

    if (PKIX_TEST_ERROR_RECEIVED) {
        PKIX_TEST_DECREF_AC(procParams);
    }

    PKIX_TEST_DECREF_AC(dateString);
    PKIX_TEST_DECREF_AC(testDate);
    PKIX_TEST_DECREF_AC(anchorsList);
    PKIX_TEST_DECREF_AC(firstAnchor);
    PKIX_TEST_DECREF_AC(secondAnchor);

    PKIX_TEST_RETURN();

    return (procParams);
}

PKIX_ValidateParams *
createValidateParams(
    char *dirName,
    char *firstAnchorFileName,
    char *secondAnchorFileName,
    char *dateAscii,
    PKIX_List *initialPolicies, /* List of PKIX_PL_OID */
    PKIX_Boolean initialPolicyMappingInhibit,
    PKIX_Boolean initialAnyPolicyInhibit,
    PKIX_Boolean initialExplicitPolicy,
    PKIX_Boolean isCrlEnabled,
    PKIX_List *chain,
    void *plContext)
{

    PKIX_ProcessingParams *procParams = NULL;
    PKIX_ValidateParams *valParams = NULL;

    PKIX_TEST_STD_VARS();

    procParams =
        createProcessingParams(dirName,
                               firstAnchorFileName,
                               secondAnchorFileName,
                               dateAscii,
                               NULL,
                               isCrlEnabled,
                               plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetInitialPolicies(procParams, initialPolicies, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetPolicyMappingInhibited(procParams, initialPolicyMappingInhibit, NULL));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetAnyPolicyInhibited(procParams, initialAnyPolicyInhibit, NULL));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetExplicitPolicyRequired(procParams, initialExplicitPolicy, NULL));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_Create(procParams, chain, &valParams, plContext));

cleanup:

    if (PKIX_TEST_ERROR_RECEIVED) {
        PKIX_TEST_DECREF_AC(valParams);
    }

    PKIX_TEST_DECREF_AC(procParams);

    PKIX_TEST_RETURN();

    return (valParams);
}

PKIX_ValidateResult *
createValidateResult(
    char *dirName,
    char *anchorFileName,
    char *pubKeyCertFileName,
    void *plContext)
{

    PKIX_TrustAnchor *anchor = NULL;
    PKIX_ValidateResult *valResult = NULL;
    PKIX_PL_Cert *cert = NULL;
    PKIX_PL_PublicKey *pubKey = NULL;

    PKIX_TEST_STD_VARS();

    anchor = createTrustAnchor(dirName, anchorFileName, PKIX_FALSE, plContext);
    cert = createCert(dirName, pubKeyCertFileName, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey(cert, &pubKey, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_ValidateResult_Create(pubKey, anchor, NULL, &valResult, plContext));

cleanup:

    if (PKIX_TEST_ERROR_RECEIVED) {
        PKIX_TEST_DECREF_AC(valResult);
    }

    PKIX_TEST_DECREF_AC(anchor);
    PKIX_TEST_DECREF_AC(cert);
    PKIX_TEST_DECREF_AC(pubKey);

    PKIX_TEST_RETURN();

    return (valResult);
}

PKIX_PL_GeneralName *
createGeneralName(
    PKIX_UInt32 nameType,
    char *asciiName,
    void *plContext)
{

    PKIX_PL_GeneralName *generalName = NULL;
    PKIX_PL_String *plString = NULL;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII, asciiName, 0, &plString, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_GeneralName_Create(nameType, plString, &generalName, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(plString);

    PKIX_TEST_RETURN();

    return (generalName);
}

PKIX_BuildResult *
createBuildResult(
    char *dirName,
    char *anchorFileName,
    char *pubKeyCertFileName,
    char *firstChainCertFileName,
    char *secondChainCertFileName,
    void *plContext)
{
    PKIX_BuildResult *buildResult = NULL;
    PKIX_ValidateResult *valResult = NULL;
    PKIX_List *certChain = NULL;

    PKIX_TEST_STD_VARS();

    valResult = createValidateResult(dirName, anchorFileName, pubKeyCertFileName, plContext);
    certChain = createCertChain(dirName,
                                firstChainCertFileName,
                                secondChainCertFileName,
                                plContext);

    PKIX_TEST_EXPECT_NO_ERROR(pkix_BuildResult_Create(valResult, certChain, &buildResult, plContext));

cleanup:

    if (PKIX_TEST_ERROR_RECEIVED) {
        PKIX_TEST_DECREF_AC(buildResult);
    }

    PKIX_TEST_DECREF_AC(valResult);
    PKIX_TEST_DECREF_AC(certChain);

    PKIX_TEST_RETURN();

    return (buildResult);
}
