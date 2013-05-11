/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_ldapcertstore.c
 *
 * LDAPCertStore Function Definitions
 *
 */

/* We can't decode the length of a message without at least this many bytes */
#define MINIMUM_MSG_LENGTH 5

#include "pkix_pl_ldapcertstore.h"

/* --Private-Ldap-CertStore-Database-Functions----------------------- */

/*
 * FUNCTION: pkix_pl_LdapCertStore_DecodeCrossCertPair
 * DESCRIPTION:
 *
 *  This function decodes a DER-encoded CrossCertPair pointed to by
 *  "responseList" and extracts and decodes the Certificates in that pair,
 *  adding the resulting Certs, if the decoding was successful, to the List
 *  (possibly empty) pointed to by "certList". If none of the objects
 *  can be decoded into a Cert, the List is returned unchanged.
 *
 * PARAMETERS:
 *  "derCCPItem"
 *      The address of the SECItem containing the DER representation of the
 *      CrossCertPair. Must be non-NULL.
 *  "certList"
 *      The address of the List to which the decoded Certs are added. May be
 *      empty, but must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapCertStore_DecodeCrossCertPair(
        SECItem *derCCPItem,
        PKIX_List *certList,
        void *plContext)
{
        LDAPCertPair certPair = {{ siBuffer, NULL, 0 }, { siBuffer, NULL, 0 }};
        SECStatus rv = SECFailure;

        PLArenaPool *tempArena = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_DecodeCrossCertPair");
        PKIX_NULLCHECK_TWO(derCCPItem, certList);

        tempArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (!tempArena) {
            PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        rv = SEC_ASN1DecodeItem(tempArena, &certPair, PKIX_PL_LDAPCrossCertPairTemplate,
                                derCCPItem);
        if (rv != SECSuccess) {
                goto cleanup;
        }

        if (certPair.forward.data != NULL) {

            PKIX_CHECK(
                pkix_pl_Cert_CreateToList(&certPair.forward, certList,
                                          plContext),
                PKIX_CERTCREATETOLISTFAILED);
        }

        if (certPair.reverse.data != NULL) {

            PKIX_CHECK(
                pkix_pl_Cert_CreateToList(&certPair.reverse, certList,
                                          plContext),
                PKIX_CERTCREATETOLISTFAILED);
        }

cleanup:
        if (tempArena) {
            PORT_FreeArena(tempArena, PR_FALSE);
        }

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_LdapCertStore_BuildCertList
 * DESCRIPTION:
 *
 *  This function takes a List of LdapResponse objects pointed to by
 *  "responseList" and extracts and decodes the Certificates in those responses,
 *  storing the List of those Certificates at "pCerts". If none of the objects
 *  can be decoded into a Cert, the returned List is empty.
 *
 * PARAMETERS:
 *  "responseList"
 *      The address of the List of LdapResponses. Must be non-NULL.
 *  "pCerts"
 *      The address at which the result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapCertStore_BuildCertList(
        PKIX_List *responseList,
        PKIX_List **pCerts,
        void *plContext)
{
        PKIX_UInt32 numResponses = 0;
        PKIX_UInt32 respIx = 0;
        LdapAttrMask attrBits = 0;
        PKIX_PL_LdapResponse *response = NULL;
        PKIX_List *certList = NULL;
        LDAPMessage *message = NULL;
        LDAPSearchResponseEntry *sre = NULL;
        LDAPSearchResponseAttr **sreAttrArray = NULL;
        LDAPSearchResponseAttr *sreAttr = NULL;
        SECItem *attrType = NULL;
        SECItem **attrVal = NULL;
        SECItem *derCertItem = NULL;


        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_BuildCertList");
        PKIX_NULLCHECK_TWO(responseList, pCerts);

        PKIX_CHECK(PKIX_List_Create(&certList, plContext),
                PKIX_LISTCREATEFAILED);

        /* extract certs from response */
        PKIX_CHECK(PKIX_List_GetLength
                (responseList, &numResponses, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (respIx = 0; respIx < numResponses; respIx++) {
                PKIX_CHECK(PKIX_List_GetItem
                        (responseList,
                        respIx,
                        (PKIX_PL_Object **)&response,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(pkix_pl_LdapResponse_GetMessage
                        (response, &message, plContext),
                        PKIX_LDAPRESPONSEGETMESSAGEFAILED);

                sre = &(message->protocolOp.op.searchResponseEntryMsg);
                sreAttrArray = sre->attributes;

                /* Get next element of null-terminated array */
                sreAttr = *sreAttrArray++;
                while (sreAttr != NULL) {
                    attrType = &(sreAttr->attrType);
                    PKIX_CHECK(pkix_pl_LdapRequest_AttrTypeToBit
                        (attrType, &attrBits, plContext),
                        PKIX_LDAPREQUESTATTRTYPETOBITFAILED);
                    /* Is this attrVal a Certificate? */
                    if (((LDAPATTR_CACERT | LDAPATTR_USERCERT) &
                            attrBits) == attrBits) {
                        attrVal = sreAttr->val;
                        derCertItem = *attrVal++;
                        while (derCertItem != 0) {
                            /* create a PKIX_PL_Cert from derCert */
                            PKIX_CHECK(pkix_pl_Cert_CreateToList
                                (derCertItem, certList, plContext),
                                PKIX_CERTCREATETOLISTFAILED);
                            derCertItem = *attrVal++;
                        }
                    } else if ((LDAPATTR_CROSSPAIRCERT & attrBits) == attrBits){
                        /* Is this attrVal a CrossPairCertificate? */
                        attrVal = sreAttr->val;
                        derCertItem = *attrVal++;
                        while (derCertItem != 0) {
                            /* create PKIX_PL_Certs from derCert */
                            PKIX_CHECK(pkix_pl_LdapCertStore_DecodeCrossCertPair
                                (derCertItem, certList, plContext),
                                PKIX_LDAPCERTSTOREDECODECROSSCERTPAIRFAILED);
                            derCertItem = *attrVal++;
                        }
                    }
                    sreAttr = *sreAttrArray++;
                }
                PKIX_DECREF(response);
        }

        *pCerts = certList;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(certList);
        }

        PKIX_DECREF(response);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_LdapCertStore_BuildCrlList
 * DESCRIPTION:
 *
 *  This function takes a List of LdapResponse objects pointed to by
 *  "responseList" and extracts and decodes the CRLs in those responses, storing
 *  the List of those CRLs at "pCrls". If none of the objects can be decoded
 *  into a CRL, the returned List is empty.
 *
 * PARAMETERS:
 *  "responseList"
 *      The address of the List of LdapResponses. Must be non-NULL.
 *  "pCrls"
 *      The address at which the result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapCertStore_BuildCrlList(
        PKIX_List *responseList,
        PKIX_List **pCrls,
        void *plContext)
{
        PKIX_UInt32 numResponses = 0;
        PKIX_UInt32 respIx = 0;
        LdapAttrMask attrBits = 0;
        CERTSignedCrl *nssCrl = NULL;
        PKIX_PL_LdapResponse *response = NULL;
        PKIX_List *crlList = NULL;
        PKIX_PL_CRL *crl = NULL;
        LDAPMessage *message = NULL;
        LDAPSearchResponseEntry *sre = NULL;
        LDAPSearchResponseAttr **sreAttrArray = NULL;
        LDAPSearchResponseAttr *sreAttr = NULL;
        SECItem *attrType = NULL;
        SECItem **attrVal = NULL;
        SECItem *derCrlCopy = NULL;
        SECItem *derCrlItem = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_BuildCrlList");
        PKIX_NULLCHECK_TWO(responseList, pCrls);

        PKIX_CHECK(PKIX_List_Create(&crlList, plContext),
                PKIX_LISTCREATEFAILED);

        /* extract crls from response */
        PKIX_CHECK(PKIX_List_GetLength
                (responseList, &numResponses, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (respIx = 0; respIx < numResponses; respIx++) {
                PKIX_CHECK(PKIX_List_GetItem
                        (responseList,
                        respIx,
                        (PKIX_PL_Object **)&response,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(pkix_pl_LdapResponse_GetMessage
                        (response, &message, plContext),
                        PKIX_LDAPRESPONSEGETMESSAGEFAILED);

                sre = &(message->protocolOp.op.searchResponseEntryMsg);
                sreAttrArray = sre->attributes;

                /* Get next element of null-terminated array */
                sreAttr = *sreAttrArray++;
                while (sreAttr != NULL) {
                    attrType = &(sreAttr->attrType);
                    PKIX_CHECK(pkix_pl_LdapRequest_AttrTypeToBit
                        (attrType, &attrBits, plContext),
                        PKIX_LDAPREQUESTATTRTYPETOBITFAILED);
                    /* Is this attrVal a Revocation List? */
                    if (((LDAPATTR_CERTREVLIST | LDAPATTR_AUTHREVLIST) &
                            attrBits) == attrBits) {
                        attrVal = sreAttr->val;
                        derCrlItem = *attrVal++;
                        while (derCrlItem != 0) {
                            /* create a PKIX_PL_Crl from derCrl */
                            derCrlCopy = SECITEM_DupItem(derCrlItem);
                            if (!derCrlCopy) {
                                PKIX_ERROR(PKIX_ALLOCERROR);
                            }
                            /* crl will be based on derCrlCopy, but wont
                             * own the der. */
                            nssCrl =
                                CERT_DecodeDERCrlWithFlags(NULL, derCrlCopy,
                                                       SEC_CRL_TYPE,
                                                       CRL_DECODE_DONT_COPY_DER |
                                                       CRL_DECODE_SKIP_ENTRIES);
                            if (!nssCrl) {
                                SECITEM_FreeItem(derCrlCopy, PKIX_TRUE);
                                continue;
                            }
                            /* pkix crl own the der. */
                            PKIX_CHECK(
                                pkix_pl_CRL_CreateWithSignedCRL(nssCrl, 
                                       derCrlCopy, NULL, &crl, plContext),
                                PKIX_CRLCREATEWITHSIGNEDCRLFAILED);
                            /* Left control over memory pointed by derCrlCopy and
                             * nssCrl to pkix crl. */
                            derCrlCopy = NULL;
                            nssCrl = NULL;
                            PKIX_CHECK(PKIX_List_AppendItem
                                       (crlList, (PKIX_PL_Object *) crl, plContext),
                                       PKIX_LISTAPPENDITEMFAILED);
                            PKIX_DECREF(crl);
                            derCrlItem = *attrVal++;
                        }
                        /* Clean up after PKIX_CHECK_ONLY_FATAL */
                        pkixTempErrorReceived = PKIX_FALSE;
                    }
                    sreAttr = *sreAttrArray++;
                }
                PKIX_DECREF(response);
        }

        *pCrls = crlList;
        crlList = NULL;
cleanup:
        if (derCrlCopy) {
            SECITEM_FreeItem(derCrlCopy, PKIX_TRUE);
        }
        if (nssCrl) {
            SEC_DestroyCrl(nssCrl);
        }
        PKIX_DECREF(crl);
        PKIX_DECREF(crlList);
        PKIX_DECREF(response);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_LdapCertStore_DestroyAVAList
 * DESCRIPTION:
 *
 *  This function frees the space allocated for the components of the
 *  equalFilters that make up the andFilter pointed to by "filter".
 *
 * PARAMETERS:
 *  "requestParams"
 *      The address of the andFilter whose components are to be freed. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapCertStore_DestroyAVAList(
        LDAPNameComponent **nameComponents,
        void *plContext)
{
        LDAPNameComponent **currentNC = NULL;
        unsigned char *component = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_DestroyAVAList");
        PKIX_NULLCHECK_ONE(nameComponents);

        /* Set currentNC to point to first AVA pointer */
        currentNC = nameComponents;

        while ((*currentNC) != NULL) {
                component = (*currentNC)->attrValue;
                if (component != NULL) {
                        PORT_Free(component);
                }
                currentNC++;
        }

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_LdapCertStore_MakeNameAVAList
 * DESCRIPTION:
 *
 *  This function allocates space from the arena pointed to by "arena" to
 *  construct a filter that will match components of the X500Name pointed to
 *  by "name", and stores the resulting filter at "pFilter".
 *
 *  "name" is checked for commonName and organizationName components (cn=,
 *  and o=). The component strings are extracted using the family of
 *  CERT_Get* functions, and each must be freed with PORT_Free.
 *
 *  It is not clear which components should be in a request, so, for now,
 *  we stop adding components after we have found one.
 *
 * PARAMETERS:
 *  "arena"
 *      The address of the PLArenaPool used in creating the filter. Must be
 *       non-NULL.
 *  "name"
 *      The address of the X500Name whose components define the desired
 *      matches. Must be non-NULL.
 *  "pList"
 *      The address at which the result is stored.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapCertStore_MakeNameAVAList(
        PLArenaPool *arena,
        PKIX_PL_X500Name *subjectName, 
        LDAPNameComponent ***pList,
        void *plContext)
{
        LDAPNameComponent **setOfNameComponents;
        LDAPNameComponent *currentNameComponent = NULL;
        PKIX_UInt32 componentsPresent = 0;
        void *v = NULL;
        unsigned char *component = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_MakeNameAVAList");
        PKIX_NULLCHECK_THREE(arena, subjectName, pList);

        /* Increase this if additional components may be extracted */
#define MAX_NUM_COMPONENTS 3

        /* Space for (MAX_NUM_COMPONENTS + 1) pointers to LDAPNameComponents */
        PKIX_PL_NSSCALLRV(CERTSTORE, v, PORT_ArenaZAlloc,
                (arena, (MAX_NUM_COMPONENTS + 1)*sizeof(LDAPNameComponent *)));
        setOfNameComponents = (LDAPNameComponent **)v;

        /* Space for MAX_NUM_COMPONENTS LDAPNameComponents */
        PKIX_PL_NSSCALLRV(CERTSTORE, v, PORT_ArenaZNewArray,
                (arena, LDAPNameComponent, MAX_NUM_COMPONENTS));

        currentNameComponent = (LDAPNameComponent *)v;

        /* Try for commonName */
        PKIX_CHECK(pkix_pl_X500Name_GetCommonName
                (subjectName, &component, plContext),
                PKIX_X500NAMEGETCOMMONNAMEFAILED);
        if (component) {
                setOfNameComponents[componentsPresent] = currentNameComponent;
                currentNameComponent->attrType = (unsigned char *)"cn";
                currentNameComponent->attrValue = component;
                componentsPresent++;
                currentNameComponent++;
        }

        /*
         * The LDAP specification says we can send multiple name components
         * in an "AND" filter, but the LDAP Servers don't seem to be able to
         * handle such requests. So we'll quit after the cn component.
         */
#if 0
        /* Try for orgName */
        PKIX_CHECK(pkix_pl_X500Name_GetOrgName
                (subjectName, &component, plContext),
                PKIX_X500NAMEGETORGNAMEFAILED);
        if (component) {
                setOfNameComponents[componentsPresent] = currentNameComponent;
                currentNameComponent->attrType = (unsigned char *)"o";
                currentNameComponent->attrValue = component;
                componentsPresent++;
                currentNameComponent++;
        }

        /* Try for countryName */
        PKIX_CHECK(pkix_pl_X500Name_GetCountryName
                (subjectName, &component, plContext),
                PKIX_X500NAMEGETCOUNTRYNAMEFAILED);
        if (component) {
                setOfNameComponents[componentsPresent] = currentNameComponent;
                currentNameComponent->attrType = (unsigned char *)"c";
                currentNameComponent->attrValue = component;
                componentsPresent++;
                currentNameComponent++;
        }
#endif

        setOfNameComponents[componentsPresent] = NULL;

        *pList = setOfNameComponents;

cleanup:

        PKIX_RETURN(CERTSTORE);

}

#if 0
/*
 * FUNCTION: pkix_pl_LdapCertstore_ConvertCertResponses
 * DESCRIPTION:
 *
 *  This function processes the List of LDAPResponses pointed to by "responses"
 *  into a List of resulting Certs, storing the result at "pCerts". If there
 *  are no responses converted successfully, a NULL may be stored.
 *
 * PARAMETERS:
 *  "responses"
 *      The LDAPResponses whose contents are to be converted. Must be non-NULL.
 *  "pCerts"
 *      Address at which the returned List is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapCertStore_ConvertCertResponses(
        PKIX_List *responses,
        PKIX_List **pCerts,
        void *plContext)
{
        PKIX_List *unfiltered = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_ConvertCertResponses");
        PKIX_NULLCHECK_TWO(responses, pCerts);

        /*
         * We have a List of LdapResponse objects that have to be
         * turned into Certs.
         */
        PKIX_CHECK(pkix_pl_LdapCertStore_BuildCertList
                (responses, &unfiltered, plContext),
                PKIX_LDAPCERTSTOREBUILDCERTLISTFAILED);

        *pCerts = unfiltered;

cleanup:

        PKIX_RETURN(CERTSTORE);
}
#endif

/*
 * FUNCTION: pkix_pl_LdapCertStore_GetCert
 *  (see description of PKIX_CertStore_CertCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_LdapCertStore_GetCert(
        PKIX_CertStore *store,
        PKIX_CertSelector *selector,
        PKIX_VerifyNode *verifyNode,
        void **pNBIOContext,
        PKIX_List **pCertList,
        void *plContext)
{
        PLArenaPool *requestArena = NULL;
        LDAPRequestParams requestParams;
        void *pollDesc = NULL;
        PKIX_Int32 minPathLen = 0;
        PKIX_Boolean cacheFlag = PKIX_FALSE;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_PL_LdapCertStoreContext *lcs = NULL;
        PKIX_List *responses = NULL;
        PKIX_List *unfilteredCerts = NULL;
        PKIX_List *filteredCerts = NULL;
        PKIX_PL_X500Name *subjectName = 0;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_GetCert");
        PKIX_NULLCHECK_THREE(store, selector, pCertList);

        requestParams.baseObject = "c=US";
        requestParams.scope = WHOLE_SUBTREE;
        requestParams.derefAliases = NEVER_DEREF;
        requestParams.sizeLimit = 0;
        requestParams.timeLimit = 0;

        /* Prepare elements for request filter */

        /*
         * Get a short-lived arena. We'll be done with this space once
         * the request is encoded.
         */
        requestArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (!requestArena) {
                PKIX_ERROR_FATAL(PKIX_OUTOFMEMORY);
        }

        PKIX_CHECK(PKIX_CertSelector_GetCommonCertSelectorParams
                (selector, &params, plContext),
                PKIX_CERTSELECTORGETCOMCERTSELPARAMSFAILED);

        /*
         * If we have the subject name for the desired subject,
         * ask the server for Certs with that subject.
         */
        PKIX_CHECK(PKIX_ComCertSelParams_GetSubject
                (params, &subjectName, plContext),
                PKIX_COMCERTSELPARAMSGETSUBJECTFAILED);

        PKIX_CHECK(PKIX_ComCertSelParams_GetBasicConstraints
                (params, &minPathLen, plContext),
                PKIX_COMCERTSELPARAMSGETBASICCONSTRAINTSFAILED);

        if (subjectName) {
                PKIX_CHECK(pkix_pl_LdapCertStore_MakeNameAVAList
                        (requestArena,
                        subjectName,
                        &(requestParams.nc),
                        plContext),
                        PKIX_LDAPCERTSTOREMAKENAMEAVALISTFAILED);

                if (*requestParams.nc == NULL) {
                        /*
                         * The subjectName may not include any components
                         * that we know how to encode. We do not return
                         * an error, because the caller did not necessarily
                         * do anything wrong, but we return an empty List.
                         */
                        PKIX_PL_NSSCALL(CERTSTORE, PORT_FreeArena,
                                (requestArena, PR_FALSE));

                        PKIX_CHECK(PKIX_List_Create(&filteredCerts, plContext),
                                PKIX_LISTCREATEFAILED);

                        PKIX_CHECK(PKIX_List_SetImmutable
                                (filteredCerts, plContext),
                                PKIX_LISTSETIMMUTABLEFAILED);

                        *pNBIOContext = NULL;
                        *pCertList = filteredCerts;
                        filteredCerts = NULL;
                        goto cleanup;
                }
        } else {
                PKIX_ERROR(PKIX_INSUFFICIENTCRITERIAFORCERTQUERY);
        }

        /* Prepare attribute field of request */

        requestParams.attributes = 0;

        if (minPathLen < 0) {
                requestParams.attributes |= LDAPATTR_USERCERT;
        }

        if (minPathLen > -2) {
                requestParams.attributes |=
                        LDAPATTR_CACERT | LDAPATTR_CROSSPAIRCERT;
        }

        /* All request fields are done */

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                (store, (PKIX_PL_Object **)&lcs, plContext),
                PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        PKIX_CHECK(PKIX_PL_LdapClient_InitiateRequest
                ((PKIX_PL_LdapClient *)lcs,
                &requestParams,
                &pollDesc,
                &responses,
                plContext),
                PKIX_LDAPCLIENTINITIATEREQUESTFAILED);

        PKIX_CHECK(pkix_pl_LdapCertStore_DestroyAVAList
                (requestParams.nc, plContext),
                PKIX_LDAPCERTSTOREDESTROYAVALISTFAILED);

        if (requestArena) {
                PKIX_PL_NSSCALL(CERTSTORE, PORT_FreeArena,
                        (requestArena, PR_FALSE));
                requestArena = NULL;
        }

        if (pollDesc != NULL) {
                /* client is waiting for non-blocking I/O to complete */
                *pNBIOContext = (void *)pollDesc;
                *pCertList = NULL;
                goto cleanup;
        }
        /* LdapClient has given us a response! */

        if (responses) {
                PKIX_CHECK(PKIX_CertStore_GetCertStoreCacheFlag
                        (store, &cacheFlag, plContext),
                        PKIX_CERTSTOREGETCERTSTORECACHEFLAGFAILED);

                PKIX_CHECK(pkix_pl_LdapCertStore_BuildCertList
                        (responses, &unfilteredCerts, plContext),
                        PKIX_LDAPCERTSTOREBUILDCERTLISTFAILED);

                PKIX_CHECK(pkix_CertSelector_Select
                        (selector, unfilteredCerts, &filteredCerts, plContext),
                        PKIX_CERTSELECTORSELECTFAILED);
        }

        *pNBIOContext = NULL;
        *pCertList = filteredCerts;
        filteredCerts = NULL;

cleanup:

        PKIX_DECREF(params);
        PKIX_DECREF(subjectName);
        PKIX_DECREF(responses);
        PKIX_DECREF(unfilteredCerts);
        PKIX_DECREF(filteredCerts);
        PKIX_DECREF(lcs);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_LdapCertStore_GetCertContinue
 *  (see description of PKIX_CertStore_CertCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_LdapCertStore_GetCertContinue(
        PKIX_CertStore *store,
        PKIX_CertSelector *selector,
        PKIX_VerifyNode *verifyNode,
        void **pNBIOContext,
        PKIX_List **pCertList,
        void *plContext)
{
        PKIX_Boolean cacheFlag = PKIX_FALSE;
        PKIX_PL_LdapCertStoreContext *lcs = NULL;
        void *pollDesc = NULL;
        PKIX_List *responses = NULL;
        PKIX_List *unfilteredCerts = NULL;
        PKIX_List *filteredCerts = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_GetCertContinue");
        PKIX_NULLCHECK_THREE(store, selector, pCertList);

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                (store, (PKIX_PL_Object **)&lcs, plContext),
                PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        PKIX_CHECK(PKIX_PL_LdapClient_ResumeRequest
                ((PKIX_PL_LdapClient *)lcs, &pollDesc, &responses, plContext),
                PKIX_LDAPCLIENTRESUMEREQUESTFAILED);

        if (pollDesc != NULL) {
                /* client is waiting for non-blocking I/O to complete */
                *pNBIOContext = (void *)pollDesc;
                *pCertList = NULL;
                goto cleanup;
        }
        /* LdapClient has given us a response! */

        if (responses) {
                PKIX_CHECK(PKIX_CertStore_GetCertStoreCacheFlag
                        (store, &cacheFlag, plContext),
                        PKIX_CERTSTOREGETCERTSTORECACHEFLAGFAILED);

                PKIX_CHECK(pkix_pl_LdapCertStore_BuildCertList
                        (responses, &unfilteredCerts, plContext),
                        PKIX_LDAPCERTSTOREBUILDCERTLISTFAILED);

                PKIX_CHECK(pkix_CertSelector_Select
                        (selector, unfilteredCerts, &filteredCerts, plContext),
                        PKIX_CERTSELECTORSELECTFAILED);
        }

        *pNBIOContext = NULL;
        *pCertList = filteredCerts;

cleanup:

        PKIX_DECREF(responses);
        PKIX_DECREF(unfilteredCerts);
        PKIX_DECREF(lcs);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_LdapCertStore_GetCRL
 *  (see description of PKIX_CertStore_CRLCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_LdapCertStore_GetCRL(
        PKIX_CertStore *store,
        PKIX_CRLSelector *selector,
        void **pNBIOContext,
        PKIX_List **pCrlList,
        void *plContext)
{
        LDAPRequestParams requestParams;
        void *pollDesc = NULL;
        PLArenaPool *requestArena = NULL;
        PKIX_UInt32 numNames = 0;
        PKIX_UInt32 thisName = 0;
        PKIX_PL_CRL *candidate = NULL;
        PKIX_List *responses = NULL;
        PKIX_List *issuerNames = NULL;
        PKIX_List *filteredCRLs = NULL;
        PKIX_List *unfilteredCRLs = NULL;
        PKIX_PL_X500Name *issuer = NULL;
        PKIX_PL_LdapCertStoreContext *lcs = NULL;
        PKIX_ComCRLSelParams *params = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_GetCRL");
        PKIX_NULLCHECK_THREE(store, selector, pCrlList);

        requestParams.baseObject = "c=US";
        requestParams.scope = WHOLE_SUBTREE;
        requestParams.derefAliases = NEVER_DEREF;
        requestParams.sizeLimit = 0;
        requestParams.timeLimit = 0;
        requestParams.attributes = LDAPATTR_CERTREVLIST | LDAPATTR_AUTHREVLIST;
        /* Prepare elements for request filter */

        /* XXX Place CRLDP code here. Handle the case when */
        /* RFC 5280. Paragraph: 4.2.1.13: */
        /* If the distributionPoint field contains a directoryName, the entry */
        /* for that directoryName contains the current CRL for the associated */
        /* reasons and the CRL is issued by the associated cRLIssuer.  The CRL */
        /* may be stored in either the certificateRevocationList or */
        /* authorityRevocationList attribute.  The CRL is to be obtained by the */
        /* application from whatever directory server is locally configured. */
        /* The protocol the application uses to access the directory (e.g., DAP */
        /* or LDAP) is a local matter. */



        /*
         * Get a short-lived arena. We'll be done with this space once
         * the request is encoded.
         */
        PKIX_PL_NSSCALLRV
            (CERTSTORE, requestArena, PORT_NewArena, (DER_DEFAULT_CHUNKSIZE));

        if (!requestArena) {
                PKIX_ERROR_FATAL(PKIX_OUTOFMEMORY);
        }

        PKIX_CHECK(PKIX_CRLSelector_GetCommonCRLSelectorParams
                (selector, &params, plContext),
                PKIX_CRLSELECTORGETCOMCERTSELPARAMSFAILED);

        PKIX_CHECK(PKIX_ComCRLSelParams_GetIssuerNames
                (params, &issuerNames, plContext),
                PKIX_COMCRLSELPARAMSGETISSUERNAMESFAILED);

        /*
         * The specification for PKIX_ComCRLSelParams_GetIssuerNames in
         * pkix_crlsel.h says that if the criterion is not set we get a null
         * pointer. If we get an empty List the criterion is impossible to
         * meet ("must match at least one of the names in the List").
         */
        if (issuerNames) {

                PKIX_CHECK(PKIX_List_GetLength
                        (issuerNames, &numNames, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                if (numNames > 0) {
                        for (thisName = 0; thisName < numNames; thisName++) {
                                PKIX_CHECK(PKIX_List_GetItem
                                (issuerNames,
                                thisName,
                                (PKIX_PL_Object **)&issuer,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                                PKIX_CHECK
                                        (pkix_pl_LdapCertStore_MakeNameAVAList
                                        (requestArena,
                                        issuer,
                                        &(requestParams.nc),
                                        plContext),
                                        PKIX_LDAPCERTSTOREMAKENAMEAVALISTFAILED);

                                PKIX_DECREF(issuer);

                                if (*requestParams.nc == NULL) {
                                        /*
                                         * The issuer may not include any
                                         * components that we know how to
                                         * encode. We do not return an error,
                                         * because the caller did not
                                         * necessarily do anything wrong, but
                                         * we return an empty List.
                                         */
                                        PKIX_PL_NSSCALL
                                                (CERTSTORE, PORT_FreeArena,
                                                (requestArena, PR_FALSE));

                                        PKIX_CHECK(PKIX_List_Create
                                                (&filteredCRLs, plContext),
                                                PKIX_LISTCREATEFAILED);

                                        PKIX_CHECK(PKIX_List_SetImmutable
                                                (filteredCRLs, plContext),
                                               PKIX_LISTSETIMMUTABLEFAILED);

                                        *pNBIOContext = NULL;
                                        *pCrlList = filteredCRLs;
                                        goto cleanup;
                                }

                                /*
                                 * LDAP Servers don't seem to be able to handle
                                 * requests with more than more than one name.
                                 */
                                break;
                        }
                } else {
                        PKIX_ERROR(PKIX_IMPOSSIBLECRITERIONFORCRLQUERY);
                }
        } else {
                PKIX_ERROR(PKIX_IMPOSSIBLECRITERIONFORCRLQUERY);
        }

        /* All request fields are done */

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                (store, (PKIX_PL_Object **)&lcs, plContext),
                PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        PKIX_CHECK(PKIX_PL_LdapClient_InitiateRequest
                ((PKIX_PL_LdapClient *)lcs,
                &requestParams,
                &pollDesc,
                &responses,
                plContext),
                PKIX_LDAPCLIENTINITIATEREQUESTFAILED);

        PKIX_CHECK(pkix_pl_LdapCertStore_DestroyAVAList
                (requestParams.nc, plContext),
                PKIX_LDAPCERTSTOREDESTROYAVALISTFAILED);

        if (requestArena) {
                PKIX_PL_NSSCALL(CERTSTORE, PORT_FreeArena,
                        (requestArena, PR_FALSE));
        }

        if (pollDesc != NULL) {
                /* client is waiting for non-blocking I/O to complete */
                *pNBIOContext = (void *)pollDesc;
                *pCrlList = NULL;
                goto cleanup;
        }
        /* client has finished! */

        if (responses) {

                /*
                 * We have a List of LdapResponse objects that still have to be
                 * turned into Crls.
                 */
                PKIX_CHECK(pkix_pl_LdapCertStore_BuildCrlList
                        (responses, &unfilteredCRLs, plContext),
                        PKIX_LDAPCERTSTOREBUILDCRLLISTFAILED);

                PKIX_CHECK(pkix_CRLSelector_Select
                        (selector, unfilteredCRLs, &filteredCRLs, plContext),
                        PKIX_CRLSELECTORSELECTFAILED);

        }

        /* Don't throw away the list if one CRL was bad! */
        pkixTempErrorReceived = PKIX_FALSE;

        *pNBIOContext = NULL;
        *pCrlList = filteredCRLs;

cleanup:

        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(filteredCRLs);
        }

        PKIX_DECREF(params);
        PKIX_DECREF(issuerNames);
        PKIX_DECREF(issuer);
        PKIX_DECREF(candidate);
        PKIX_DECREF(responses);
        PKIX_DECREF(unfilteredCRLs);
        PKIX_DECREF(lcs);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_LdapCertStore_GetCRLContinue
 *  (see description of PKIX_CertStore_CRLCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_LdapCertStore_GetCRLContinue(
        PKIX_CertStore *store,
        PKIX_CRLSelector *selector,
        void **pNBIOContext,
        PKIX_List **pCrlList,
        void *plContext)
{
        void *nbio = NULL;
        PKIX_PL_CRL *candidate = NULL;
        PKIX_List *responses = NULL;
        PKIX_PL_LdapCertStoreContext *lcs = NULL;
        PKIX_List *filteredCRLs = NULL;
        PKIX_List *unfilteredCRLs = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapCertStore_GetCRLContinue");
        PKIX_NULLCHECK_FOUR(store, selector, pNBIOContext, pCrlList);

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                (store, (PKIX_PL_Object **)&lcs, plContext),
                PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        PKIX_CHECK(PKIX_PL_LdapClient_ResumeRequest
                ((PKIX_PL_LdapClient *)lcs, &nbio, &responses, plContext),
                PKIX_LDAPCLIENTRESUMEREQUESTFAILED);

        if (nbio != NULL) {
                /* client is waiting for non-blocking I/O to complete */
                *pNBIOContext = (void *)nbio;
                *pCrlList = NULL;
                goto cleanup;
        }
        /* client has finished! */

        if (responses) {

                /*
                 * We have a List of LdapResponse objects that still have to be
                 * turned into Crls.
                 */
                PKIX_CHECK(pkix_pl_LdapCertStore_BuildCrlList
                        (responses, &unfilteredCRLs, plContext),
                        PKIX_LDAPCERTSTOREBUILDCRLLISTFAILED);

                PKIX_CHECK(pkix_CRLSelector_Select
                        (selector, unfilteredCRLs, &filteredCRLs, plContext),
                        PKIX_CRLSELECTORSELECTFAILED);

                PKIX_CHECK(PKIX_List_SetImmutable(filteredCRLs, plContext),
                        PKIX_LISTSETIMMUTABLEFAILED);

        }

        /* Don't throw away the list if one CRL was bad! */
        pkixTempErrorReceived = PKIX_FALSE;

        *pCrlList = filteredCRLs;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(filteredCRLs);
        }

        PKIX_DECREF(candidate);
        PKIX_DECREF(responses);
        PKIX_DECREF(unfilteredCRLs);
        PKIX_DECREF(lcs);

        PKIX_RETURN(CERTSTORE);
}

/* --Public-LdapCertStore-Functions----------------------------------- */

/*
 * FUNCTION: PKIX_PL_LdapCertStore_Create
 * (see comments in pkix_samples_modules.h)
 */
PKIX_Error *
PKIX_PL_LdapCertStore_Create(
        PKIX_PL_LdapClient *client,
        PKIX_CertStore **pCertStore,
        void *plContext)
{
        PKIX_CertStore *certStore = NULL;

        PKIX_ENTER(CERTSTORE, "PKIX_PL_LdapCertStore_Create");
        PKIX_NULLCHECK_TWO(client, pCertStore);

        PKIX_CHECK(PKIX_CertStore_Create
                (pkix_pl_LdapCertStore_GetCert,
                pkix_pl_LdapCertStore_GetCRL,
                pkix_pl_LdapCertStore_GetCertContinue,
                pkix_pl_LdapCertStore_GetCRLContinue,
                NULL,       /* don't support trust */
                NULL,      /* can not store crls */
                NULL,      /* can not do revocation check */
                (PKIX_PL_Object *)client,
                PKIX_TRUE,  /* cache flag */
                PKIX_FALSE, /* not local */
                &certStore,
                plContext),
                PKIX_CERTSTORECREATEFAILED);

        *pCertStore = certStore;

cleanup:

        PKIX_RETURN(CERTSTORE);
}
