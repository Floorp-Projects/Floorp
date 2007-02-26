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
 * test_comcertselparams.c
 *
 * Test Common Cert Selector Params
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void test_CreateOIDList(PKIX_List *certPolicyInfos, PKIX_List **pPolicyOIDs)
{
        PKIX_UInt32 i = 0;
        PKIX_UInt32 numInfos = 0;
        PKIX_PL_CertPolicyInfo *certPolicyInfo = NULL;
        PKIX_PL_OID *policyOID = NULL;
        PKIX_List *certPolicies = NULL;

        PKIX_TEST_STD_VARS();

        /* Convert from List of CertPolicyInfos to List of OIDs */
        if (certPolicyInfos) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                        (certPolicyInfos, &numInfos, plContext));
        }

        if (numInfos > 0) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create
                        (&certPolicies, plContext));
        }
        for (i = 0; i < numInfos; i++) {
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (certPolicyInfos,
                i,
                (PKIX_PL_Object **)&certPolicyInfo,
                plContext));
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolicyId
                (certPolicyInfo, &policyOID, plContext));
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (certPolicies, (PKIX_PL_Object *)policyOID, plContext));
            PKIX_TEST_DECREF_BC(certPolicyInfo);
            PKIX_TEST_DECREF_BC(policyOID);
        }

        *pPolicyOIDs = certPolicies;

cleanup:

        PKIX_TEST_DECREF_AC(certPolicyInfo);
        PKIX_TEST_DECREF_AC(policyOID);

        PKIX_TEST_RETURN();
}

void test_NameConstraints(char *dirName)
{
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_CertNameConstraints *getNameConstraints = NULL;
        PKIX_PL_CertNameConstraints *setNameConstraints = NULL;
        PKIX_ComCertSelParams *goodParams = NULL;
        char *expectedAscii =
                "[\n"
                "\t\tPermitted Name:  (OU=permittedSubtree1,"
                "O=Test Certificates,C=US, OU=permittedSubtree2,"
                "O=Test Certificates,C=US)\n"
                "\t\tExcluded Name:   (EMPTY)\n"
                "\t]\n";

        PKIX_TEST_STD_VARS();

        subTest("Create Cert for NameConstraints test");

        goodCert = createCert
                (dirName, "nameConstraintsDN2CACert.crt", plContext);

        subTest("PKIX_PL_Cert_GetNameConstraints");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (goodCert, &setNameConstraints, plContext));

        subTest("PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        subTest("PKIX_ComCertSelParams_SetNameConstraints");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetNameConstraints
                (goodParams, setNameConstraints, plContext));

        subTest("PKIX_ComCertSelParams_GetNameConstraints");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetNameConstraints
                (goodParams, &getNameConstraints, plContext));

        subTest("Compare NameConstraints");
        testEqualsHelper((PKIX_PL_Object *)setNameConstraints,
                (PKIX_PL_Object *)getNameConstraints,
                PKIX_TRUE,
                plContext);

        subTest("Compare NameConstraints with canned string");
        testToStringHelper
                ((PKIX_PL_Object *)getNameConstraints,
                expectedAscii,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(getNameConstraints);
        PKIX_TEST_DECREF_AC(setNameConstraints);
        PKIX_TEST_DECREF_AC(goodParams);

        PKIX_TEST_RETURN();
}

void test_PathToNames(void)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_List *setGenNames = NULL;
        PKIX_List *getGenNames = NULL;
        PKIX_PL_GeneralName *rfc822GenName = NULL;
        PKIX_PL_GeneralName *dnsGenName = NULL;
        PKIX_PL_GeneralName *dirGenName = NULL;
        PKIX_PL_GeneralName *uriGenName = NULL;
        PKIX_PL_GeneralName *oidGenName = NULL;
        char *rfc822Name = "john.doe@labs.com";
        char *dnsName = "comcast.net";
        char *dirName = "cn=john, ou=labs, o=sun, c=us";
        char *uriName = "http://comcast.net";
        char *oidName = "1.2.840.11";
        char *expectedAscii =
                "(john.doe@labs.com, "
                "comcast.net, "
                "CN=john,OU=labs,O=sun,C=us, "
                "http://comcast.net)";
        char *expectedAsciiAll =
                "(john.doe@labs.com, "
                "comcast.net, "
                "CN=john,OU=labs,O=sun,C=us, "
                "http://comcast.net, "
                "1.2.840.11)";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_GeneralName_Create");
        dnsGenName = createGeneralName(PKIX_DNS_NAME, dnsName, plContext);
        uriGenName = createGeneralName(PKIX_URI_NAME, uriName, plContext);
        oidGenName = createGeneralName(PKIX_OID_NAME, oidName, plContext);
        dirGenName = createGeneralName(PKIX_DIRECTORY_NAME, dirName, plContext);
        rfc822GenName = createGeneralName
                (PKIX_RFC822_NAME,
                rfc822Name,
                plContext);

        subTest("PKIX_PL_GeneralName List create and append");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&setGenNames, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setGenNames, (PKIX_PL_Object *)rfc822GenName, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setGenNames, (PKIX_PL_Object *)dnsGenName, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setGenNames, (PKIX_PL_Object *)dirGenName, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setGenNames, (PKIX_PL_Object *)uriGenName, plContext));

        subTest("PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        subTest("PKIX_ComCertSelParams_SetPathToNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPathToNames
                (goodParams, setGenNames, plContext));

        subTest("PKIX_ComCertSelParams_GetPathToNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetPathToNames
                (goodParams, &getGenNames, plContext));

        subTest("Compare GeneralName List");
        testEqualsHelper((PKIX_PL_Object *)setGenNames,
                (PKIX_PL_Object *)getGenNames,
                PKIX_TRUE,
                plContext);

        subTest("Compare GeneralName List with canned string");
        testToStringHelper
                ((PKIX_PL_Object *)getGenNames,
                expectedAscii,
                plContext);

        subTest("PKIX_ComCertSelParams_AddPathToName");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_AddPathToName
                (goodParams, oidGenName, plContext));

        PKIX_TEST_DECREF_BC(getGenNames);

        subTest("PKIX_ComCertSelParams_GetPathToNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetPathToNames
                (goodParams, &getGenNames, plContext));

        subTest("Compare GeneralName List with canned string");
        testToStringHelper
                ((PKIX_PL_Object *)getGenNames,
                expectedAsciiAll,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(setGenNames);
        PKIX_TEST_DECREF_AC(getGenNames);
        PKIX_TEST_DECREF_AC(rfc822GenName);
        PKIX_TEST_DECREF_AC(dnsGenName);
        PKIX_TEST_DECREF_AC(dirGenName);
        PKIX_TEST_DECREF_AC(uriGenName);
        PKIX_TEST_DECREF_AC(oidGenName);

        PKIX_TEST_RETURN();
}

void test_SubjAltNames(void)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_List *setGenNames = NULL;
        PKIX_List *getGenNames = NULL;
        PKIX_PL_GeneralName *rfc822GenName = NULL;
        PKIX_PL_GeneralName *dnsGenName = NULL;
        PKIX_PL_GeneralName *dirGenName = NULL;
        PKIX_PL_GeneralName *uriGenName = NULL;
        PKIX_PL_GeneralName *oidGenName = NULL;
        PKIX_Boolean matchAll = PKIX_TRUE;
        char *rfc822Name = "john.doe@labs.com";
        char *dnsName = "comcast.net";
        char *dirName = "cn=john, ou=labs, o=sun, c=us";
        char *uriName = "http://comcast.net";
        char *oidName = "1.2.840.11";
        char *expectedAscii =
                "(john.doe@labs.com, "
                "comcast.net, "
                "CN=john,OU=labs,O=sun,C=us, "
                "http://comcast.net)";
        char *expectedAsciiAll =
                "(john.doe@labs.com, "
                "comcast.net, "
                "CN=john,OU=labs,O=sun,C=us, "
                "http://comcast.net, "
                "1.2.840.11)";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_GeneralName_Create");
        dnsGenName = createGeneralName(PKIX_DNS_NAME, dnsName, plContext);
        uriGenName = createGeneralName(PKIX_URI_NAME, uriName, plContext);
        oidGenName = createGeneralName(PKIX_OID_NAME, oidName, plContext);
        dirGenName = createGeneralName(PKIX_DIRECTORY_NAME, dirName, plContext);
        rfc822GenName = createGeneralName
                (PKIX_RFC822_NAME,
                rfc822Name,
                plContext);

        subTest("PKIX_PL_GeneralName List create and append");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&setGenNames, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setGenNames, (PKIX_PL_Object *)rfc822GenName, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setGenNames, (PKIX_PL_Object *)dnsGenName, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setGenNames, (PKIX_PL_Object *)dirGenName, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setGenNames, (PKIX_PL_Object *)uriGenName, plContext));

        subTest("PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        subTest("PKIX_ComCertSelParams_SetSubjAltNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubjAltNames
                (goodParams, setGenNames, plContext));

        subTest("PKIX_ComCertSelParams_GetSubjAltNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubjAltNames
                (goodParams, &getGenNames, plContext));

        subTest("Compare GeneralName List");
        testEqualsHelper((PKIX_PL_Object *)setGenNames,
                (PKIX_PL_Object *)getGenNames,
                PKIX_TRUE,
                plContext);

        subTest("Compare GeneralName List with canned string");
        testToStringHelper
                ((PKIX_PL_Object *)getGenNames,
                expectedAscii,
                plContext);


        subTest("PKIX_ComCertSelParams_AddSubjAltName");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_AddSubjAltName
                (goodParams, oidGenName, plContext));

        PKIX_TEST_DECREF_BC(getGenNames);

        subTest("PKIX_ComCertSelParams_GetSubjAltNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubjAltNames
                (goodParams, &getGenNames, plContext));

        subTest("Compare GeneralName List with canned string");
        testToStringHelper
                ((PKIX_PL_Object *)getGenNames,
                expectedAsciiAll,
                plContext);

        subTest("PKIX_ComCertSelParams_GetMatchAllSubjAltNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetMatchAllSubjAltNames
                (goodParams, &matchAll, plContext));
        if (matchAll != PKIX_TRUE) {
                testError("unexpected mismatch <expect TRUE>");
        }

        subTest("PKIX_ComCertSelParams_SetMatchAllSubjAltNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetMatchAllSubjAltNames
                (goodParams, PKIX_FALSE, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetMatchAllSubjAltNames
                (goodParams, &matchAll, plContext));
        if (matchAll != PKIX_FALSE) {
                testError("unexpected mismatch <expect FALSE>");
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(setGenNames);
        PKIX_TEST_DECREF_AC(getGenNames);
        PKIX_TEST_DECREF_AC(rfc822GenName);
        PKIX_TEST_DECREF_AC(dnsGenName);
        PKIX_TEST_DECREF_AC(dirGenName);
        PKIX_TEST_DECREF_AC(uriGenName);
        PKIX_TEST_DECREF_AC(oidGenName);

        PKIX_TEST_RETURN();
}

void test_KeyUsages(void)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_PL_OID *ekuOid = NULL;
        PKIX_List *setExtKeyUsage = NULL;
        PKIX_List *getExtKeyUsage = NULL;
        PKIX_UInt32 getKeyUsage = 0;
        PKIX_UInt32 setKeyUsage = 0x1FF;
        PKIX_Boolean isEqual = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        subTest("PKIX_ComCertSelParams_SetKeyUsage");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetKeyUsage
                (goodParams, setKeyUsage, plContext));

        subTest("PKIX_ComCertSelParams_GetKeyUsage");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetKeyUsage
                (goodParams, &getKeyUsage, plContext));

        if (setKeyUsage != getKeyUsage) {
                testError("unexpected KeyUsage mismatch <expect equal>");
        }

        subTest("PKIX_PL_OID List create and append");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&setExtKeyUsage, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                ("1.3.6.1.5.5.7.3.1",   &ekuOid, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setExtKeyUsage, (PKIX_PL_Object *)ekuOid, plContext));
        PKIX_TEST_DECREF_BC(ekuOid);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                ("1.3.6.1.5.5.7.3.8",   &ekuOid, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (setExtKeyUsage, (PKIX_PL_Object *)ekuOid, plContext));
        PKIX_TEST_DECREF_BC(ekuOid);

        subTest("PKIX_ComCertSelParams_SetExtendedKeyUsage");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetExtendedKeyUsage
                (goodParams, setExtKeyUsage, plContext));

        subTest("PKIX_ComCertSelParams_GetExtendedKeyUsage");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetExtendedKeyUsage
                (goodParams, &getExtKeyUsage, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)setExtKeyUsage,
                (PKIX_PL_Object *)getExtKeyUsage,
                &isEqual,
                plContext));

        if (isEqual == PKIX_FALSE) {
                testError("unexpected ExtKeyUsage mismatch <expect equal>");
        }

cleanup:

        PKIX_TEST_DECREF_AC(ekuOid);
        PKIX_TEST_DECREF_AC(setExtKeyUsage);
        PKIX_TEST_DECREF_AC(getExtKeyUsage);
        PKIX_TEST_DECREF_AC(goodParams);

        PKIX_TEST_RETURN();
}

void test_Version_Issuer_SerialNumber(void)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_UInt32 version = 0;
        PKIX_PL_X500Name *setIssuer = NULL;
        PKIX_PL_X500Name *getIssuer = NULL;
        PKIX_PL_String *str = NULL;
        PKIX_PL_BigInt *setSerialNumber = NULL;
        PKIX_PL_BigInt *getSerialNumber = NULL;
        PKIX_Boolean isEqual = PKIX_FALSE;
        char *bigInt = "999999999999999999";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        /* Version */
        subTest("PKIX_ComCertSelParams_SetVersion");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetVersion
                (goodParams, 2, plContext));

        subTest("PKIX_ComCertSelParams_GetVersion");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetVersion
                (goodParams, &version, plContext));

        if (version != 2) {
                testError("unexpected Version mismatch <expect 2>");
        }

        /* Issuer */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, "CN=Test,O=Sun,C=US", 0, &str, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_X500Name_Create
                (str, &setIssuer, plContext));

        PKIX_TEST_DECREF_BC(str);

        subTest("PKIX_ComCertSelParams_SetIssuer");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetIssuer
                (goodParams, setIssuer, plContext));

        subTest("PKIX_ComCertSelParams_GetIssuer");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetIssuer
                (goodParams, &getIssuer, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)setIssuer,
                (PKIX_PL_Object *)getIssuer,
                &isEqual,
                plContext));

        if (isEqual == PKIX_FALSE) {
                testError("unexpected Issuer mismatch <expect equal>");
        }

        /* Serial Number */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, bigInt, PL_strlen(bigInt), &str, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BigInt_Create
                (str, &setSerialNumber, plContext));

        subTest("PKIX_ComCertSelParams_SetSerialNumber");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSerialNumber
                (goodParams, setSerialNumber, plContext));

        subTest("PKIX_ComCertSelParams_GetSerialNumber");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSerialNumber
                (goodParams, &getSerialNumber, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)setSerialNumber,
                (PKIX_PL_Object *)getSerialNumber,
                &isEqual,
                plContext));

        if (isEqual == PKIX_FALSE) {
                testError("unexpected Serial Number mismatch <expect equal>");
        }

cleanup:

        PKIX_TEST_DECREF_AC(str);
        PKIX_TEST_DECREF_AC(setIssuer);
        PKIX_TEST_DECREF_AC(getIssuer);
        PKIX_TEST_DECREF_AC(setSerialNumber);
        PKIX_TEST_DECREF_AC(getSerialNumber);
        PKIX_TEST_DECREF_AC(goodParams);

        PKIX_TEST_RETURN();
}

void test_SubjKeyId_AuthKeyId(void)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_PL_ByteArray *setKeyId = NULL;
        PKIX_PL_ByteArray *getKeyId = NULL;
        PKIX_Boolean isEqual = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        /* Subject Key Identifier */
        subTest("PKIX_PL_ByteArray_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_Create
                ((void*)"66099", 1, &setKeyId, plContext));

        subTest("PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        subTest("PKIX_ComCertSelParams_SetSubjectKeyIdentifier");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubjKeyIdentifier
                (goodParams, setKeyId, plContext));

        subTest("PKIX_ComCertSelParams_GetSubjectKeyIdentifier");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubjKeyIdentifier
                (goodParams, &getKeyId, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)setKeyId,
                (PKIX_PL_Object *)getKeyId,
                &isEqual,
                plContext));

        if (isEqual == PKIX_FALSE) {
                testError("unexpected Subject Key Id mismatch <expect equal>");
        }

        PKIX_TEST_DECREF_BC(setKeyId);
        PKIX_TEST_DECREF_BC(getKeyId);

        /* Authority Key Identifier */
        subTest("PKIX_PL_ByteArray_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_Create
                ((void*)"11022", 1, &setKeyId, plContext));

        subTest("PKIX_ComCertSelParams_SetAuthorityKeyIdentifier");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetAuthorityKeyIdentifier
                (goodParams, setKeyId, plContext));

        subTest("PKIX_ComCertSelParams_GetAuthorityKeyIdentifier");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_GetAuthorityKeyIdentifier
                (goodParams, &getKeyId, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)setKeyId,
                (PKIX_PL_Object *)getKeyId,
                &isEqual,
                plContext));

        if (isEqual == PKIX_FALSE) {
                testError("unexpected Auth Key Id mismatch <expect equal>");
        }

cleanup:

        PKIX_TEST_DECREF_AC(setKeyId);
        PKIX_TEST_DECREF_AC(getKeyId);
        PKIX_TEST_DECREF_AC(goodParams);

        PKIX_TEST_RETURN();
}

void test_SubjAlgId_SubjPublicKey(char *dirName)
{
        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_PL_OID *setAlgId = NULL;
        PKIX_PL_OID *getAlgId = NULL;
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_PublicKey *setPublicKey = NULL;
        PKIX_PL_PublicKey *getPublicKey = NULL;
        PKIX_Boolean isEqual = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        /* Subject Algorithm Identifier */
        subTest("PKIX_PL_OID_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                ("1.1.2.3", &setAlgId, plContext));

        subTest("PKIX_ComCertSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));

        subTest("PKIX_ComCertSelParams_SetSubjPKAlgId");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubjPKAlgId
                (goodParams, setAlgId, plContext));

        subTest("PKIX_ComCertSelParams_GetSubjPKAlgId");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubjPKAlgId
                (goodParams, &getAlgId, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)setAlgId,
                (PKIX_PL_Object *)getAlgId,
                &isEqual,
                plContext));

        if (isEqual == PKIX_FALSE) {
                testError("unexpected Subject Public Key Alg mismatch "
                            "<expect equal>");
        }

        /* Subject Public Key */
        subTest("Getting Cert for Subject Public Key");

        goodCert = createCert
                (dirName, "nameConstraintsDN2CACert.crt", plContext);

        subTest("PKIX_PL_Cert_GetSubjectPublicKey");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey
                (goodCert, &setPublicKey, plContext));

        subTest("PKIX_ComCertSelParams_SetSubjPubKey");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubjPubKey
                (goodParams, setPublicKey, plContext));

        subTest("PKIX_ComCertSelParams_GetSubjPubKey");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubjPubKey
                (goodParams, &getPublicKey, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)setPublicKey,
                (PKIX_PL_Object *)getPublicKey,
                &isEqual,
                plContext));

        if (isEqual == PKIX_FALSE) {
                testError("unexpected Subject Public Key mismatch "
                            "<expect equal>");
        }

cleanup:

        PKIX_TEST_DECREF_AC(setAlgId);
        PKIX_TEST_DECREF_AC(getAlgId);
        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(setPublicKey);
        PKIX_TEST_DECREF_AC(getPublicKey);

        PKIX_TEST_RETURN();
}

void printUsage(void) {
        (void) printf("\nUSAGE:\ttest_comcertselparams <NIST_FILES_DIR> \n\n");
}

int main(int argc, char *argv[]) {

        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_PL_Cert *testCert = NULL;
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_Cert *equalCert = NULL;
        PKIX_PL_Cert *diffCert = NULL;
        PKIX_PL_CertBasicConstraints *goodBasicConstraints = NULL;
        PKIX_PL_CertBasicConstraints *diffBasicConstraints = NULL;
        PKIX_List *testPolicyInfos = NULL; /* CertPolicyInfos */
        PKIX_List *cert2PolicyInfos = NULL; /* CertPolicyInfos */

        PKIX_ComCertSelParams *goodParams = NULL;
        PKIX_ComCertSelParams *equalParams = NULL;
        PKIX_PL_X500Name *goodSubject = NULL;
        PKIX_PL_X500Name *equalSubject = NULL;
        PKIX_PL_X500Name *diffSubject = NULL;
        PKIX_PL_X500Name *testSubject = NULL;
        PKIX_Int32 goodMinPathLength = 0;
        PKIX_Int32 equalMinPathLength = 0;
        PKIX_Int32 diffMinPathLength = 0;
        PKIX_Int32 testMinPathLength = 0;
        PKIX_List *goodPolicies = NULL; /* OIDs */
        PKIX_List *equalPolicies = NULL; /* OIDs */
        PKIX_List *testPolicies = NULL; /* OIDs */
        PKIX_List *cert2Policies = NULL; /* OIDs */

        PKIX_PL_Date *testDate = NULL;
        PKIX_PL_Date *goodDate = NULL;
        PKIX_PL_Date *equalDate = NULL;
        PKIX_PL_String *stringRep = NULL;
        char *asciiRep = NULL;
        char *dirName = NULL;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        if (argc < 2) {
                printUsage();
                return (0);
        }

        startTests("ComCertSelParams");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                (PKIX_TRUE, /* nssInitNeeded */
                useArenas,
                PKIX_MAJOR_VERSION,
                PKIX_MINOR_VERSION,
                PKIX_MINOR_VERSION,
                &actualMinorVersion,
                &plContext));

        dirName = argv[j+1];

        asciiRep = "050501000000Z";

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_String_Create
                (PKIX_ESCASCII, asciiRep, 0, &stringRep, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Date_Create_UTCTime(stringRep, &testDate, plContext));

        testCert = createCert
                (dirName, "PoliciesP1234CACert.crt",  plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubject
                (testCert, &testSubject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetBasicConstraints
                (testCert, &goodBasicConstraints, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BasicConstraints_GetPathLenConstraint
                (goodBasicConstraints, &testMinPathLength, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (testCert, &testPolicyInfos, plContext));

        /* Convert from List of CertPolicyInfos to List of OIDs */
        test_CreateOIDList(testPolicyInfos, &testPolicies);

        subTest("Create goodParams and set its fields");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&goodParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubject
                (goodParams, testSubject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetBasicConstraints
                (goodParams, testMinPathLength, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetCertificateValid
                (goodParams, testDate, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPolicy
                (goodParams, testPolicies, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetCertificate
                (goodParams, testCert, plContext));

        subTest("Duplicate goodParams and verify copy");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Duplicate
                ((PKIX_PL_Object *)goodParams,
                (PKIX_PL_Object **)&equalParams,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubject
                (goodParams, &goodSubject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetBasicConstraints
                (goodParams, &goodMinPathLength, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_GetCertificate
                (goodParams, &goodCert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetCertificateValid
                (goodParams, &goodDate, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetPolicy
                (goodParams, &goodPolicies, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubject
                (equalParams, &equalSubject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetBasicConstraints
                (equalParams, &equalMinPathLength, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetPolicy
                (equalParams, &equalPolicies, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetCertificate
                (equalParams, &equalCert, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetCertificateValid
                (equalParams, &equalDate, plContext));

        testEqualsHelper
                ((PKIX_PL_Object *)goodSubject,
                (PKIX_PL_Object *)equalSubject,
                PKIX_TRUE,
                plContext);

        if (goodMinPathLength != equalMinPathLength) {
                testError("unexpected mismatch");
                (void) printf("goodMinPathLength:\t%d\n", goodMinPathLength);
                (void) printf("equalMinPathLength:\t%d\n", equalMinPathLength);
        }

        testEqualsHelper((PKIX_PL_Object *)goodPolicies,
                (PKIX_PL_Object *)equalPolicies,
                PKIX_TRUE,
                plContext);

        testEqualsHelper((PKIX_PL_Object *)goodCert,
                (PKIX_PL_Object *)equalCert,
                PKIX_TRUE,
                plContext);

        testEqualsHelper((PKIX_PL_Object *)goodDate,
                (PKIX_PL_Object *)equalDate,
                PKIX_TRUE,
                plContext);

        PKIX_TEST_DECREF_BC(equalSubject);
        PKIX_TEST_DECREF_BC(equalPolicies);
        PKIX_TEST_DECREF_BC(equalCert);
        PKIX_TEST_DECREF_AC(equalDate);

        subTest("Set different values and verify differences");

        diffCert = createCert
                (dirName, "pathLenConstraint6CACert.crt", plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubject
                (diffCert, &diffSubject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetBasicConstraints
                (diffCert, &diffBasicConstraints, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BasicConstraints_GetPathLenConstraint
                (diffBasicConstraints, &diffMinPathLength, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (diffCert, &cert2PolicyInfos, plContext));
        test_CreateOIDList(cert2PolicyInfos, &cert2Policies);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetSubject(
                equalParams, diffSubject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetBasicConstraints
                (equalParams, diffMinPathLength, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetPolicy
                (equalParams, cert2Policies, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetSubject
                (equalParams, &equalSubject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetBasicConstraints
                (equalParams, &equalMinPathLength, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_GetPolicy
                (equalParams, &equalPolicies, plContext));

        testEqualsHelper
                ((PKIX_PL_Object *)goodSubject,
                (PKIX_PL_Object *)equalSubject,
                PKIX_FALSE,
                plContext);

        if (goodMinPathLength == equalMinPathLength) {
                testError("unexpected match");
                (void) printf("goodMinPathLength:\t%d\n", goodMinPathLength);
                (void) printf("equalMinPathLength:\t%d\n", equalMinPathLength);
        }

        testEqualsHelper
                ((PKIX_PL_Object *)goodPolicies,
                (PKIX_PL_Object *)equalPolicies,
                PKIX_FALSE,
                plContext);

        test_NameConstraints(dirName);
        test_PathToNames();
        test_SubjAltNames();
        test_KeyUsages();
        test_Version_Issuer_SerialNumber();
        test_SubjKeyId_AuthKeyId();
        test_SubjAlgId_SubjPublicKey(dirName);

cleanup:

        PKIX_TEST_DECREF_AC(testSubject);
        PKIX_TEST_DECREF_AC(goodSubject);
        PKIX_TEST_DECREF_AC(equalSubject);
        PKIX_TEST_DECREF_AC(diffSubject);
        PKIX_TEST_DECREF_AC(testSubject);
        PKIX_TEST_DECREF_AC(goodPolicies);
        PKIX_TEST_DECREF_AC(equalPolicies);
        PKIX_TEST_DECREF_AC(testPolicies);
        PKIX_TEST_DECREF_AC(cert2Policies);
        PKIX_TEST_DECREF_AC(goodParams);
        PKIX_TEST_DECREF_AC(equalParams);
        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(diffCert);
        PKIX_TEST_DECREF_AC(testCert);
        PKIX_TEST_DECREF_AC(goodBasicConstraints);
        PKIX_TEST_DECREF_AC(diffBasicConstraints);
        PKIX_TEST_DECREF_AC(testPolicyInfos);
        PKIX_TEST_DECREF_AC(cert2PolicyInfos);
        PKIX_TEST_DECREF_AC(stringRep);
        PKIX_TEST_DECREF_AC(testDate);
        PKIX_TEST_DECREF_AC(goodDate);


        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("ComCertSelParams");

        return (0);
}
