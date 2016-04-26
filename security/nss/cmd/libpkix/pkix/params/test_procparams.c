/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_procparams.c
 *
 * Test ProcessingParams Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
    PKIX_TEST_STD_VARS();

    subTest("PKIX_ProcessingParams_Destroy");

    PKIX_TEST_DECREF_BC(goodObject);
    PKIX_TEST_DECREF_BC(equalObject);
    PKIX_TEST_DECREF_BC(diffObject);

cleanup:

    PKIX_TEST_RETURN();
}

static void
testGetAnchors(
    PKIX_ProcessingParams *goodObject,
    PKIX_ProcessingParams *equalObject)
{

    PKIX_List *goodAnchors = NULL;
    PKIX_List *equalAnchors = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ProcessingParams_GetTrustAnchors");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetTrustAnchors(goodObject, &goodAnchors, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetTrustAnchors(equalObject, &equalAnchors, plContext));

    testEqualsHelper((PKIX_PL_Object *)goodAnchors,
                     (PKIX_PL_Object *)equalAnchors,
                     PKIX_TRUE,
                     plContext);

cleanup:

    PKIX_TEST_DECREF_AC(goodAnchors);
    PKIX_TEST_DECREF_AC(equalAnchors);

    PKIX_TEST_RETURN();
}

static void
testGetSetDate(
    PKIX_ProcessingParams *goodObject,
    PKIX_ProcessingParams *equalObject)
{

    PKIX_PL_Date *setDate = NULL;
    PKIX_PL_Date *getDate = NULL;
    char *asciiDate = "040329134847Z";

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ProcessingParams_Get/SetDate");

    setDate = createDate(asciiDate, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetDate(goodObject, setDate, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetDate(goodObject, &getDate, plContext));

    testEqualsHelper((PKIX_PL_Object *)setDate,
                     (PKIX_PL_Object *)getDate,
                     PKIX_TRUE,
                     plContext);

    /* we want to make sure that goodObject and equalObject are "equal" */
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetDate(equalObject, setDate, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(setDate);
    PKIX_TEST_DECREF_AC(getDate);

    PKIX_TEST_RETURN();
}

static PKIX_Error *
userChecker1cb(
    PKIX_CertChainChecker *checker,
    PKIX_PL_Cert *cert,
    PKIX_List *unresolvedCriticalExtensions, /* list of PKIX_PL_OID */
    void **pNBIOContext,
    void *plContext)
{
    return (NULL);
}

static void
testGetSetCertChainCheckers(
    PKIX_ProcessingParams *goodObject,
    PKIX_ProcessingParams *equalObject)
{

    PKIX_CertChainChecker *checker = NULL;
    PKIX_List *setCheckersList = NULL;
    PKIX_List *getCheckersList = NULL;
    PKIX_PL_Date *date = NULL;
    char *asciiDate = "040329134847Z";

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ProcessingParams_Get/SetCertChainCheckers");

    date = createDate(asciiDate, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertChainChecker_Create(userChecker1cb,
                                                           PKIX_FALSE,
                                                           PKIX_FALSE,
                                                           NULL,
                                                           (PKIX_PL_Object *)date,
                                                           &checker,
                                                           plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&setCheckersList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(setCheckersList, (PKIX_PL_Object *)checker, plContext));
    PKIX_TEST_DECREF_BC(checker);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetCertChainCheckers(goodObject, setCheckersList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertChainChecker_Create(userChecker1cb,
                                                           PKIX_FALSE,
                                                           PKIX_FALSE,
                                                           NULL,
                                                           (PKIX_PL_Object *)date,
                                                           &checker,
                                                           plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_AddCertChainChecker(goodObject, checker, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetCertChainCheckers(goodObject, &getCheckersList, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(setCheckersList);
    PKIX_TEST_DECREF_AC(getCheckersList);
    PKIX_TEST_DECREF_AC(date);
    PKIX_TEST_DECREF_BC(checker);

    PKIX_TEST_RETURN();
}

static PKIX_Error *
userChecker2cb(
    PKIX_RevocationChecker *checker,
    PKIX_PL_Cert *cert,
    PKIX_UInt32 *pResult,
    void *plContext)
{
    return (NULL);
}

static void
testGetSetRevocationCheckers(
    PKIX_ProcessingParams *goodObject,
    PKIX_ProcessingParams *equalObject)
{

    PKIX_RevocationChecker *checker = NULL;
    PKIX_List *setCheckersList = NULL;
    PKIX_List *getCheckersList = NULL;
    PKIX_PL_Date *date = NULL;
    char *asciiDate = "040329134847Z";

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ProcessingParams_Get/SetRevocationCheckers");

    date = createDate(asciiDate, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_RevocationChecker_Create(userChecker2cb,
                                                            (PKIX_PL_Object *)date,
                                                            &checker,
                                                            plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&setCheckersList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(setCheckersList,
                                                   (PKIX_PL_Object *)checker,
                                                   plContext));
    PKIX_TEST_DECREF_BC(checker);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetRevocationCheckers(goodObject, setCheckersList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_RevocationChecker_Create(userChecker2cb,
                                                            (PKIX_PL_Object *)date,
                                                            &checker,
                                                            plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_AddRevocationChecker(goodObject, checker, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetRevocationCheckers(goodObject, &getCheckersList, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(setCheckersList);
    PKIX_TEST_DECREF_AC(getCheckersList);
    PKIX_TEST_DECREF_AC(date);
    PKIX_TEST_DECREF_BC(checker);

    PKIX_TEST_RETURN();
}

static void
testGetSetResourceLimits(
    PKIX_ProcessingParams *goodObject,
    PKIX_ProcessingParams *equalObject)

{
    PKIX_ResourceLimits *resourceLimits1 = NULL;
    PKIX_ResourceLimits *resourceLimits2 = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ProcessingParams_Get/SetResourceLimits");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create(&resourceLimits1, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create(&resourceLimits2, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout(resourceLimits1, 3, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth(resourceLimits1, 3, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime(resourceLimits1, 2, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetResourceLimits(goodObject, resourceLimits1, plContext));

    PKIX_TEST_DECREF_BC(resourceLimits2);
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetResourceLimits(goodObject, &resourceLimits2, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetResourceLimits(equalObject, resourceLimits2, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(resourceLimits1);
    PKIX_TEST_DECREF_AC(resourceLimits2);

    PKIX_TEST_RETURN();
}

static void
testGetSetConstraints(PKIX_ProcessingParams *goodObject)
{

    PKIX_CertSelector *setConstraints = NULL;
    PKIX_CertSelector *getConstraints = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ProcessingParams_Get/SetTargetCertConstraints");

    /*
        * After createConstraints is implemented
        * setConstraints = createConstraints();
        */

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetTargetCertConstraints(goodObject, setConstraints, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetTargetCertConstraints(goodObject, &getConstraints, plContext));

    testEqualsHelper((PKIX_PL_Object *)setConstraints,
                     (PKIX_PL_Object *)getConstraints,
                     PKIX_TRUE,
                     plContext);

cleanup:

    PKIX_TEST_DECREF_AC(setConstraints);
    PKIX_TEST_DECREF_AC(getConstraints);

    PKIX_TEST_RETURN();
}

static void
testGetSetInitialPolicies(
    PKIX_ProcessingParams *goodObject,
    char *asciiPolicyOID)
{
    PKIX_PL_OID *policyOID = NULL;
    PKIX_List *setPolicyList = NULL;
    PKIX_List *getPolicyList = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ProcessingParams_Get/SetInitialPolicies");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create(asciiPolicyOID, &policyOID, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&setPolicyList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(setPolicyList, (PKIX_PL_Object *)policyOID, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetImmutable(setPolicyList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetInitialPolicies(goodObject, setPolicyList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetInitialPolicies(goodObject, &getPolicyList, plContext));

    testEqualsHelper((PKIX_PL_Object *)setPolicyList,
                     (PKIX_PL_Object *)getPolicyList,
                     PKIX_TRUE,
                     plContext);

cleanup:
    PKIX_TEST_DECREF_AC(policyOID);
    PKIX_TEST_DECREF_AC(setPolicyList);
    PKIX_TEST_DECREF_AC(getPolicyList);

    PKIX_TEST_RETURN();
}

static void
testGetSetPolicyQualifiersRejected(
    PKIX_ProcessingParams *goodObject,
    PKIX_Boolean rejected)
{
    PKIX_Boolean getRejected = PKIX_FALSE;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ProcessingParams_Get/SetPolicyQualifiersRejected");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetPolicyQualifiersRejected(goodObject, rejected, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_GetPolicyQualifiersRejected(goodObject, &getRejected, plContext));

    if (rejected != getRejected) {
        testError("GetPolicyQualifiersRejected returned unexpected value");
    }

cleanup:

    PKIX_TEST_RETURN();
}

static void
printUsage(char *pName)
{
    printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int
test_procparams(int argc, char *argv[])
{

    PKIX_ProcessingParams *goodObject = NULL;
    PKIX_ProcessingParams *equalObject = NULL;
    PKIX_ProcessingParams *diffObject = NULL;
    PKIX_UInt32 actualMinorVersion;
    char *dataCentralDir = NULL;
    PKIX_UInt32 j = 0;

    char *oidAnyPolicy = PKIX_CERTIFICATEPOLICIES_ANYPOLICY_OID;
    char *oidNist1Policy = "2.16.840.1.101.3.2.1.48.2";

    char *goodInput = "yassir2yassir";
    char *diffInput = "yassir2bcn";

    char *expectedAscii =
        "[\n"
        "\tTrust Anchors: \n"
        "\t********BEGIN LIST OF TRUST ANCHORS********\n"
        "\t\t"
        "([\n"
        "\tTrusted CA Name:         "
        "CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
        "\tTrusted CA PublicKey:    ANSI X9.57 DSA Signature\n"
        "\tInitial Name Constraints:(null)\n"
        "]\n"
        ", [\n"
        "\tTrusted CA Name:         OU=bcn,OU=east,O=sun,C=us\n"
        "\tTrusted CA PublicKey:    ANSI X9.57 DSA Signature\n"
        "\tInitial Name Constraints:(null)\n"
        "]\n"
        ")\n"
        "\t********END LIST OF TRUST ANCHORS********\n"
        "\tDate:    \t\tMon Mar 29 08:48:47 2004\n"
        "\tTarget Constraints:    (null)\n"
        "\tInitial Policies:      (2.5.29.32.0)\n"
        "\tQualifiers Rejected:   FALSE\n"
        "\tCert Stores:           (EMPTY)\n"
        "\tResource Limits:       [\n"
        "\tMaxTime:                     2\n"
        "\tMaxFanout:                   3\n"
        "\tMaxDepth:                    3\n"
        "]\n\n"
        "\tCRL Checking Enabled:  0\n"
        "]\n";

    PKIX_TEST_STD_VARS();

    startTests("ProcessingParams");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc < 2) {
        printUsage(argv[0]);
        return (0);
    }

    dataCentralDir = argv[j + 1];

    subTest("PKIX_ProcessingParams_Create");
    goodObject = createProcessingParams(dataCentralDir,
                                        goodInput,
                                        diffInput,
                                        NULL,
                                        NULL,
                                        PKIX_FALSE,
                                        plContext);

    equalObject = createProcessingParams(dataCentralDir,
                                         goodInput,
                                         diffInput,
                                         NULL,
                                         NULL,
                                         PKIX_FALSE,
                                         plContext);

    diffObject = createProcessingParams(dataCentralDir,
                                        diffInput,
                                        goodInput,
                                        NULL,
                                        NULL,
                                        PKIX_FALSE,
                                        plContext);

    testGetAnchors(goodObject, equalObject);
    testGetSetDate(goodObject, equalObject);
    testGetSetCertChainCheckers(goodObject, equalObject);
    testGetSetRevocationCheckers(goodObject, equalObject);
    testGetSetResourceLimits(goodObject, equalObject);

    /*
        * XXX testGetSetConstraints(goodObject);
        */

    testGetSetInitialPolicies(goodObject, oidAnyPolicy);
    testGetSetInitialPolicies(equalObject, oidAnyPolicy);
    testGetSetInitialPolicies(diffObject, oidNist1Policy);
    testGetSetPolicyQualifiersRejected(goodObject, PKIX_FALSE);
    testGetSetPolicyQualifiersRejected(equalObject, PKIX_FALSE);
    testGetSetPolicyQualifiersRejected(diffObject, PKIX_TRUE);

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodObject,
                                equalObject,
                                diffObject,
                                NULL, /* expectedAscii, */
                                ProcessingParams,
                                PKIX_FALSE);

    testDestroy(goodObject, equalObject, diffObject);

cleanup:

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("ProcessingParams");

    return (0);
}
