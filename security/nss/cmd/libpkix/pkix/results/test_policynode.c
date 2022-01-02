/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_policynode.c
 *
 * Test PolicyNode Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
test_GetChildren(
    PKIX_PolicyNode *goodNode,
    PKIX_PolicyNode *equalNode,
    PKIX_PolicyNode *diffNode)
{

    /*
 * Caution: be careful where you insert this test. PKIX_PolicyNode_GetChildren
 * is required by the API to return an immutable List, and it does it by setting
 * the List immutable. We don't make a copy because the assumption is that
 * certificate and policy processing have been completed before the user gets at
 * the public API. So subsequent tests of functions that modify the policy tree,
 * such as Prune, will fail if called after the execution of this test.
 */

    PKIX_Boolean isImmutable = PKIX_FALSE;
    PKIX_List *goodList = NULL;
    PKIX_List *equalList = NULL;
    PKIX_List *diffList = NULL;

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PolicyNode_GetChildren");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetChildren(goodNode, &goodList, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetChildren(equalNode, &equalList, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetChildren(diffNode, &diffList, plContext));

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodList, equalList, diffList, NULL, List, NULL);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_IsImmutable(goodList, &isImmutable, plContext));

    if (isImmutable != PKIX_TRUE) {
        testError("PKIX_PolicyNode_GetChildren returned a mutable List");
    }

cleanup:
    PKIX_TEST_DECREF_AC(goodList);
    PKIX_TEST_DECREF_AC(equalList);
    PKIX_TEST_DECREF_AC(diffList);

    PKIX_TEST_RETURN();
}

static void
test_GetParent(
    PKIX_PolicyNode *goodNode,
    PKIX_PolicyNode *equalNode,
    PKIX_PolicyNode *diffNode,
    char *expectedAscii)
{
    PKIX_PolicyNode *goodParent = NULL;
    PKIX_PolicyNode *equalParent = NULL;
    PKIX_PolicyNode *diffParent = NULL;

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PolicyNode_GetParent");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetParent(goodNode, &goodParent, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetParent(equalNode, &equalParent, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetParent(diffNode, &diffParent, plContext));

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodParent,
                                equalParent,
                                diffParent,
                                expectedAscii,
                                CertPolicyNode,
                                NULL);

cleanup:
    PKIX_TEST_DECREF_AC(goodParent);
    PKIX_TEST_DECREF_AC(equalParent);
    PKIX_TEST_DECREF_AC(diffParent);

    PKIX_TEST_RETURN();
}

/*
 * This test is the same as testDuplicateHelper, except that it
 * produces a more useful "Actual value" and "Expected value"
 * in the case of an unexpected mismatch.
 */
static void
test_DuplicateHelper(PKIX_PolicyNode *object, void *plContext)
{
    PKIX_PolicyNode *newObject = NULL;
    PKIX_Boolean cmpResult;
    PKIX_PL_String *original = NULL;
    PKIX_PL_String *copy = NULL;

    PKIX_TEST_STD_VARS();

    subTest("testing pkix_PolicyNode_Duplicate");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Duplicate((PKIX_PL_Object *)object,
                                                       (PKIX_PL_Object **)&newObject,
                                                       plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals((PKIX_PL_Object *)object,
                                                    (PKIX_PL_Object *)newObject,
                                                    &cmpResult,
                                                    plContext));

    if (!cmpResult) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object *)object, &original, plContext));
        testError("unexpected mismatch");
        (void)printf("original value:\t%s\n", original->escAsciiString);

        if (newObject) {
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object *)newObject, &copy, plContext));
            (void)printf("copy value:\t%s\n", copy->escAsciiString);
        } else {
            (void)printf("copy value:\t(NULL)\n");
        }
    }

cleanup:

    PKIX_TEST_DECREF_AC(newObject);
    PKIX_TEST_DECREF_AC(original);
    PKIX_TEST_DECREF_AC(copy);

    PKIX_TEST_RETURN();
}

static void
test_GetValidPolicy(
    PKIX_PolicyNode *goodNode,
    PKIX_PolicyNode *equalNode,
    PKIX_PolicyNode *diffNode,
    char *expectedAscii)
{
    PKIX_PL_OID *goodPolicy = NULL;
    PKIX_PL_OID *equalPolicy = NULL;
    PKIX_PL_OID *diffPolicy = NULL;

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PolicyNode_GetValidPolicy");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetValidPolicy(goodNode, &goodPolicy, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetValidPolicy(equalNode, &equalPolicy, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetValidPolicy(diffNode, &diffPolicy, plContext));

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodPolicy, equalPolicy, diffPolicy, expectedAscii, OID, NULL);

cleanup:
    PKIX_TEST_DECREF_AC(goodPolicy);
    PKIX_TEST_DECREF_AC(equalPolicy);
    PKIX_TEST_DECREF_AC(diffPolicy);

    PKIX_TEST_RETURN();
}

static void
test_GetPolicyQualifiers(
    PKIX_PolicyNode *goodNode,
    PKIX_PolicyNode *equalNode,
    PKIX_PolicyNode *diffNode,
    char *expectedAscii)
{
    PKIX_Boolean isImmutable = PKIX_FALSE;
    PKIX_List *goodList = NULL;
    PKIX_List *equalList = NULL;
    PKIX_List *diffList = NULL;

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PolicyNode_GetPolicyQualifiers");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetPolicyQualifiers(goodNode, &goodList, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetPolicyQualifiers(equalNode, &equalList, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetPolicyQualifiers(diffNode, &diffList, plContext));

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodList, equalList, diffList, expectedAscii, List, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_IsImmutable(goodList, &isImmutable, plContext));

    if (isImmutable != PKIX_TRUE) {
        testError("PKIX_PolicyNode_GetPolicyQualifiers returned a mutable List");
    }
cleanup:
    PKIX_TEST_DECREF_AC(goodList);
    PKIX_TEST_DECREF_AC(equalList);
    PKIX_TEST_DECREF_AC(diffList);

    PKIX_TEST_RETURN();
}

static void
test_GetExpectedPolicies(
    PKIX_PolicyNode *goodNode,
    PKIX_PolicyNode *equalNode,
    PKIX_PolicyNode *diffNode,
    char *expectedAscii)
{
    PKIX_Boolean isImmutable = PKIX_FALSE;
    PKIX_List *goodList = NULL;
    PKIX_List *equalList = NULL;
    PKIX_List *diffList = NULL;

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PolicyNode_GetExpectedPolicies");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetExpectedPolicies(goodNode, &goodList, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetExpectedPolicies(equalNode, &equalList, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetExpectedPolicies(diffNode, &diffList, plContext));

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodList, equalList, diffList, expectedAscii, List, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_IsImmutable(goodList, &isImmutable, plContext));

    if (isImmutable != PKIX_TRUE) {
        testError("PKIX_PolicyNode_GetExpectedPolicies returned a mutable List");
    }
cleanup:
    PKIX_TEST_DECREF_AC(goodList);
    PKIX_TEST_DECREF_AC(equalList);
    PKIX_TEST_DECREF_AC(diffList);

    PKIX_TEST_RETURN();
}

static void
test_IsCritical(
    PKIX_PolicyNode *goodNode,
    PKIX_PolicyNode *equalNode,
    PKIX_PolicyNode *diffNode)
{
    PKIX_Boolean goodBool = PKIX_FALSE;
    PKIX_Boolean equalBool = PKIX_FALSE;
    PKIX_Boolean diffBool = PKIX_FALSE;
    PKIX_TEST_STD_VARS();

    subTest("PKIX_PolicyNode_IsCritical");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_IsCritical(goodNode, &goodBool, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_IsCritical(equalNode, &equalBool, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_IsCritical(diffNode, &diffBool, plContext));

    if ((!goodBool) || (!equalBool) || (diffBool)) {
        testError("IsCritical returned unexpected value");
    }
cleanup:

    PKIX_TEST_RETURN();
}

static void
test_GetDepth(
    PKIX_PolicyNode *depth1Node,
    PKIX_PolicyNode *depth2Node,
    PKIX_PolicyNode *depth3Node)
{
    PKIX_UInt32 depth1 = 0;
    PKIX_UInt32 depth2 = 0;
    PKIX_UInt32 depth3 = 0;
    PKIX_TEST_STD_VARS();

    subTest("PKIX_PolicyNode_GetDepth");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetDepth(depth1Node, &depth1, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetDepth(depth2Node, &depth2, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PolicyNode_GetDepth(depth3Node, &depth3, plContext));

    if ((depth1 != 1) || (depth2 != 2) || (depth3 != 3)) {
        testError("GetDepth returned unexpected value");
    }

cleanup:

    PKIX_TEST_RETURN();
}

static void
printUsage(void)
{
    (void)printf("\nUSAGE:\ttest_policynode <NIST_FILES_DIR> \n\n");
}

int
test_policynode(int argc, char *argv[])
{

    /*
         * Create a tree with parent = anyPolicy,
         * child1 with Nist1+Nist2, child2 with Nist1.
         * Give each child another child, with policies Nist2
         * and Nist1, respectively. Pruning with a depth of two
         * should have no effect. Give one of the children
         * another child. Then pruning with a depth of three
         * should reduce the tree to a single strand, as child1
         * and child3 are removed.
         *
         *              parent (anyPolicy)
         *          /                   \
         *      child1(Nist1+Nist2)     child2(Nist1)
         *          |                       |
         *      child3(Nist2)           child4(Nist1)
         *                                  |
         *                              child5(Nist1)
         *
         */
    char *asciiAnyPolicy = "2.5.29.32.0";
    PKIX_PL_Cert *cert = NULL;
    PKIX_PL_CertPolicyInfo *nist1Policy = NULL;
    PKIX_PL_CertPolicyInfo *nist2Policy = NULL;
    PKIX_List *policyQualifierList = NULL;
    PKIX_PL_OID *oidAnyPolicy = NULL;
    PKIX_PL_OID *oidNist1Policy = NULL;
    PKIX_PL_OID *oidNist2Policy = NULL;
    PKIX_List *expectedAnyList = NULL;
    PKIX_List *expectedNist1List = NULL;
    PKIX_List *expectedNist2List = NULL;
    PKIX_List *expectedNist1Nist2List = NULL;
    PKIX_List *emptyList = NULL;
    PKIX_PolicyNode *parentNode = NULL;
    PKIX_PolicyNode *childNode1 = NULL;
    PKIX_PolicyNode *childNode2 = NULL;
    PKIX_PolicyNode *childNode3 = NULL;
    PKIX_PolicyNode *childNode4 = NULL;
    PKIX_PolicyNode *childNode5 = NULL;
    PKIX_PL_String *parentString = NULL;
    PKIX_Boolean pDelete = PKIX_FALSE;
    char *expectedParentAscii =
        "{2.16.840.1.101.3.2.1.48.2,(1.3.6.1.5.5.7.2.2:[30 5C "
        "1A 5A 71 31 3A 20 20 54 68 69 73 20 69 73 20 74 68 65"
        " 20 75 73 65 72 20 6E 6F 74 69 63 65 20 66 72 6F 6D 2"
        "0 71 75 61 6C 69 66 69 65 72 20 31 2E 20 20 54 68 69 "
        "73 20 63 65 72 74 69 66 69 63 61 74 65 20 69 73 20 66"
        " 6F 72 20 74 65 73 74 20 70 75 72 70 6F 73 65 73 20 6"
        "F 6E 6C 79]),Critical,(2.16.840.1.101.3.2.1.48.1[(1.3"
        ".6.1.5.5.7.2.2:[30 5C 1A 5A 71 31 3A 20 20 54 68 69 7"
        "3 20 69 73 20 74 68 65 20 75 73 65 72 20 6E 6F 74 69 "
        "63 65 20 66 72 6F 6D 20 71 75 61 6C 69 66 69 65 72 20"
        " 31 2E 20 20 54 68 69 73 20 63 65 72 74 69 66 69 63 6"
        "1 74 65 20 69 73 20 66 6F 72 20 74 65 73 74 20 70 75 "
        "72 70 6F 73 65 73 20 6F 6E 6C 79])], 2.16.840.1.101.3"
        ".2.1.48.2[(1.3.6.1.5.5.7.2.2:[30 5A 1A 58 71 32 3A 20"
        " 20 54 68 69 73 20 69 73 20 74 68 65 20 75 73 65 72 2"
        "0 6E 6F 74 69 63 65 20 66 72 6F 6D 20 71 75 61 6C 69 "
        "66 69 65 72 20 32 2E 20 20 54 68 69 73 20 75 73 65 72"
        " 20 6E 6F 74 69 63 65 20 73 68 6F 75 6C 64 20 6E 6F 7"
        "4 20 62 65 20 64 69 73 70 6C 61 79 65 64])]),1}\n"
        ". {2.16.840.1.101.3.2.1.48.2,(1.3.6.1.5.5.7.2.2:[30 5"
        "C 1A 5A 71 31 3A 20 20 54 68 69 73 20 69 73 20 74 68 "
        "65 20 75 73 65 72 20 6E 6F 74 69 63 65 20 66 72 6F 6D"
        " 20 71 75 61 6C 69 66 69 65 72 20 31 2E 20 20 54 68 6"
        "9 73 20 63 65 72 74 69 66 69 63 61 74 65 20 69 73 20 "
        "66 6F 72 20 74 65 73 74 20 70 75 72 70 6F 73 65 73 20"
        " 6F 6E 6C 79]),Critical,(2.16.840.1.101.3.2.1.48.2),2}";
    char *expectedValidAscii =
        "2.16.840.1.101.3.2.1.48.2";
    char *expectedQualifiersAscii =
        /* "(1.3.6.1.5.5.7.2.2)"; */
        "(1.3.6.1.5.5.7.2.2:[30 5C 1A 5A 71 31 3A 20 20 54 68 "
        "69 73 20 69 73 20 74 68 65 20 75 73 65 72 20 6E 6F 74"
        " 69 63 65 20 66 72 6F 6D 20 71 75 61 6C 69 66 69 65 7"
        "2 20 31 2E 20 20 54 68 69 73 20 63 65 72 74 69 66 69 "
        "63 61 74 65 20 69 73 20 66 6F 72 20 74 65 73 74 20 70"
        " 75 72 70 6F 73 65 73 20 6F 6E 6C 79])";
    char *expectedPoliciesAscii =
        "(2.16.840.1.101.3.2.1.48.1)";
    char *expectedTree =
        "{2.5.29.32.0,{},Critical,(2.5.29.32.0),0}\n"
        ". {2.16.840.1.101.3.2.1.48.2,(1.3.6.1.5.5.7.2.2:[30 5"
        "C 1A 5A 71 31 3A 20 20 54 68 69 73 20 69 73 20 74 68 "
        "65 20 75 73 65 72 20 6E 6F 74 69 63 65 20 66 72 6F 6D"
        " 20 71 75 61 6C 69 66 69 65 72 20 31 2E 20 20 54 68 6"
        "9 73 20 63 65 72 74 69 66 69 63 61 74 65 20 69 73 20 "
        "66 6F 72 20 74 65 73 74 20 70 75 72 70 6F 73 65 73 20"
        " 6F 6E 6C 79]),Critical,(2.16.840.1.101.3.2.1.48.1[(1"
        ".3.6.1.5.5.7.2.2:[30 5C 1A 5A 71 31 3A 20 20 54 68 69"
        " 73 20 69 73 20 74 68 65 20 75 73 65 72 20 6E 6F 74 6"
        "9 63 65 20 66 72 6F 6D 20 71 75 61 6C 69 66 69 65 72 "
        "20 31 2E 20 20 54 68 69 73 20 63 65 72 74 69 66 69 63"
        " 61 74 65 20 69 73 20 66 6F 72 20 74 65 73 74 20 70 7"
        "5 72 70 6F 73 65 73 20 6F 6E 6C 79])], 2.16.840.1.101"
        ".3.2.1.48.2[(1.3.6.1.5.5.7.2.2:[30 5A 1A 58 71 32 3A "
        "20 20 54 68 69 73 20 69 73 20 74 68 65 20 75 73 65 72"
        " 20 6E 6F 74 69 63 65 20 66 72 6F 6D 20 71 75 61 6C 6"
        "9 66 69 65 72 20 32 2E 20 20 54 68 69 73 20 75 73 65 "
        "72 20 6E 6F 74 69 63 65 20 73 68 6F 75 6C 64 20 6E 6F"
        " 74 20 62 65 20 64 69 73 70 6C 61 79 65 64])]"
        "),1}\n"
        ". . {2.16.840.1.101.3.2.1.48.2,(1.3.6.1.5.5.7.2.2:[30"
        " 5C 1A 5A 71 31 3A 20 20 54 68 69 73 20 69 73 20 74 6"
        "8 65 20 75 73 65 72 20 6E 6F 74 69 63 65 20 66 72 6F "
        "6D 20 71 75 61 6C 69 66 69 65 72 20 31 2E 20 20 54 68"
        " 69 73 20 63 65 72 74 69 66 69 63 61 74 65 20 69 73 2"
        "0 66 6F 72 20 74 65 73 74 20 70 75 72 70 6F 73 65 73 "
        "20 6F 6E 6C 79]),Critical,(2.16.840.1.101.3.2.1.48.2)"
        ",2}\n"
        ". {2.16.840.1.101.3.2.1.48.1,(1.3.6.1.5.5.7.2.2:[30 5"
        "C 1A 5A 71 31 3A 20 20 54 68 69 73 20 69 73 20 74 68 "
        "65 20 75 73 65 72 20 6E 6F 74 69 63 65 20 66 72 6F 6D"
        " 20 71 75 61 6C 69 66 69 65 72 20 31 2E 20 20 54 68 6"
        "9 73 20 63 65 72 74 69 66 69 63 61 74 65 20 69 73 20 "
        "66 6F 72 20 74 65 73 74 20 70 75 72 70 6F 73 65 73 20"
        " 6F 6E 6C 79]),Critical,(2.16.840.1.101.3.2.1.48.1),1}\n"
        ". . {2.16.840.1.101.3.2.1.48.1,(EMPTY),Not Critical,"
        "(2.16.840.1.101.3.2.1.48.1),2}\n"
        ". . . {2.16.840.1.101.3.2.1.48.1,{},Critical,(2.16.84"
        "0.1.101.3.2.1.48.1),3}";
    char *expectedPrunedTree =
        "{2.5.29.32.0,{},Critical,(2.5.29.32.0),0}\n"
        ". {2.16.840.1.101.3.2.1.48.1,(1.3.6.1.5.5.7.2.2:[30 5"
        "C 1A 5A 71 31 3A 20 20 54 68 69 73 20 69 73 20 74 68 "
        "65 20 75 73 65 72 20 6E 6F 74 69 63 65 20 66 72 6F 6D"
        " 20 71 75 61 6C 69 66 69 65 72 20 31 2E 20 20 54 68 6"
        "9 73 20 63 65 72 74 69 66 69 63 61 74 65 20 69 73 20 "
        "66 6F 72 20 74 65 73 74 20 70 75 72 70 6F 73 65 73 20"
        " 6F 6E 6C 79]),Critical,(2.16.840.1.101.3.2.1.48.1),1}\n"
        ". . {2.16.840.1.101.3.2.1.48.1,(EMPTY),Not Critical,"
        "(2.16.840.1.101.3.2.1.48.1),2}\n"
        ". . . {2.16.840.1.101.3.2.1.48.1,{},Critical,(2.16.84"
        "0.1.101.3.2.1.48.1),3}";

    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;
    char *dirName = NULL;

    PKIX_TEST_STD_VARS();

    if (argc < 2) {
        printUsage();
        return (0);
    }

    startTests("PolicyNode");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    dirName = argv[j + 1];

    subTest("Creating OID objects");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create(asciiAnyPolicy, &oidAnyPolicy, plContext));

    /* Read certificates to get real policies, qualifiers */

    cert = createCert(dirName, "UserNoticeQualifierTest16EE.crt", plContext);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation(cert, &expectedNist1Nist2List, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem(expectedNist1Nist2List,
                                                0,
                                                (PKIX_PL_Object **)&nist1Policy,
                                                plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem(expectedNist1Nist2List,
                                                1,
                                                (PKIX_PL_Object **)&nist2Policy,
                                                plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers(nist1Policy, &policyQualifierList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolicyId(nist1Policy, &oidNist1Policy, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolicyId(nist2Policy, &oidNist2Policy, plContext));

    subTest("Creating expectedPolicy List objects");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&expectedAnyList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&expectedNist1List, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&expectedNist2List, plContext));

    subTest("Populating expectedPolicy List objects");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(expectedAnyList, (PKIX_PL_Object *)oidAnyPolicy, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(expectedNist1List,
                                                   (PKIX_PL_Object *)oidNist1Policy,
                                                   plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(expectedNist2List,
                                                   (PKIX_PL_Object *)oidNist2Policy,
                                                   plContext));

    subTest("Creating PolicyNode objects");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&emptyList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_Create(oidAnyPolicy,
                                                     NULL,
                                                     PKIX_TRUE,
                                                     expectedAnyList,
                                                     &parentNode,
                                                     plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_Create(oidNist2Policy,
                                                     policyQualifierList,
                                                     PKIX_TRUE,
                                                     expectedNist1Nist2List,
                                                     &childNode1,
                                                     plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_Create(oidNist1Policy,
                                                     policyQualifierList,
                                                     PKIX_TRUE,
                                                     expectedNist1List,
                                                     &childNode2,
                                                     plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_Create(oidNist2Policy,
                                                     policyQualifierList,
                                                     PKIX_TRUE,
                                                     expectedNist2List,
                                                     &childNode3,
                                                     plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_Create(oidNist1Policy,
                                                     emptyList,
                                                     PKIX_FALSE,
                                                     expectedNist1List,
                                                     &childNode4,
                                                     plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_Create(oidNist1Policy,
                                                     NULL,
                                                     PKIX_TRUE,
                                                     expectedNist1List,
                                                     &childNode5,
                                                     plContext));

    subTest("Creating the PolicyNode tree");

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_AddToParent(parentNode, childNode1, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_AddToParent(parentNode, childNode2, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_AddToParent(childNode1, childNode3, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_AddToParent(childNode2, childNode4, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_AddToParent(childNode4, childNode5, plContext));

    subTest("Displaying PolicyNode objects");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object *)parentNode, &parentString, plContext));
    (void)printf("parentNode is\n\t%s\n", parentString->escAsciiString);

    testToStringHelper((PKIX_PL_Object *)parentNode, expectedTree, plContext);

    test_DuplicateHelper(parentNode, plContext);

    test_GetParent(childNode3, childNode3, childNode4, expectedParentAscii);
    test_GetValidPolicy(childNode1, childNode3, parentNode, expectedValidAscii);
    test_GetPolicyQualifiers(childNode1, childNode3, childNode4, expectedQualifiersAscii);
    test_GetExpectedPolicies(childNode2, childNode4, childNode3, expectedPoliciesAscii);
    test_IsCritical(childNode1, childNode2, childNode4);
    test_GetDepth(childNode2, childNode4, childNode5);

    subTest("pkix_PolicyNode_Prune");

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_Prune(parentNode, 2, &pDelete, plContext));

    testToStringHelper((PKIX_PL_Object *)parentNode, expectedTree, plContext);

    PKIX_TEST_EXPECT_NO_ERROR(pkix_PolicyNode_Prune(parentNode, 3, &pDelete, plContext));

    testToStringHelper((PKIX_PL_Object *)parentNode, expectedPrunedTree, plContext);

    test_GetChildren(parentNode, parentNode, childNode2);

cleanup:

    PKIX_TEST_DECREF_AC(cert);
    PKIX_TEST_DECREF_AC(nist1Policy);
    PKIX_TEST_DECREF_AC(nist2Policy);
    PKIX_TEST_DECREF_AC(policyQualifierList);
    PKIX_TEST_DECREF_AC(oidAnyPolicy);
    PKIX_TEST_DECREF_AC(oidNist1Policy);
    PKIX_TEST_DECREF_AC(oidNist2Policy);
    PKIX_TEST_DECREF_AC(expectedAnyList);
    PKIX_TEST_DECREF_AC(expectedNist1List);
    PKIX_TEST_DECREF_AC(expectedNist2List);
    PKIX_TEST_DECREF_AC(expectedNist1Nist2List);
    PKIX_TEST_DECREF_AC(emptyList);
    PKIX_TEST_DECREF_AC(parentNode);
    PKIX_TEST_DECREF_AC(childNode1);
    PKIX_TEST_DECREF_AC(childNode2);
    PKIX_TEST_DECREF_AC(childNode3);
    PKIX_TEST_DECREF_AC(childNode4);
    PKIX_TEST_DECREF_AC(childNode5);
    PKIX_TEST_DECREF_AC(parentString);

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("PolicyNode");

    return (0);
}
