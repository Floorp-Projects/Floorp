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
 * test_comcrlselparams.c
 *
 * Test ComCRLSelParams Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static void
testIssuer(PKIX_ComCRLSelParams *goodObject)
{
        PKIX_PL_String *issuer1String = NULL;
        PKIX_PL_String *issuer2String = NULL;
        PKIX_PL_String *issuer3String = NULL;
        PKIX_PL_X500Name *issuerName1 = NULL;
        PKIX_PL_X500Name *issuerName2 = NULL;
        PKIX_PL_X500Name *issuerName3 = NULL;
        PKIX_List *setIssuerList = NULL;
        PKIX_List *getIssuerList = NULL;
        PKIX_PL_String *issuerListString = NULL;
        char *name1 = "CN=yassir,OU=bcn,OU=east,O=sun,C=us";
        char *name2 = "CN=richard,OU=bcn,OU=east,O=sun,C=us";
        char *name3 = "CN=hanfei,OU=bcn,OU=east,O=sun,C=us";
        PKIX_Int32 length;
        PKIX_Boolean result = PKIX_FALSE;
        char *expectedAscii =
                "(CN=yassir,OU=bcn,OU=east,O=sun,"
                "C=us, CN=richard,OU=bcn,OU=east,O=sun,C=us, "
                "CN=hanfei,OU=bcn,OU=east,O=sun,C=us)";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_ComCRLSelParams Create Issuers");

        length = PL_strlen(name1);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_UTF8,
                                    name1,
                                    length,
                                    &issuer1String,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_X500Name_Create(issuer1String,
                                    &issuerName1,
                                    plContext));

        length = PL_strlen(name2);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_UTF8,
                                    name2,
                                    length,
                                    &issuer2String,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_X500Name_Create(issuer2String,
                                    &issuerName2,
                                    plContext));

        length = PL_strlen(name3);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_UTF8,
                                    name3,
                                    length,
                                    &issuer3String,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_X500Name_Create
                                    (issuer3String,
                                    &issuerName3,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&setIssuerList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                                    (setIssuerList,
                                    (PKIX_PL_Object *)issuerName1,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                                    (setIssuerList,
                                    (PKIX_PL_Object *)issuerName2,
                                    plContext));

        subTest("PKIX_ComCRLSelParams_AddIssuerName");

        /* Test adding an issuer to an empty list */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_AddIssuerName
                                    (goodObject, issuerName3, plContext));

        subTest("PKIX_ComCRLSelParams_GetIssuerNames");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_GetIssuerNames
                                    (goodObject, &getIssuerList, plContext));

        /* DECREF for GetIssuerNames */
        PKIX_TEST_DECREF_BC(getIssuerList);
        /* DECREF for AddIssuerName so next SetIssuerName start clean */
        PKIX_TEST_DECREF_BC(getIssuerList);

        /* Test setting issuer names on the list */
        subTest("PKIX_ComCRLSelParams_SetIssuerNames");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_SetIssuerNames
                                    (goodObject, setIssuerList, plContext));

        subTest("PKIX_ComCRLSelParams_GetIssuerNames");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_GetIssuerNames
                                    (goodObject, &getIssuerList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                    ((PKIX_PL_Object *)setIssuerList,
                                    (PKIX_PL_Object *)getIssuerList,
                                    &result,
                                    plContext));

        if (result != PKIX_TRUE) {
                pkixTestErrorMsg = "unexpected Issuers mismatch";
        }

        /* Test adding an issuer to existing list */
        subTest("PKIX_ComCRLSelParams_AddIssuerName");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_AddIssuerName
                                    (goodObject, issuerName3, plContext));

        subTest("PKIX_ComCRLSelParams_GetIssuerNames");
        PKIX_TEST_DECREF_BC(getIssuerList);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_GetIssuerNames
                                    (goodObject, &getIssuerList, plContext));


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                                    ((PKIX_PL_Object *)getIssuerList,
                                    &issuerListString,
                                    plContext));

        testToStringHelper((PKIX_PL_Object *)getIssuerList,
                            expectedAscii, plContext);

cleanup:

        PKIX_TEST_DECREF_AC(issuer1String);
        PKIX_TEST_DECREF_AC(issuer2String);
        PKIX_TEST_DECREF_AC(issuer3String);
        PKIX_TEST_DECREF_AC(issuerListString);
        PKIX_TEST_DECREF_AC(issuerName1);
        PKIX_TEST_DECREF_AC(issuerName2);
        PKIX_TEST_DECREF_AC(issuerName3);
        PKIX_TEST_DECREF_AC(setIssuerList);
        PKIX_TEST_DECREF_AC(getIssuerList);

        PKIX_TEST_RETURN();

}

void testCertificateChecking(
        char *dataCentralDir,
        char *goodInput,
        PKIX_ComCRLSelParams *goodObject)
{
        PKIX_PL_Cert *setCert = NULL;
        PKIX_PL_Cert *getCert = NULL;
        PKIX_Boolean result = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        subTest("Test CertificateChecking Cert Create");
        setCert = createCert(dataCentralDir, goodInput, plContext);
        if (setCert == NULL) {
                pkixTestErrorMsg = "create certificate failed";
                goto cleanup;
        }

        subTest("PKIX_ComCRLSelParams_SetCertificateChecking");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_SetCertificateChecking
                                    (goodObject, setCert, plContext));

        subTest("PKIX_ComCRLSelParams_GetCertificateChecking");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_GetCertificateChecking
                                    (goodObject, &getCert, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                    ((PKIX_PL_Object *)setCert,
                                    (PKIX_PL_Object *)getCert,
                                    &result, plContext));

        if (result != PKIX_TRUE) {
                pkixTestErrorMsg = "unexpected Cert mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(setCert);
        PKIX_TEST_DECREF_AC(getCert);

        PKIX_TEST_RETURN();
}

void testDateAndTime(PKIX_ComCRLSelParams *goodObject){

        PKIX_PL_Date *setDate = NULL;
        PKIX_PL_Date *getDate = NULL;
        char *asciiDate = "040329134847Z";
        PKIX_Boolean result = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_ComCRLSelParams_Date Create");
        setDate = createDate(asciiDate, plContext);

        subTest("PKIX_ComCRLSelParams_SetDateAndTime");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCRLSelParams_SetDateAndTime
                (goodObject, setDate, plContext));

        subTest("PKIX_ComCRLSelParams_GetDateAndTime");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCRLSelParams_GetDateAndTime
                (goodObject, &getDate, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                    ((PKIX_PL_Object *)setDate,
                                    (PKIX_PL_Object *)getDate,
                                    &result, plContext));

        if (result != PKIX_TRUE) {
                pkixTestErrorMsg = "unexpected DateAndTime mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(setDate);
        PKIX_TEST_DECREF_AC(getDate);

        PKIX_TEST_RETURN();
}

void testMaxMinCRLNumbers(PKIX_ComCRLSelParams *goodObject){
        PKIX_PL_BigInt *setMaxCrlNumber = NULL;
        PKIX_PL_BigInt *getMaxCrlNumber = NULL;
        PKIX_PL_BigInt *setMinCrlNumber = NULL;
        PKIX_PL_BigInt *getMinCrlNumber = NULL;
        char *asciiCrlNumber1 = "01";
        char *asciiCrlNumber99999 = "0909090909";
        PKIX_PL_String *crlNumber1String = NULL;
        PKIX_PL_String *crlNumber99999String = NULL;

        PKIX_Boolean result = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_ComCRLSelParams_SetMinCRLNumber");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiCrlNumber1,
                    PL_strlen(asciiCrlNumber1),
                    &crlNumber1String,
                    NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BigInt_Create
                    (crlNumber1String, &setMinCrlNumber, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_SetMinCRLNumber
                    (goodObject, setMinCrlNumber, NULL));

        subTest("PKIX_ComCRLSelParams_GetMinCRLNumber");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCRLSelParams_GetMinCRLNumber
                (goodObject, &getMinCrlNumber, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                    ((PKIX_PL_Object *)setMinCrlNumber,
                                    (PKIX_PL_Object *)getMinCrlNumber,
                                    &result, NULL));

        if (result != PKIX_TRUE) {
                pkixTestErrorMsg = "unexpected Minimum CRL Number mismatch";
        }

        subTest("PKIX_ComCRLSelParams_SetMaxCRLNumber");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiCrlNumber99999,
                    PL_strlen(asciiCrlNumber99999),
                    &crlNumber99999String,
                    NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BigInt_Create
                    (crlNumber99999String, &setMaxCrlNumber, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_SetMaxCRLNumber
                    (goodObject, setMaxCrlNumber, NULL));

        subTest("PKIX_ComCRLSelParams_GetMaxCRLNumber");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCRLSelParams_GetMaxCRLNumber
                (goodObject, &getMaxCrlNumber, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                    ((PKIX_PL_Object *)setMaxCrlNumber,
                                    (PKIX_PL_Object *)getMaxCrlNumber,
                                    &result, NULL));

        if (result != PKIX_TRUE) {
                pkixTestErrorMsg = "unexpected Maximum CRL Number mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(setMaxCrlNumber);
        PKIX_TEST_DECREF_AC(getMaxCrlNumber);
        PKIX_TEST_DECREF_AC(setMinCrlNumber);
        PKIX_TEST_DECREF_AC(getMinCrlNumber);
        PKIX_TEST_DECREF_AC(crlNumber1String);
        PKIX_TEST_DECREF_AC(crlNumber99999String);

        PKIX_TEST_RETURN();
}

void testDuplicate(PKIX_ComCRLSelParams *goodObject){

        PKIX_ComCRLSelParams *dupObject = NULL;
        PKIX_Boolean result = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_ComCRLSelParams_Duplicate");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Duplicate
                ((PKIX_PL_Object *)goodObject,
                (PKIX_PL_Object **)&dupObject,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                    ((PKIX_PL_Object *)goodObject,
                                    (PKIX_PL_Object *)dupObject,
                                    &result, plContext));

        if (result != PKIX_TRUE) {
                pkixTestErrorMsg =
                        "unexpected Duplicate ComCRLSelParams mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(dupObject);
        PKIX_TEST_RETURN();
}

void printUsage(char *pName){
        printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

/* Functional tests for ComCRLSelParams public functions */

int main(int argc, char *argv[]){

        char *dataCentralDir = NULL;
        char *goodInput = "yassir2yassir";
        PKIX_ComCRLSelParams *goodObject = NULL;
        PKIX_ComCRLSelParams *diffObject = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        startTests("ComCRLSelParams");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        if (argc < 2){
                printUsage(argv[0]);
                return (0);
        }

        dataCentralDir = argv[j+1];

        subTest("PKIX_ComCRLSelParams_Create");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_Create
                                    (&goodObject,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_Create
                                    (&diffObject,
                                    plContext));

        testIssuer(goodObject);

        testCertificateChecking(dataCentralDir, goodInput, goodObject);

        testDateAndTime(goodObject);

        testMaxMinCRLNumbers(goodObject);

        testDuplicate(goodObject);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                goodObject,
                diffObject,
                NULL,
                ComCRLSelParams,
                PKIX_TRUE);

cleanup:

        PKIX_TEST_DECREF_AC(goodObject);
        PKIX_TEST_DECREF_AC(diffObject);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("ComCRLSelParams");

        return (0);
}
