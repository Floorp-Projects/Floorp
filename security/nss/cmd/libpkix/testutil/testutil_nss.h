/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * testutil_nss.h
 *
 * NSS-specific utility functions for handling test errors
 *
 */

#ifndef _TESTUTIL_NSS_H
#define _TESTUTIL_NSS_H

#include "pkix_tools.h"
#include "plstr.h"
#include "prprf.h"
#include "prlong.h"
#include "secutil.h"
#include <stdio.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "pkix_pl_generalname.h"

/* see source file for function documentation */

PKIX_PL_Cert *
createCert(
    char *dirName,
    char *certFile,
    void *plContext);

PKIX_PL_CRL *
createCRL(
    char *dirName,
    char *crlFileName,
    void *plContext);

PKIX_TrustAnchor *
createTrustAnchor(
    char *dirName,
    char *taFileName,
    PKIX_Boolean useCert,
    void *plContext);

PKIX_List *
createCertChain(
    char *dirName,
    char *firstCertFileName,
    char *secondCertFileName,
    void *plContext);

PKIX_List *
createCertChainPlus(
    char *dirName,
    char *certNames[],
    PKIX_PL_Cert *certs[],
    PKIX_UInt32 numCerts,
    void *plContext);

PKIX_PL_Date *
createDate(
    char *asciiDate,
    void *plContext);

PKIX_ProcessingParams *
createProcessingParams(
    char *dirName,
    char *firstAnchorFileName,
    char *secondAnchorFileName,
    char *dateAscii,
    PKIX_List *initialPolicies, /* List of PKIX_PL_OID */
    PKIX_Boolean isCrlEnabled,
    void *plContext);

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
    void *plContext);

PKIX_ValidateResult *
createValidateResult(
    char *dirName,
    char *anchorFileName,
    char *pubKeyCertFileName,
    void *plContext);

PKIX_BuildResult *
createBuildResult(
    char *dirName,
    char *anchorFileName,
    char *pubKeyCertFileName,
    char *firstChainCertFileName,
    char *secondChainCertFileName,
    void *plContext);

PKIX_PL_GeneralName *
createGeneralName(
    PKIX_UInt32 nameType,
    char *asciiName,
    void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* TESTUTIL_NSS_H */
