/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_certselector.c
 *
 * Test Cert Selector
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

#define PKIX_TEST_CERTSELECTOR_KEYUSAGE_NUM_CERTS 5
#define PKIX_TEST_CERTSELECTOR_EXTKEYUSAGE_NUM_CERTS 2
#define PKIX_TEST_CERTSELECTOR_CERTVALID_NUM_CERTS 2
#define PKIX_TEST_CERTSELECTOR_ISSUER_NUM_CERTS 4
#define PKIX_TEST_CERTSELECTOR_SERIALNUMBER_NUM_CERTS 1

static void *plContext = NULL;

/*
 * The first three certs are used to obtain policies to test
 * policy matching. Changing the table could break tests.
 */
static char *certList[] = {
#define POLICY1CERT 0
                "GoodCACert.crt",
#define ANYPOLICYCERT 1
                "anyPolicyCACert.crt",
#define POLICY2CERT 2
                "PoliciesP12CACert.crt",
#define SUBJECTCERT 3
                "PoliciesP3CACert.crt",
                "PoliciesP1234CACert.crt",
                "pathLenConstraint0CACert.crt",
                "pathLenConstraint1CACert.crt",
                "pathLenConstraint6CACert.crt",
                "TrustAnchorRootCertificate.crt",
                "GoodsubCACert.crt",
                "AnyPolicyTest14EE.crt",
                "UserNoticeQualifierTest16EE.crt"
        };
#define NUMCERTS (sizeof (certList)/sizeof (certList[0]))

/*
 * Following are Certs values for NameConstraints tests
 *
 * Cert0:nameConstraintsDN1subCA1Cert.crt:
 * Subject:CN=nameConstraints DN1 subCA1,OU=permittedSubtree1,
 *              O=Test Certificates,C=US
 *      Permitted Name:(OU=permittedSubtree2,OU=permittedSubtree1,
 *              O=Test Certificates,C=US)
 *      Excluded Name: (EMPTY)
 * Cert1:nameConstraintsDN3subCA2Cert.crt:
 *      Subject:CN=nameConstraints DN3 subCA2,O=Test Certificates,C=US
 *      Permitted Name:(O=Test Certificates,C=US)
 *      Excluded Name:(EMPTY)
 * Cert2:nameConstraintsDN2CACert.crt
 *      Subject:CN=nameConstraints DN2 CA,O=Test Certificates,C=US
 *      Permitted Name:(OU=permittedSubtree1,O=Test Certificates,C=US,
 *              OU=permittedSubtree2,O=Test Certificates,C=US)
 *      Excluded Name:(EMPTY)
 * Cert3:nameConstraintsDN3subCA1Cert.crt
 *      Subject:CN=nameConstraints DN3 subCA1,O=Test Certificates,C=US
 *      Permitted Name:(EMPTY)
 *      Excluded Name:(OU=excludedSubtree2,O=Test Certificates,C=US)
 * Cert4:nameConstraintsDN4CACert.crt
 *      Subject:CN=nameConstraints DN4 CA,O=Test Certificates,C=US
 *      Permitted Name:(EMPTY)
 *      Excluded Name:(OU=excludedSubtree1,O=Test Certificates,C=US,
 *              OU=excludedSubtree2,O=Test Certificates,C=US)
 * Cert5:nameConstraintsDN5CACert.crt
 *      Subject:CN=nameConstraints DN5 CA,O=Test Certificates,C=US
 *      Permitted Name:(OU=permittedSubtree1,O=Test Certificates,C=US)
 *      Excluded Name:(OU=excludedSubtree1,OU=permittedSubtree1,
 *              O=Test Certificates,C=US)
 * Cert6:ValidDNnameConstraintsTest1EE.crt
 *      Subject:CN=Valid DN nameConstraints EE Certificate Test1,
 *      OU=permittedSubtree1,O=Test Certificates,C=US
 *
 */
static char *ncCertList[] = {
        "nameConstraintsDN1subCA1Cert.crt",
        "nameConstraintsDN3subCA2Cert.crt",
        "nameConstraintsDN2CACert.crt",
        "nameConstraintsDN3subCA1Cert.crt",
        "nameConstraintsDN4CACert.crt",
        "nameConstraintsDN5CACert.crt",
        "ValidDNnameConstraintsTest1EE.crt"
};
#define NUMNCCERTS (sizeof (ncCertList)/sizeof (ncCertList[0]))

static char *sanCertList[] = {
        "InvalidDNnameConstraintsTest3EE.crt",
        "InvalidDNSnameConstraintsTest38EE.crt"
};
#define NUMSANCERTS (sizeof (sanCertList)/sizeof (sanCertList[0]))

/*
 * This function calls the CertSelector pointed to by "selector" for each
 * cert in the List pointed to by "certs", and compares the results against
 * the bit array given by the UInt32 "expectedResults". If the first cert is
 * expected to pass, the lower-order bit of "expectedResults" should be 1.
 * If the second cert is expected to pass, the second bit of "expectedResults"
 * should be 1, and so on. If more than 32 certs are provided, only the first
 * 32 will be checked. It is not an error to provide more bits than needed.
 * (For example, if you expect every cert to pass, "expectedResult" can be
 * set to 0xFFFFFFFF, even if the chain has fewer than 32 certs.)
 */
static
void testSelector(
        PKIX_CertSelector *selector,
        PKIX_List *certs,
        PKIX_UInt32 expectedResults)
{
        PKIX_UInt32 i = 0;
        PKIX_UInt32 numCerts = 0;
        PKIX_PL_Cert *cert = NULL;
        PKIX_CertSelector_MatchCallback callback = NULL;
        PKIX_Error *errReturn = NULL;
        PKIX_Boolean result = PKIX_TRUE;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_GetMatchCallback
                (selector, &callback, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (certs, &numCerts, plContext));
        if (numCerts > 32) {
                numCerts = 32;
        }

        for (i = 0; i < numCerts; i++) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                        (certs, i, (PKIX_PL_Object **)&cert, plContext));
                errReturn = callback(selector, cert, &result, plContext);

                if (errReturn || result == PKIX_FALSE) {
                        if ((expectedResults & 1) == 1) {
                                testError("selector unexpectedly failed");
                                (void) printf("    processing cert:\t%d\n", i);
                        }
                } else {
                        if ((expectedResults & 1) == 0) {
                                testError("selector unexpectedly passed");
                                (void) printf("    processing cert:\t%d\n", i);
                        }
                }

                expectedResults = expectedResults >> 1;
                PKIX_TEST_DECREF_BC(cert);
                PKIX_TEST_DECREF_BC(errReturn);
        }

cleanup:

        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(errReturn);

        PKIX_TEST_RETURN();
}

/*
 * This function gets a policy from the Cert pointed to by "cert", according
 * to the index provided by "index", creates an immutable List containing the
 * OID of that policy, and stores the result at "pPolicyList".
 */
static void testGetPolicyFromCert(
        PKIX_PL_Cert *cert,
        PKIX_UInt32 index,
        PKIX_List **pPolicyList)
{
        PKIX_List *policyInfo = NULL;
        PKIX_PL_CertPolicyInfo *firstPolicy = NULL;
        PKIX_PL_OID *policyOID = NULL;
        PKIX_List *list = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (cert, &policyInfo, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (policyInfo,
                index,
                (PKIX_PL_Object **)&firstPolicy,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolicyId
                (firstPolicy, &policyOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&list, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (list, (PKIX_PL_Object *)policyOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetImmutable(list, plContext));

        *pPolicyList = list;

cleanup:

        PKIX_TEST_DECREF_AC(policyInfo);
        PKIX_TEST_DECREF_AC(firstPolicy);
        PKIX_TEST_DECREF_AC(policyOID);

        PKIX_TEST_RETURN();
}

/*
 * This custom matchCallback will pass any Certificate which has no
 * CertificatePolicies extension and any Certificate whose Policies
 * extension include a CertPolicyQualifier.
 */
static PKIX_Error *
custom_CertSelector_MatchCallback(
        PKIX_CertSelector *selector,
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 i = 0;
        PKIX_UInt32 numPolicies = 0;
        PKIX_List *certPolicies = NULL;
        PKIX_List *quals = NULL;
        PKIX_PL_CertPolicyInfo *policy = NULL;
        PKIX_Error *error = NULL;

        PKIX_TEST_STD_VARS();

        *pResult = PKIX_TRUE;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (cert, &certPolicies, plContext));

        if (certPolicies) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                        (certPolicies, &numPolicies, plContext));
                      
                for (i = 0; i < numPolicies; i++) {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                            (certPolicies,
                            i,
                            (PKIX_PL_Object **)&policy,
                            plContext));
                        PKIX_TEST_EXPECT_NO_ERROR
                            (PKIX_PL_CertPolicyInfo_GetPolQualifiers
                            (policy, &quals, plContext));
                        if (quals) {
                            goto cleanup;
                        }
                        PKIX_TEST_DECREF_BC(policy);
                }
                PKIX_TEST_DECREF_BC(certPolicies);
                *pResult = PKIX_FALSE;

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                        (PKIX_CERTSELECTOR_ERROR,
                        NULL,
                        NULL,
                        PKIX_TESTPOLICYEXTWITHNOPOLICYQUALIFIERS,
                        &error,
                        plContext));

        }

cleanup:

        PKIX_TEST_DECREF_AC(certPolicies);
        PKIX_TEST_DECREF_AC(policy);
        PKIX_TEST_DECREF_AC(quals);

        return(error);
}

/*
 * This custom matchCallback will pass any Certificate whose
 * CertificatePolicies extension asserts the Policy specified by
 * the OID in the CertSelectorContext object.
 */
static PKIX_Error *
custom_CertSelector_MatchOIDCallback(
        PKIX_CertSelector *selector,
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 i = 0;
        PKIX_UInt32 numPolicies = 0;
        PKIX_Boolean match = PKIX_FALSE;
        PKIX_PL_Object *certSelectorContext = NULL;
        PKIX_PL_OID *constraintOID = NULL;
        PKIX_List *certPolicies = NULL;
        PKIX_PL_CertPolicyInfo *policy = NULL;
        PKIX_PL_OID *policyOID = NULL;
        PKIX_PL_String *errorDesc = NULL;
        PKIX_Error *error = NULL;

        PKIX_TEST_STD_VARS();

        *pResult = PKIX_TRUE;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_GetCertSelectorContext
                (selector, &certSelectorContext, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(pkix_CheckType
                (certSelectorContext, PKIX_OID_TYPE, plContext));

        constraintOID = (PKIX_PL_OID *)certSelectorContext;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (cert, &certPolicies, plContext));

        if (certPolicies) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                        (certPolicies, &numPolicies, plContext));

                for (i = 0; i < numPolicies; i++) {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                                (certPolicies,
                                i,
                                (PKIX_PL_Object **)&policy,
                                plContext));

                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_PL_CertPolicyInfo_GetPolicyId
                                (policy, &policyOID, plContext));

                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                ((PKIX_PL_Object *)policyOID,
                                (PKIX_PL_Object *)constraintOID,
                                &match,
                                plContext));

                        if (match) {
                                goto cleanup;
                        }
                        PKIX_TEST_DECREF_BC(policy);
                        PKIX_TEST_DECREF_BC(policyOID);
                }
        }

        PKIX_TEST_DECREF_BC(certSelectorContext);
        PKIX_TEST_DECREF_BC(certPolicies);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                        (PKIX_CERTSELECTOR_ERROR,
                        NULL,
                        NULL,
                        PKIX_TESTNOMATCHINGPOLICY,
                        &error,
                        plContext));

cleanup:

        PKIX_TEST_DECREF_AC(certSelectorContext);
        PKIX_TEST_DECREF_AC(certPolicies);
        PKIX_TEST_DECREF_AC(policy);
        PKIX_TEST_DECREF_AC(policyOID);
        PKIX_TEST_DECREF_AC(errorDesc);

        return(error);
}

static 
void testSubjectMatch(
        PKIX_List *certs,
        PKIX_PL_Cert *certNameToMatch)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *subjParams = NULL;
        PKIX_PL_X500Name *subjectName = NULL;

        PKIX_TEST_STD_VARS();

        subTest("Subject name match");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&subjParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubject
                (certNameToMatch, &subjectName, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubject
                (subjParams, subjectName, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, subjParams, plContext));
        testSelector(selector, certs, 0x008);

cleanup:

        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(subjParams);
        PKIX_TEST_DECREF_AC(subjectName);

        PKIX_TEST_RETURN();
}

static 
void testBasicConstraintsMatch(
        PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *bcParams = NULL;

        PKIX_TEST_STD_VARS();

        subTest("Basic Constraints match");
        subTest("    pathLenContraint = -2: pass only EE's");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&bcParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetBasicConstraints
                (bcParams, -2, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, bcParams, plContext));
        testSelector(selector, certs, 0xC00);

        subTest("    pathLenContraint = -1: pass all certs");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetBasicConstraints
                (bcParams, -1, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, bcParams, plContext));
        testSelector(selector, certs, 0xFFF);

        subTest("    pathLenContraint = 1: pass only certs with pathLen >= 1");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetBasicConstraints
                (bcParams, 1, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, bcParams, plContext));
        testSelector(selector, certs, 0x3DF);

        subTest("    pathLenContraint = 2: pass only certs with  pathLen >= 2");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetBasicConstraints
                (bcParams, 2, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, bcParams, plContext));
        testSelector(selector, certs, 0x39F);

cleanup:
        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(bcParams);

        PKIX_TEST_RETURN();
}

static 
void testPolicyMatch(
        PKIX_List *certs,
        PKIX_PL_Cert *NIST1Cert, /* a source for policy NIST1 */
        PKIX_PL_Cert *NIST2Cert, /* a source for policy NIST2 */
        PKIX_PL_Cert *anyPolicyCert) /* a source for policy anyPolicy */
{
        PKIX_CertSelector *selector = NULL;
        PKIX_List *emptyList = NULL; /* no members */
        PKIX_List *policy1List = NULL; /* OIDs */
        PKIX_List *policy2List = NULL; /* OIDs */
        PKIX_List *anyPolicyList = NULL; /* OIDs */
        PKIX_ComCertSelParams *polParams = NULL;

        PKIX_TEST_STD_VARS();

        subTest("Policy match");
        testGetPolicyFromCert(NIST1Cert, 0, &policy1List);
        testGetPolicyFromCert(NIST2Cert, 1, &policy2List);
        testGetPolicyFromCert(anyPolicyCert, 0, &anyPolicyList);

        subTest("    Pass certs with any CertificatePolicies extension");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&emptyList, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_Create(&polParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetPolicy
                (polParams, emptyList, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, polParams, plContext));
        testSelector(selector, certs, 0xEFF);
        PKIX_TEST_DECREF_BC(polParams);

        subTest("    Pass only certs with policy NIST1");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_Create(&polParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetPolicy
                (polParams, policy1List, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, polParams, plContext));
        testSelector(selector, certs, 0xEF5);
        PKIX_TEST_DECREF_BC(polParams);

        subTest("    Pass only certs with policy NIST2");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_Create(&polParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetPolicy
                (polParams, policy2List, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, polParams, plContext));
        testSelector(selector, certs, 0x814);
        PKIX_TEST_DECREF_BC(polParams);

        subTest("    Pass only certs with policy anyPolicy");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_Create(&polParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetPolicy
                (polParams, anyPolicyList, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, polParams, plContext));
        testSelector(selector, certs, 0x002);

cleanup:

        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(emptyList);
        PKIX_TEST_DECREF_AC(policy1List);
        PKIX_TEST_DECREF_AC(policy2List);
        PKIX_TEST_DECREF_AC(anyPolicyList);
        PKIX_TEST_DECREF_AC(polParams);

        PKIX_TEST_RETURN();
}

static 
void testCertificateMatch(
        PKIX_List *certs,
        PKIX_PL_Cert *certToMatch)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;

        PKIX_TEST_STD_VARS();

        subTest("Certificate match");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetCertificate
                (params, certToMatch, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));
        testSelector(selector, certs, 0x008);

cleanup:

        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(params);

        PKIX_TEST_RETURN();
}

static 
void testNameConstraintsMatch(PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_PL_CertNameConstraints *permitNameConstraints1 = NULL;
        PKIX_PL_CertNameConstraints *permitNameConstraints2 = NULL;
        PKIX_PL_CertNameConstraints *permitNameConstraints3 = NULL;
        PKIX_PL_CertNameConstraints *excludeNameConstraints1 = NULL;
        PKIX_PL_CertNameConstraints *excludeNameConstraints2 = NULL;
        PKIX_PL_CertNameConstraints *excludeNameConstraints3 = NULL;
        PKIX_UInt32 numCerts = 0;

        PKIX_TEST_STD_VARS();

        subTest("test NameConstraints Cert Selector");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (certs, &numCerts, plContext));

        subTest("    PKIX_PL_Cert_GetNameConstraints <cert0-permitted>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, 0, (PKIX_PL_Object **)&cert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (cert, &permitNameConstraints1, plContext));
        PKIX_TEST_DECREF_BC(cert);

        subTest("    PKIX_PL_Cert_GetNameConstraints <cert1-permitted>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, 1, (PKIX_PL_Object **)&cert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (cert, &permitNameConstraints2, plContext));
        PKIX_TEST_DECREF_BC(cert);

        subTest("    PKIX_PL_Cert_GetNameConstraints <cert2-permitted>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, 2, (PKIX_PL_Object **)&cert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (cert, &permitNameConstraints3, plContext));
        PKIX_TEST_DECREF_BC(cert);

        subTest("    PKIX_PL_Cert_GetNameConstraints <cert3-excluded>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, 3, (PKIX_PL_Object **)&cert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (cert, &excludeNameConstraints1, plContext));
        PKIX_TEST_DECREF_BC(cert);

        subTest("    PKIX_PL_Cert_GetNameConstraints <cert4-excluded>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, 4, (PKIX_PL_Object **)&cert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (cert, &excludeNameConstraints2, plContext));
        PKIX_TEST_DECREF_BC(cert);

        subTest("    PKIX_PL_Cert_GetNameConstraints <cert5-excluded>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, 5, (PKIX_PL_Object **)&cert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (cert, &excludeNameConstraints3, plContext));
        PKIX_TEST_DECREF_BC(cert);

        subTest("    Create Selector and ComCertSelParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    CertNameConstraints testing permitted NONE");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (params, permitNameConstraints1, plContext));
        testSelector(selector, certs, 0x0);

        subTest("    PKIX_ComCertSelParams_SetNameConstraint Reset");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (params, NULL, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    CertNameConstraints testing permitted ALL");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (params, permitNameConstraints2, plContext));
        testSelector(selector, certs, 0x07F);

        subTest("    CertNameConstraints testing permitted TWO");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (params, permitNameConstraints3, plContext));
        testSelector(selector, certs, 0x0041);

        subTest("    PKIX_ComCertSelParams_SetNameConstraint Reset");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (params, NULL, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    CertNameConstraints testing excluded");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (params, excludeNameConstraints1, plContext));
        testSelector(selector, certs, 0x07F);

        subTest("    CertNameConstraints testing excluded");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (params, excludeNameConstraints2, plContext));
        testSelector(selector, certs, 0x07F);

        subTest("    CertNameConstraints testing excluded");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (params, excludeNameConstraints3, plContext));
        testSelector(selector, certs, 0x41);

cleanup:

        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(params);
        PKIX_TEST_DECREF_AC(permitNameConstraints1);
        PKIX_TEST_DECREF_AC(permitNameConstraints2);
        PKIX_TEST_DECREF_AC(permitNameConstraints3);
        PKIX_TEST_DECREF_AC(excludeNameConstraints1);
        PKIX_TEST_DECREF_AC(excludeNameConstraints2);
        PKIX_TEST_DECREF_AC(excludeNameConstraints3);

        PKIX_TEST_RETURN();
}

static 
void testPathToNamesMatch(PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_List *nameList = NULL;
        PKIX_PL_GeneralName *name = NULL;

        PKIX_TEST_STD_VARS();

        subTest("test PathToName Cert Selector");

        subTest("    PKIX_PL_GeneralName List create");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&nameList, plContext));

        subTest("    Add directory name <O=NotATest Certificates,C=US>");
        name = createGeneralName
                (PKIX_DIRECTORY_NAME,
                "O=NotATest Certificates,C=US",
                plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (nameList, (PKIX_PL_Object *)name, plContext));

        subTest("    Create Selector and ComCertSelParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    PKIX_ComCertSelParams_SetPathToNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, nameList, plContext));

        subTest("    Permitting THREE");
        testSelector(selector, certs, 0x58);

        subTest("    Remove directory name <O=NotATest Certificates,C=US...>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_DeleteItem
                                (nameList, 0, plContext));
        PKIX_TEST_DECREF_BC(name);

        subTest("    PKIX_ComCertSelParams_SetPathToNames Reset");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, NULL, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    Add directory name <OU=permittedSubtree1,O=Test...>");
        name = createGeneralName
                (PKIX_DIRECTORY_NAME,
                "OU=permittedSubtree1,O=Test Certificates,C=US",
                plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (nameList, (PKIX_PL_Object *)name, plContext));

        subTest("    PKIX_ComCertSelParams_SetPathToNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, nameList, plContext));

        subTest("    Permitting SIX");
        testSelector(selector, certs, 0x5F);

        subTest("    Remove directory name <OU=permittedSubtree1,O=Test...>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_DeleteItem
                                (nameList, 0, plContext));
        PKIX_TEST_DECREF_BC(name);

        subTest("    PKIX_ComCertSelParams_SetNameConstraint Reset");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, NULL, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    Add directory name <O=Test Certificates,C=US...>");
        name = createGeneralName
                (PKIX_DIRECTORY_NAME,
                "O=Test Certificates,C=US",
                plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (nameList, (PKIX_PL_Object *)name, plContext));
        PKIX_TEST_DECREF_BC(name);

        subTest("    PKIX_ComCertSelParams_SetPathToNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, nameList, plContext));

        subTest("    Permitting FOUR");
        testSelector(selector, certs, 0x47);

        subTest("    Only directory name <OU=permittedSubtree1,O=Test ...>");
        name = createGeneralName
                (PKIX_DIRECTORY_NAME,
                "OU=permittedSubtree1,O=Test Certificates,C=US",
                plContext);

        subTest("    PKIX_ComCertSelParams_AddPathToName");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_AddPathToName
                (params, name, plContext));
        PKIX_TEST_DECREF_BC(name);

        subTest("    Permitting FOUR");
        testSelector(selector, certs, 0x47);

        subTest("    PKIX_ComCertSelParams_SetNameConstraint Reset");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, NULL, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));
        PKIX_TEST_DECREF_BC(nameList);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&nameList, plContext));

        subTest("    Add directory name <CN=Valid DN nameConstraints EE...>");
        name = createGeneralName
                (PKIX_DIRECTORY_NAME, "CN=Valid DN nameConstraints EE "
                "Certificate Test1,OU=permittedSubtree1,"
                "O=Test Certificates,C=US",
                plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (nameList, (PKIX_PL_Object *)name, plContext));
        PKIX_TEST_DECREF_BC(name);

        subTest("    PKIX_ComCertSelParams_SetPathToNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, nameList, plContext));

        subTest("    Permitting SIX");
        testSelector(selector, certs, 0x7e);

        subTest("    Add directory name <OU=permittedSubtree1,O=Test>");
        name = createGeneralName
                (PKIX_DIRECTORY_NAME,
                "OU=permittedSubtree1,O=Test",
                plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (nameList, (PKIX_PL_Object *)name, plContext));
        PKIX_TEST_DECREF_BC(name);

        subTest("    PKIX_ComCertSelParams_SetPathToNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, nameList, plContext));

        subTest("    Permitting SIX");
        testSelector(selector, certs, 0x58);

        subTest("    Add directory name <O=Test Certificates,C=US>");
        name = createGeneralName
                (PKIX_DIRECTORY_NAME, "O=Test Certificates,C=US", plContext);

        subTest("    PKIX_ComCertSelParams_SetPathToNames Reset");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (params, NULL, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_AddPathToName
                (params, name, plContext));
        PKIX_TEST_DECREF_BC(name);

        subTest("    Permitting FOUR");
        testSelector(selector, certs, 0x47);

cleanup:

        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(params);
        PKIX_TEST_DECREF_AC(nameList);

        PKIX_TEST_RETURN();
}

static 
void testSubjAltNamesMatch(PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_List *nameList = NULL;
        PKIX_PL_GeneralName *name = NULL;

        PKIX_TEST_STD_VARS();

        subTest("test SubjAltNames Cert Selector");

        subTest("    PKIX_PL_GeneralName List create");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&nameList, plContext));

        subTest("    Create Selector and ComCertSelParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    Add directory name <CN=Invalid DN nameConstraints EE...>");
        name = createGeneralName
                (PKIX_DIRECTORY_NAME,
                "CN=Invalid DN nameConstraints EE Certificate Test3,"
                "OU=excludedSubtree1,O=Test Certificates,C=US",
                plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (nameList, (PKIX_PL_Object *)name, plContext));

        subTest("    PKIX_ComCertSelParams_SetSubjAltNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubjAltNames
                (params, nameList, plContext));

        PKIX_TEST_DECREF_BC(name);
        PKIX_TEST_DECREF_BC(nameList);

        subTest("    Permitting ONE");
        testSelector(selector, certs, 0x1);

        subTest("    Add DNS name <mytestcertificates.gov>");
        name = createGeneralName
                (PKIX_DNS_NAME,
                "mytestcertificates.gov",
                plContext);

        subTest("    PKIX_ComCertSelParams_AddSubjAltName");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_AddSubjAltName
                (params, name, plContext));
        PKIX_TEST_DECREF_BC(name);

        subTest("    Permitting NONE");
        testSelector(selector, certs, 0x0);

        subTest("    PKIX_ComCertSelParams_SetMatchAllSubjAltNames to FALSE");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetMatchAllSubjAltNames
                (params, PKIX_FALSE, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    Permitting TWO");
        testSelector(selector, certs, 0x3);

cleanup:

        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(params);
        PKIX_TEST_DECREF_AC(name);
        PKIX_TEST_DECREF_AC(nameList);

        PKIX_TEST_RETURN();
}

static 
void testCertificateValidMatch(
        PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_PL_String *stringRep = NULL;
        PKIX_PL_Date *testDate = NULL;
        char *asciiRep = "050501000000Z";

        PKIX_TEST_STD_VARS();

        subTest("CertificateValid match");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_String_Create
                (PKIX_ESCASCII, asciiRep, 0, &stringRep, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Date_Create_UTCTime(stringRep, &testDate, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetCertificateValid
                (params, testDate, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));
        testSelector(selector, certs, 0xFFFFFFFF);

cleanup:

        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(params);
        PKIX_TEST_DECREF_AC(stringRep);
        PKIX_TEST_DECREF_AC(testDate);

        PKIX_TEST_RETURN();
}

static
void test_customCallback1(PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;

        PKIX_TEST_STD_VARS();

        subTest("custom matchCallback");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (custom_CertSelector_MatchCallback,
                NULL,
                &selector,
                plContext));

        testSelector(selector, certs, 0x900);

cleanup:

        PKIX_TEST_DECREF_AC(selector);

        PKIX_TEST_RETURN();
}

static 
void test_customCallback2
        (PKIX_List *certs,
        PKIX_PL_Cert *anyPolicyCert) /* a source for policy anyPolicy */
{
        PKIX_CertSelector *selector = NULL;
        PKIX_List *anyPolicyList = NULL; /* OIDs */
        PKIX_PL_OID *policyOID = NULL;

        PKIX_TEST_STD_VARS();

        subTest("custom matchCallback with CertSelectorContext");

        testGetPolicyFromCert(anyPolicyCert, 0, &anyPolicyList);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (anyPolicyList, 0, (PKIX_PL_Object **)&policyOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (custom_CertSelector_MatchOIDCallback,
                (PKIX_PL_Object *)policyOID,
                &selector,
                plContext));

        testSelector(selector, certs, (1 << ANYPOLICYCERT));

cleanup:

        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(anyPolicyList);
        PKIX_TEST_DECREF_AC(policyOID);

        PKIX_TEST_RETURN();
}

static 
void testExtendedKeyUsageMatch(char *certDir)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_PL_OID *ekuOid = NULL;
        PKIX_List *ekuOidList = NULL;
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore_CertCallback certCallback;
        PKIX_CertStore *certStore = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_List *certList = NULL;
        PKIX_UInt32 numCert = 0;
        void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        subTest("test Extended KeyUsage Cert Selector");

        subTest("    PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        subTest("    Create Extended Key Usage OID List");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&ekuOidList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                ("1.3.6.1.5.5.7.3.2", &ekuOid, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (ekuOidList, (PKIX_PL_Object *)ekuOid, plContext));

        PKIX_TEST_DECREF_BC(ekuOid);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                ("1.3.6.1.5.5.7.3.3", &ekuOid, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (ekuOidList, (PKIX_PL_Object *)ekuOid, plContext));

        PKIX_TEST_DECREF_BC(ekuOid);

        subTest("    PKIX_ComCertSelParams_SetExtendedKeyUsage");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetExtendedKeyUsage
                (goodParams, ekuOidList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, certDir, 0, &dirString, plContext));

        subTest("    PKIX_PL_CollectionCertStoreContext_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                (dirString, &certStore, plContext));

        subTest("    PKIX_CertSelector_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &certSelector, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (certSelector, goodParams, plContext));

        subTest("    PKIX_CertStore_GetCertCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertCallback
                (certStore, &certCallback, NULL));

        subTest("    Getting data from Cert Callback");
        PKIX_TEST_EXPECT_NO_ERROR(certCallback
                (certStore, certSelector, &nbioContext, &certList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (certList, &numCert, plContext));

        if (numCert != PKIX_TEST_CERTSELECTOR_EXTKEYUSAGE_NUM_CERTS) {
                pkixTestErrorMsg = "unexpected Cert number mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(ekuOid);
        PKIX_TEST_DECREF_AC(ekuOidList);
        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(certList);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(certStore);

        PKIX_TEST_RETURN();
}

static 
void testKeyUsageMatch(char *certDir)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore_CertCallback certCallback;
        PKIX_CertStore *certStore = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_List *certList = NULL;
        PKIX_UInt32 numCert = 0;
        void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        subTest("test KeyUsage Cert Selector");

        subTest("    PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        subTest("    PKIX_ComCertSelParams_SetKeyUsage");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetKeyUsage
                (goodParams, PKIX_CRL_SIGN, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, certDir, 0, &dirString, plContext));

        subTest("    PKIX_PL_CollectionCertStoreContext_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                (dirString, &certStore, plContext));

        subTest("    PKIX_CertSelector_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &certSelector, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (certSelector, goodParams, plContext));

        subTest("    PKIX_CertStore_GetCertCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertCallback
                (certStore, &certCallback, NULL));

        subTest("    Getting data from Cert Callback");
        PKIX_TEST_EXPECT_NO_ERROR(certCallback
                (certStore, certSelector, &nbioContext, &certList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (certList, &numCert, plContext));

        if (numCert != PKIX_TEST_CERTSELECTOR_KEYUSAGE_NUM_CERTS) {
                pkixTestErrorMsg = "unexpected Cert number mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(certList);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(certStore);

        PKIX_TEST_RETURN();
}

static 
void testCertValidMatch(char *certDir)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_PL_Date *validDate = NULL;
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore_CertCallback certCallback;
        PKIX_CertStore *certStore = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_List *certList = NULL;
        PKIX_UInt32 numCert = 0;
        void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        subTest("test CertValid Cert Selector");

        subTest("    PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        validDate = createDate("050601000000Z", plContext);

        subTest("    PKIX_ComCertSelParams_SetCertificateValid");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetCertificateValid
                (goodParams, validDate, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, certDir, 0, &dirString, plContext));

        subTest("    PKIX_PL_CollectionCertStoreContext_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                (dirString, &certStore, plContext));

        subTest("    PKIX_CertSelector_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &certSelector, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (certSelector, goodParams, plContext));

        subTest("    PKIX_CertStore_GetCertCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertCallback
                (certStore, &certCallback, NULL));

        subTest("    Getting data from Cert Callback");
        PKIX_TEST_EXPECT_NO_ERROR(certCallback
                (certStore, certSelector, &nbioContext, &certList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (certList, &numCert, plContext));

        if (numCert != PKIX_TEST_CERTSELECTOR_CERTVALID_NUM_CERTS) {
                pkixTestErrorMsg = "unexpected Cert number mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(validDate);
        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(certList);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(certStore);

        PKIX_TEST_RETURN();
}

static 
void testIssuerMatch(char *certDir)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_PL_X500Name *issuer = NULL;
        PKIX_PL_String *issuerStr = NULL;
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore_CertCallback certCallback;
        PKIX_CertStore *certStore = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_List *certList = NULL;
        char *issuerName = "CN=science,O=mit,C=US";
        PKIX_UInt32 numCert = 0;
        void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        subTest("test Issuer Cert Selector");

        subTest("    PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, issuerName, 0, &issuerStr, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_X500Name_Create
                (issuerStr, &issuer, plContext));

        subTest("    PKIX_ComCertSelParams_SetIssuer");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetIssuer
                (goodParams, issuer, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, certDir, 0, &dirString, plContext));

        subTest("    PKIX_PL_CollectionCertStoreContext_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                (dirString, &certStore, plContext));

        subTest("    PKIX_CertSelector_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &certSelector, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (certSelector, goodParams, plContext));

        subTest("    PKIX_CertStore_GetCertCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertCallback
                (certStore, &certCallback, NULL));

        subTest("    Getting data from Cert Callback");
        PKIX_TEST_EXPECT_NO_ERROR(certCallback
                (certStore, certSelector, &nbioContext, &certList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (certList, &numCert, plContext));

        if (numCert != PKIX_TEST_CERTSELECTOR_ISSUER_NUM_CERTS) {
                pkixTestErrorMsg = "unexpected Cert number mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(issuer);
        PKIX_TEST_DECREF_AC(issuerStr);
        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(certList);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(certStore);

        PKIX_TEST_RETURN();
}

static 
void testSerialNumberVersionMatch(char *certDir)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_PL_BigInt *serialNumber = NULL;
        PKIX_PL_String *serialNumberStr = NULL;
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore_CertCallback certCallback;
        PKIX_CertStore *certStore = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_List *certList = NULL;
        PKIX_UInt32 numCert = 0;
        void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        subTest("test Serial Number Cert Selector");

        subTest("    PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, "01", 0, &serialNumberStr, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BigInt_Create
                (serialNumberStr, &serialNumber, plContext));

        subTest("    PKIX_ComCertSelParams_SetSerialNumber");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSerialNumber
                (goodParams, serialNumber, plContext));

        subTest("    PKIX_ComCertSelParams_SetVersion");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetVersion
                (goodParams, 0, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, certDir, 0, &dirString, plContext));

        subTest("    PKIX_PL_CollectionCertStoreContext_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                (dirString, &certStore, plContext));

        subTest("    PKIX_CertSelector_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &certSelector, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (certSelector, goodParams, plContext));

        subTest("    PKIX_CertStore_GetCertCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertCallback
                (certStore, &certCallback, NULL));

        subTest("    Getting data from Cert Callback");
        PKIX_TEST_EXPECT_NO_ERROR(certCallback
                (certStore, certSelector, &nbioContext, &certList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (certList, &numCert, plContext));

        PKIX_TEST_DECREF_BC(certList);

        if (numCert != 0) {
                pkixTestErrorMsg = "unexpected Version mismatch";
        }

        subTest("    PKIX_ComCertSelParams_SetVersion");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetVersion
                (goodParams, 2, plContext));

        subTest("    Getting data from Cert Callback");
        PKIX_TEST_EXPECT_NO_ERROR(certCallback
                (certStore, certSelector, &nbioContext, &certList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (certList, &numCert, plContext));

        if (numCert != PKIX_TEST_CERTSELECTOR_SERIALNUMBER_NUM_CERTS) {
                pkixTestErrorMsg = "unexpected Serial Number mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(serialNumber);
        PKIX_TEST_DECREF_AC(serialNumberStr);
        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(certList);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(certStore);

        PKIX_TEST_RETURN();
}

static 
void testSubjKeyIdMatch(PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_PL_ByteArray *selSubjKeyId = NULL;
        PKIX_UInt32 item = 0;

        PKIX_TEST_STD_VARS();

        subTest("test Subject Key Id Cert Selector");

        item = 2;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, item, (PKIX_PL_Object **)&cert, plContext));

        subTest("    PKIX_PL_Cert_GetSubjectKeyIdentifier");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectKeyIdentifier
                (cert, &selSubjKeyId, plContext));

        subTest("    Create Selector and ComCertSelParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    PKIX_ComCertSelParams_SetSubjKeyIdentifier");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubjKeyIdentifier
                (params, selSubjKeyId, plContext));

        subTest("    Select One");
        testSelector(selector, certs, 1<<item);

cleanup:

        PKIX_TEST_DECREF_AC(selSubjKeyId);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(params);
        PKIX_TEST_DECREF_AC(selector);

        PKIX_TEST_RETURN();
}

static 
void testAuthKeyIdMatch(PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_PL_ByteArray *selAuthKeyId = NULL;
        PKIX_UInt32 item = 0;

        PKIX_TEST_STD_VARS();

        subTest("test Auth Key Id Cert Selector");

        item = 3;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, item, (PKIX_PL_Object **)&cert, plContext));

        subTest("    PKIX_PL_Cert_GetAuthorityKeyIdentifier");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetAuthorityKeyIdentifier
                (cert, &selAuthKeyId, plContext));

        subTest("    Create Selector and ComCertSelParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    PKIX_ComCertSelParams_SetAuthorityKeyIdentifier");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetAuthorityKeyIdentifier
                (params, selAuthKeyId, plContext));

        subTest("    Select TWO");
        testSelector(selector, certs, (1<<item)|(1<<1));

cleanup:

        PKIX_TEST_DECREF_AC(selAuthKeyId);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(params);
        PKIX_TEST_DECREF_AC(selector);

        PKIX_TEST_RETURN();
}

static 
void testSubjPKAlgIdMatch(PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_PL_OID *selAlgId = NULL;
        PKIX_UInt32 item = 0;

        PKIX_TEST_STD_VARS();

        subTest("test Subject Public Key Algorithm Id Cert Selector");

        item = 0;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, item, (PKIX_PL_Object **)&cert, plContext));

        subTest("    PKIX_PL_Cert_GetSubjectPublicKeyAlgId");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKeyAlgId
                (cert, &selAlgId, plContext));

        subTest("    Create Selector and ComCertSelParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    PKIX_ComCertSelParams_SetSubjPKAlgId");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubjPKAlgId
                (params, selAlgId, plContext));

        subTest("    Select All");
        testSelector(selector, certs, 0x7F);

cleanup:

        PKIX_TEST_DECREF_AC(selAlgId);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(params);
        PKIX_TEST_DECREF_AC(selector);

        PKIX_TEST_RETURN();
}

static 
void testSubjPublicKeyMatch(PKIX_List *certs)
{
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_PL_PublicKey *selPublicKey = NULL;
        PKIX_UInt32 item = 0;

        PKIX_TEST_STD_VARS();

        subTest("test Subject Public Key Cert Selector");

        item = 5;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certs, item, (PKIX_PL_Object **)&cert, plContext));

        subTest("    PKIX_PL_Cert_GetSubjectPublicKey");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey
                (cert, &selPublicKey, plContext));

        subTest("    Create Selector and ComCertSelParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&params, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, params, plContext));

        subTest("    PKIX_ComCertSelParams_SetSubjPubKey");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubjPubKey
                (params, selPublicKey, plContext));

        subTest("    Select ONE");
        testSelector(selector, certs, 1<<item);

cleanup:

        PKIX_TEST_DECREF_AC(selPublicKey);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(params);
        PKIX_TEST_DECREF_AC(selector);

        PKIX_TEST_RETURN();
}

static 
void test_CertSelector_Duplicate(PKIX_CertSelector *selector)
{
        PKIX_Int32 goodBasicConstraints = 0;
        PKIX_Int32 equalBasicConstraints = 0;
        PKIX_CertSelector *dupSelector = NULL;
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_ComCertSelParams *equalParams = NULL;
        PKIX_CertSelector_MatchCallback goodCallback = NULL;
        PKIX_CertSelector_MatchCallback equalCallback = NULL;
        PKIX_PL_X500Name *goodSubject = NULL;
        PKIX_PL_X500Name *equalSubject = NULL;
        PKIX_List *goodPolicy = NULL;
        PKIX_List *equalPolicy = NULL;
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_Cert *equalCert = NULL;
        PKIX_PL_Date *goodDate = NULL;
        PKIX_PL_Date *equalDate = NULL;

        PKIX_TEST_STD_VARS();

        subTest("test_CertSelector_Duplicate");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Duplicate
                ((PKIX_PL_Object *)selector,
                (PKIX_PL_Object **)&dupSelector,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_GetCommonCertSelectorParams
                (selector, &goodParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_GetCommonCertSelectorParams
                (dupSelector, &equalParams, plContext));
        /* There is no equals function, so look at components separately. */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubject
                (goodParams, &goodSubject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubject
                (equalParams, &equalSubject, plContext));
        if (goodSubject && equalSubject) {
                testEqualsHelper
                        ((PKIX_PL_Object *)goodSubject,
                        (PKIX_PL_Object *)equalSubject,
                        PKIX_TRUE,
                        plContext);
        } else {
                if PKIX_EXACTLY_ONE_NULL(goodSubject, equalSubject) {
                        pkixTestErrorMsg = "Subject Names are not equal!";
                        goto cleanup;
                }
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetPolicy
                (goodParams, &goodPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetPolicy
                (equalParams, &equalPolicy, plContext));
        if (goodPolicy && equalPolicy) {
                testEqualsHelper
                        ((PKIX_PL_Object *)goodPolicy,
                        (PKIX_PL_Object *)equalPolicy,
                        PKIX_TRUE,
                        plContext);
        } else {
                if PKIX_EXACTLY_ONE_NULL(goodPolicy, equalPolicy) {
                        pkixTestErrorMsg = "Policy Lists are not equal!";
                        goto cleanup;
                }
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetCertificate
                (goodParams, &goodCert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetCertificate
                (equalParams, &equalCert, plContext));
        if (goodCert && equalCert) {
                testEqualsHelper
                        ((PKIX_PL_Object *)goodCert,
                        (PKIX_PL_Object *)equalCert,
                        PKIX_TRUE,
                        plContext);
        } else {
                if PKIX_EXACTLY_ONE_NULL(goodCert, equalCert) {
                        pkixTestErrorMsg = "Cert Lists are not equal!";
                        goto cleanup;
                }
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetCertificateValid
                (goodParams, &goodDate, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetCertificateValid
                (equalParams, &equalDate, plContext));
        if (goodCert && equalCert) {
                testEqualsHelper
                        ((PKIX_PL_Object *)goodDate,
                        (PKIX_PL_Object *)equalDate,
                        PKIX_TRUE,
                        plContext);
        } else {
                if PKIX_EXACTLY_ONE_NULL(goodDate, equalDate) {
                        pkixTestErrorMsg = "Date Lists are not equal!";
                        goto cleanup;
                }
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetBasicConstraints
                (goodParams, &goodBasicConstraints, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetBasicConstraints
                (equalParams, &equalBasicConstraints, plContext));
        if (goodBasicConstraints != equalBasicConstraints) {
                pkixTestErrorMsg = "BasicConstraints are not equal!";
                goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_GetMatchCallback
                (selector, &goodCallback, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_GetMatchCallback
                (dupSelector, &equalCallback, plContext));
        if (goodCallback != equalCallback) {
                pkixTestErrorMsg = "MatchCallbacks are not equal!";
        }

cleanup:

        PKIX_TEST_DECREF_AC(dupSelector);
        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(equalParams);
        PKIX_TEST_DECREF_AC(goodSubject);
        PKIX_TEST_DECREF_AC(equalSubject);
        PKIX_TEST_DECREF_AC(goodPolicy);
        PKIX_TEST_DECREF_AC(equalPolicy);
        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(equalCert);
        PKIX_TEST_DECREF_AC(goodDate);
        PKIX_TEST_DECREF_AC(equalDate);

        PKIX_TEST_RETURN();
}

static 
void printUsage(void) {
        (void) printf("\nUSAGE:\ttest_certselector <NIST_FILES_DIR> <cert-dir>\n\n");
}

int test_certselector(int argc, char *argv[]) {

        PKIX_UInt32 i = 0;
        PKIX_UInt32 j = 0;
        PKIX_UInt32 actualMinorVersion;

        PKIX_CertSelector *emptySelector = NULL;
        PKIX_List *certs = NULL;
        PKIX_List *nameConstraintsCerts = NULL;
        PKIX_List *subjAltNamesCerts = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_PL_Cert *policy1Cert = NULL;
        PKIX_PL_Cert *policy2Cert = NULL;
        PKIX_PL_Cert *anyPolicyCert = NULL;
        PKIX_PL_Cert *subjectCert = NULL;
        PKIX_ComCertSelParams *selParams = NULL;
        char *certDir = NULL;
        char *dirName = NULL;

        PKIX_TEST_STD_VARS();

        startTests("CertSelector");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        if (argc < 3) {
                printUsage();
                return (0);
        }

        dirName = argv[j+1];
        certDir = argv[j+3];

        /* Create a List of certs to use in testing the selector */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&certs, plContext));

        for (i = 0; i < NUMCERTS; i++) {

                cert = createCert(dirName, certList[i], plContext);

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                        (certs, (PKIX_PL_Object *)cert, plContext));
                if (i == POLICY1CERT) {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                                ((PKIX_PL_Object *)cert, plContext));
                        policy1Cert = cert;
                }
                if (i == ANYPOLICYCERT) {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                                ((PKIX_PL_Object *)cert, plContext));
                        anyPolicyCert = cert;
                }
                if (i == POLICY2CERT) {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                                ((PKIX_PL_Object *)cert, plContext));
                        policy2Cert = cert;
                }
                if (i == SUBJECTCERT) {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                                ((PKIX_PL_Object *)cert, plContext));
                        subjectCert = cert;
                }
                PKIX_TEST_DECREF_BC(cert);
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create
                (&nameConstraintsCerts, plContext));

        for (i = 0; i < NUMNCCERTS; i++) {

                cert = createCert(dirName, ncCertList[i], plContext);

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                        (nameConstraintsCerts,
                        (PKIX_PL_Object *)cert,
                        plContext));

                PKIX_TEST_DECREF_BC(cert);
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create
                (&subjAltNamesCerts, plContext));

        for (i = 0; i < NUMSANCERTS; i++) {

                cert = createCert(dirName, sanCertList[i], plContext);

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                        (subjAltNamesCerts,
                        (PKIX_PL_Object *)cert,
                        plContext));

                PKIX_TEST_DECREF_BC(cert);
        }

        subTest("test_CertSelector_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &emptySelector, plContext));

        subTest("Default Match, no parameters set");
        testSelector(emptySelector, certs, 0xFFFFFFFF);

        testSubjectMatch(certs, subjectCert);

        testBasicConstraintsMatch(certs);

        testPolicyMatch(certs, policy1Cert, policy2Cert, anyPolicyCert);

        testCertificateMatch(certs, subjectCert);

        testCertificateValidMatch(certs);

        subTest("Combination: pass only EE certs that assert some policy");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_Create(&selParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetBasicConstraints
                (selParams, -2, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (emptySelector, selParams, plContext));
        testSelector(emptySelector, certs, 0xC00);

        testNameConstraintsMatch(nameConstraintsCerts);

        testPathToNamesMatch(nameConstraintsCerts);

        testSubjAltNamesMatch(subjAltNamesCerts);

        testExtendedKeyUsageMatch(certDir);

        testKeyUsageMatch(certDir);

        testIssuerMatch(certDir);

        testSerialNumberVersionMatch(certDir);

        testCertValidMatch(certDir);

        testSubjKeyIdMatch(nameConstraintsCerts);

        testAuthKeyIdMatch(nameConstraintsCerts);

        testSubjPKAlgIdMatch(nameConstraintsCerts);

        testSubjPublicKeyMatch(nameConstraintsCerts);

        test_CertSelector_Duplicate(emptySelector);

        test_customCallback1(certs);

        test_customCallback2(certs, anyPolicyCert);

        subTest("test_CertSelector_Destroy");

        PKIX_TEST_DECREF_BC(emptySelector);



cleanup:

        PKIX_TEST_DECREF_AC(emptySelector);
        PKIX_TEST_DECREF_AC(certs);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(policy1Cert);
        PKIX_TEST_DECREF_AC(policy2Cert);
        PKIX_TEST_DECREF_AC(anyPolicyCert);
        PKIX_TEST_DECREF_AC(subjectCert);
        PKIX_TEST_DECREF_AC(selParams);
        PKIX_TEST_DECREF_AC(nameConstraintsCerts);
        PKIX_TEST_DECREF_AC(subjAltNamesCerts);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("CertSelector");

        return (0);
}
