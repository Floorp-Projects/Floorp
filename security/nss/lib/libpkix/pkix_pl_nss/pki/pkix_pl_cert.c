/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_cert.c
 *
 * Certificate Object Functions
 *
 */

#include "pkix_pl_cert.h"

extern PKIX_PL_HashTable *cachedCertSigTable;

/* --Private-Cert-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_Cert_IsExtensionCritical
 * DESCRIPTION:
 *
 *  Checks the Cert specified by "cert" to determine whether the extension
 *  whose tag is the UInt32 value given by "tag" is marked as a critical
 *  extension, and stores the result in "pCritical".
 *
 *  Tags are the index into the table "oids" of SECOidData defined in the
 *  file secoid.c. Constants, such as SEC_OID_X509_CERTIFICATE_POLICIES, are
 *  are defined in secoidt.h for most of the table entries.
 *
 *  If the specified tag is invalid (not in the list of tags) or if the
 *  extension is not found in the certificate, PKIX_FALSE is stored.
 *
 * PARAMETERS
 *  "cert"
 *      Address of Cert whose extensions are to be examined. Must be non-NULL.
 *  "tag"
 *      The UInt32 value of the tag for the extension whose criticality is
 *      to be determined
 *  "pCritical"
 *      Address where the Boolean value will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Cert_IsExtensionCritical(
        PKIX_PL_Cert *cert,
        PKIX_UInt32 tag,
        PKIX_Boolean *pCritical,
        void *plContext)
{
        PKIX_Boolean criticality = PKIX_FALSE;
        CERTCertExtension **extensions = NULL;
        SECStatus rv;

        PKIX_ENTER(CERT, "pkix_pl_Cert_IsExtensionCritical");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pCritical);

        extensions = cert->nssCert->extensions;
        PKIX_NULLCHECK_ONE(extensions);

        PKIX_CERT_DEBUG("\t\tCalling CERT_GetExtenCriticality).\n");
        rv = CERT_GetExtenCriticality(extensions, tag, &criticality);
        if (SECSuccess == rv) {
                *pCritical = criticality;
        } else {
                *pCritical = PKIX_FALSE;
        }

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_DecodePolicyInfo
 * DESCRIPTION:
 *
 *  Decodes the contents of the CertificatePolicy extension in the
 *  CERTCertificate pointed to by "nssCert", to create a List of
 *  CertPolicyInfos, which is stored at the address "pCertPolicyInfos".
 *  A CERTCertificate contains the DER representation of the Cert.
 *  If this certificate does not have a CertificatePolicy extension,
 *  NULL will be stored. If a List is returned, it will be immutable.
 *
 * PARAMETERS
 *  "nssCert"
 *      Address of the Cert data whose extension is to be examined. Must be
 *      non-NULL.
 *  "pCertPolicyInfos"
 *      Address where the List of CertPolicyInfos will be stored. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Cert_DecodePolicyInfo(
        CERTCertificate *nssCert,
        PKIX_List **pCertPolicyInfos,
        void *plContext)
{

        SECStatus rv;
        SECItem encodedCertPolicyInfo;

        /* Allocated in the arena; freed in CERT_Destroy... */
        CERTCertificatePolicies *certPol = NULL;
        CERTPolicyInfo **policyInfos = NULL;

        /* Holder for the return value */
        PKIX_List *infos = NULL;

        PKIX_PL_OID *pkixOID = NULL;
        PKIX_List *qualifiers = NULL;
        PKIX_PL_CertPolicyInfo *certPolicyInfo = NULL;
        PKIX_PL_CertPolicyQualifier *certPolicyQualifier = NULL;
        PKIX_PL_ByteArray *qualifierArray = NULL;

        PKIX_ENTER(CERT, "pkix_pl_Cert_DecodePolicyInfo");
        PKIX_NULLCHECK_TWO(nssCert, pCertPolicyInfos);

        /* get PolicyInfo as a SECItem */
        PKIX_CERT_DEBUG("\t\tCERT_FindCertExtension).\n");
        rv = CERT_FindCertExtension
                (nssCert,
                SEC_OID_X509_CERTIFICATE_POLICIES,
                &encodedCertPolicyInfo);
        if (SECSuccess != rv) {
                *pCertPolicyInfos = NULL;
                goto cleanup;
        }

        /* translate PolicyInfo to CERTCertificatePolicies */
        PKIX_CERT_DEBUG("\t\tCERT_DecodeCertificatePoliciesExtension).\n");
        certPol = CERT_DecodeCertificatePoliciesExtension
                (&encodedCertPolicyInfo);

        PORT_Free(encodedCertPolicyInfo.data);

        if (NULL == certPol) {
                PKIX_ERROR(PKIX_CERTDECODECERTIFICATEPOLICIESEXTENSIONFAILED);
        }

        /*
         * Check whether there are any policyInfos, so we can
         * avoid creating an unnecessary List
         */
        policyInfos = certPol->policyInfos;
        if (!policyInfos) {
                *pCertPolicyInfos = NULL;
                goto cleanup;
        }

        /* create a List of CertPolicyInfo Objects */
        PKIX_CHECK(PKIX_List_Create(&infos, plContext),
                PKIX_LISTCREATEFAILED);

        /*
         * Traverse the CERTCertificatePolicies structure,
         * building each PKIX_PL_CertPolicyInfo object in turn
         */
        while (*policyInfos != NULL) {
                CERTPolicyInfo *policyInfo = *policyInfos;
                CERTPolicyQualifier **policyQualifiers =
                                          policyInfo->policyQualifiers;
                if (policyQualifiers) {
                        /* create a PKIX_List of PKIX_PL_CertPolicyQualifiers */
                        PKIX_CHECK(PKIX_List_Create(&qualifiers, plContext),
                                PKIX_LISTCREATEFAILED);

                        while (*policyQualifiers != NULL) {
                            CERTPolicyQualifier *policyQualifier =
                                                         *policyQualifiers;

                            /* create the qualifier's OID object */
                            PKIX_CHECK(PKIX_PL_OID_CreateBySECItem
                                (&policyQualifier->qualifierID,
                                 &pkixOID, plContext),
                                PKIX_OIDCREATEFAILED);

                            /* create qualifier's ByteArray object */

                            PKIX_CHECK(PKIX_PL_ByteArray_Create
                                (policyQualifier->qualifierValue.data,
                                policyQualifier->qualifierValue.len,
                                &qualifierArray,
                                plContext),
                                PKIX_BYTEARRAYCREATEFAILED);

                            /* create a CertPolicyQualifier object */

                            PKIX_CHECK(pkix_pl_CertPolicyQualifier_Create
                                (pkixOID,
                                qualifierArray,
                                &certPolicyQualifier,
                                plContext),
                                PKIX_CERTPOLICYQUALIFIERCREATEFAILED);

                            PKIX_CHECK(PKIX_List_AppendItem
                                (qualifiers,
                                (PKIX_PL_Object *)certPolicyQualifier,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);

                            PKIX_DECREF(pkixOID);
                            PKIX_DECREF(qualifierArray);
                            PKIX_DECREF(certPolicyQualifier);

                            policyQualifiers++;
                        }

                        PKIX_CHECK(PKIX_List_SetImmutable
                                (qualifiers, plContext),
                                PKIX_LISTSETIMMUTABLEFAILED);
                }


                /*
                 * Create an OID object pkixOID from policyInfo->policyID.
                 * (The CERTPolicyInfo structure has an oid field, but it
                 * is of type SECOidTag. This function wants a SECItem.)
                 */
                PKIX_CHECK(PKIX_PL_OID_CreateBySECItem
                        (&policyInfo->policyID, &pkixOID, plContext),
                        PKIX_OIDCREATEFAILED);

                /* Create a CertPolicyInfo object */
                PKIX_CHECK(pkix_pl_CertPolicyInfo_Create
                        (pkixOID, qualifiers, &certPolicyInfo, plContext),
                        PKIX_CERTPOLICYINFOCREATEFAILED);

                /* Append the new CertPolicyInfo object to the list */
                PKIX_CHECK(PKIX_List_AppendItem
                        (infos, (PKIX_PL_Object *)certPolicyInfo, plContext),
                        PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(pkixOID);
                PKIX_DECREF(qualifiers);
                PKIX_DECREF(certPolicyInfo);

                policyInfos++;
        }

        /*
         * If there were no policies, we went straight to
         * cleanup, so we don't have to NULLCHECK infos.
         */
        PKIX_CHECK(PKIX_List_SetImmutable(infos, plContext),
                PKIX_LISTSETIMMUTABLEFAILED);

        *pCertPolicyInfos = infos;
        infos = NULL;

cleanup:
        if (certPol) {
            PKIX_CERT_DEBUG
                ("\t\tCalling CERT_DestroyCertificatePoliciesExtension).\n");
            CERT_DestroyCertificatePoliciesExtension(certPol);
        }

        PKIX_DECREF(infos);
        PKIX_DECREF(pkixOID);
        PKIX_DECREF(qualifiers);
        PKIX_DECREF(certPolicyInfo);
        PKIX_DECREF(certPolicyQualifier);
        PKIX_DECREF(qualifierArray);

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_DecodePolicyMapping
 * DESCRIPTION:
 *
 *  Decodes the contents of the PolicyMapping extension of the CERTCertificate
 *  pointed to by "nssCert", storing the resulting List of CertPolicyMaps at
 *  the address pointed to by "pCertPolicyMaps". If this certificate does not
 *  have a PolicyMapping extension, NULL will be stored. If a List is returned,
 *  it will be immutable.
 *
 * PARAMETERS
 *  "nssCert"
 *      Address of the Cert data whose extension is to be examined. Must be
 *      non-NULL.
 *  "pCertPolicyMaps"
 *      Address where the List of CertPolicyMaps will be stored. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Cert_DecodePolicyMapping(
        CERTCertificate *nssCert,
        PKIX_List **pCertPolicyMaps,
        void *plContext)
{
        SECStatus rv;
        SECItem encodedCertPolicyMaps;

        /* Allocated in the arena; freed in CERT_Destroy... */
        CERTCertificatePolicyMappings *certPolMaps = NULL;
        CERTPolicyMap **policyMaps = NULL;

        /* Holder for the return value */
        PKIX_List *maps = NULL;

        PKIX_PL_OID *issuerDomainOID = NULL;
        PKIX_PL_OID *subjectDomainOID = NULL;
        PKIX_PL_CertPolicyMap *certPolicyMap = NULL;

        PKIX_ENTER(CERT, "pkix_pl_Cert_DecodePolicyMapping");
        PKIX_NULLCHECK_TWO(nssCert, pCertPolicyMaps);

        /* get PolicyMappings as a SECItem */
        PKIX_CERT_DEBUG("\t\tCERT_FindCertExtension).\n");
        rv = CERT_FindCertExtension
                (nssCert, SEC_OID_X509_POLICY_MAPPINGS, &encodedCertPolicyMaps);
        if (SECSuccess != rv) {
                *pCertPolicyMaps = NULL;
                goto cleanup;
        }

        /* translate PolicyMaps to CERTCertificatePolicyMappings */
        certPolMaps = CERT_DecodePolicyMappingsExtension
                (&encodedCertPolicyMaps);

        PORT_Free(encodedCertPolicyMaps.data);

        if (!certPolMaps) {
                PKIX_ERROR(PKIX_CERTDECODEPOLICYMAPPINGSEXTENSIONFAILED);
        }

        PKIX_NULLCHECK_ONE(certPolMaps->policyMaps);

        policyMaps = certPolMaps->policyMaps;

        /* create a List of CertPolicyMap Objects */
        PKIX_CHECK(PKIX_List_Create(&maps, plContext),
                PKIX_LISTCREATEFAILED);

        /*
         * Traverse the CERTCertificatePolicyMappings structure,
         * building each CertPolicyMap object in turn
         */
        do {
                CERTPolicyMap *policyMap = *policyMaps;

                /* create the OID for the issuer Domain Policy */
                PKIX_CHECK(PKIX_PL_OID_CreateBySECItem
                        (&policyMap->issuerDomainPolicy,
                         &issuerDomainOID, plContext),
                        PKIX_OIDCREATEFAILED);

                /* create the OID for the subject Domain Policy */
                PKIX_CHECK(PKIX_PL_OID_CreateBySECItem
                        (&policyMap->subjectDomainPolicy,
                         &subjectDomainOID, plContext),
                        PKIX_OIDCREATEFAILED);

                /* create the CertPolicyMap */

                PKIX_CHECK(pkix_pl_CertPolicyMap_Create
                        (issuerDomainOID,
                        subjectDomainOID,
                        &certPolicyMap,
                        plContext),
                        PKIX_CERTPOLICYMAPCREATEFAILED);

                PKIX_CHECK(PKIX_List_AppendItem
                        (maps, (PKIX_PL_Object *)certPolicyMap, plContext),
                        PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(issuerDomainOID);
                PKIX_DECREF(subjectDomainOID);
                PKIX_DECREF(certPolicyMap);

                policyMaps++;
        } while (*policyMaps != NULL);

        PKIX_CHECK(PKIX_List_SetImmutable(maps, plContext),
                PKIX_LISTSETIMMUTABLEFAILED);

        *pCertPolicyMaps = maps;
        maps = NULL;

cleanup:
        if (certPolMaps) {
            PKIX_CERT_DEBUG
                ("\t\tCalling CERT_DestroyPolicyMappingsExtension).\n");
            CERT_DestroyPolicyMappingsExtension(certPolMaps);
        }

        PKIX_DECREF(maps);
        PKIX_DECREF(issuerDomainOID);
        PKIX_DECREF(subjectDomainOID);
        PKIX_DECREF(certPolicyMap);

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_DecodePolicyConstraints
 * DESCRIPTION:
 *
 *  Decodes the contents of the PolicyConstraints extension in the
 *  CERTCertificate pointed to by "nssCert", to obtain SkipCerts values
 *  which are stored at the addresses "pExplicitPolicySkipCerts" and
 *  "pInhibitMappingSkipCerts", respectively. If this certificate does
 *  not have an PolicyConstraints extension, or if either of the optional
 *  components is not supplied, this function stores a value of -1 for any
 *  missing component.
 *
 * PARAMETERS
 *  "nssCert"
 *      Address of the Cert data whose extension is to be examined. Must be
 *      non-NULL.
 *  "pExplicitPolicySkipCerts"
 *      Address where the SkipCert value for the requireExplicitPolicy
 *      component will be stored. Must be non-NULL.
 *  "pInhibitMappingSkipCerts"
 *      Address where the SkipCert value for the inhibitPolicyMapping
 *      component will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Cert_DecodePolicyConstraints(
        CERTCertificate *nssCert,
        PKIX_Int32 *pExplicitPolicySkipCerts,
        PKIX_Int32 *pInhibitMappingSkipCerts,
        void *plContext)
{
        CERTCertificatePolicyConstraints policyConstraints;
        SECStatus rv;
        SECItem encodedCertPolicyConstraints;
        PKIX_Int32 explicitPolicySkipCerts = -1;
        PKIX_Int32 inhibitMappingSkipCerts = -1;

        PKIX_ENTER(CERT, "pkix_pl_Cert_DecodePolicyConstraints");
        PKIX_NULLCHECK_THREE
                (nssCert, pExplicitPolicySkipCerts, pInhibitMappingSkipCerts);

        /* get the two skipCert values as SECItems */
        PKIX_CERT_DEBUG("\t\tCalling CERT_FindCertExtension).\n");
        rv = CERT_FindCertExtension
                (nssCert,
                SEC_OID_X509_POLICY_CONSTRAINTS,
                &encodedCertPolicyConstraints);

        if (rv == SECSuccess) {

                policyConstraints.explicitPolicySkipCerts.data =
                        (unsigned char *)&explicitPolicySkipCerts;
                policyConstraints.inhibitMappingSkipCerts.data =
                        (unsigned char *)&inhibitMappingSkipCerts;

                /* translate DER to CERTCertificatePolicyConstraints */
                rv = CERT_DecodePolicyConstraintsExtension
                        (&policyConstraints, &encodedCertPolicyConstraints);

                PORT_Free(encodedCertPolicyConstraints.data);

                if (rv != SECSuccess) {
                    PKIX_ERROR
                        (PKIX_CERTDECODEPOLICYCONSTRAINTSEXTENSIONFAILED);
                }
        }

        *pExplicitPolicySkipCerts = explicitPolicySkipCerts;
        *pInhibitMappingSkipCerts = inhibitMappingSkipCerts;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_DecodeInhibitAnyPolicy
 * DESCRIPTION:
 *
 *  Decodes the contents of the InhibitAnyPolicy extension in the
 *  CERTCertificate pointed to by "nssCert", to obtain a SkipCerts value,
 *  which is stored at the address "pSkipCerts". If this certificate does
 *  not have an InhibitAnyPolicy extension, -1 will be stored.
 *
 * PARAMETERS
 *  "nssCert"
 *      Address of the Cert data whose InhibitAnyPolicy extension is to be
 *      processed. Must be non-NULL.
 *  "pSkipCerts"
 *      Address where the SkipCert value from the InhibitAnyPolicy extension
 *      will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Cert_DecodeInhibitAnyPolicy(
        CERTCertificate *nssCert,
        PKIX_Int32 *pSkipCerts,
        void *plContext)
{
        CERTCertificateInhibitAny inhibitAny;
        SECStatus rv;
        SECItem encodedCertInhibitAny;
        PKIX_Int32 skipCerts = -1;

        PKIX_ENTER(CERT, "pkix_pl_Cert_DecodeInhibitAnyPolicy");
        PKIX_NULLCHECK_TWO(nssCert, pSkipCerts);

        /* get InhibitAny as a SECItem */
        PKIX_CERT_DEBUG("\t\tCalling CERT_FindCertExtension).\n");
        rv = CERT_FindCertExtension
                (nssCert, SEC_OID_X509_INHIBIT_ANY_POLICY, &encodedCertInhibitAny);

        if (rv == SECSuccess) {
                inhibitAny.inhibitAnySkipCerts.data =
                        (unsigned char *)&skipCerts;

                /* translate DER to CERTCertificateInhibitAny */
                rv = CERT_DecodeInhibitAnyExtension
                        (&inhibitAny, &encodedCertInhibitAny);

                PORT_Free(encodedCertInhibitAny.data);

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_CERTDECODEINHIBITANYEXTENSIONFAILED);
                }
        }

        *pSkipCerts = skipCerts;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_GetNssSubjectAltNames
 * DESCRIPTION:
 *
 *  Retrieves the Subject Alternative Names of the certificate specified by
 *  "cert" and stores it at "pNssSubjAltNames". If the Subject Alternative
 *  Name extension is not present, NULL is returned at "pNssSubjAltNames".
 *  If the Subject Alternative Names has not been previously decoded, it is
 *  decoded here with lock on the "cert" unless the flag "hasLock" indicates
 *  the lock had been obtained at a higher call level.
 *
 * PARAMETERS
 *  "cert"
 *      Address of the certificate whose Subject Alternative Names extensions
 *      is retrieved. Must be non-NULL.
 *  "hasLock"
 *      Boolean indicates caller has acquired a lock.
 *      Must be non-NULL.
 *  "pNssSubjAltNames"
 *      Address where the returned Subject Alternative Names will be stored.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Cert_GetNssSubjectAltNames(
        PKIX_PL_Cert *cert,
        PKIX_Boolean hasLock,
        CERTGeneralName **pNssSubjAltNames,
        void *plContext)
{
        CERTCertificate *nssCert = NULL;
        CERTGeneralName *nssOriginalAltName = NULL;
        PLArenaPool *arena = NULL;
        SECItem altNameExtension = {siBuffer, NULL, 0};
        SECStatus rv = SECFailure;

        PKIX_ENTER(CERT, "pkix_pl_Cert_GetNssSubjectAltNames");
        PKIX_NULLCHECK_THREE(cert, pNssSubjAltNames, cert->nssCert);

        nssCert = cert->nssCert;

        if ((cert->nssSubjAltNames == NULL) && (!cert->subjAltNamesAbsent)){

                if (!hasLock) {
                    PKIX_OBJECT_LOCK(cert);
                }

                if ((cert->nssSubjAltNames == NULL) &&
                    (!cert->subjAltNamesAbsent)){

                    PKIX_PL_NSSCALLRV(CERT, rv, CERT_FindCertExtension,
                        (nssCert,
                        SEC_OID_X509_SUBJECT_ALT_NAME,
                        &altNameExtension));

                    if (rv != SECSuccess) {
                        *pNssSubjAltNames = NULL;
                        cert->subjAltNamesAbsent = PKIX_TRUE;
                        goto cleanup;
                    }

                    if (cert->arenaNameConstraints == NULL) {
                        PKIX_PL_NSSCALLRV(CERT, arena, PORT_NewArena,
                                (DER_DEFAULT_CHUNKSIZE));

                        if (arena == NULL) {
                            PKIX_ERROR(PKIX_OUTOFMEMORY);
                        }
                        cert->arenaNameConstraints = arena;
                    }

                    PKIX_PL_NSSCALLRV
                        (CERT,
                        nssOriginalAltName,
                        (CERTGeneralName *) CERT_DecodeAltNameExtension,
                        (cert->arenaNameConstraints, &altNameExtension));

                    PKIX_PL_NSSCALL(CERT, PORT_Free, (altNameExtension.data));

                    if (nssOriginalAltName == NULL) {
                        PKIX_ERROR(PKIX_CERTDECODEALTNAMEEXTENSIONFAILED);
                    }
                    cert->nssSubjAltNames = nssOriginalAltName;

                }

                if (!hasLock) {
                    PKIX_OBJECT_UNLOCK(cert);
                }
        }

        *pNssSubjAltNames = cert->nssSubjAltNames;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_CheckExtendKeyUsage
 * DESCRIPTION:
 *
 *  For each of the ON bit in "requiredExtendedKeyUsages" that represents its
 *  SECCertUsageEnum type, this function checks "cert"'s certType (extended
 *  key usage) and key usage with what is required for SECCertUsageEnum type.
 *
 * PARAMETERS
 *  "cert"
 *      Address of the certificate whose Extended Key Usage extensions
 *      is retrieved. Must be non-NULL.
 *  "requiredExtendedKeyUsages"
 *      An unsigned integer, its bit location is ON based on the required key
 *      usage value representing in SECCertUsageEnum.
 *  "pPass"
 *      Address where the return value, indicating key usage check passed, is
 *       stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Cert_CheckExtendedKeyUsage(
        PKIX_PL_Cert *cert,
        PKIX_UInt32 requiredExtendedKeyUsages,
        PKIX_Boolean *pPass,
        void *plContext)
{
        PKIX_PL_CertBasicConstraints *basicConstraints = NULL;
        PKIX_UInt32 certType = 0;
        PKIX_UInt32 requiredKeyUsage = 0;
        PKIX_UInt32 requiredCertType = 0;
        PKIX_UInt32 requiredExtendedKeyUsage = 0;
        PKIX_UInt32 i;
        PKIX_Boolean isCA = PKIX_FALSE;
        SECStatus rv = SECFailure;

        PKIX_ENTER(CERT, "pkix_pl_Cert_CheckExtendKeyUsage");
        PKIX_NULLCHECK_THREE(cert, pPass, cert->nssCert);

        *pPass = PKIX_FALSE;

        PKIX_CERT_DEBUG("\t\tCalling cert_GetCertType).\n");
        cert_GetCertType(cert->nssCert);
        certType = cert->nssCert->nsCertType;

        PKIX_CHECK(PKIX_PL_Cert_GetBasicConstraints
                    (cert,
                    &basicConstraints,
                    plContext),
                    PKIX_CERTGETBASICCONSTRAINTFAILED);

        if (basicConstraints != NULL) {
                PKIX_CHECK(PKIX_PL_BasicConstraints_GetCAFlag
                    (basicConstraints, &isCA, plContext),
                    PKIX_BASICCONSTRAINTSGETCAFLAGFAILED);
        }

        i = 0;
        while (requiredExtendedKeyUsages != 0) {

                /* Find the bit location of the right-most non-zero bit */
                while (requiredExtendedKeyUsages != 0) {
                        if (((1 << i) & requiredExtendedKeyUsages) != 0) {
                                requiredExtendedKeyUsage = 1 << i;
                                break;
                        }
                        i++;
                }
                requiredExtendedKeyUsages ^= requiredExtendedKeyUsage;

                requiredExtendedKeyUsage = i;

                PKIX_PL_NSSCALLRV(CERT, rv, CERT_KeyUsageAndTypeForCertUsage,
                        (requiredExtendedKeyUsage,
                        isCA,
                        &requiredKeyUsage,
                        &requiredCertType));

                if (!(certType & requiredCertType)) {
                        goto cleanup;
                }

                PKIX_PL_NSSCALLRV(CERT, rv, CERT_CheckKeyUsage,
                        (cert->nssCert, requiredKeyUsage));
                if (rv != SECSuccess) {
                        goto cleanup;
                }
                i++;

        }

        *pPass = PKIX_TRUE;

cleanup:
        PKIX_DECREF(basicConstraints);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the Cert pointed
 *  to by "cert" and stores it at "pString", where the value of
 *  "partialString" determines whether a full or partial representation of
 *  the Cert is stored.
 *
 * PARAMETERS
 *  "cert"
 *      Address of Cert whose string representation is desired.
 *      Must be non-NULL.
 *  "partialString"
 *      Boolean indicating whether a partial Cert representation is desired.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Cert_ToString_Helper(
        PKIX_PL_Cert *cert,
        PKIX_Boolean partialString,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *certString = NULL;
        char *asciiFormat = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_UInt32 certVersion;
        PKIX_PL_BigInt *certSN = NULL;
        PKIX_PL_String *certSNString = NULL;
        PKIX_PL_X500Name *certIssuer = NULL;
        PKIX_PL_String *certIssuerString = NULL;
        PKIX_PL_X500Name *certSubject = NULL;
        PKIX_PL_String *certSubjectString = NULL;
        PKIX_PL_String *notBeforeString = NULL;
        PKIX_PL_String *notAfterString = NULL;
        PKIX_List *subjAltNames = NULL;
        PKIX_PL_String *subjAltNamesString = NULL;
        PKIX_PL_ByteArray *authKeyId = NULL;
        PKIX_PL_String *authKeyIdString = NULL;
        PKIX_PL_ByteArray *subjKeyId = NULL;
        PKIX_PL_String *subjKeyIdString = NULL;
        PKIX_PL_PublicKey *nssPubKey = NULL;
        PKIX_PL_String *nssPubKeyString = NULL;
        PKIX_List *critExtOIDs = NULL;
        PKIX_PL_String *critExtOIDsString = NULL;
        PKIX_List *extKeyUsages = NULL;
        PKIX_PL_String *extKeyUsagesString = NULL;
        PKIX_PL_CertBasicConstraints *basicConstraint = NULL;
        PKIX_PL_String *certBasicConstraintsString = NULL;
        PKIX_List *policyInfo = NULL;
        PKIX_PL_String *certPolicyInfoString = NULL;
        PKIX_List *certPolicyMappings = NULL;
        PKIX_PL_String *certPolicyMappingsString = NULL;
        PKIX_Int32 certExplicitPolicy = 0;
        PKIX_Int32 certInhibitMapping = 0;
        PKIX_Int32 certInhibitAnyPolicy = 0;
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;
        PKIX_PL_String *nameConstraintsString = NULL;
        PKIX_List *authorityInfoAccess = NULL;
        PKIX_PL_String *authorityInfoAccessString = NULL;
        PKIX_List *subjectInfoAccess = NULL;
        PKIX_PL_String *subjectInfoAccessString = NULL;

        PKIX_ENTER(CERT, "pkix_pl_Cert_ToString_Helper");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pString);

        /*
         * XXX Add to this format as certificate components are developed.
         */

        if (partialString){
                asciiFormat =
                        "\t[Issuer:          %s\n"
                        "\t Subject:         %s]";
        } else {
                asciiFormat =
                        "[\n"
                        "\tVersion:         v%d\n"
                        "\tSerialNumber:    %s\n"
                        "\tIssuer:          %s\n"
                        "\tSubject:         %s\n"
                        "\tValidity: [From: %s\n"
                        "\t           To:   %s]\n"
                        "\tSubjectAltNames: %s\n"
                        "\tAuthorityKeyId:  %s\n"
                        "\tSubjectKeyId:    %s\n"
                        "\tSubjPubKeyAlgId: %s\n"
                        "\tCritExtOIDs:     %s\n"
                        "\tExtKeyUsages:    %s\n"
                        "\tBasicConstraint: %s\n"
                        "\tCertPolicyInfo:  %s\n"
                        "\tPolicyMappings:  %s\n"
                        "\tExplicitPolicy:  %d\n"
                        "\tInhibitMapping:  %d\n"
                        "\tInhibitAnyPolicy:%d\n"
                        "\tNameConstraints: %s\n"
                        "\tAuthorityInfoAccess: %s\n"
                        "\tSubjectInfoAccess: %s\n"
                        "\tCacheFlag:       %d\n"
                        "]\n";
        }



        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, asciiFormat, 0, &formatString, plContext),
                PKIX_STRINGCREATEFAILED);

        /* Issuer */
        PKIX_CHECK(PKIX_PL_Cert_GetIssuer
                (cert, &certIssuer, plContext),
                PKIX_CERTGETISSUERFAILED);

        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object *)certIssuer, &certIssuerString, plContext),
                PKIX_X500NAMETOSTRINGFAILED);

        /* Subject */
        PKIX_CHECK(PKIX_PL_Cert_GetSubject(cert, &certSubject, plContext),
                PKIX_CERTGETSUBJECTFAILED);

        PKIX_TOSTRING(certSubject, &certSubjectString, plContext,
                PKIX_X500NAMETOSTRINGFAILED);

        if (partialString){
                PKIX_CHECK(PKIX_PL_Sprintf
                            (&certString,
                            plContext,
                            formatString,
                            certIssuerString,
                            certSubjectString),
                            PKIX_SPRINTFFAILED);

                *pString = certString;
                goto cleanup;
        }

        /* Version */
        PKIX_CHECK(PKIX_PL_Cert_GetVersion(cert, &certVersion, plContext),
                PKIX_CERTGETVERSIONFAILED);

        /* SerialNumber */
        PKIX_CHECK(PKIX_PL_Cert_GetSerialNumber(cert, &certSN, plContext),
                PKIX_CERTGETSERIALNUMBERFAILED);

        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object *)certSN, &certSNString, plContext),
                PKIX_BIGINTTOSTRINGFAILED);

        /* Validity: NotBefore */
        PKIX_CHECK(pkix_pl_Date_ToString_Helper
                (&(cert->nssCert->validity.notBefore),
                &notBeforeString,
                plContext),
                PKIX_DATETOSTRINGHELPERFAILED);

        /* Validity: NotAfter */
        PKIX_CHECK(pkix_pl_Date_ToString_Helper
                (&(cert->nssCert->validity.notAfter),
                &notAfterString,
                plContext),
                PKIX_DATETOSTRINGHELPERFAILED);

        /* SubjectAltNames */
        PKIX_CHECK(PKIX_PL_Cert_GetSubjectAltNames
                (cert, &subjAltNames, plContext),
                PKIX_CERTGETSUBJECTALTNAMESFAILED);

        PKIX_TOSTRING(subjAltNames, &subjAltNamesString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        /* AuthorityKeyIdentifier */
        PKIX_CHECK(PKIX_PL_Cert_GetAuthorityKeyIdentifier
                (cert, &authKeyId, plContext),
                PKIX_CERTGETAUTHORITYKEYIDENTIFIERFAILED);

        PKIX_TOSTRING(authKeyId, &authKeyIdString, plContext,
                PKIX_BYTEARRAYTOSTRINGFAILED);

        /* SubjectKeyIdentifier */
        PKIX_CHECK(PKIX_PL_Cert_GetSubjectKeyIdentifier
                (cert, &subjKeyId, plContext),
                PKIX_CERTGETSUBJECTKEYIDENTIFIERFAILED);

        PKIX_TOSTRING(subjKeyId, &subjKeyIdString, plContext,
                PKIX_BYTEARRAYTOSTRINGFAILED);

        /* SubjectPublicKey */
        PKIX_CHECK(PKIX_PL_Cert_GetSubjectPublicKey
                    (cert, &nssPubKey, plContext),
                    PKIX_CERTGETSUBJECTPUBLICKEYFAILED);

        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object *)nssPubKey, &nssPubKeyString, plContext),
                PKIX_PUBLICKEYTOSTRINGFAILED);

        /* CriticalExtensionOIDs */
        PKIX_CHECK(PKIX_PL_Cert_GetCriticalExtensionOIDs
                (cert, &critExtOIDs, plContext),
                PKIX_CERTGETCRITICALEXTENSIONOIDSFAILED);

        PKIX_TOSTRING(critExtOIDs, &critExtOIDsString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        /* ExtendedKeyUsages */
        PKIX_CHECK(PKIX_PL_Cert_GetExtendedKeyUsage
                (cert, &extKeyUsages, plContext),
                PKIX_CERTGETEXTENDEDKEYUSAGEFAILED);

        PKIX_TOSTRING(extKeyUsages, &extKeyUsagesString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        /* CertBasicConstraints */
        PKIX_CHECK(PKIX_PL_Cert_GetBasicConstraints
                (cert, &basicConstraint, plContext),
                PKIX_CERTGETBASICCONSTRAINTSFAILED);

        PKIX_TOSTRING(basicConstraint, &certBasicConstraintsString, plContext,
                PKIX_CERTBASICCONSTRAINTSTOSTRINGFAILED);

        /* CertPolicyInfo */
        PKIX_CHECK(PKIX_PL_Cert_GetPolicyInformation
                (cert, &policyInfo, plContext),
                PKIX_CERTGETPOLICYINFORMATIONFAILED);

        PKIX_TOSTRING(policyInfo, &certPolicyInfoString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        /* Advanced Policies */
        PKIX_CHECK(PKIX_PL_Cert_GetPolicyMappings
                (cert, &certPolicyMappings, plContext),
                PKIX_CERTGETPOLICYMAPPINGSFAILED);

        PKIX_TOSTRING(certPolicyMappings, &certPolicyMappingsString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetRequireExplicitPolicy
                (cert, &certExplicitPolicy, plContext),
                PKIX_CERTGETREQUIREEXPLICITPOLICYFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetPolicyMappingInhibited
                (cert, &certInhibitMapping, plContext),
                PKIX_CERTGETPOLICYMAPPINGINHIBITEDFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetInhibitAnyPolicy
                (cert, &certInhibitAnyPolicy, plContext),
                PKIX_CERTGETINHIBITANYPOLICYFAILED);

        /* Name Constraints */
        PKIX_CHECK(PKIX_PL_Cert_GetNameConstraints
                (cert, &nameConstraints, plContext),
                PKIX_CERTGETNAMECONSTRAINTSFAILED);

        PKIX_TOSTRING(nameConstraints, &nameConstraintsString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        /* Authority Information Access */
        PKIX_CHECK(PKIX_PL_Cert_GetAuthorityInfoAccess
                (cert, &authorityInfoAccess, plContext),
                PKIX_CERTGETAUTHORITYINFOACCESSFAILED);

        PKIX_TOSTRING(authorityInfoAccess, &authorityInfoAccessString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        /* Subject Information Access */
        PKIX_CHECK(PKIX_PL_Cert_GetSubjectInfoAccess
                (cert, &subjectInfoAccess, plContext),
                PKIX_CERTGETSUBJECTINFOACCESSFAILED);

        PKIX_TOSTRING(subjectInfoAccess, &subjectInfoAccessString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&certString,
                    plContext,
                    formatString,
                    certVersion + 1,
                    certSNString,
                    certIssuerString,
                    certSubjectString,
                    notBeforeString,
                    notAfterString,
                    subjAltNamesString,
                    authKeyIdString,
                    subjKeyIdString,
                    nssPubKeyString,
                    critExtOIDsString,
                    extKeyUsagesString,
                    certBasicConstraintsString,
                    certPolicyInfoString,
                    certPolicyMappingsString,
                    certExplicitPolicy,         /* an Int32, not a String */
                    certInhibitMapping,         /* an Int32, not a String */
                    certInhibitAnyPolicy,       /* an Int32, not a String */
                    nameConstraintsString,
                    authorityInfoAccessString,
                    subjectInfoAccessString,
                    cert->cacheFlag),           /* a boolean */
                    PKIX_SPRINTFFAILED);

        *pString = certString;

cleanup:
        PKIX_DECREF(certSN);
        PKIX_DECREF(certSNString);
        PKIX_DECREF(certIssuer);
        PKIX_DECREF(certIssuerString);
        PKIX_DECREF(certSubject);
        PKIX_DECREF(certSubjectString);
        PKIX_DECREF(notBeforeString);
        PKIX_DECREF(notAfterString);
        PKIX_DECREF(subjAltNames);
        PKIX_DECREF(subjAltNamesString);
        PKIX_DECREF(authKeyId);
        PKIX_DECREF(authKeyIdString);
        PKIX_DECREF(subjKeyId);
        PKIX_DECREF(subjKeyIdString);
        PKIX_DECREF(nssPubKey);
        PKIX_DECREF(nssPubKeyString);
        PKIX_DECREF(critExtOIDs);
        PKIX_DECREF(critExtOIDsString);
        PKIX_DECREF(extKeyUsages);
        PKIX_DECREF(extKeyUsagesString);
        PKIX_DECREF(basicConstraint);
        PKIX_DECREF(certBasicConstraintsString);
        PKIX_DECREF(policyInfo);
        PKIX_DECREF(certPolicyInfoString);
        PKIX_DECREF(certPolicyMappings);
        PKIX_DECREF(certPolicyMappingsString);
        PKIX_DECREF(nameConstraints);
        PKIX_DECREF(nameConstraintsString);
        PKIX_DECREF(authorityInfoAccess);
        PKIX_DECREF(authorityInfoAccessString);
        PKIX_DECREF(subjectInfoAccess);
        PKIX_DECREF(subjectInfoAccessString);
        PKIX_DECREF(formatString);

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Cert_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_Cert *cert = NULL;

        PKIX_ENTER(CERT, "pkix_pl_Cert_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CERT_TYPE, plContext),
                    PKIX_OBJECTNOTCERT);

        cert = (PKIX_PL_Cert*)object;

        PKIX_DECREF(cert->subject);
        PKIX_DECREF(cert->issuer);
        PKIX_DECREF(cert->subjAltNames);
        PKIX_DECREF(cert->publicKeyAlgId);
        PKIX_DECREF(cert->publicKey);
        PKIX_DECREF(cert->serialNumber);
        PKIX_DECREF(cert->critExtOids);
        PKIX_DECREF(cert->authKeyId);
        PKIX_DECREF(cert->subjKeyId);
        PKIX_DECREF(cert->extKeyUsages);
        PKIX_DECREF(cert->certBasicConstraints);
        PKIX_DECREF(cert->certPolicyInfos);
        PKIX_DECREF(cert->certPolicyMappings);
        PKIX_DECREF(cert->nameConstraints);
        PKIX_DECREF(cert->store);
        PKIX_DECREF(cert->authorityInfoAccess);
        PKIX_DECREF(cert->subjectInfoAccess);
        PKIX_DECREF(cert->crldpList);

        if (cert->arenaNameConstraints){
                /* This arena was allocated for SubjectAltNames */
                PKIX_PL_NSSCALL(CERT, PORT_FreeArena,
                        (cert->arenaNameConstraints, PR_FALSE));

                cert->arenaNameConstraints = NULL;
                cert->nssSubjAltNames = NULL;
        }

        CERT_DestroyCertificate(cert->nssCert);
        cert->nssCert = NULL;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Cert_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *certString = NULL;
        PKIX_PL_Cert *pkixCert = NULL;

        PKIX_ENTER(CERT, "pkix_pl_Cert_toString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CERT_TYPE, plContext),
                    PKIX_OBJECTNOTCERT);

        pkixCert = (PKIX_PL_Cert *)object;

        PKIX_CHECK(pkix_pl_Cert_ToString_Helper
                    (pkixCert, PKIX_FALSE, &certString, plContext),
                    PKIX_CERTTOSTRINGHELPERFAILED);

        *pString = certString;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Cert_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_Cert *pkixCert = NULL;
        CERTCertificate *nssCert = NULL;
        unsigned char *derBytes = NULL;
        PKIX_UInt32 derLength;
        PKIX_UInt32 certHash;

        PKIX_ENTER(CERT, "pkix_pl_Cert_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CERT_TYPE, plContext),
                    PKIX_OBJECTNOTCERT);

        pkixCert = (PKIX_PL_Cert *)object;

        nssCert = pkixCert->nssCert;
        derBytes = (nssCert->derCert).data;
        derLength = (nssCert->derCert).len;

        PKIX_CHECK(pkix_hash(derBytes, derLength, &certHash, plContext),
                    PKIX_HASHFAILED);

        *pHashcode = certHash;

cleanup:
        PKIX_RETURN(CERT);
}


/*
 * FUNCTION: pkix_pl_Cert_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Cert_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        CERTCertificate *firstCert = NULL;
        CERTCertificate *secondCert = NULL;
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult;

        PKIX_ENTER(CERT, "pkix_pl_Cert_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a Cert */
        PKIX_CHECK(pkix_CheckType(firstObject, PKIX_CERT_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTCERT);

        /*
         * Since we know firstObject is a Cert, if both references are
         * identical, they must be equal
         */
        if (firstObject == secondObject){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't a Cert, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObject, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_CERT_TYPE) goto cleanup;

        firstCert = ((PKIX_PL_Cert *)firstObject)->nssCert;
        secondCert = ((PKIX_PL_Cert *)secondObject)->nssCert;

        PKIX_NULLCHECK_TWO(firstCert, secondCert);

        /* CERT_CompareCerts does byte comparison on DER encodings of certs */
        PKIX_CERT_DEBUG("\t\tCalling CERT_CompareCerts).\n");
        cmpResult = CERT_CompareCerts(firstCert, secondCert);

        *pResult = cmpResult;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_CERT_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_Cert_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CERT, "pkix_pl_Cert_RegisterSelf");

        entry.description = "Cert";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_Cert);
        entry.destructor = pkix_pl_Cert_Destroy;
        entry.equalsFunction = pkix_pl_Cert_Equals;
        entry.hashcodeFunction = pkix_pl_Cert_Hashcode;
        entry.toStringFunction = pkix_pl_Cert_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_CERT_TYPE] = entry;

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_CreateWithNSSCert
 * DESCRIPTION:
 *
 *  Creates a new certificate using the CERTCertificate pointed to by "nssCert"
 *  and stores it at "pCert". Once created, a Cert is immutable.
 *
 *  This function is primarily used as a convenience function for the
 *  performance tests that have easy access to a CERTCertificate.
 *
 * PARAMETERS:
 *  "nssCert"
 *      Address of CERTCertificate representing the NSS certificate.
 *      Must be non-NULL.
 *  "pCert"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Cert_CreateWithNSSCert(
        CERTCertificate *nssCert,
        PKIX_PL_Cert **pCert,
        void *plContext)
{
        PKIX_PL_Cert *cert = NULL;

        PKIX_ENTER(CERT, "pkix_pl_Cert_CreateWithNSSCert");
        PKIX_NULLCHECK_TWO(pCert, nssCert);

        /* create a PKIX_PL_Cert object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CERT_TYPE,
                    sizeof (PKIX_PL_Cert),
                    (PKIX_PL_Object **)&cert,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        /* populate the nssCert field */
        cert->nssCert = nssCert;

        /* initialize remaining fields */
        /*
         * Fields ending with Absent are initialized to PKIX_FALSE so that the
         * first time we need the value we will look for it. If we find it is
         * actually absent, the flag will at that time be set to PKIX_TRUE to
         * prevent searching for it later.
         * Fields ending with Processed are those where a value is defined
         * for the Absent case, and a value of zero is possible. When the
         * flag is still true we have to look for the field, set the default
         * value if necessary, and set the Processed flag to PKIX_TRUE.
         */
        cert->subject = NULL;
        cert->issuer = NULL;
        cert->subjAltNames = NULL;
        cert->subjAltNamesAbsent = PKIX_FALSE;
        cert->publicKeyAlgId = NULL;
        cert->publicKey = NULL;
        cert->serialNumber = NULL;
        cert->critExtOids = NULL;
        cert->subjKeyId = NULL;
        cert->subjKeyIdAbsent = PKIX_FALSE;
        cert->authKeyId = NULL;
        cert->authKeyIdAbsent = PKIX_FALSE;
        cert->extKeyUsages = NULL;
        cert->extKeyUsagesAbsent = PKIX_FALSE;
        cert->certBasicConstraints = NULL;
        cert->basicConstraintsAbsent = PKIX_FALSE;
        cert->certPolicyInfos = NULL;
        cert->policyInfoAbsent = PKIX_FALSE;
        cert->policyMappingsAbsent = PKIX_FALSE;
        cert->certPolicyMappings = NULL;
        cert->policyConstraintsProcessed = PKIX_FALSE;
        cert->policyConstraintsExplicitPolicySkipCerts = 0;
        cert->policyConstraintsInhibitMappingSkipCerts = 0;
        cert->inhibitAnyPolicyProcessed = PKIX_FALSE;
        cert->inhibitAnySkipCerts = 0;
        cert->nameConstraints = NULL;
        cert->nameConstraintsAbsent = PKIX_FALSE;
        cert->arenaNameConstraints = NULL;
        cert->nssSubjAltNames = NULL;
        cert->cacheFlag = PKIX_FALSE;
        cert->store = NULL;
        cert->authorityInfoAccess = NULL;
        cert->subjectInfoAccess = NULL;
        cert->isUserTrustAnchor = PKIX_FALSE;
        cert->crldpList = NULL;

        *pCert = cert;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_pl_Cert_CreateToList
 * DESCRIPTION:
 *
 *  Creates a new certificate using the DER-encoding pointed to by "derCertItem"
 *  and appends it to the list pointed to by "certList". If Cert creation fails,
 *  the function returns with certList unchanged, but any decoding Error is
 *  discarded.
 *
 * PARAMETERS:
 *  "derCertItem"
 *      Address of SECItem containing the DER representation of a certificate.
 *      Must be non-NULL.
 *  "certList"
 *      Address of List to which the Cert will be appended, if successfully
 *      created. May be empty, but must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Cert_CreateToList(
        SECItem *derCertItem,
        PKIX_List *certList,
        void *plContext)
{
        CERTCertificate *nssCert = NULL;
        PKIX_PL_Cert *cert = NULL;
        CERTCertDBHandle *handle;

        PKIX_ENTER(CERT, "pkix_pl_Cert_CreateToList");
        PKIX_NULLCHECK_TWO(derCertItem, certList);

        handle  = CERT_GetDefaultCertDB();
        nssCert = CERT_NewTempCertificate(handle, derCertItem,
					  /* nickname */ NULL, 
					  /* isPerm   */ PR_FALSE, 
					  /* copyDer  */ PR_TRUE);
        if (!nssCert) {
            goto cleanup;
        }

        PKIX_CHECK(pkix_pl_Cert_CreateWithNSSCert
                   (nssCert, &cert, plContext),
                   PKIX_CERTCREATEWITHNSSCERTFAILED);

        nssCert = NULL;

        PKIX_CHECK(PKIX_List_AppendItem
                   (certList, (PKIX_PL_Object *) cert, plContext),
                   PKIX_LISTAPPENDITEMFAILED);

cleanup:
        if (nssCert) {
            CERT_DestroyCertificate(nssCert);
        }

        PKIX_DECREF(cert);
        PKIX_RETURN(CERT);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_Cert_Create (see comments in pkix_pl_pki.h)
 * XXX We may want to cache the cert after parsing it, so it can be reused
 * XXX Are the NSS/NSPR functions thread safe
 */
PKIX_Error *
PKIX_PL_Cert_Create(
        PKIX_PL_ByteArray *byteArray,
        PKIX_PL_Cert **pCert,
        void *plContext)
{
        CERTCertificate *nssCert = NULL;
        SECItem *derCertItem = NULL;
        void *derBytes = NULL;
        PKIX_UInt32 derLength;
        PKIX_PL_Cert *cert = NULL;
        CERTCertDBHandle *handle;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_Create");
        PKIX_NULLCHECK_TWO(pCert, byteArray);

        PKIX_CHECK(PKIX_PL_ByteArray_GetPointer
                    (byteArray, &derBytes, plContext),
                    PKIX_BYTEARRAYGETPOINTERFAILED);

        PKIX_CHECK(PKIX_PL_ByteArray_GetLength
                    (byteArray, &derLength, plContext),
                    PKIX_BYTEARRAYGETLENGTHFAILED);

        derCertItem = SECITEM_AllocItem(NULL, NULL, derLength);
        if (derCertItem == NULL){
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        (void) PORT_Memcpy(derCertItem->data, derBytes, derLength);

        /*
         * setting copyDER to true forces NSS to make its own copy of the DER,
         * allowing us to free our copy without worrying about whether NSS
         * is still using it
         */
        handle  = CERT_GetDefaultCertDB();
        nssCert = CERT_NewTempCertificate(handle, derCertItem,
					  /* nickname */ NULL, 
					  /* isPerm   */ PR_FALSE, 
					  /* copyDer  */ PR_TRUE);
        if (!nssCert){
                PKIX_ERROR(PKIX_CERTDECODEDERCERTIFICATEFAILED);
        }

        PKIX_CHECK(pkix_pl_Cert_CreateWithNSSCert
                (nssCert, &cert, plContext),
                PKIX_CERTCREATEWITHNSSCERTFAILED);

        *pCert = cert;

cleanup:
        if (derCertItem){
                SECITEM_FreeItem(derCertItem, PKIX_TRUE);
        }

        if (nssCert && PKIX_ERROR_RECEIVED){
                PKIX_CERT_DEBUG("\t\tCalling CERT_DestroyCertificate).\n");
                CERT_DestroyCertificate(nssCert);
                nssCert = NULL;
        }

        PKIX_FREE(derBytes);
        PKIX_RETURN(CERT);
}


/*
 * FUNCTION: PKIX_PL_Cert_CreateFromCERTCertificate
 *  (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_CreateFromCERTCertificate(
        const CERTCertificate *nssCert,
        PKIX_PL_Cert **pCert,
        void *plContext)
{
        void *buf = NULL;
        PKIX_UInt32 len;
        PKIX_PL_ByteArray *byteArray = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_CreateWithNssCert");
        PKIX_NULLCHECK_TWO(pCert, nssCert);

        buf = (void*)nssCert->derCert.data;
        len = nssCert->derCert.len;

        PKIX_CHECK(
            PKIX_PL_ByteArray_Create(buf, len, &byteArray, plContext),
            PKIX_BYTEARRAYCREATEFAILED);

        PKIX_CHECK(
            PKIX_PL_Cert_Create(byteArray, pCert, plContext),
            PKIX_CERTCREATEWITHNSSCERTFAILED);

#ifdef PKIX_UNDEF
        /* will be tested and used as a patch for bug 391612 */
        nssCert = CERT_DupCertificate(nssInCert);

        PKIX_CHECK(pkix_pl_Cert_CreateWithNSSCert
                (nssCert, &cert, plContext),
                PKIX_CERTCREATEWITHNSSCERTFAILED);
#endif /* PKIX_UNDEF */

cleanup:

#ifdef PKIX_UNDEF
        if (nssCert && PKIX_ERROR_RECEIVED){
                PKIX_CERT_DEBUG("\t\tCalling CERT_DestroyCertificate).\n");
                CERT_DestroyCertificate(nssCert);
                nssCert = NULL;
        }
#endif /* PKIX_UNDEF */

        PKIX_DECREF(byteArray);
        PKIX_RETURN(CERT);
}


/*
 * FUNCTION: PKIX_PL_Cert_GetVersion (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetVersion(
        PKIX_PL_Cert *cert,
        PKIX_UInt32 *pVersion,
        void *plContext)
{
        CERTCertificate *nssCert = NULL;
        PKIX_UInt32 myVersion = 0;  /* v1 */

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetVersion");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pVersion);

        nssCert = cert->nssCert;
        if (nssCert->version.len != 0) {
                myVersion = *(nssCert->version.data);
        }

        if (myVersion > 2){
                PKIX_ERROR(PKIX_VERSIONVALUEMUSTBEV1V2ORV3);
        }

        *pVersion = myVersion;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetSerialNumber (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetSerialNumber(
        PKIX_PL_Cert *cert,
        PKIX_PL_BigInt **pSerialNumber,
        void *plContext)
{
        CERTCertificate *nssCert = NULL;
        SECItem serialNumItem;
        PKIX_PL_BigInt *serialNumber = NULL;
        char *bytes = NULL;
        PKIX_UInt32 length;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetSerialNumber");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pSerialNumber);

        if (cert->serialNumber == NULL){

                PKIX_OBJECT_LOCK(cert);

                if (cert->serialNumber == NULL){

                        nssCert = cert->nssCert;
                        serialNumItem = nssCert->serialNumber;

                        length = serialNumItem.len;
                        bytes = (char *)serialNumItem.data;

                        PKIX_CHECK(pkix_pl_BigInt_CreateWithBytes
                                    (bytes, length, &serialNumber, plContext),
                                    PKIX_BIGINTCREATEWITHBYTESFAILED);

                        /* save a cached copy in case it is asked for again */
                        cert->serialNumber = serialNumber;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->serialNumber);
        *pSerialNumber = cert->serialNumber;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetSubject (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetSubject(
        PKIX_PL_Cert *cert,
        PKIX_PL_X500Name **pCertSubject,
        void *plContext)
{
        PKIX_PL_X500Name *pkixSubject = NULL;
        CERTName *subjName = NULL;
        SECItem  *derSubjName = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetSubject");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pCertSubject);

        /* if we don't have a cached copy from before, we create one */
        if (cert->subject == NULL){

                PKIX_OBJECT_LOCK(cert);

                if (cert->subject == NULL){

                        subjName = &cert->nssCert->subject;
                        derSubjName = &cert->nssCert->derSubject;

                        /* if there is no subject name */
                        if (derSubjName->data == NULL) {

                                pkixSubject = NULL;

                        } else {
                                PKIX_CHECK(PKIX_PL_X500Name_CreateFromCERTName
                                    (derSubjName, subjName, &pkixSubject,
                                     plContext),
                                    PKIX_X500NAMECREATEFROMCERTNAMEFAILED);

                        }
                        /* save a cached copy in case it is asked for again */
                        cert->subject = pkixSubject;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->subject);
        *pCertSubject = cert->subject;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetIssuer (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetIssuer(
        PKIX_PL_Cert *cert,
        PKIX_PL_X500Name **pCertIssuer,
        void *plContext)
{
        PKIX_PL_X500Name *pkixIssuer = NULL;
        SECItem  *derIssuerName = NULL;
        CERTName *issuerName = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetIssuer");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pCertIssuer);

        /* if we don't have a cached copy from before, we create one */
        if (cert->issuer == NULL){

                PKIX_OBJECT_LOCK(cert);

                if (cert->issuer == NULL){

                        issuerName = &cert->nssCert->issuer;
                        derIssuerName = &cert->nssCert->derIssuer;

                        /* if there is no subject name */
                        PKIX_CHECK(PKIX_PL_X500Name_CreateFromCERTName
                                    (derIssuerName, issuerName,
                                     &pkixIssuer, plContext),
                                    PKIX_X500NAMECREATEFROMCERTNAMEFAILED);

                        /* save a cached copy in case it is asked for again */
                        cert->issuer = pkixIssuer;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->issuer);
        *pCertIssuer = cert->issuer;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetSubjectAltNames (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetSubjectAltNames(
        PKIX_PL_Cert *cert,
        PKIX_List **pSubjectAltNames,  /* list of PKIX_PL_GeneralName */
        void *plContext)
{
        PKIX_PL_GeneralName *pkixAltName = NULL;
        PKIX_List *altNamesList = NULL;

        CERTGeneralName *nssOriginalAltName = NULL;
        CERTGeneralName *nssTempAltName = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetSubjectAltNames");
        PKIX_NULLCHECK_TWO(cert, pSubjectAltNames);

        /* if we don't have a cached copy from before, we create one */
        if ((cert->subjAltNames == NULL) && (!cert->subjAltNamesAbsent)){

                PKIX_OBJECT_LOCK(cert);

                if ((cert->subjAltNames == NULL) &&
                    (!cert->subjAltNamesAbsent)){

                        PKIX_CHECK(pkix_pl_Cert_GetNssSubjectAltNames
                                (cert,
                                PKIX_TRUE,
                                &nssOriginalAltName,
                                plContext),
                                PKIX_CERTGETNSSSUBJECTALTNAMESFAILED);

                        if (nssOriginalAltName == NULL) {
                                cert->subjAltNamesAbsent = PKIX_TRUE;
                                pSubjectAltNames = NULL;
                                goto cleanup;
                        }

                        nssTempAltName = nssOriginalAltName;

                        PKIX_CHECK(PKIX_List_Create(&altNamesList, plContext),
                                PKIX_LISTCREATEFAILED);

                        do {
                            PKIX_CHECK(pkix_pl_GeneralName_Create
                                (nssTempAltName, &pkixAltName, plContext),
                                PKIX_GENERALNAMECREATEFAILED);

                            PKIX_CHECK(PKIX_List_AppendItem
                                (altNamesList,
                                (PKIX_PL_Object *)pkixAltName,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);

                            PKIX_DECREF(pkixAltName);

                            PKIX_CERT_DEBUG
                                ("\t\tCalling CERT_GetNextGeneralName).\n");
                            nssTempAltName = CERT_GetNextGeneralName
                                (nssTempAltName);

                        } while (nssTempAltName != nssOriginalAltName);

                        /* save a cached copy in case it is asked for again */
                        cert->subjAltNames = altNamesList;
                        PKIX_CHECK(PKIX_List_SetImmutable
                                (cert->subjAltNames, plContext),
                                PKIX_LISTSETIMMUTABLEFAILED);

                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->subjAltNames);

        *pSubjectAltNames = cert->subjAltNames;

cleanup:
        PKIX_DECREF(pkixAltName);
        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(altNamesList);
        }
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetAllSubjectNames (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetAllSubjectNames(
        PKIX_PL_Cert *cert,
        PKIX_List **pAllSubjectNames,  /* list of PKIX_PL_GeneralName */
        void *plContext)
{
        CERTGeneralName *nssOriginalSubjectName = NULL;
        CERTGeneralName *nssTempSubjectName = NULL;
        PKIX_List *allSubjectNames = NULL;
        PKIX_PL_GeneralName *pkixSubjectName = NULL;
        PLArenaPool *arena = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetAllSubjectNames");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pAllSubjectNames);


        if (cert->nssCert->subjectName == NULL){
                /* if there is no subject DN, just get altnames */

                PKIX_CHECK(pkix_pl_Cert_GetNssSubjectAltNames
                            (cert,
                            PKIX_FALSE, /* hasLock */
                            &nssOriginalSubjectName,
                            plContext),
                            PKIX_CERTGETNSSSUBJECTALTNAMESFAILED);

        } else { /* get subject DN and altnames */

                arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                if (arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }

                /* This NSS call returns both Subject and  Subject Alt Names */
                PKIX_CERT_DEBUG("\t\tCalling CERT_GetCertificateNames\n");
                nssOriginalSubjectName =
                        CERT_GetCertificateNames(cert->nssCert, arena);
        }

        if (nssOriginalSubjectName == NULL) {
                pAllSubjectNames = NULL;
                goto cleanup;
        }

        nssTempSubjectName = nssOriginalSubjectName;

        PKIX_CHECK(PKIX_List_Create(&allSubjectNames, plContext),
                    PKIX_LISTCREATEFAILED);

        do {
                PKIX_CHECK(pkix_pl_GeneralName_Create
                            (nssTempSubjectName, &pkixSubjectName, plContext),
                            PKIX_GENERALNAMECREATEFAILED);

                PKIX_CHECK(PKIX_List_AppendItem
                            (allSubjectNames,
                            (PKIX_PL_Object *)pkixSubjectName,
                            plContext),
                            PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(pkixSubjectName);

                PKIX_CERT_DEBUG
                        ("\t\tCalling CERT_GetNextGeneralName).\n");
                nssTempSubjectName = CERT_GetNextGeneralName
                        (nssTempSubjectName);
        } while (nssTempSubjectName != nssOriginalSubjectName);

        *pAllSubjectNames = allSubjectNames;

cleanup:
        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(allSubjectNames);
        }

        if (arena){
                PORT_FreeArena(arena, PR_FALSE);
        }
        PKIX_DECREF(pkixSubjectName);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetSubjectPublicKeyAlgId
 *      (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetSubjectPublicKeyAlgId(
        PKIX_PL_Cert *cert,
        PKIX_PL_OID **pSubjKeyAlgId,
        void *plContext)
{
        PKIX_PL_OID *pubKeyAlgId = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetSubjectPublicKeyAlgId");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pSubjKeyAlgId);

        /* if we don't have a cached copy from before, we create one */
        if (cert->publicKeyAlgId == NULL){
                PKIX_OBJECT_LOCK(cert);
                if (cert->publicKeyAlgId == NULL){
                        CERTCertificate *nssCert = cert->nssCert;
                        SECAlgorithmID *algorithm;
                        SECItem *algBytes;

                        algorithm = &nssCert->subjectPublicKeyInfo.algorithm;
                        algBytes = &algorithm->algorithm;
                        if (!algBytes->data || !algBytes->len) {
                            PKIX_ERROR_FATAL(PKIX_ALGORITHMBYTESLENGTH0);
                        }
                        PKIX_CHECK(PKIX_PL_OID_CreateBySECItem
                                    (algBytes, &pubKeyAlgId, plContext),
                                    PKIX_OIDCREATEFAILED);

                        /* save a cached copy in case it is asked for again */
                        cert->publicKeyAlgId = pubKeyAlgId;
                        pubKeyAlgId = NULL;
                }
                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->publicKeyAlgId);
        *pSubjKeyAlgId = cert->publicKeyAlgId;

cleanup:
        PKIX_DECREF(pubKeyAlgId);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetSubjectPublicKey (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetSubjectPublicKey(
        PKIX_PL_Cert *cert,
        PKIX_PL_PublicKey **pPublicKey,
        void *plContext)
{
        PKIX_PL_PublicKey *pkixPubKey = NULL;
        SECStatus rv;

        CERTSubjectPublicKeyInfo *from = NULL;
        CERTSubjectPublicKeyInfo *to = NULL;
        SECItem *fromItem = NULL;
        SECItem *toItem = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetSubjectPublicKey");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pPublicKey);

        /* if we don't have a cached copy from before, we create one */
        if (cert->publicKey == NULL){

                PKIX_OBJECT_LOCK(cert);

                if (cert->publicKey == NULL){

                        /* create a PKIX_PL_PublicKey object */
                        PKIX_CHECK(PKIX_PL_Object_Alloc
                                    (PKIX_PUBLICKEY_TYPE,
                                    sizeof (PKIX_PL_PublicKey),
                                    (PKIX_PL_Object **)&pkixPubKey,
                                    plContext),
                                    PKIX_COULDNOTCREATEOBJECT);

                        /* initialize fields */
                        pkixPubKey->nssSPKI = NULL;

                        /* populate the SPKI field */
                        PKIX_CHECK(PKIX_PL_Malloc
                                    (sizeof (CERTSubjectPublicKeyInfo),
                                    (void **)&pkixPubKey->nssSPKI,
                                    plContext),
                                    PKIX_MALLOCFAILED);

                        to = pkixPubKey->nssSPKI;
                        from  = &cert->nssCert->subjectPublicKeyInfo;

                        PKIX_NULLCHECK_TWO(to, from);

                        PKIX_CERT_DEBUG
                                ("\t\tCalling SECOID_CopyAlgorithmID).\n");
                        rv = SECOID_CopyAlgorithmID
                                (NULL, &to->algorithm, &from->algorithm);
                        if (rv != SECSuccess) {
                                PKIX_ERROR(PKIX_SECOIDCOPYALGORITHMIDFAILED);
                        }

                        /*
                         * NSS stores the length of subjectPublicKey in bits.
                         * Therefore, we use that length converted to bytes
                         * using ((length+7)>>3) before calling PORT_Memcpy
                         * in order to avoid "read from uninitialized memory"
                         * errors.
                         */

                        toItem = &to->subjectPublicKey;
                        fromItem = &from->subjectPublicKey;

                        PKIX_NULLCHECK_TWO(toItem, fromItem);

                        toItem->type = fromItem->type;

                        toItem->data =
                                (unsigned char*) PORT_ZAlloc(fromItem->len);
                        if (!toItem->data){
                                PKIX_ERROR(PKIX_OUTOFMEMORY);
                        }

                        (void) PORT_Memcpy(toItem->data,
                                    fromItem->data,
                                    (fromItem->len + 7)>>3);
                        toItem->len = fromItem->len;

                        /* save a cached copy in case it is asked for again */
                        cert->publicKey = pkixPubKey;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->publicKey);
        *pPublicKey = cert->publicKey;

cleanup:

        if (PKIX_ERROR_RECEIVED && pkixPubKey){
                PKIX_DECREF(pkixPubKey);
                cert->publicKey = NULL;
        }
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetCriticalExtensionOIDs
 *      (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetCriticalExtensionOIDs(
        PKIX_PL_Cert *cert,
        PKIX_List **pList,  /* list of PKIX_PL_OID */
        void *plContext)
{
        PKIX_List *oidsList = NULL;
        CERTCertExtension **extensions = NULL;
        CERTCertificate *nssCert = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetCriticalExtensionOIDs");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pList);

        /* if we don't have a cached copy from before, we create one */
        if (cert->critExtOids == NULL) {

            PKIX_OBJECT_LOCK(cert);

            if (cert->critExtOids == NULL) {

                nssCert = cert->nssCert;

                /*
                 * ASN.1 for Extension
                 *
                 * Extension  ::=  SEQUENCE  {
                 *      extnID          OBJECT IDENTIFIER,
                 *      critical        BOOLEAN DEFAULT FALSE,
                 *      extnValue       OCTET STRING  }
                 *
                 */

                extensions = nssCert->extensions;

                PKIX_CHECK(pkix_pl_OID_GetCriticalExtensionOIDs
                            (extensions, &oidsList, plContext),
                            PKIX_GETCRITICALEXTENSIONOIDSFAILED);

                /* save a cached copy in case it is asked for again */
                cert->critExtOids = oidsList;
            }

            PKIX_OBJECT_UNLOCK(cert);
        }

        /* We should return a copy of the List since this list changes */
        PKIX_DUPLICATE(cert->critExtOids, pList, plContext,
                PKIX_OBJECTDUPLICATELISTFAILED);

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetAuthorityKeyIdentifier
 *      (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetAuthorityKeyIdentifier(
        PKIX_PL_Cert *cert,
        PKIX_PL_ByteArray **pAuthKeyId,
        void *plContext)
{
        PKIX_PL_ByteArray *authKeyId = NULL;
        CERTCertificate *nssCert = NULL;
        CERTAuthKeyID *authKeyIdExtension = NULL;
        PLArenaPool *arena = NULL;
        SECItem retItem;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetAuthorityKeyIdentifier");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pAuthKeyId);

        /* if we don't have a cached copy from before, we create one */
        if ((cert->authKeyId == NULL) && (!cert->authKeyIdAbsent)){

                PKIX_OBJECT_LOCK(cert);

                if ((cert->authKeyId == NULL) && (!cert->authKeyIdAbsent)){

                        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                        if (arena == NULL) {
                                PKIX_ERROR(PKIX_OUTOFMEMORY);
                        }

                        nssCert = cert->nssCert;

                        authKeyIdExtension =
                                CERT_FindAuthKeyIDExten(arena, nssCert);
                        if (authKeyIdExtension == NULL){
                                cert->authKeyIdAbsent = PKIX_TRUE;
                                *pAuthKeyId = NULL;
                                goto cleanup;
                        }

                        retItem = authKeyIdExtension->keyID;

                        if (retItem.len == 0){
                                cert->authKeyIdAbsent = PKIX_TRUE;
                                *pAuthKeyId = NULL;
                                goto cleanup;
                        }

                        PKIX_CHECK(PKIX_PL_ByteArray_Create
                                    (retItem.data,
                                    retItem.len,
                                    &authKeyId,
                                    plContext),
                                    PKIX_BYTEARRAYCREATEFAILED);

                        /* save a cached copy in case it is asked for again */
                        cert->authKeyId = authKeyId;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->authKeyId);
        *pAuthKeyId = cert->authKeyId;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        if (arena){
                PORT_FreeArena(arena, PR_FALSE);
        }
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetSubjectKeyIdentifier
 *      (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetSubjectKeyIdentifier(
        PKIX_PL_Cert *cert,
        PKIX_PL_ByteArray **pSubjKeyId,
        void *plContext)
{
        PKIX_PL_ByteArray *subjKeyId = NULL;
        CERTCertificate *nssCert = NULL;
        SECItem *retItem = NULL;
        SECStatus status;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetSubjectKeyIdentifier");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pSubjKeyId);

        /* if we don't have a cached copy from before, we create one */
        if ((cert->subjKeyId == NULL) && (!cert->subjKeyIdAbsent)){

                PKIX_OBJECT_LOCK(cert);

                if ((cert->subjKeyId == NULL) && (!cert->subjKeyIdAbsent)){

                        retItem = SECITEM_AllocItem(NULL, NULL, 0);
                        if (retItem == NULL){
                                PKIX_ERROR(PKIX_OUTOFMEMORY);
                        }

                        nssCert = cert->nssCert;

                        status = CERT_FindSubjectKeyIDExtension
                                (nssCert, retItem);
                        if (status != SECSuccess) {
                                cert->subjKeyIdAbsent = PKIX_TRUE;
                                *pSubjKeyId = NULL;
                                goto cleanup;
                        }

                        PKIX_CHECK(PKIX_PL_ByteArray_Create
                                    (retItem->data,
                                    retItem->len,
                                    &subjKeyId,
                                    plContext),
                                    PKIX_BYTEARRAYCREATEFAILED);

                        /* save a cached copy in case it is asked for again */
                        cert->subjKeyId = subjKeyId;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->subjKeyId);
        *pSubjKeyId = cert->subjKeyId;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        if (retItem){
                SECITEM_FreeItem(retItem, PKIX_TRUE);
        }
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetExtendedKeyUsage (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetExtendedKeyUsage(
        PKIX_PL_Cert *cert,
        PKIX_List **pKeyUsage,  /* list of PKIX_PL_OID */
        void *plContext)
{
        CERTOidSequence *extKeyUsage = NULL;
        CERTCertificate *nssCert = NULL;
        PKIX_PL_OID *pkixOID = NULL;
        PKIX_List *oidsList = NULL;
        SECItem **oids = NULL;
        SECItem encodedExtKeyUsage;
        SECStatus rv;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetExtendedKeyUsage");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pKeyUsage);

        /* if we don't have a cached copy from before, we create one */
        if ((cert->extKeyUsages == NULL) && (!cert->extKeyUsagesAbsent)){

                PKIX_OBJECT_LOCK(cert);

                if ((cert->extKeyUsages == NULL) &&
                    (!cert->extKeyUsagesAbsent)){

                        nssCert = cert->nssCert;

                        rv = CERT_FindCertExtension
                                (nssCert, SEC_OID_X509_EXT_KEY_USAGE,
                                &encodedExtKeyUsage);
                        if (rv != SECSuccess){
                                cert->extKeyUsagesAbsent = PKIX_TRUE;
                                *pKeyUsage = NULL;
                                goto cleanup;
                        }

                        extKeyUsage =
                                CERT_DecodeOidSequence(&encodedExtKeyUsage);
                        if (extKeyUsage == NULL){
                                PKIX_ERROR(PKIX_CERTDECODEOIDSEQUENCEFAILED);
                        }

                        PORT_Free(encodedExtKeyUsage.data);

                        oids = extKeyUsage->oids;

                        if (!oids){
                                /* no extended key usage extensions found */
                                cert->extKeyUsagesAbsent = PKIX_TRUE;
                                *pKeyUsage = NULL;
                                goto cleanup;
                        }

                        PKIX_CHECK(PKIX_List_Create(&oidsList, plContext),
                                    PKIX_LISTCREATEFAILED);

                        while (*oids){
                                SECItem *oid = *oids++;

                                PKIX_CHECK(PKIX_PL_OID_CreateBySECItem
                                            (oid, &pkixOID, plContext),
                                            PKIX_OIDCREATEFAILED);

                                PKIX_CHECK(PKIX_List_AppendItem
                                            (oidsList,
                                            (PKIX_PL_Object *)pkixOID,
                                            plContext),
                                            PKIX_LISTAPPENDITEMFAILED);
                                PKIX_DECREF(pkixOID);
                        }

                        PKIX_CHECK(PKIX_List_SetImmutable
                                    (oidsList, plContext),
                                    PKIX_LISTSETIMMUTABLEFAILED);

                        /* save a cached copy in case it is asked for again */
                        cert->extKeyUsages = oidsList;
                        oidsList = NULL;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->extKeyUsages);
        *pKeyUsage = cert->extKeyUsages;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);

        PKIX_DECREF(pkixOID);
        PKIX_DECREF(oidsList);
        CERT_DestroyOidSequence(extKeyUsage);

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetBasicConstraints
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetBasicConstraints(
        PKIX_PL_Cert *cert,
        PKIX_PL_CertBasicConstraints **pBasicConstraints,
        void *plContext)
{
        CERTCertificate *nssCert = NULL;
        CERTBasicConstraints nssBasicConstraint;
        SECStatus rv;
        PKIX_PL_CertBasicConstraints *basic;
        PKIX_Int32 pathLen = 0;
        PKIX_Boolean isCA = PKIX_FALSE;
        enum {
          realBC, synthBC, absentBC
        } constraintSource = absentBC;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetBasicConstraints");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pBasicConstraints);

        /* if we don't have a cached copy from before, we create one */
        if ((cert->certBasicConstraints == NULL) &&
                (!cert->basicConstraintsAbsent)) {

                PKIX_OBJECT_LOCK(cert);

                if ((cert->certBasicConstraints == NULL) &&
                    (!cert->basicConstraintsAbsent)) {

                        nssCert = cert->nssCert;

                        PKIX_CERT_DEBUG(
                            "\t\tCalling Cert_FindBasicConstraintExten\n");
                        rv = CERT_FindBasicConstraintExten
                                (nssCert, &nssBasicConstraint);
                        if (rv == SECSuccess) {
                            constraintSource = realBC;
                        }

                        if (constraintSource == absentBC) {
                            /* can we deduce it's a CA and create a 
                               synthetic constraint?
                            */
                            CERTCertTrust trust;
                            rv = CERT_GetCertTrust(nssCert, &trust);
                            if (rv == SECSuccess) {
                                int anyWantedFlag = CERTDB_TRUSTED_CA | CERTDB_VALID_CA;
                                if ((trust.sslFlags & anyWantedFlag) 
                                    || (trust.emailFlags & anyWantedFlag) 
                                    || (trust.objectSigningFlags & anyWantedFlag)) {

                                    constraintSource = synthBC;
                                }
                            }
                        }

                        if (constraintSource == absentBC) {
                            cert->basicConstraintsAbsent = PKIX_TRUE;
                            *pBasicConstraints = NULL;
                            goto cleanup;
                        }
                }

                if (constraintSource == synthBC) {
                    isCA = PKIX_TRUE;
                    pathLen = PKIX_UNLIMITED_PATH_CONSTRAINT;
                } else {
                    isCA = (nssBasicConstraint.isCA)?PKIX_TRUE:PKIX_FALSE;
    
                    /* The pathLen has meaning only for CAs */
                    if (isCA) {
                        if (CERT_UNLIMITED_PATH_CONSTRAINT ==
                            nssBasicConstraint.pathLenConstraint) {
                            pathLen = PKIX_UNLIMITED_PATH_CONSTRAINT;
                        } else {
                            pathLen = nssBasicConstraint.pathLenConstraint;
                        }
                    }
                }

                PKIX_CHECK(pkix_pl_CertBasicConstraints_Create
                            (isCA, pathLen, &basic, plContext),
                            PKIX_CERTBASICCONSTRAINTSCREATEFAILED);

                /* save a cached copy in case it is asked for again */
                cert->certBasicConstraints = basic;
        }

        PKIX_INCREF(cert->certBasicConstraints);
        *pBasicConstraints = cert->certBasicConstraints;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetPolicyInformation
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetPolicyInformation(
        PKIX_PL_Cert *cert,
        PKIX_List **pPolicyInfo,
        void *plContext)
{
        PKIX_List *policyList = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetPolicyInformation");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pPolicyInfo);

        /* if we don't have a cached copy from before, we create one */
        if ((cert->certPolicyInfos == NULL) &&
                (!cert->policyInfoAbsent)) {

                PKIX_OBJECT_LOCK(cert);

                if ((cert->certPolicyInfos == NULL) &&
                    (!cert->policyInfoAbsent)) {

                        PKIX_CHECK(pkix_pl_Cert_DecodePolicyInfo
                                (cert->nssCert, &policyList, plContext),
                                PKIX_CERTDECODEPOLICYINFOFAILED);

                        if (!policyList) {
                                cert->policyInfoAbsent = PKIX_TRUE;
                                *pPolicyInfo = NULL;
                                goto cleanup;
                        }
                }

                PKIX_OBJECT_UNLOCK(cert);

                /* save a cached copy in case it is asked for again */
                cert->certPolicyInfos = policyList;
                policyList = NULL;
        }

        PKIX_INCREF(cert->certPolicyInfos);
        *pPolicyInfo = cert->certPolicyInfos;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);

        PKIX_DECREF(policyList);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetPolicyMappings (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetPolicyMappings(
        PKIX_PL_Cert *cert,
        PKIX_List **pPolicyMappings, /* list of PKIX_PL_CertPolicyMap */
        void *plContext)
{
        PKIX_List *policyMappings = NULL; /* list of PKIX_PL_CertPolicyMap */

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetPolicyMappings");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pPolicyMappings);

        /* if we don't have a cached copy from before, we create one */
        if (!(cert->certPolicyMappings) && !(cert->policyMappingsAbsent)) {

                PKIX_OBJECT_LOCK(cert);

                if (!(cert->certPolicyMappings) &&
                    !(cert->policyMappingsAbsent)) {

                        PKIX_CHECK(pkix_pl_Cert_DecodePolicyMapping
                                (cert->nssCert, &policyMappings, plContext),
                                PKIX_CERTDECODEPOLICYMAPPINGFAILED);

                        if (!policyMappings) {
                                cert->policyMappingsAbsent = PKIX_TRUE;
                                *pPolicyMappings = NULL;
                                goto cleanup;
                        }
                }

                PKIX_OBJECT_UNLOCK(cert);

                /* save a cached copy in case it is asked for again */
                cert->certPolicyMappings = policyMappings; 
                policyMappings = NULL;
        }

        PKIX_INCREF(cert->certPolicyMappings);
        *pPolicyMappings = cert->certPolicyMappings;
        
cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);

        PKIX_DECREF(policyMappings);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetRequireExplicitPolicy
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetRequireExplicitPolicy(
        PKIX_PL_Cert *cert,
        PKIX_Int32 *pSkipCerts,
        void *plContext)
{
        PKIX_Int32 explicitPolicySkipCerts = 0;
        PKIX_Int32 inhibitMappingSkipCerts = 0;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetRequireExplicitPolicy");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pSkipCerts);

        if (!(cert->policyConstraintsProcessed)) {
                PKIX_OBJECT_LOCK(cert);

                if (!(cert->policyConstraintsProcessed)) {

                        /*
                         * If we can't process it now, we probably will be
                         * unable to process it later. Set the default value.
                         */
                        cert->policyConstraintsProcessed = PKIX_TRUE;
                        cert->policyConstraintsExplicitPolicySkipCerts = -1;
                        cert->policyConstraintsInhibitMappingSkipCerts = -1;

                        PKIX_CHECK(pkix_pl_Cert_DecodePolicyConstraints
                                (cert->nssCert,
                                &explicitPolicySkipCerts,
                                &inhibitMappingSkipCerts,
                                plContext),
                                PKIX_CERTDECODEPOLICYCONSTRAINTSFAILED);

                        cert->policyConstraintsExplicitPolicySkipCerts =
                                explicitPolicySkipCerts;
                        cert->policyConstraintsInhibitMappingSkipCerts =
                                inhibitMappingSkipCerts;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        *pSkipCerts = cert->policyConstraintsExplicitPolicySkipCerts;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetPolicyMappingInhibited
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetPolicyMappingInhibited(
        PKIX_PL_Cert *cert,
        PKIX_Int32 *pSkipCerts,
        void *plContext)
{
        PKIX_Int32 explicitPolicySkipCerts = 0;
        PKIX_Int32 inhibitMappingSkipCerts = 0;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetPolicyMappingInhibited");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pSkipCerts);

        if (!(cert->policyConstraintsProcessed)) {
                PKIX_OBJECT_LOCK(cert);

                if (!(cert->policyConstraintsProcessed)) {

                        /*
                         * If we can't process it now, we probably will be
                         * unable to process it later. Set the default value.
                         */
                        cert->policyConstraintsProcessed = PKIX_TRUE;
                        cert->policyConstraintsExplicitPolicySkipCerts = -1;
                        cert->policyConstraintsInhibitMappingSkipCerts = -1;

                        PKIX_CHECK(pkix_pl_Cert_DecodePolicyConstraints
                                (cert->nssCert,
                                &explicitPolicySkipCerts,
                                &inhibitMappingSkipCerts,
                                plContext),
                                PKIX_CERTDECODEPOLICYCONSTRAINTSFAILED);

                        cert->policyConstraintsExplicitPolicySkipCerts =
                                explicitPolicySkipCerts;
                        cert->policyConstraintsInhibitMappingSkipCerts =
                                inhibitMappingSkipCerts;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        *pSkipCerts = cert->policyConstraintsInhibitMappingSkipCerts;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetInhibitAnyPolicy (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetInhibitAnyPolicy(
        PKIX_PL_Cert *cert,
        PKIX_Int32 *pSkipCerts,
        void *plContext)
{
        PKIX_Int32 skipCerts = 0;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetInhibitAnyPolicy");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pSkipCerts);

        if (!(cert->inhibitAnyPolicyProcessed)) {

                PKIX_OBJECT_LOCK(cert);

                if (!(cert->inhibitAnyPolicyProcessed)) {

                        /*
                         * If we can't process it now, we probably will be
                         * unable to process it later. Set the default value.
                         */
                        cert->inhibitAnyPolicyProcessed = PKIX_TRUE;
                        cert->inhibitAnySkipCerts = -1;

                        PKIX_CHECK(pkix_pl_Cert_DecodeInhibitAnyPolicy
                                (cert->nssCert, &skipCerts, plContext),
                                PKIX_CERTDECODEINHIBITANYPOLICYFAILED);

                        cert->inhibitAnySkipCerts = skipCerts;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        *pSkipCerts = cert->inhibitAnySkipCerts;
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_AreCertPoliciesCritical
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_AreCertPoliciesCritical(
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pCritical,
        void *plContext)
{
        PKIX_Boolean criticality = PKIX_FALSE;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_AreCertPoliciesCritical");
        PKIX_NULLCHECK_TWO(cert, pCritical);

        PKIX_CHECK(pkix_pl_Cert_IsExtensionCritical(
                cert,
                SEC_OID_X509_CERTIFICATE_POLICIES,
                &criticality,
                plContext),
                PKIX_CERTISEXTENSIONCRITICALFAILED);

        *pCritical = criticality;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_VerifySignature (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_VerifySignature(
        PKIX_PL_Cert *cert,
        PKIX_PL_PublicKey *pubKey,
        void *plContext)
{
        CERTCertificate *nssCert = NULL;
        SECKEYPublicKey *nssPubKey = NULL;
        CERTSignedData *tbsCert = NULL;
        PKIX_PL_Cert *cachedCert = NULL;
        PKIX_Error *verifySig = NULL;
        PKIX_Error *cachedSig = NULL;
        PKIX_Error *checkSig = NULL;
        SECStatus status;
        PKIX_Boolean certEqual = PKIX_FALSE;
        PKIX_Boolean certInHash = PKIX_FALSE;
        PKIX_Boolean checkCertSig = PKIX_TRUE;
        void* wincx = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_VerifySignature");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pubKey);

        /* if the cert check flag is off, skip the check */
        checkSig = pkix_pl_NssContext_GetCertSignatureCheck(
                   (PKIX_PL_NssContext *)plContext, &checkCertSig);
        if ((checkCertSig == PKIX_FALSE) && (checkSig == NULL)) {
            goto cleanup;
        }

        verifySig = PKIX_PL_HashTable_Lookup
                        (cachedCertSigTable,
                        (PKIX_PL_Object *) pubKey,
                        (PKIX_PL_Object **) &cachedCert,
                        plContext);

        if (cachedCert != NULL && verifySig == NULL) {
                /* Cached Signature Table lookup succeed */
                PKIX_EQUALS(cert, cachedCert, &certEqual, plContext,
                            PKIX_OBJECTEQUALSFAILED);
                if (certEqual == PKIX_TRUE) {
                        goto cleanup;
                }
                /* Different PubKey may hash to same value, skip add */
                certInHash = PKIX_TRUE;
        }

        nssCert = cert->nssCert;
        tbsCert = &nssCert->signatureWrap;

        PKIX_CERT_DEBUG("\t\tCalling SECKEY_ExtractPublicKey).\n");
        nssPubKey = SECKEY_ExtractPublicKey(pubKey->nssSPKI);
        if (!nssPubKey){
                PKIX_ERROR(PKIX_SECKEYEXTRACTPUBLICKEYFAILED);
        }

        PKIX_CERT_DEBUG("\t\tCalling CERT_VerifySignedDataWithPublicKey).\n");

        PKIX_CHECK(pkix_pl_NssContext_GetWincx
                   ((PKIX_PL_NssContext *)plContext, &wincx),
                   PKIX_NSSCONTEXTGETWINCXFAILED);

        status = CERT_VerifySignedDataWithPublicKey(tbsCert, nssPubKey, wincx);

        if (status != SECSuccess) {
                if (PORT_GetError() != SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED) {
                        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                }
                PKIX_ERROR(PKIX_SIGNATUREDIDNOTVERIFYWITHTHEPUBLICKEY);
        }

        if (certInHash == PKIX_FALSE) {
                cachedSig = PKIX_PL_HashTable_Add
                        (cachedCertSigTable,
                        (PKIX_PL_Object *) pubKey,
                        (PKIX_PL_Object *) cert,
                        plContext);

                if (cachedSig != NULL) {
                        PKIX_DEBUG("PKIX_PL_HashTable_Add skipped: entry existed\n");
                }
        }

cleanup:
        if (nssPubKey){
                PKIX_CERT_DEBUG("\t\tCalling SECKEY_DestroyPublicKey).\n");
                SECKEY_DestroyPublicKey(nssPubKey);
        }

        PKIX_DECREF(cachedCert);
        PKIX_DECREF(checkSig);
        PKIX_DECREF(verifySig);
        PKIX_DECREF(cachedSig);

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_CheckValidity (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_CheckValidity(
        PKIX_PL_Cert *cert,
        PKIX_PL_Date *date,
        void *plContext)
{
        SECCertTimeValidity val;
        PRTime timeToCheck;
        PKIX_Boolean allowOverride;
        SECCertificateUsage requiredUsages;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_CheckValidity");
        PKIX_NULLCHECK_ONE(cert);

        /* if the caller supplies a date, we use it; else, use current time */
        if (date != NULL){
                PKIX_CHECK(pkix_pl_Date_GetPRTime
                        (date, &timeToCheck, plContext),
                        PKIX_DATEGETPRTIMEFAILED);
        } else {
                timeToCheck = PR_Now();
        }

        requiredUsages = ((PKIX_PL_NssContext*)plContext)->certificateUsage;
        allowOverride =
            (PRBool)((requiredUsages & certificateUsageSSLServer) ||
                     (requiredUsages & certificateUsageSSLServerWithStepUp) ||
                     (requiredUsages & certificateUsageIPsec));
        val = CERT_CheckCertValidTimes(cert->nssCert, timeToCheck, allowOverride);
        if (val != secCertTimeValid){
                PKIX_ERROR(PKIX_CERTCHECKCERTVALIDTIMESFAILED);
        }

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetValidityNotAfter (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetValidityNotAfter(
        PKIX_PL_Cert *cert,
        PKIX_PL_Date **pDate,
        void *plContext)
{
        PRTime prtime;
        SECStatus rv = SECFailure;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetValidityNotAfter");
        PKIX_NULLCHECK_TWO(cert, pDate);

        PKIX_DATE_DEBUG("\t\tCalling DER_DecodeTimeChoice).\n");
        rv = DER_DecodeTimeChoice(&prtime, &(cert->nssCert->validity.notAfter));
        if (rv != SECSuccess){
                PKIX_ERROR(PKIX_DERDECODETIMECHOICEFAILED);
        }

        PKIX_CHECK(pkix_pl_Date_CreateFromPRTime
                    (prtime, pDate, plContext),
                    PKIX_DATECREATEFROMPRTIMEFAILED);

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_VerifyCertAndKeyType (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_VerifyCertAndKeyType(
        PKIX_PL_Cert *cert,
        PKIX_Boolean isChainCert,
        void *plContext)
{
    PKIX_PL_CertBasicConstraints *basicConstraints = NULL;
    SECCertificateUsage certificateUsage;
    SECCertUsage certUsage = 0;
    unsigned int requiredKeyUsage;
    unsigned int requiredCertType;
    unsigned int certType;
    SECStatus rv = SECSuccess;
    
    PKIX_ENTER(CERT, "PKIX_PL_Cert_VerifyCertType");
    PKIX_NULLCHECK_TWO(cert, plContext);
    
    certificateUsage = ((PKIX_PL_NssContext*)plContext)->certificateUsage;
    
    /* ensure we obtained a single usage bit only */
    PORT_Assert(!(certificateUsage & (certificateUsage - 1)));
    
    /* convert SECertificateUsage (bit mask) to SECCertUsage (enum) */
    while (0 != (certificateUsage = certificateUsage >> 1)) { certUsage++; }

    /* check key usage and netscape cert type */
    cert_GetCertType(cert->nssCert);
    certType = cert->nssCert->nsCertType;
    if (isChainCert ||
        (certUsage != certUsageVerifyCA && certUsage != certUsageAnyCA)) {
	rv = CERT_KeyUsageAndTypeForCertUsage(certUsage, isChainCert,
					      &requiredKeyUsage,
					      &requiredCertType);
        if (rv == SECFailure) {
            PKIX_ERROR(PKIX_UNSUPPORTEDCERTUSAGE);
        }
    } else {
        /* use this key usage and cert type for certUsageAnyCA and
         * certUsageVerifyCA. */
	requiredKeyUsage = KU_KEY_CERT_SIGN;
	requiredCertType = NS_CERT_TYPE_CA;
    }
    if (CERT_CheckKeyUsage(cert->nssCert, requiredKeyUsage) != SECSuccess) {
        PKIX_ERROR(PKIX_CERTCHECKKEYUSAGEFAILED);
    }
    if (!(certType & requiredCertType)) {
        PKIX_ERROR(PKIX_CERTCHECKCERTTYPEFAILED);
    }
cleanup:
    PKIX_DECREF(basicConstraints);
    PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_VerifyKeyUsage (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_VerifyKeyUsage(
        PKIX_PL_Cert *cert,
        PKIX_UInt32 keyUsage,
        void *plContext)
{
        CERTCertificate *nssCert = NULL;
        PKIX_UInt32 nssKeyUsage = 0;
        SECStatus status;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_VerifyKeyUsage");
        PKIX_NULLCHECK_TWO(cert, cert->nssCert);

        nssCert = cert->nssCert;

        /* if cert doesn't have keyUsage extension, all keyUsages are valid */
        if (!nssCert->keyUsagePresent){
                goto cleanup;
        }

        if (keyUsage & PKIX_DIGITAL_SIGNATURE){
                nssKeyUsage = nssKeyUsage | KU_DIGITAL_SIGNATURE;
        }

        if (keyUsage & PKIX_NON_REPUDIATION){
                nssKeyUsage = nssKeyUsage | KU_NON_REPUDIATION;
        }

        if (keyUsage & PKIX_KEY_ENCIPHERMENT){
                nssKeyUsage = nssKeyUsage | KU_KEY_ENCIPHERMENT;
        }

        if (keyUsage & PKIX_DATA_ENCIPHERMENT){
                nssKeyUsage = nssKeyUsage | KU_DATA_ENCIPHERMENT;
        }

        if (keyUsage & PKIX_KEY_AGREEMENT){
                nssKeyUsage = nssKeyUsage | KU_KEY_AGREEMENT;
        }

        if (keyUsage & PKIX_KEY_CERT_SIGN){
                nssKeyUsage = nssKeyUsage | KU_KEY_CERT_SIGN;
        }

        if (keyUsage & PKIX_CRL_SIGN){
                nssKeyUsage = nssKeyUsage | KU_CRL_SIGN;
        }

        if (keyUsage & PKIX_ENCIPHER_ONLY){
                nssKeyUsage = nssKeyUsage | 0x01;
        }

        if (keyUsage & PKIX_DECIPHER_ONLY){
                /* XXX we should support this once it is fixed in NSS */
                PKIX_ERROR(PKIX_DECIPHERONLYKEYUSAGENOTSUPPORTED);
        }

        status = CERT_CheckKeyUsage(nssCert, nssKeyUsage);
        if (status != SECSuccess) {
                PKIX_ERROR(PKIX_CERTCHECKKEYUSAGEFAILED);
        }

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetNameConstraints
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetNameConstraints(
        PKIX_PL_Cert *cert,
        PKIX_PL_CertNameConstraints **pNameConstraints,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetNameConstraints");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pNameConstraints);

        /* if we don't have a cached copy from before, we create one */
        if (cert->nameConstraints == NULL && !cert->nameConstraintsAbsent) {

                PKIX_OBJECT_LOCK(cert);

                if (cert->nameConstraints == NULL &&
                    !cert->nameConstraintsAbsent) {

                        PKIX_CHECK(pkix_pl_CertNameConstraints_Create
                                (cert->nssCert, &nameConstraints, plContext),
                                PKIX_CERTNAMECONSTRAINTSCREATEFAILED);

                        if (nameConstraints == NULL) {
                                cert->nameConstraintsAbsent = PKIX_TRUE;
                        }

                        cert->nameConstraints = nameConstraints;
                }

                PKIX_OBJECT_UNLOCK(cert);

        }

        PKIX_INCREF(cert->nameConstraints);

        *pNameConstraints = cert->nameConstraints;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_CheckNameConstraints
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_CheckNameConstraints(
        PKIX_PL_Cert *cert,
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_Boolean treatCommonNameAsDNSName,
        void *plContext)
{
        PKIX_Boolean checkPass = PKIX_TRUE;
        CERTGeneralName *nssSubjectNames = NULL;
        PLArenaPool *arena = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_CheckNameConstraints");
        PKIX_NULLCHECK_ONE(cert);

        if (nameConstraints != NULL) {

                arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                if (arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }
                /* only check common Name if the usage requires it */
                if (treatCommonNameAsDNSName) {
                    SECCertificateUsage certificateUsage;
                    certificateUsage = ((PKIX_PL_NssContext*)plContext)->certificateUsage;
                    if ((certificateUsage != certificateUsageSSLServer) &&
                        (certificateUsage != certificateUsageIPsec)) {
                        treatCommonNameAsDNSName = PKIX_FALSE;
                    }
                }

                /* This NSS call returns Subject Alt Names. If
                 * treatCommonNameAsDNSName is true, it also returns the
                 * Subject Common Name
                 */
                PKIX_CERT_DEBUG
                    ("\t\tCalling CERT_GetConstrainedCertificateNames\n");
                nssSubjectNames = CERT_GetConstrainedCertificateNames
                        (cert->nssCert, arena, treatCommonNameAsDNSName);

                PKIX_CHECK(pkix_pl_CertNameConstraints_CheckNameSpaceNssNames
                        (nssSubjectNames,
                        nameConstraints,
                        &checkPass,
                        plContext),
                        PKIX_CERTNAMECONSTRAINTSCHECKNAMESPACENSSNAMESFAILED);

                if (checkPass != PKIX_TRUE) {
                        PKIX_ERROR(PKIX_CERTFAILEDNAMECONSTRAINTSCHECKING);
                }
        }

cleanup:
        if (arena){
                PORT_FreeArena(arena, PR_FALSE);
        }

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_MergeNameConstraints
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_MergeNameConstraints(
        PKIX_PL_CertNameConstraints *firstNC,
        PKIX_PL_CertNameConstraints *secondNC,
        PKIX_PL_CertNameConstraints **pResultNC,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *mergedNC = NULL;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_MergeNameConstraints");
        PKIX_NULLCHECK_TWO(firstNC, pResultNC);

        if (secondNC == NULL) {

                PKIX_INCREF(firstNC);
                *pResultNC = firstNC;

                goto cleanup;
        }

        PKIX_CHECK(pkix_pl_CertNameConstraints_Merge
                (firstNC, secondNC, &mergedNC, plContext),
                PKIX_CERTNAMECONSTRAINTSMERGEFAILED);

        *pResultNC = mergedNC;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * Find out the state of the NSS trust bits for the requested usage.
 * Returns SECFailure if the cert is explicitly distrusted.
 * Returns SECSuccess if the cert can be used to form a chain (normal case),
 *   or it is explicitly trusted. The trusted bool is set to true if it is
 *   explicitly trusted.
 */
static SECStatus
pkix_pl_Cert_GetTrusted(void *plContext,
                        PKIX_PL_Cert *cert,
                        PKIX_Boolean *trusted,
                        PKIX_Boolean isCA)
{
        SECStatus rv;
        CERTCertificate *nssCert = NULL;
        SECCertUsage certUsage = 0;
        SECCertificateUsage certificateUsage;
        SECTrustType trustType;
        unsigned int trustFlags;
        unsigned int requiredFlags;
        CERTCertTrust trust;

        *trusted = PKIX_FALSE;

        /* no key usage information  */
        if (plContext == NULL) {
                return SECSuccess;
        }

        certificateUsage = ((PKIX_PL_NssContext*)plContext)->certificateUsage;

        /* ensure we obtained a single usage bit only */
        PORT_Assert(!(certificateUsage & (certificateUsage - 1)));

        /* convert SECertificateUsage (bit mask) to SECCertUsage (enum) */
        while (0 != (certificateUsage = certificateUsage >> 1)) { certUsage++; }

        nssCert = cert->nssCert;

        if (!isCA) {
                PRBool prTrusted;
                unsigned int failedFlags;
                rv = cert_CheckLeafTrust(nssCert, certUsage,
                                         &failedFlags, &prTrusted);
                *trusted = (PKIX_Boolean) prTrusted;
                return rv;
        }
        rv = CERT_TrustFlagsForCACertUsage(certUsage, &requiredFlags,
                                           &trustType);
        if (rv != SECSuccess) {
                return SECSuccess;
        }

        rv = CERT_GetCertTrust(nssCert, &trust);
        if (rv != SECSuccess) {
                return SECSuccess;
        }
        trustFlags = SEC_GET_TRUST_FLAGS(&trust, trustType);
        /* normally trustTypeNone usages accept any of the given trust bits
         * being on as acceptable. If any are distrusted (and none are trusted),
         * then we will also distrust the cert */
        if ((trustFlags == 0) && (trustType == trustTypeNone)) {
                trustFlags = trust.sslFlags | trust.emailFlags |
                             trust.objectSigningFlags;
        }
        if ((trustFlags & requiredFlags) == requiredFlags) {
                *trusted = PKIX_TRUE;
                return SECSuccess;
        }
        if ((trustFlags & CERTDB_TERMINAL_RECORD) &&
            ((trustFlags & (CERTDB_VALID_CA|CERTDB_TRUSTED)) == 0)) {
                return SECFailure;
        }
        return SECSuccess;
}

/*
 * FUNCTION: PKIX_PL_Cert_IsCertTrusted
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_IsCertTrusted(
        PKIX_PL_Cert *cert,
        PKIX_PL_TrustAnchorMode trustAnchorMode,
        PKIX_Boolean *pTrusted,
        void *plContext)
{
        PKIX_CertStore_CheckTrustCallback trustCallback = NULL;
        PKIX_Boolean trusted = PKIX_FALSE;
        SECStatus rv = SECFailure;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_IsCertTrusted");
        PKIX_NULLCHECK_TWO(cert, pTrusted);

        /* Call GetTrusted first to see if we are going to distrust the
         * certificate */
        rv = pkix_pl_Cert_GetTrusted(plContext, cert, &trusted, PKIX_TRUE);
        if (rv != SECSuccess) {
                /* Failure means the cert is explicitly distrusted,
                 * let the next level know not to use it. */
                *pTrusted = PKIX_FALSE;
                PKIX_ERROR(PKIX_CERTISCERTTRUSTEDFAILED);
        }

        if (trustAnchorMode == PKIX_PL_TrustAnchorMode_Exclusive ||
            (trustAnchorMode == PKIX_PL_TrustAnchorMode_Additive &&
             cert->isUserTrustAnchor)) {
            /* Use the trust anchor's |trusted| value */
            *pTrusted = cert->isUserTrustAnchor;
            goto cleanup;
        }

        /* no key usage information or store is not trusted */
        if (plContext == NULL || cert->store == NULL) {
                *pTrusted = PKIX_FALSE;
                goto cleanup;
        }

        PKIX_CHECK(PKIX_CertStore_GetTrustCallback
                (cert->store, &trustCallback, plContext),
                PKIX_CERTSTOREGETTRUSTCALLBACKFAILED);

        PKIX_CHECK_ONLY_FATAL(trustCallback
                (cert->store, cert, &trusted, plContext),
                PKIX_CHECKTRUSTCALLBACKFAILED);

        /* allow trust store to override if we can trust the trust
         * bits */
        if (PKIX_ERROR_RECEIVED || (trusted == PKIX_FALSE)) {
                *pTrusted = PKIX_FALSE;
                goto cleanup;
        }

        *pTrusted = trusted;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_IsLeafCertTrusted
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_IsLeafCertTrusted(
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pTrusted,
        void *plContext)
{
        SECStatus rv;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_IsLeafCertTrusted");
        PKIX_NULLCHECK_TWO(cert, pTrusted);

        *pTrusted = PKIX_FALSE;

        rv = pkix_pl_Cert_GetTrusted(plContext, cert, pTrusted, PKIX_FALSE);
        if (rv != SECSuccess) {
                /* Failure means the cert is explicitly distrusted,
                 * let the next level know not to use it. */
                *pTrusted = PKIX_FALSE;
                PKIX_ERROR(PKIX_CERTISCERTTRUSTEDFAILED);
        }

cleanup:
        PKIX_RETURN(CERT);
}

/* FUNCTION: PKIX_PL_Cert_SetAsTrustAnchor */
PKIX_Error*
PKIX_PL_Cert_SetAsTrustAnchor(PKIX_PL_Cert *cert, 
                              void *plContext)
{
    PKIX_ENTER(CERT, "PKIX_PL_Cert_SetAsTrustAnchor");
    PKIX_NULLCHECK_ONE(cert);
    
    cert->isUserTrustAnchor = PKIX_TRUE;
    
    PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetCacheFlag (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetCacheFlag(
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pCacheFlag,
        void *plContext)
{
        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetCacheFlag");
        PKIX_NULLCHECK_TWO(cert, pCacheFlag);

        *pCacheFlag = cert->cacheFlag;

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_SetCacheFlag (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_SetCacheFlag(
        PKIX_PL_Cert *cert,
        PKIX_Boolean cacheFlag,
        void *plContext)
{
        PKIX_ENTER(CERT, "PKIX_PL_Cert_SetCacheFlag");
        PKIX_NULLCHECK_ONE(cert);

        cert->cacheFlag = cacheFlag;

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetTrustCertStore (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetTrustCertStore(
        PKIX_PL_Cert *cert,
        PKIX_CertStore **pTrustCertStore,
        void *plContext)
{
        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetTrustCertStore");
        PKIX_NULLCHECK_TWO(cert, pTrustCertStore);

        PKIX_INCREF(cert->store);
        *pTrustCertStore = cert->store;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_SetTrustCertStore (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_SetTrustCertStore(
        PKIX_PL_Cert *cert,
        PKIX_CertStore *trustCertStore,
        void *plContext)
{
        PKIX_ENTER(CERT, "PKIX_PL_Cert_SetTrustCertStore");
        PKIX_NULLCHECK_TWO(cert, trustCertStore);

        PKIX_INCREF(trustCertStore);
        cert->store = trustCertStore;

cleanup:
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetAuthorityInfoAccess
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetAuthorityInfoAccess(
        PKIX_PL_Cert *cert,
        PKIX_List **pAiaList, /* of PKIX_PL_InfoAccess */
        void *plContext)
{
        PKIX_List *aiaList = NULL; /* of PKIX_PL_InfoAccess */
        SECItem *encodedAIA = NULL;
        CERTAuthInfoAccess **aia = NULL;
        PLArenaPool *arena = NULL;
        SECStatus rv;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetAuthorityInfoAccess");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pAiaList);

        /* if we don't have a cached copy from before, we create one */
        if (cert->authorityInfoAccess == NULL) {

                PKIX_OBJECT_LOCK(cert);

                if (cert->authorityInfoAccess == NULL) {

                    PKIX_PL_NSSCALLRV(CERT, encodedAIA, SECITEM_AllocItem,
                        (NULL, NULL, 0));

                    if (encodedAIA == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                    }

                    PKIX_PL_NSSCALLRV(CERT, rv, CERT_FindCertExtension,
                        (cert->nssCert,
                        SEC_OID_X509_AUTH_INFO_ACCESS,
                        encodedAIA));

                    if (rv == SECFailure) {
                        goto cleanup;
                    }

                    PKIX_PL_NSSCALLRV(CERT, arena, PORT_NewArena,
                        (DER_DEFAULT_CHUNKSIZE));

                    if (arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                    }

                    PKIX_PL_NSSCALLRV
                        (CERT, aia, CERT_DecodeAuthInfoAccessExtension,
                        (arena, encodedAIA));

                    PKIX_CHECK(pkix_pl_InfoAccess_CreateList
                        (aia, &aiaList, plContext),
                        PKIX_INFOACCESSCREATELISTFAILED);

                    cert->authorityInfoAccess = aiaList;
                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->authorityInfoAccess);

        *pAiaList = cert->authorityInfoAccess;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        if (arena != NULL) {
                PORT_FreeArena(arena, PR_FALSE);
        }

        if (encodedAIA != NULL) {
                SECITEM_FreeItem(encodedAIA, PR_TRUE);
        }

        PKIX_RETURN(CERT);
}

/* XXX Following defines belongs to NSS */
static const unsigned char siaOIDString[] = {0x2b, 0x06, 0x01, 0x05, 0x05,
                                0x07, 0x01, 0x0b};
#define OI(x) { siDEROID, (unsigned char *)x, sizeof x }

/*
 * FUNCTION: PKIX_PL_Cert_GetSubjectInfoAccess
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetSubjectInfoAccess(
        PKIX_PL_Cert *cert,
        PKIX_List **pSiaList, /* of PKIX_PL_InfoAccess */
        void *plContext)
{
        PKIX_List *siaList; /* of PKIX_PL_InfoAccess */
        SECItem siaOID = OI(siaOIDString);
        SECItem *encodedSubjInfoAccess = NULL;
        CERTAuthInfoAccess **subjInfoAccess = NULL;
        PLArenaPool *arena = NULL;
        SECStatus rv;

        PKIX_ENTER(CERT, "PKIX_PL_Cert_GetSubjectInfoAccess");
        PKIX_NULLCHECK_THREE(cert, cert->nssCert, pSiaList);

        /* XXX
         * Codes to deal with SubjectInfoAccess OID should be moved to
         * NSS soon. I implemented them here so we don't touch NSS
         * source tree, from JP's suggestion.
         */

        /* if we don't have a cached copy from before, we create one */
        if (cert->subjectInfoAccess == NULL) {

                PKIX_OBJECT_LOCK(cert);

                if (cert->subjectInfoAccess == NULL) {

                    encodedSubjInfoAccess = SECITEM_AllocItem(NULL, NULL, 0);
                    if (encodedSubjInfoAccess == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                    }

                    PKIX_CERT_DEBUG
                        ("\t\tCalling CERT_FindCertExtensionByOID).\n");
                    rv = CERT_FindCertExtensionByOID
                                (cert->nssCert, &siaOID, encodedSubjInfoAccess);

                    if (rv == SECFailure) {
                        goto cleanup;
                    }

                    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                    if (arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                    }

                    /* XXX
                     * Decode Subject Information Access -
                     * since its type is the same as Authority Information
                     * Access, reuse the call. NSS- change name to avoid
                     * confusion.
                     */
                    PKIX_CERT_DEBUG
                        ("\t\tCalling CERT_DecodeAuthInfoAccessExtension).\n");
                    subjInfoAccess = CERT_DecodeAuthInfoAccessExtension
                        (arena, encodedSubjInfoAccess);

                    PKIX_CHECK(pkix_pl_InfoAccess_CreateList
                            (subjInfoAccess, &siaList, plContext),
                            PKIX_INFOACCESSCREATELISTFAILED);

                    cert->subjectInfoAccess = siaList;

                }

                PKIX_OBJECT_UNLOCK(cert);
        }

        PKIX_INCREF(cert->subjectInfoAccess);
        *pSiaList = cert->subjectInfoAccess;

cleanup:
	PKIX_OBJECT_UNLOCK(lockedObject);
        if (arena != NULL) {
                PORT_FreeArena(arena, PR_FALSE);
        }

        if (encodedSubjInfoAccess != NULL) {
                SECITEM_FreeItem(encodedSubjInfoAccess, PR_TRUE);
        }
        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetCrlDp
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetCrlDp(
    PKIX_PL_Cert *cert,
    PKIX_List **pDpList,
    void *plContext)
{
    PKIX_UInt32 dpIndex = 0;
    pkix_pl_CrlDp *dp = NULL; 
    CERTCrlDistributionPoints *dpoints = NULL;

    PKIX_ENTER(CERT, "PKIX_PL_Cert_GetCrlDp");
    PKIX_NULLCHECK_THREE(cert, cert->nssCert, pDpList);
                
    /* if we don't have a cached copy from before, we create one */
    if (cert->crldpList == NULL) {
        PKIX_OBJECT_LOCK(cert);
        if (cert->crldpList != NULL) {
            goto cleanup;
        }
        PKIX_CHECK(PKIX_List_Create(&cert->crldpList, plContext),
                   PKIX_LISTCREATEFAILED);
        dpoints = CERT_FindCRLDistributionPoints(cert->nssCert);
        if (!dpoints || !dpoints->distPoints) {
            goto cleanup;
        }
        for (;dpoints->distPoints[dpIndex];dpIndex++) {
            PKIX_CHECK(
                pkix_pl_CrlDp_Create(dpoints->distPoints[dpIndex],
                                     &cert->nssCert->issuer,
                                     &dp, plContext),
                PKIX_CRLDPCREATEFAILED);
            /* Create crldp list in reverse order in attempt to get
             * to the whole crl first. */
            PKIX_CHECK(
                PKIX_List_InsertItem(cert->crldpList, 0,
                                     (PKIX_PL_Object*)dp,
                                     plContext),
                PKIX_LISTAPPENDITEMFAILED);
            PKIX_DECREF(dp);
        }
    }
cleanup:
    PKIX_INCREF(cert->crldpList);
    *pDpList = cert->crldpList;

    PKIX_OBJECT_UNLOCK(lockedObject);
    PKIX_DECREF(dp);

    PKIX_RETURN(CERT);
}

/*
 * FUNCTION: PKIX_PL_Cert_GetCERTCertificate
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Cert_GetCERTCertificate(
        PKIX_PL_Cert *cert,
        CERTCertificate **pnssCert, 
        void *plContext)
{
    PKIX_ENTER(CERT, "PKIX_PL_Cert_GetNssCert");
    PKIX_NULLCHECK_TWO(cert, pnssCert);

    *pnssCert = CERT_DupCertificate(cert->nssCert);

    PKIX_RETURN(CERT);
}
