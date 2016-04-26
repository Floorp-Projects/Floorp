/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_policychecker.c
 *
 * Test Policy Checking
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

#define PKIX_TEST_MAX_CERTS     10

static void *plContext = NULL;

static
void printUsage(char *testname) {
        char *fmt =
                "USAGE: %s testname"
                " [ENE|EE] \"{OID[:OID]*}\" [A|E|P] cert [cert]*\n"
                "(The quotes are needed around the OID argument for dbx.)\n"
                "(The optional arg A indicates initialAnyPolicyInhibit.)\n"
                "(The optional arg E indicates initialExplicitPolicy.)\n"
                "(The optional arg P indicates initialPolicyMappingInhibit.)\n";
        printf(fmt, testname);
}

static
void printUsageMax(PKIX_UInt32 numCerts)
{
        printf("\nUSAGE ERROR: number of certs %d exceed maximum %d\n",
                numCerts, PKIX_TEST_MAX_CERTS);
}

static
PKIX_List *policySetParse(char *policyString)
{
        char *p = NULL;
        char *oid = NULL;
        char c = '\0';
        PKIX_Boolean validString = PKIX_FALSE;
        PKIX_PL_OID *plOID = NULL;
        PKIX_List *policySet = NULL;

        PKIX_TEST_STD_VARS();

        p = policyString;

        /*
         * There may or may not be quotes around the initial-policy-set
         * string. If they are omitted, dbx will strip off the curly braces.
         * If they are included, dbx will strip off the quotes, but if you
         * are running directly from a script, without dbx, the quotes will
         * not be stripped. We need to be able to handle both cases.
         */
        if (*p == '"') {
                p++;
        }

        if ('{' != *p++) {
                return (NULL);
        }
        oid = p;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&policySet, plContext));

        /* scan to the end of policyString */
        while (!validString) {
                /* scan to the end of the current OID string */
                c = *oid;
                while ((c != '\0') && (c != ':') && (c != '}')) {
                        c = *++oid;
                }

                if ((c != ':') || (c != '}')) {
                        *oid = '\0'; /* store a null terminator */
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                                (p, &plOID, plContext));

                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_List_AppendItem
                                (policySet,
                                (PKIX_PL_Object *)plOID,
                                plContext));

                        PKIX_TEST_DECREF_BC(plOID);
                        plOID = NULL;
                        if (c == '}') {
                                /*
                                * Any exit but this one means
                                * we were given a badly-formed string.
                                */
                                validString = PKIX_TRUE;
                        }
                        p = ++oid;
                }
        }


cleanup:
        if (!validString) {
                PKIX_TEST_DECREF_AC(plOID);
                PKIX_TEST_DECREF_AC(policySet);
                policySet = NULL;
        }

        PKIX_TEST_RETURN();

        return (policySet);
}

/*
 * FUNCTION: treeToStringHelper
 *  This function obtains the string representation of a PolicyNode
 *  Tree and compares it to the expected value.
 * PARAMETERS:
 *  "parent" - a PolicyNode, the root of a PolicyNodeTree;
 *      must be non-NULL.
 *  "expected" - the desired string.
 * THREAD SAFETY:
 *  Thread Safe
 *
 *  Multiple threads can safely call this function without worrying
 *  about conflicts, even if they're operating on the same object.
 * RETURNS:
 *  Nothing.
 */
static void
treeToStringHelper(PKIX_PolicyNode *parent, char *expected)
{
        PKIX_PL_String *stringRep = NULL;
        char *actual = NULL;
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                        ((PKIX_PL_Object *)parent, &stringRep, plContext));

        actual = PKIX_String2ASCII(stringRep, plContext);
        if (actual == NULL){
                pkixTestErrorMsg = "PKIX_String2ASCII Failed";
                goto cleanup;
        }

        if (PL_strcmp(actual, expected) != 0){
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%s\n", actual);
                (void) printf("Expected value:\t%s\n", expected);
        }

cleanup:

        PKIX_PL_Free(actual, plContext);

        PKIX_TEST_DECREF_AC(stringRep);

        PKIX_TEST_RETURN();
}

static
void testPass(char *dirName, char *goodInput, char *diffInput, char *dateAscii){

        PKIX_List *chain = NULL;
        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;

        PKIX_TEST_STD_VARS();

        subTest("Basic-Common-Fields <pass>");
        /*
         * Tests the Expiration, NameChaining, and Signature Checkers
         */

        chain = createCertChain(dirName, goodInput, diffInput, plContext);

        valParams = createValidateParams
                (dirName,
                goodInput,
                diffInput,
                dateAscii,
                NULL,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain
                                    (valParams, &valResult, NULL, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(chain);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);

        PKIX_TEST_RETURN();
}

static
void testNistTest1(char *dirName)
{
#define PKIX_TEST_NUM_CERTS     2
        char *trustAnchor =
                "TrustAnchorRootCertificate.crt";
        char *intermediateCert =
                "GoodCACert.crt";
        char *endEntityCert =
                "ValidCertificatePathTest1EE.crt";
        char *certNames[PKIX_TEST_NUM_CERTS];
        char *asciiAnyPolicy = "2.5.29.32.0";
        PKIX_PL_Cert *certs[PKIX_TEST_NUM_CERTS] = { NULL, NULL };

        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;
        PKIX_List *chain = NULL;
        PKIX_PL_OID *anyPolicyOID = NULL;
        PKIX_List *initialPolicies = NULL;
        char *anchorName = NULL;

        PKIX_TEST_STD_VARS();

        subTest("testNistTest1: Creating the cert chain");
        /*
         * Create a chain, but don't include the first certName.
         * That's the anchor, and is supplied separately from
         * the chain.
         */
        certNames[0] = intermediateCert;
        certNames[1] = endEntityCert;
        chain = createCertChainPlus
                (dirName, certNames, certs, PKIX_TEST_NUM_CERTS, plContext);

        subTest("testNistTest1: Creating the Validate Parameters");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                (asciiAnyPolicy, &anyPolicyOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_Create(&initialPolicies, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (initialPolicies, (PKIX_PL_Object *)anyPolicyOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetImmutable
                (initialPolicies, plContext));

        valParams = createValidateParams
                (dirName,
                trustAnchor,
                NULL,
                NULL,
                initialPolicies,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        subTest("testNistTest1: Validating the chain");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain
                (valParams, &valResult, NULL, plContext));


cleanup:

        PKIX_PL_Free(anchorName, plContext);

        PKIX_TEST_DECREF_AC(anyPolicyOID);
        PKIX_TEST_DECREF_AC(initialPolicies);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);
        PKIX_TEST_DECREF_AC(chain);

        PKIX_TEST_RETURN();
}

static
void testNistTest2(char *dirName)
{
#define PKIX_TEST_NUM_CERTS     2
        char *trustAnchor =
                "TrustAnchorRootCertificate.crt";
        char *intermediateCert =
                "GoodCACert.crt";
        char *endEntityCert =
                "ValidCertificatePathTest1EE.crt";
        char *certNames[PKIX_TEST_NUM_CERTS];
        char *asciiNist1Policy = "2.16.840.1.101.3.2.1.48.1";
        PKIX_PL_Cert *certs[PKIX_TEST_NUM_CERTS] = { NULL, NULL };

        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;
        PKIX_List *chain = NULL;
        PKIX_PL_OID *Nist1PolicyOID = NULL;
        PKIX_List *initialPolicies = NULL;
        char *anchorName = NULL;

        PKIX_TEST_STD_VARS();

        subTest("testNistTest2: Creating the cert chain");
        /*
         * Create a chain, but don't include the first certName.
         * That's the anchor, and is supplied separately from
         * the chain.
         */
        certNames[0] = intermediateCert;
        certNames[1] = endEntityCert;
        chain = createCertChainPlus
                (dirName, certNames, certs, PKIX_TEST_NUM_CERTS, plContext);

        subTest("testNistTest2: Creating the Validate Parameters");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                (asciiNist1Policy, &Nist1PolicyOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_Create(&initialPolicies, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (initialPolicies, (PKIX_PL_Object *)Nist1PolicyOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetImmutable
                (initialPolicies, plContext));

        valParams = createValidateParams
                (dirName,
                trustAnchor,
                NULL,
                NULL,
                initialPolicies,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        subTest("testNistTest2: Validating the chain");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain
                (valParams, &valResult, NULL, plContext));


cleanup:

        PKIX_PL_Free(anchorName, plContext);

        PKIX_TEST_DECREF_AC(Nist1PolicyOID);
        PKIX_TEST_DECREF_AC(initialPolicies);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);
        PKIX_TEST_DECREF_AC(chain);

        PKIX_TEST_RETURN();
}

static void printValidPolicyTree(PKIX_ValidateResult *valResult)
{
        PKIX_PolicyNode* validPolicyTree = NULL;
        PKIX_PL_String *treeString = NULL;

        PKIX_TEST_STD_VARS();
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPolicyTree
                (valResult, &validPolicyTree, plContext));
        if (validPolicyTree) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                        ((PKIX_PL_Object*)validPolicyTree,
                        &treeString,
                        plContext));
                (void) printf("validPolicyTree is\n\t%s\n",
                        treeString->escAsciiString);
        } else {
                (void) printf("validPolicyTree is NULL\n");
        }

cleanup:

        PKIX_TEST_DECREF_AC(validPolicyTree);
        PKIX_TEST_DECREF_AC(treeString);

        PKIX_TEST_RETURN();
}

int test_policychecker(int argc, char *argv[])
{

        PKIX_Boolean initialPolicyMappingInhibit = PKIX_FALSE;
        PKIX_Boolean initialAnyPolicyInhibit = PKIX_FALSE;
        PKIX_Boolean initialExplicitPolicy = PKIX_FALSE;
        PKIX_Boolean expectedResult = PKIX_FALSE;
        PKIX_UInt32 chainLength = 0;
        PKIX_UInt32 initArgs = 0;
        PKIX_UInt32 firstCert = 0;
        PKIX_UInt32 i = 0;
        PKIX_Int32  j = 0;
        PKIX_UInt32 actualMinorVersion;
        PKIX_ProcessingParams *procParams = NULL;
        char *firstTrustAnchor = "yassir2yassir";
        char *secondTrustAnchor = "yassir2bcn";
        char *dateAscii = "991201000000Z";
        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;
        PKIX_List *userInitialPolicySet = NULL; /* List of PKIX_PL_OID */
        char *certNames[PKIX_TEST_MAX_CERTS];
        PKIX_PL_Cert *certs[PKIX_TEST_MAX_CERTS];
        PKIX_List *chain = NULL;
        PKIX_Error *validationError = NULL;
	PKIX_VerifyNode *verifyTree = NULL;
	PKIX_PL_String *verifyString = NULL;
        char *dirName = NULL;
        char *dataCentralDir = NULL;
        char *anchorName = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        /*
         * Perform hard-coded tests if no command line args.
         * If command line args are provided, they must be:
         * arg[1]: test name
         * arg[2]: "ENE" or "EE", for "expect no error" or "expect error"
         * arg[3]: directory for certificates
         * arg[4]: user-initial-policy-set, consisting of braces
         *      containing zero or more OID sequences, separated by commas
         * arg[5]: (optional) "E", indicating initialExplicitPolicy
         * arg[firstCert]: the path and filename of the trust anchor certificate
         * arg[firstCert+1..(n-1)]: successive certificates in the chain
         * arg[n]: the end entity certificate
         *
         * Example: test_policychecker test1EE ENE
         *      {2.5.29.32.0,2.5.29.32.3.6} Anchor CA EndEntity
         */

        dirName = argv[3+j];
        dataCentralDir = argv[4+j];

        if (argc <= 5 || ((6 == argc) && (j))) {

                testPass
                    (dataCentralDir,
                    firstTrustAnchor,
                    secondTrustAnchor,
                    dateAscii);

                testNistTest1(dirName);

                testNistTest2(dirName);

                goto cleanup;
        }

        if (argc < (7 + j)) {
                printUsage(argv[0]);
                pkixTestErrorMsg = "Invalid command line arguments.";
                goto cleanup;
        }

        if (PORT_Strcmp(argv[2+j], "ENE") == 0) {
                expectedResult = PKIX_TRUE;
        } else if (PORT_Strcmp(argv[2+j], "EE") == 0) {
                expectedResult = PKIX_FALSE;
        } else {
                printUsage(argv[0]);
                pkixTestErrorMsg = "Invalid command line arguments.";
                goto cleanup;
        }

        userInitialPolicySet = policySetParse(argv[5+j]);
        if (!userInitialPolicySet) {
                printUsage(argv[0]);
                pkixTestErrorMsg = "Invalid command line arguments.";
                goto cleanup;
        }

        for (initArgs = 0; initArgs < 3; initArgs++) {
                if (PORT_Strcmp(argv[6+j+initArgs], "A") == 0) {
                        initialAnyPolicyInhibit = PKIX_TRUE;
                } else if (PORT_Strcmp(argv[6+j+initArgs], "E") == 0) {
                        initialExplicitPolicy = PKIX_TRUE;
                } else if (PORT_Strcmp(argv[6+j+initArgs], "P") == 0) {
                        initialPolicyMappingInhibit = PKIX_TRUE;
                } else {
                        break;
                }
        }

        firstCert = initArgs + j + 6;
        chainLength = argc - (firstCert + 1);
        if (chainLength > PKIX_TEST_MAX_CERTS) {
                printUsageMax(chainLength);
                pkixTestErrorMsg = "Invalid command line arguments.";
                goto cleanup;
        }

        /*
         * Create a chain, but don't include the first certName.
         * That's the anchor, and is supplied separately from
         * the chain.
         */
        for (i = 0; i < chainLength; i++) {

                certNames[i] = argv[i + (firstCert + 1)];
                certs[i] = NULL;
        }
        chain = createCertChainPlus
                (dirName, certNames, certs, chainLength, plContext);

        subTest(argv[1+j]);

        valParams = createValidateParams
                (dirName,
                argv[firstCert],
                NULL,
                NULL,
                userInitialPolicySet,
                initialPolicyMappingInhibit,
                initialAnyPolicyInhibit,
                initialExplicitPolicy,
                PKIX_FALSE,
                chain,
                plContext);

        if (expectedResult == PKIX_TRUE) {
                subTest("   (expecting successful validation)");

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain
                        (valParams, &valResult, &verifyTree, plContext));

                printValidPolicyTree(valResult);

        } else {
                subTest("   (expecting validation to fail)");
                validationError = PKIX_ValidateChain
                        (valParams, &valResult, &verifyTree, plContext);
                if (!validationError) {
                        printValidPolicyTree(valResult);
                        pkixTestErrorMsg = "Should have thrown an error here.";
                }
                PKIX_TEST_DECREF_BC(validationError);
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)verifyTree, &verifyString, plContext));
        (void) printf("verifyTree is\n%s\n", verifyString->escAsciiString);

cleanup:

        PKIX_PL_Free(anchorName, plContext);

        PKIX_TEST_DECREF_AC(verifyString);
        PKIX_TEST_DECREF_AC(verifyTree);
        PKIX_TEST_DECREF_AC(userInitialPolicySet);
        PKIX_TEST_DECREF_AC(chain);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);
        PKIX_TEST_DECREF_AC(validationError);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("PolicyChecker");

        return (0);
}
