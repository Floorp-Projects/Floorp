/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * nss_pkix_proxy.h
 *
 * PKIX - NSS proxy functions
 *
 * NOTE: All structures, functions, data types are parts of library private
 * api and are subjects to change in any following releases.
 *
 */
#include "prerror.h"
#include "prprf.h"

#include "nspr.h"
#include "pk11func.h"
#include "certdb.h"
#include "cert.h"
#include "secerr.h"
#include "nssb64.h"
#include "secasn1.h"
#include "secder.h"
#include "pkit.h"

#ifndef NSS_DISABLE_LIBPKIX
#include "pkix_pl_common.h"

extern PRLogModuleInfo *pkixLog;

#ifdef PKIX_OBJECT_LEAK_TEST

extern PKIX_UInt32
pkix_pl_lifecycle_ObjectLeakCheck(int *);

extern SECStatus
pkix_pl_lifecycle_ObjectTableUpdate(int *objCountTable);

PRInt32 parallelFnInvocationCount;
#endif /* PKIX_OBJECT_LEAK_TEST */

static PRBool usePKIXValidationEngine = PR_FALSE;
#endif /* NSS_DISABLE_LIBPKIX */

/*
 * FUNCTION: CERT_SetUsePKIXForValidation
 * DESCRIPTION:
 *
 * Enables or disables use of libpkix for certificate validation
 *
 * PARAMETERS:
 *  "enable"
 *      PR_TRUE: enables use of libpkix for cert validation.
 *      PR_FALSE: disables.
 * THREAD SAFETY:
 *  NOT Thread Safe.
 * RETURNS:
 *  Returns SECSuccess if successfully enabled
 */
SECStatus
CERT_SetUsePKIXForValidation(PRBool enable)
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
#else
    usePKIXValidationEngine = (enable > 0) ? PR_TRUE : PR_FALSE;
    return SECSuccess;
#endif /* NSS_DISABLE_LIBPKIX */
}

/*
 * FUNCTION: CERT_GetUsePKIXForValidation
 * DESCRIPTION:
 *
 * Checks if libpkix building function should be use for certificate
 * chain building.
 *
 * PARAMETERS:
 *  NONE
 * THREAD SAFETY:
 *  NOT Thread Safe
 * RETURNS:
 *  Returns PR_TRUE if libpkix should be used. PR_FALSE otherwise.
 */
PRBool
CERT_GetUsePKIXForValidation()
{
#ifdef NSS_DISABLE_LIBPKIX
    return PR_FALSE;
#else
    return usePKIXValidationEngine;
#endif /* NSS_DISABLE_LIBPKIX */
}

#ifndef NSS_DISABLE_LIBPKIX
#ifdef NOTDEF
/*
 * FUNCTION: cert_NssKeyUsagesToPkix
 * DESCRIPTION:
 *
 * Converts nss key usage bit field(PRUint32) to pkix key usage
 * bit field.
 *
 * PARAMETERS:
 *  "nssKeyUsage"
 *      Nss key usage bit field.
 *  "pkixKeyUsage"
 *      Pkix key usage big field.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_NssKeyUsagesToPkix(
    PRUint32 nssKeyUsage,
    PKIX_UInt32 *pPkixKeyUsage,
    void *plContext)
{
    PKIX_UInt32 pkixKeyUsage = 0;

    PKIX_ENTER(CERTVFYPKIX, "cert_NssKeyUsagesToPkix");
    PKIX_NULLCHECK_ONE(pPkixKeyUsage);

    *pPkixKeyUsage = 0;

    if (nssKeyUsage & KU_DIGITAL_SIGNATURE) {
        pkixKeyUsage |= PKIX_DIGITAL_SIGNATURE;
    }

    if (nssKeyUsage & KU_NON_REPUDIATION) {
        pkixKeyUsage |= PKIX_NON_REPUDIATION;
    }

    if (nssKeyUsage & KU_KEY_ENCIPHERMENT) {
        pkixKeyUsage |= PKIX_KEY_ENCIPHERMENT;
    }

    if (nssKeyUsage & KU_DATA_ENCIPHERMENT) {
        pkixKeyUsage |= PKIX_DATA_ENCIPHERMENT;
    }

    if (nssKeyUsage & KU_KEY_AGREEMENT) {
        pkixKeyUsage |= PKIX_KEY_AGREEMENT;
    }

    if (nssKeyUsage & KU_KEY_CERT_SIGN) {
        pkixKeyUsage |= PKIX_KEY_CERT_SIGN;
    }

    if (nssKeyUsage & KU_CRL_SIGN) {
        pkixKeyUsage |= PKIX_CRL_SIGN;
    }

    if (nssKeyUsage & KU_ENCIPHER_ONLY) {
        pkixKeyUsage |= PKIX_ENCIPHER_ONLY;
    }

    /* Not supported. XXX we should support this once it is
     * fixed in NSS */
    /* pkixKeyUsage |= PKIX_DECIPHER_ONLY; */

    *pPkixKeyUsage = pkixKeyUsage;

    PKIX_RETURN(CERTVFYPKIX);
}

extern SECOidTag ekuOidStrings[];

enum {
    ekuIndexSSLServer = 0,
    ekuIndexSSLClient,
    ekuIndexCodeSigner,
    ekuIndexEmail,
    ekuIndexTimeStamp,
    ekuIndexStatusResponder,
    ekuIndexUnknown
} ekuIndex;

typedef struct {
    SECCertUsage certUsage;
    PRUint32 ekuStringIndex;
} SECCertUsageToEku;

const SECCertUsageToEku certUsageEkuStringMap[] = {
    { certUsageSSLClient, ekuIndexSSLClient },
    { certUsageSSLServer, ekuIndexSSLServer },
    { certUsageSSLCA, ekuIndexSSLServer },
    { certUsageEmailSigner, ekuIndexEmail },
    { certUsageEmailRecipient, ekuIndexEmail },
    { certUsageObjectSigner, ekuIndexCodeSigner },
    { certUsageUserCertImport, ekuIndexUnknown },
    { certUsageVerifyCA, ekuIndexUnknown },
    { certUsageProtectedObjectSigner, ekuIndexUnknown },
    { certUsageStatusResponder, ekuIndexStatusResponder },
    { certUsageAnyCA, ekuIndexUnknown },
};

/*
 * FUNCTION: cert_NssCertificateUsageToPkixKUAndEKU
 * DESCRIPTION:
 *
 * Converts nss CERTCertificateUsage bit field to pkix key and
 * extended key usages.
 *
 * PARAMETERS:
 *  "cert"
 *      Pointer to CERTCertificate structure of validating cert.
 *  "requiredCertUsages"
 *      Required usage that will be converted to pkix eku and ku.
 *  "requiredKeyUsage",
 *      Additional key usages impose to cert.
 *  "isCA",
 *      it true, convert usages for cert that is a CA cert.
 *  "ppkixEKUList"
 *      Returned address of a list of pkix extended key usages.
 *  "ppkixKU"
 *      Returned address of pkix required key usages bit field.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Verify Error if the function fails in an unrecoverable way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_NssCertificateUsageToPkixKUAndEKU(
    CERTCertificate *cert,
    SECCertUsage requiredCertUsage,
    PRUint32 requiredKeyUsages,
    PRBool isCA,
    PKIX_List **ppkixEKUList,
    PKIX_UInt32 *ppkixKU,
    void *plContext)
{
    PKIX_List *ekuOidsList = NULL;
    PKIX_PL_OID *ekuOid = NULL;
    int i = 0;
    int ekuIndex = ekuIndexUnknown;

    PKIX_ENTER(CERTVFYPKIX, "cert_NssCertificateUsageToPkixEku");
    PKIX_NULLCHECK_TWO(ppkixEKUList, ppkixKU);

    PKIX_CHECK(
        PKIX_List_Create(&ekuOidsList, plContext),
        PKIX_LISTCREATEFAILED);

    for (; i < PR_ARRAY_SIZE(certUsageEkuStringMap); i++) {
        const SECCertUsageToEku *usageToEkuElem =
            &certUsageEkuStringMap[i];
        if (usageToEkuElem->certUsage == requiredCertUsage) {
            ekuIndex = usageToEkuElem->ekuStringIndex;
            break;
        }
    }
    if (ekuIndex != ekuIndexUnknown) {
        PRUint32 reqKeyUsage = 0;
        PRUint32 reqCertType = 0;

        CERT_KeyUsageAndTypeForCertUsage(requiredCertUsage, isCA,
                                         &reqKeyUsage,
                                         &reqCertType);

        requiredKeyUsages |= reqKeyUsage;

        PKIX_CHECK(
            PKIX_PL_OID_Create(ekuOidStrings[ekuIndex], &ekuOid,
                               plContext),
            PKIX_OIDCREATEFAILED);

        PKIX_CHECK(
            PKIX_List_AppendItem(ekuOidsList, (PKIX_PL_Object *)ekuOid,
                                 plContext),
            PKIX_LISTAPPENDITEMFAILED);

        PKIX_DECREF(ekuOid);
    }

    PKIX_CHECK(
        cert_NssKeyUsagesToPkix(requiredKeyUsages, ppkixKU, plContext),
        PKIX_NSSCERTIFICATEUSAGETOPKIXKUANDEKUFAILED);

    *ppkixEKUList = ekuOidsList;
    ekuOidsList = NULL;

cleanup:

    PKIX_DECREF(ekuOid);
    PKIX_DECREF(ekuOidsList);

    PKIX_RETURN(CERTVFYPKIX);
}

#endif

/*
 * FUNCTION: cert_ProcessingParamsSetKeyAndCertUsage
 * DESCRIPTION:
 *
 * Converts cert usage to pkix KU type and sets
 * converted data into PKIX_ProcessingParams object. It also sets
 * proper cert usage into nsscontext object.
 *
 * PARAMETERS:
 *  "procParams"
 *      Pointer to PKIX_ProcessingParams used during validation.
 *  "requiredCertUsage"
 *      Required certificate usages the certificate and chain is built and
 *      validated for.
 *  "requiredKeyUsage"
 *      Request additional key usages the certificate should be validated for.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Verify Error if the function fails in an unrecoverable way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_ProcessingParamsSetKeyAndCertUsage(
    PKIX_ProcessingParams *procParams,
    SECCertUsage requiredCertUsage,
    PRUint32 requiredKeyUsages,
    void *plContext)
{
    PKIX_CertSelector *certSelector = NULL;
    PKIX_ComCertSelParams *certSelParams = NULL;
    PKIX_PL_NssContext *nssContext = (PKIX_PL_NssContext *)plContext;

    PKIX_ENTER(CERTVFYPKIX, "cert_ProcessingParamsSetKeyAndCertUsage");
    PKIX_NULLCHECK_TWO(procParams, nssContext);

    PKIX_CHECK(
        pkix_pl_NssContext_SetCertUsage(
            ((SECCertificateUsage)1) << requiredCertUsage, nssContext),
        PKIX_NSSCONTEXTSETCERTUSAGEFAILED);

    if (requiredKeyUsages) {
        PKIX_CHECK(
            PKIX_ProcessingParams_GetTargetCertConstraints(procParams,
                                                           &certSelector, plContext),
            PKIX_PROCESSINGPARAMSGETTARGETCERTCONSTRAINTSFAILED);

        PKIX_CHECK(
            PKIX_CertSelector_GetCommonCertSelectorParams(certSelector,
                                                          &certSelParams, plContext),
            PKIX_CERTSELECTORGETCOMMONCERTSELECTORPARAMSFAILED);

        PKIX_CHECK(
            PKIX_ComCertSelParams_SetKeyUsage(certSelParams, requiredKeyUsages,
                                              plContext),
            PKIX_COMCERTSELPARAMSSETKEYUSAGEFAILED);
    }
cleanup:
    PKIX_DECREF(certSelector);
    PKIX_DECREF(certSelParams);

    PKIX_RETURN(CERTVFYPKIX);
}

/*
 * Unused parameters:
 *
 *  CERTCertList *initialChain,
 *  CERTCertStores certStores,
 *  CERTCertRevCheckers certRevCheckers,
 *  CERTCertChainCheckers certChainCheckers,
 *  SECItem *initPolicies,
 *  PRBool policyQualifierRejected,
 *  PRBool anyPolicyInhibited,
 *  PRBool reqExplicitPolicy,
 *  PRBool policyMappingInhibited,
 *  PKIX_CertSelector certConstraints,
 */

/*
 * FUNCTION: cert_CreatePkixProcessingParams
 * DESCRIPTION:
 *
 * Creates and fills in PKIX_ProcessingParams structure to be used
 * for certificate chain building.
 *
 * PARAMETERS:
 *  "cert"
 *      Pointer to the CERTCertificate: the leaf certificate of a chain.
 *  "time"
 *      Validity time.
 *  "wincx"
 *      Nss db password token.
 *  "useArena"
 *      Flags to use arena for data allocation during chain building process.
 *  "pprocParams"
 *      Address to return created processing parameters.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Verify Error if the function fails in an unrecoverable way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_CreatePkixProcessingParams(
    CERTCertificate *cert,
    PRBool checkSig,
    PRTime time,
    void *wincx,
    PRBool useArena,
    PRBool disableOCSPRemoteFetching,
    PKIX_ProcessingParams **pprocParams,
    void **pplContext)
{
    PKIX_List *anchors = NULL;
    PKIX_PL_Cert *targetCert = NULL;
    PKIX_PL_Date *date = NULL;
    PKIX_ProcessingParams *procParams = NULL;
    PKIX_CertSelector *certSelector = NULL;
    PKIX_ComCertSelParams *certSelParams = NULL;
    PKIX_CertStore *certStore = NULL;
    PKIX_List *certStores = NULL;
    PKIX_RevocationChecker *revChecker = NULL;
    PKIX_UInt32 methodFlags = 0;
    void *plContext = NULL;
    CERTStatusConfig *statusConfig = NULL;

    PKIX_ENTER(CERTVFYPKIX, "cert_CreatePkixProcessingParams");
    PKIX_NULLCHECK_TWO(cert, pprocParams);

    PKIX_CHECK(
        PKIX_PL_NssContext_Create(0, useArena, wincx, &plContext),
        PKIX_NSSCONTEXTCREATEFAILED);

    *pplContext = plContext;

    /* Functions should be implemented in patch for 390532 */
    PKIX_CHECK(
        pkix_pl_NssContext_SetCertSignatureCheck(checkSig,
                                                 (PKIX_PL_NssContext *)plContext),
        PKIX_NSSCONTEXTSETCERTSIGNCHECKFAILED);

    PKIX_CHECK(
        PKIX_ProcessingParams_Create(&procParams, plContext),
        PKIX_PROCESSINGPARAMSCREATEFAILED);

    PKIX_CHECK(
        PKIX_ComCertSelParams_Create(&certSelParams, plContext),
        PKIX_COMCERTSELPARAMSCREATEFAILED);

    PKIX_CHECK(
        PKIX_PL_Cert_CreateFromCERTCertificate(cert, &targetCert, plContext),
        PKIX_CERTCREATEWITHNSSCERTFAILED);

    PKIX_CHECK(
        PKIX_ComCertSelParams_SetCertificate(certSelParams,
                                             targetCert, plContext),
        PKIX_COMCERTSELPARAMSSETCERTIFICATEFAILED);

    PKIX_CHECK(
        PKIX_CertSelector_Create(NULL, NULL, &certSelector, plContext),
        PKIX_COULDNOTCREATECERTSELECTOROBJECT);

    PKIX_CHECK(
        PKIX_CertSelector_SetCommonCertSelectorParams(certSelector,
                                                      certSelParams, plContext),
        PKIX_CERTSELECTORSETCOMMONCERTSELECTORPARAMSFAILED);

    PKIX_CHECK(
        PKIX_ProcessingParams_SetTargetCertConstraints(procParams,
                                                       certSelector, plContext),
        PKIX_PROCESSINGPARAMSSETTARGETCERTCONSTRAINTSFAILED);

    /* Turn off quialification of target cert since leaf cert is
     * already check for date validity, key usages and extended
     * key usages. */
    PKIX_CHECK(
        PKIX_ProcessingParams_SetQualifyTargetCert(procParams, PKIX_FALSE,
                                                   plContext),
        PKIX_PROCESSINGPARAMSSETQUALIFYTARGETCERTFLAGFAILED);

    PKIX_CHECK(
        PKIX_PL_Pk11CertStore_Create(&certStore, plContext),
        PKIX_PK11CERTSTORECREATEFAILED);

    PKIX_CHECK(
        PKIX_List_Create(&certStores, plContext),
        PKIX_UNABLETOCREATELIST);

    PKIX_CHECK(
        PKIX_List_AppendItem(certStores, (PKIX_PL_Object *)certStore,
                             plContext),
        PKIX_LISTAPPENDITEMFAILED);

    PKIX_CHECK(
        PKIX_ProcessingParams_SetCertStores(procParams, certStores,
                                            plContext),
        PKIX_PROCESSINGPARAMSADDCERTSTOREFAILED);

    PKIX_CHECK(
        PKIX_PL_Date_CreateFromPRTime(time, &date, plContext),
        PKIX_DATECREATEFROMPRTIMEFAILED);

    PKIX_CHECK(
        PKIX_ProcessingParams_SetDate(procParams, date, plContext),
        PKIX_PROCESSINGPARAMSSETDATEFAILED);

    PKIX_CHECK(
        PKIX_RevocationChecker_Create(
            PKIX_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST |
                PKIX_REV_MI_NO_OVERALL_INFO_REQUIREMENT,
            PKIX_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST |
                PKIX_REV_MI_NO_OVERALL_INFO_REQUIREMENT,
            &revChecker, plContext),
        PKIX_REVOCATIONCHECKERCREATEFAILED);

    PKIX_CHECK(
        PKIX_ProcessingParams_SetRevocationChecker(procParams, revChecker,
                                                   plContext),
        PKIX_PROCESSINGPARAMSSETREVOCATIONCHECKERFAILED);

    /* CRL method flags */
    methodFlags =
        PKIX_REV_M_TEST_USING_THIS_METHOD |
        PKIX_REV_M_FORBID_NETWORK_FETCHING |
        PKIX_REV_M_SKIP_TEST_ON_MISSING_SOURCE | /* 0 */
        PKIX_REV_M_IGNORE_MISSING_FRESH_INFO |   /* 0 */
        PKIX_REV_M_CONTINUE_TESTING_ON_FRESH_INFO;

    /* add CRL revocation method to check the leaf certificate */
    PKIX_CHECK(
        PKIX_RevocationChecker_CreateAndAddMethod(revChecker, procParams,
                                                  PKIX_RevocationMethod_CRL, methodFlags,
                                                  0, NULL, PKIX_TRUE, plContext),
        PKIX_REVOCATIONCHECKERADDMETHODFAILED);

    /* add CRL revocation method for other certs in the chain. */
    PKIX_CHECK(
        PKIX_RevocationChecker_CreateAndAddMethod(revChecker, procParams,
                                                  PKIX_RevocationMethod_CRL, methodFlags,
                                                  0, NULL, PKIX_FALSE, plContext),
        PKIX_REVOCATIONCHECKERADDMETHODFAILED);

    /* For compatibility with the old code, need to check that
     * statusConfig is set in the db handle and status checker
     * is defined befor allow ocsp status check on the leaf cert.*/
    statusConfig = CERT_GetStatusConfig(CERT_GetDefaultCertDB());
    if (statusConfig != NULL && statusConfig->statusChecker != NULL) {

        /* Enable OCSP revocation checking for the leaf cert. */
        /* OCSP method flags */
        methodFlags =
            PKIX_REV_M_TEST_USING_THIS_METHOD |
            PKIX_REV_M_ALLOW_NETWORK_FETCHING |        /* 0 */
            PKIX_REV_M_ALLOW_IMPLICIT_DEFAULT_SOURCE | /* 0 */
            PKIX_REV_M_SKIP_TEST_ON_MISSING_SOURCE |   /* 0 */
            PKIX_REV_M_IGNORE_MISSING_FRESH_INFO |     /* 0 */
            PKIX_REV_M_CONTINUE_TESTING_ON_FRESH_INFO;

        /* Disabling ocsp fetching when checking the status
         * of ocsp response signer. Here and in the next if,
         * adjust flags for ocsp signer cert validation case. */
        if (disableOCSPRemoteFetching) {
            methodFlags |= PKIX_REV_M_FORBID_NETWORK_FETCHING;
        }

        if (ocsp_FetchingFailureIsVerificationFailure() &&
            !disableOCSPRemoteFetching) {
            methodFlags |=
                PKIX_REV_M_FAIL_ON_MISSING_FRESH_INFO;
        }

        /* add OCSP revocation method to check only the leaf certificate.*/
        PKIX_CHECK(
            PKIX_RevocationChecker_CreateAndAddMethod(revChecker, procParams,
                                                      PKIX_RevocationMethod_OCSP, methodFlags,
                                                      1, NULL, PKIX_TRUE, plContext),
            PKIX_REVOCATIONCHECKERADDMETHODFAILED);
    }

    PKIX_CHECK(
        PKIX_ProcessingParams_SetAnyPolicyInhibited(procParams, PR_FALSE,
                                                    plContext),
        PKIX_PROCESSINGPARAMSSETANYPOLICYINHIBITED);

    PKIX_CHECK(
        PKIX_ProcessingParams_SetExplicitPolicyRequired(procParams, PR_FALSE,
                                                        plContext),
        PKIX_PROCESSINGPARAMSSETEXPLICITPOLICYREQUIRED);

    PKIX_CHECK(
        PKIX_ProcessingParams_SetPolicyMappingInhibited(procParams, PR_FALSE,
                                                        plContext),
        PKIX_PROCESSINGPARAMSSETPOLICYMAPPINGINHIBITED);

    *pprocParams = procParams;
    procParams = NULL;

cleanup:
    PKIX_DECREF(anchors);
    PKIX_DECREF(targetCert);
    PKIX_DECREF(date);
    PKIX_DECREF(certSelector);
    PKIX_DECREF(certSelParams);
    PKIX_DECREF(certStore);
    PKIX_DECREF(certStores);
    PKIX_DECREF(procParams);
    PKIX_DECREF(revChecker);

    PKIX_RETURN(CERTVFYPKIX);
}

/*
 * FUNCTION: cert_PkixToNssCertsChain
 * DESCRIPTION:
 *
 * Converts pkix cert list into nss cert list.
 *
 * PARAMETERS:
 *  "pkixCertChain"
 *      Pkix certificate list.
 *  "pvalidChain"
 *      An address of returned nss certificate list.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Verify Error if the function fails in an unrecoverable way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_PkixToNssCertsChain(
    PKIX_List *pkixCertChain,
    CERTCertList **pvalidChain,
    void *plContext)
{
    PLArenaPool *arena = NULL;
    CERTCertificate *nssCert = NULL;
    CERTCertList *validChain = NULL;
    PKIX_PL_Object *certItem = NULL;
    PKIX_UInt32 length = 0;
    PKIX_UInt32 i = 0;

    PKIX_ENTER(CERTVFYPKIX, "cert_PkixToNssCertsChain");
    PKIX_NULLCHECK_ONE(pvalidChain);

    if (pkixCertChain == NULL) {
        goto cleanup;
    }
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        PKIX_ERROR(PKIX_OUTOFMEMORY);
    }
    validChain = (CERTCertList *)PORT_ArenaZAlloc(arena, sizeof(CERTCertList));
    if (validChain == NULL) {
        PKIX_ERROR(PKIX_PORTARENAALLOCFAILED);
    }
    PR_INIT_CLIST(&validChain->list);
    validChain->arena = arena;
    arena = NULL;

    PKIX_CHECK(
        PKIX_List_GetLength(pkixCertChain, &length, plContext),
        PKIX_LISTGETLENGTHFAILED);

    for (i = 0; i < length; i++) {
        CERTCertListNode *node = NULL;

        PKIX_CHECK(
            PKIX_List_GetItem(pkixCertChain, i, &certItem, plContext),
            PKIX_LISTGETITEMFAILED);

        PKIX_CHECK(
            PKIX_PL_Cert_GetCERTCertificate((PKIX_PL_Cert *)certItem, &nssCert,
                                            plContext),
            PKIX_CERTGETCERTCERTIFICATEFAILED);

        node =
            (CERTCertListNode *)PORT_ArenaZAlloc(validChain->arena,
                                                 sizeof(CERTCertListNode));
        if (node == NULL) {
            PKIX_ERROR(PKIX_PORTARENAALLOCFAILED);
        }

        PR_INSERT_BEFORE(&node->links, &validChain->list);

        node->cert = nssCert;
        nssCert = NULL;

        PKIX_DECREF(certItem);
    }

    *pvalidChain = validChain;

cleanup:
    if (PKIX_ERROR_RECEIVED) {
        if (validChain) {
            CERT_DestroyCertList(validChain);
        } else if (arena) {
            PORT_FreeArena(arena, PR_FALSE);
        }
        if (nssCert) {
            CERT_DestroyCertificate(nssCert);
        }
    }
    PKIX_DECREF(certItem);

    PKIX_RETURN(CERTVFYPKIX);
}

/*
 * FUNCTION: cert_BuildAndValidateChain
 * DESCRIPTION:
 *
 * The function builds and validates a cert chain based on certificate
 * selection criterias from procParams. This function call PKIX_BuildChain
 * to accomplish chain building. If PKIX_BuildChain returns with incomplete
 * IO, the function waits with PR_Poll until the blocking IO is finished and
 * return control back to PKIX_BuildChain.
 *
 * PARAMETERS:
 *  "procParams"
 *      Processing parameters to be used during chain building.
 *  "pResult"
 *      Returned build result.
 *  "pVerifyNode"
 *      Returned pointed to verify node structure: the tree-like structure
 *      that reports points of chain building failures.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Verify Error if the function fails in an unrecoverable way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_BuildAndValidateChain(
    PKIX_ProcessingParams *procParams,
    PKIX_BuildResult **pResult,
    PKIX_VerifyNode **pVerifyNode,
    void *plContext)
{
    PKIX_BuildResult *result = NULL;
    PKIX_VerifyNode *verifyNode = NULL;
    void *nbioContext = NULL;
    void *state = NULL;

    PKIX_ENTER(CERTVFYPKIX, "cert_BuildAndVerifyChain");
    PKIX_NULLCHECK_TWO(procParams, pResult);

    do {
        if (nbioContext && state) {
            /* PKIX-XXX: need to test functionality of NBIO handling in libPkix.
             * See bug 391180 */
            PRInt32 filesReady = 0;
            PRPollDesc *pollDesc = (PRPollDesc *)nbioContext;
            filesReady = PR_Poll(pollDesc, 1, PR_INTERVAL_NO_TIMEOUT);
            if (filesReady <= 0) {
                PKIX_ERROR(PKIX_PRPOLLRETBADFILENUM);
            }
        }

        PKIX_CHECK(
            PKIX_BuildChain(procParams, &nbioContext, &state,
                            &result, &verifyNode, plContext),
            PKIX_UNABLETOBUILDCHAIN);

    } while (nbioContext && state);

    *pResult = result;

cleanup:
    if (pVerifyNode) {
        *pVerifyNode = verifyNode;
    }

    PKIX_RETURN(CERTVFYPKIX);
}

/*
 * FUNCTION: cert_PkixErrorToNssCode
 * DESCRIPTION:
 *
 * Converts pkix error(PKIX_Error) structure to PR error codes.
 *
 * PKIX-XXX to be implemented. See 391183.
 *
 * PARAMETERS:
 *  "error"
 *      Pkix error that will be converted.
 *  "nssCode"
 *      Corresponding nss error code.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Verify Error if the function fails in an unrecoverable way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_PkixErrorToNssCode(
    PKIX_Error *error,
    SECErrorCodes *pNssErr,
    void *plContext)
{
    int errLevel = 0;
    (void)errLevel; /* Suppress unused var warning (Bug 1738028) */

    PKIX_Int32 nssErr = 0;
    PKIX_Error *errPtr = error;

    PKIX_ENTER(CERTVFYPKIX, "cert_PkixErrorToNssCode");
    PKIX_NULLCHECK_TWO(error, pNssErr);

    /* Loop until we find at least one error with non-null
     * plErr code, that is going to be nss error code. */
    while (errPtr) {
        if (errPtr->plErr && !nssErr) {
            nssErr = errPtr->plErr;
            if (!pkixLog)
                break;
        }
        if (pkixLog) {
#ifdef PKIX_ERROR_DESCRIPTION
            PR_LOG(pkixLog, 2, ("Error at level %d: %s\n", errLevel, PKIX_ErrorText[errPtr->errCode]));
#else
            PR_LOG(pkixLog, 2, ("Error at level %d: Error code %d\n", errLevel, errPtr->errCode));
#endif /* PKIX_ERROR_DESCRIPTION */
        }
        errPtr = errPtr->cause;
        errLevel += 1;
    }
    PORT_Assert(nssErr);
    if (!nssErr) {
        *pNssErr = SEC_ERROR_LIBPKIX_INTERNAL;
    } else {
        *pNssErr = nssErr;
    }

    PKIX_RETURN(CERTVFYPKIX);
}

/*
 * FUNCTION: cert_GetLogFromVerifyNode
 * DESCRIPTION:
 *
 * Recursive function that converts verify node tree-like set of structures
 * to CERTVerifyLog.
 *
 * PARAMETERS:
 *  "log"
 *      Pointed to already allocated CERTVerifyLog structure.
 *  "node"
 *      A node of PKIX_VerifyNode tree.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Verify Error if the function fails in an unrecoverable way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_GetLogFromVerifyNode(
    CERTVerifyLog *log,
    PKIX_VerifyNode *node,
    void *plContext)
{
    PKIX_List *children = NULL;
    PKIX_VerifyNode *childNode = NULL;

    PKIX_ENTER(CERTVFYPKIX, "cert_GetLogFromVerifyNode");

    children = node->children;

    if (children == NULL) {
        PKIX_ERRORCODE errCode = PKIX_ANCHORDIDNOTCHAINTOCERT;
        if (node->error && node->error->errCode != errCode) {
            if (log != NULL) {
                SECErrorCodes nssErrorCode = 0;
                CERTCertificate *cert = NULL;

                cert = node->verifyCert->nssCert;

                PKIX_CHECK(
                    cert_PkixErrorToNssCode(node->error, &nssErrorCode,
                                            plContext),
                    PKIX_GETPKIXERRORCODEFAILED);

                cert_AddToVerifyLog(log, cert, nssErrorCode, node->depth, NULL);
            }
        }
        PKIX_RETURN(CERTVFYPKIX);
    } else {
        PRUint32 i = 0;
        PKIX_UInt32 length = 0;

        PKIX_CHECK(
            PKIX_List_GetLength(children, &length, plContext),
            PKIX_LISTGETLENGTHFAILED);

        for (i = 0; i < length; i++) {

            PKIX_CHECK(
                PKIX_List_GetItem(children, i, (PKIX_PL_Object **)&childNode,
                                  plContext),
                PKIX_LISTGETITEMFAILED);

            PKIX_CHECK(
                cert_GetLogFromVerifyNode(log, childNode, plContext),
                PKIX_ERRORINRECURSIVEEQUALSCALL);

            PKIX_DECREF(childNode);
        }
    }

cleanup:
    PKIX_DECREF(childNode);

    PKIX_RETURN(CERTVFYPKIX);
}

/*
 * FUNCTION: cert_GetBuildResults
 * DESCRIPTION:
 *
 * Converts pkix build results to nss results. This function is called
 * regardless of build result.
 *
 * If it called after chain was successfully constructed, then it will
 * convert:
 *   * pkix cert list that represent the chain to nss cert list
 *   * trusted root the chain was anchored to nss certificate.
 *
 * In case of failure it will convert:
 *   * pkix error to PR error code(will set it with PORT_SetError)
 *   * pkix validation log to nss CERTVerifyLog
 *
 * PARAMETERS:
 *  "buildResult"
 *      Build results returned by PKIX_BuildChain.
 *  "verifyNode"
 *      Tree-like structure of chain building/validation failures
 *      returned by PKIX_BuildChain. Ignored in case of success.
 *  "error"
 *      Final error returned by PKIX_BuildChain. Should be NULL in
 *      case of success.
 *  "log"
 *      Address of pre-allocated(if not NULL) CERTVerifyLog structure.
 *  "ptrustedRoot"
 *      Address of returned trusted root the chain was anchored to.
 *  "pvalidChain"
 *      Address of returned valid chain.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Verify Error if the function fails in an unrecoverable way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
cert_GetBuildResults(
    PKIX_BuildResult *buildResult,
    PKIX_VerifyNode *verifyNode,
    PKIX_Error *error,
    CERTVerifyLog *log,
    CERTCertificate **ptrustedRoot,
    CERTCertList **pvalidChain,
    void *plContext)
{
    PKIX_ValidateResult *validResult = NULL;
    CERTCertList *validChain = NULL;
    CERTCertificate *trustedRoot = NULL;
    PKIX_TrustAnchor *trustAnchor = NULL;
    PKIX_PL_Cert *trustedCert = NULL;
    PKIX_List *pkixCertChain = NULL;

    PKIX_ENTER(CERTVFYPKIX, "cert_GetBuildResults");
    if (buildResult == NULL && error == NULL) {
        PKIX_ERROR(PKIX_NULLARGUMENT);
    }

    if (error) {
        SECErrorCodes nssErrorCode = 0;
        if (verifyNode) {
            PKIX_Error *tmpError =
                cert_GetLogFromVerifyNode(log, verifyNode, plContext);
            if (tmpError) {
                PKIX_PL_Object_DecRef((PKIX_PL_Object *)tmpError, plContext);
            }
        }
        cert_PkixErrorToNssCode(error, &nssErrorCode, plContext);
        PORT_SetError(nssErrorCode);
        goto cleanup;
    }

    if (pvalidChain) {
        PKIX_CHECK(
            PKIX_BuildResult_GetCertChain(buildResult, &pkixCertChain,
                                          plContext),
            PKIX_BUILDRESULTGETCERTCHAINFAILED);

        PKIX_CHECK(
            cert_PkixToNssCertsChain(pkixCertChain, &validChain, plContext),
            PKIX_CERTCHAINTONSSCHAINFAILED);
    }

    if (ptrustedRoot) {
        PKIX_CHECK(
            PKIX_BuildResult_GetValidateResult(buildResult, &validResult,
                                               plContext),
            PKIX_BUILDRESULTGETVALIDATERESULTFAILED);

        PKIX_CHECK(
            PKIX_ValidateResult_GetTrustAnchor(validResult, &trustAnchor,
                                               plContext),
            PKIX_VALIDATERESULTGETTRUSTANCHORFAILED);

        PKIX_CHECK(
            PKIX_TrustAnchor_GetTrustedCert(trustAnchor, &trustedCert,
                                            plContext),
            PKIX_TRUSTANCHORGETTRUSTEDCERTFAILED);

        PKIX_CHECK(
            PKIX_PL_Cert_GetCERTCertificate(trustedCert, &trustedRoot,
                                            plContext),
            PKIX_CERTGETCERTCERTIFICATEFAILED);
    }

    PORT_Assert(!PKIX_ERROR_RECEIVED);

    if (trustedRoot) {
        *ptrustedRoot = trustedRoot;
    }
    if (validChain) {
        *pvalidChain = validChain;
    }

cleanup:
    if (PKIX_ERROR_RECEIVED) {
        if (trustedRoot) {
            CERT_DestroyCertificate(trustedRoot);
        }
        if (validChain) {
            CERT_DestroyCertList(validChain);
        }
    }
    PKIX_DECREF(trustAnchor);
    PKIX_DECREF(trustedCert);
    PKIX_DECREF(pkixCertChain);
    PKIX_DECREF(validResult);
    PKIX_DECREF(error);
    PKIX_DECREF(verifyNode);
    PKIX_DECREF(buildResult);

    PKIX_RETURN(CERTVFYPKIX);
}
#endif /* NSS_DISABLE_LIBPKIX */

/*
 * FUNCTION: cert_VerifyCertChainPkix
 * DESCRIPTION:
 *
 * The main wrapper function that is called from CERT_VerifyCert and
 * CERT_VerifyCACertForUsage functions to validate cert with libpkix.
 *
 * PARAMETERS:
 *  "cert"
 *      Leaf certificate of a chain we want to build.
 *  "checkSig"
 *      Certificate signatures will not be verified if this
 *      flag is set to PR_FALSE.
 *  "requiredUsage"
 *      Required usage for certificate and chain.
 *  "time"
 *      Validity time.
 *  "wincx"
 *      Nss database password token.
 *  "log"
 *      Address of already allocated CERTVerifyLog structure. Not
 *      used if NULL;
 *  "pSigerror"
 *      Address of PRBool. If not NULL, returns true is cert chain
 *      was invalidated because of bad certificate signature.
 *  "pRevoked"
 *      Address of PRBool. If not NULL, returns true is cert chain
 *      was invalidated because a revoked certificate was found in
 *      the chain.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  SECFailure is chain building process has failed. SECSuccess otherwise.
 */
SECStatus
cert_VerifyCertChainPkix(
    CERTCertificate *cert,
    PRBool checkSig,
    SECCertUsage requiredUsage,
    PRTime time,
    void *wincx,
    CERTVerifyLog *log,
    PRBool *pSigerror,
    PRBool *pRevoked)
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
#else
    PKIX_ProcessingParams *procParams = NULL;
    PKIX_BuildResult *result = NULL;
    PKIX_VerifyNode *verifyNode = NULL;
    PKIX_Error *error = NULL;

    SECStatus rv = SECFailure;
    void *plContext = NULL;

#ifdef PKIX_OBJECT_LEAK_TEST
    int leakedObjNum = 0;
    int memLeakLoopCount = 0;
    int objCountTable[PKIX_NUMTYPES];
    int fnInvLocalCount = 0;
    PKIX_Boolean savedUsePkixEngFlag = usePKIXValidationEngine;

    if (usePKIXValidationEngine) {
        /* current memory leak testing implementation does not allow
         * to run simultaneous tests one the same or a different threads.
         * Setting the variable to false, to make additional chain
         * validations be handled by old nss. */
        usePKIXValidationEngine = PR_FALSE;
    }
    testStartFnStackPosition = 2;
    fnStackNameArr[0] = "cert_VerifyCertChainPkix";
    fnStackInvCountArr[0] = 0;
    PKIX_Boolean abortOnLeak =
        (PR_GetEnvSecure("PKIX_OBJECT_LEAK_TEST_ABORT_ON_LEAK") == NULL) ? PKIX_FALSE
                                                                         : PKIX_TRUE;
    runningLeakTest = PKIX_TRUE;

    /* Prevent multi-threaded run of object leak test */
    fnInvLocalCount = PR_ATOMIC_INCREMENT(&parallelFnInvocationCount);
    PORT_Assert(fnInvLocalCount == 1);

    do {
        rv = SECFailure;
        plContext = NULL;
        procParams = NULL;
        result = NULL;
        verifyNode = NULL;
        error = NULL;
        errorGenerated = PKIX_FALSE;
        stackPosition = 0;

        if (leakedObjNum) {
            pkix_pl_lifecycle_ObjectTableUpdate(objCountTable);
        }
        memLeakLoopCount += 1;
#endif /* PKIX_OBJECT_LEAK_TEST */

        error =
            cert_CreatePkixProcessingParams(cert, checkSig, time, wincx,
                                            PR_FALSE /*use arena*/,
                                            requiredUsage == certUsageStatusResponder,
                                            &procParams, &plContext);
        if (error) {
            goto cleanup;
        }

        error =
            cert_ProcessingParamsSetKeyAndCertUsage(procParams, requiredUsage, 0,
                                                    plContext);
        if (error) {
            goto cleanup;
        }

        error =
            cert_BuildAndValidateChain(procParams, &result, &verifyNode, plContext);
        if (error) {
            goto cleanup;
        }

        if (pRevoked) {
            /* Currently always PR_FALSE. Will be fixed as a part of 394077 */
            *pRevoked = PR_FALSE;
        }
        if (pSigerror) {
            /* Currently always PR_FALSE. Will be fixed as a part of 394077 */
            *pSigerror = PR_FALSE;
        }
        rv = SECSuccess;

    cleanup:
        error = cert_GetBuildResults(result, verifyNode, error, log, NULL, NULL,
                                     plContext);
        if (error) {
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)error, plContext);
        }
        if (procParams) {
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)procParams, plContext);
        }
        if (plContext) {
            PKIX_PL_NssContext_Destroy(plContext);
        }

#ifdef PKIX_OBJECT_LEAK_TEST
        leakedObjNum =
            pkix_pl_lifecycle_ObjectLeakCheck(leakedObjNum ? objCountTable : NULL);

        if (pkixLog && leakedObjNum) {
            PR_LOG(pkixLog, 1, ("The generated error caused an object leaks. Loop %d."
                                "Stack %s\n",
                                memLeakLoopCount, errorFnStackString));
        }
        PR_Free(errorFnStackString);
        errorFnStackString = NULL;
        if (abortOnLeak) {
            PORT_Assert(leakedObjNum == 0);
        }

    } while (errorGenerated);

    runningLeakTest = PKIX_FALSE;
    PR_ATOMIC_DECREMENT(&parallelFnInvocationCount);
    usePKIXValidationEngine = savedUsePkixEngFlag;
#endif /* PKIX_OBJECT_LEAK_TEST */

    return rv;
#endif /* NSS_DISABLE_LIBPKIX */
}

#ifndef NSS_DISABLE_LIBPKIX
PKIX_CertSelector *
cert_GetTargetCertConstraints(CERTCertificate *target, void *plContext)
{
    PKIX_ComCertSelParams *certSelParams = NULL;
    PKIX_CertSelector *certSelector = NULL;
    PKIX_CertSelector *r = NULL;
    PKIX_PL_Cert *eeCert = NULL;
    PKIX_Error *error = NULL;

    error = PKIX_PL_Cert_CreateFromCERTCertificate(target, &eeCert, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_CertSelector_Create(NULL, NULL, &certSelector, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_ComCertSelParams_Create(&certSelParams, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_ComCertSelParams_SetCertificate(
        certSelParams, eeCert, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_CertSelector_SetCommonCertSelectorParams(certSelector, certSelParams, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_PL_Object_IncRef((PKIX_PL_Object *)certSelector, plContext);
    if (error == NULL)
        r = certSelector;

cleanup:
    if (certSelParams != NULL)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)certSelParams, plContext);

    if (eeCert != NULL)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)eeCert, plContext);

    if (certSelector != NULL)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)certSelector, plContext);

    if (error != NULL) {
        SECErrorCodes nssErr;

        cert_PkixErrorToNssCode(error, &nssErr, plContext);
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)error, plContext);
        PORT_SetError(nssErr);
    }

    return r;
}

static PKIX_List *
cert_GetCertStores(void *plContext)
{
    PKIX_CertStore *certStore = NULL;
    PKIX_List *certStores = NULL;
    PKIX_List *r = NULL;
    PKIX_Error *error = NULL;

    error = PKIX_PL_Pk11CertStore_Create(&certStore, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_List_Create(&certStores, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_List_AppendItem(certStores,
                                 (PKIX_PL_Object *)certStore, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_PL_Object_IncRef((PKIX_PL_Object *)certStores, plContext);
    if (error == NULL)
        r = certStores;

cleanup:
    if (certStores != NULL)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)certStores, plContext);

    if (certStore != NULL)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)certStore, plContext);

    if (error != NULL) {
        SECErrorCodes nssErr;

        cert_PkixErrorToNssCode(error, &nssErr, plContext);
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)error, plContext);
        PORT_SetError(nssErr);
    }

    return r;
}

struct fake_PKIX_PL_CertStruct {
    CERTCertificate *nssCert;
};

/* This needs to be part of the PKIX_PL_* */
/* This definitely needs to go away, and be replaced with
   a real accessor function in PKIX */
static CERTCertificate *
cert_NSSCertFromPKIXCert(const PKIX_PL_Cert *pkix_cert)
{
    struct fake_PKIX_PL_CertStruct *fcert = NULL;

    fcert = (struct fake_PKIX_PL_CertStruct *)pkix_cert;

    return CERT_DupCertificate(fcert->nssCert);
}

PKIX_List *
cert_PKIXMakeOIDList(const SECOidTag *oids, int oidCount, void *plContext)
{
    PKIX_List *r = NULL;
    PKIX_List *policyList = NULL;
    PKIX_PL_OID *policyOID = NULL;
    PKIX_Error *error = NULL;
    int i;

    error = PKIX_List_Create(&policyList, plContext);
    if (error != NULL) {
        goto cleanup;
    }

    for (i = 0; i < oidCount; i++) {
        error = PKIX_PL_OID_Create(oids[i], &policyOID, plContext);
        if (error) {
            goto cleanup;
        }
        error = PKIX_List_AppendItem(policyList,
                                     (PKIX_PL_Object *)policyOID, plContext);
        if (error != NULL) {
            goto cleanup;
        }
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)policyOID, plContext);
        policyOID = NULL;
    }

    error = PKIX_List_SetImmutable(policyList, plContext);
    if (error != NULL)
        goto cleanup;

    error = PKIX_PL_Object_IncRef((PKIX_PL_Object *)policyList, plContext);
    if (error == NULL)
        r = policyList;

cleanup:
    if (policyOID != NULL) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)policyOID, plContext);
    }
    if (policyList != NULL) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)policyList, plContext);
    }
    if (error != NULL) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)error, plContext);
    }

    return r;
}

CERTValOutParam *
cert_pkix_FindOutputParam(CERTValOutParam *params, const CERTValParamOutType t)
{
    CERTValOutParam *i;
    if (params == NULL) {
        return NULL;
    }
    for (i = params; i->type != cert_po_end; i++) {
        if (i->type == t) {
            return i;
        }
    }
    return NULL;
}

static PKIX_Error *
setRevocationMethod(PKIX_RevocationChecker *revChecker,
                    PKIX_ProcessingParams *procParams,
                    const CERTRevocationTests *revTest,
                    CERTRevocationMethodIndex certRevMethod,
                    PKIX_RevocationMethodType pkixRevMethod,
                    PKIX_Boolean verifyResponderUsages,
                    PKIX_Boolean isLeafTest,
                    void *plContext)
{
    PKIX_UInt32 methodFlags = 0;
    PKIX_Error *error = NULL;
    PKIX_UInt32 priority = 0;

    if (revTest->number_of_defined_methods <= (PRUint32)certRevMethod) {
        return NULL;
    }
    if (revTest->preferred_methods) {
        unsigned int i = 0;
        for (; i < revTest->number_of_preferred_methods; i++) {
            if (revTest->preferred_methods[i] == certRevMethod)
                break;
        }
        priority = i;
    }
    methodFlags = revTest->cert_rev_flags_per_method[certRevMethod];
    if (verifyResponderUsages &&
        pkixRevMethod == PKIX_RevocationMethod_OCSP) {
        methodFlags |= PKIX_REV_M_FORBID_NETWORK_FETCHING;
    }
    error =
        PKIX_RevocationChecker_CreateAndAddMethod(revChecker, procParams,
                                                  pkixRevMethod, methodFlags,
                                                  priority, NULL,
                                                  isLeafTest, plContext);
    return error;
}

SECStatus
cert_pkixSetParam(PKIX_ProcessingParams *procParams,
                  const CERTValInParam *param, void *plContext)
{
    PKIX_Error *error = NULL;
    SECStatus r = SECSuccess;
    PKIX_PL_Date *date = NULL;
    PKIX_List *policyOIDList = NULL;
    PKIX_List *certListPkix = NULL;
    const CERTRevocationFlags *flags;
    SECErrorCodes errCode = SEC_ERROR_INVALID_ARGS;
    const CERTCertList *certList = NULL;
    CERTCertListNode *node;
    PKIX_PL_Cert *certPkix = NULL;
    PKIX_TrustAnchor *trustAnchor = NULL;
    PKIX_RevocationChecker *revChecker = NULL;
    PKIX_PL_NssContext *nssContext = (PKIX_PL_NssContext *)plContext;

    /* XXX we need a way to map generic PKIX error to generic NSS errors */

    switch (param->type) {

        case cert_pi_policyOID:

            /* needed? */
            error = PKIX_ProcessingParams_SetExplicitPolicyRequired(
                procParams, PKIX_TRUE, plContext);

            if (error != NULL) {
                break;
            }

            policyOIDList = cert_PKIXMakeOIDList(param->value.array.oids,
                                                 param->value.arraySize, plContext);
            if (policyOIDList == NULL) {
                r = SECFailure;
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                break;
            }

            error = PKIX_ProcessingParams_SetInitialPolicies(
                procParams, policyOIDList, plContext);
            break;

        case cert_pi_date:
            if (param->value.scalar.time == 0) {
                error = PKIX_PL_Date_Create_UTCTime(NULL, &date, plContext);
                if (error != NULL) {
                    errCode = SEC_ERROR_INVALID_TIME;
                    break;
                }
            } else {
                error = pkix_pl_Date_CreateFromPRTime(param->value.scalar.time,
                                                      &date, plContext);
                if (error != NULL) {
                    errCode = SEC_ERROR_INVALID_TIME;
                    break;
                }
            }

            error = PKIX_ProcessingParams_SetDate(procParams, date, plContext);
            if (error != NULL) {
                errCode = SEC_ERROR_INVALID_TIME;
            }
            break;

        case cert_pi_revocationFlags: {
            PKIX_UInt32 leafIMFlags = 0;
            PKIX_UInt32 chainIMFlags = 0;
            PKIX_Boolean validatingResponderCert = PKIX_FALSE;

            flags = param->value.pointer.revocation;
            if (!flags) {
                PORT_SetError(errCode);
                r = SECFailure;
                break;
            }

            leafIMFlags =
                flags->leafTests.cert_rev_method_independent_flags;
            chainIMFlags =
                flags->chainTests.cert_rev_method_independent_flags;

            error =
                PKIX_RevocationChecker_Create(leafIMFlags, chainIMFlags,
                                              &revChecker, plContext);
            if (error) {
                break;
            }

            error =
                PKIX_ProcessingParams_SetRevocationChecker(procParams,
                                                           revChecker, plContext);
            if (error) {
                break;
            }

            if (((PKIX_PL_NssContext *)plContext)->certificateUsage &
                certificateUsageStatusResponder) {
                validatingResponderCert = PKIX_TRUE;
            }

            error = setRevocationMethod(revChecker,
                                        procParams, &flags->leafTests,
                                        cert_revocation_method_crl,
                                        PKIX_RevocationMethod_CRL,
                                        validatingResponderCert,
                                        PKIX_TRUE, plContext);
            if (error) {
                break;
            }

            error = setRevocationMethod(revChecker,
                                        procParams, &flags->leafTests,
                                        cert_revocation_method_ocsp,
                                        PKIX_RevocationMethod_OCSP,
                                        validatingResponderCert,
                                        PKIX_TRUE, plContext);
            if (error) {
                break;
            }

            error = setRevocationMethod(revChecker,
                                        procParams, &flags->chainTests,
                                        cert_revocation_method_crl,
                                        PKIX_RevocationMethod_CRL,
                                        validatingResponderCert,
                                        PKIX_FALSE, plContext);
            if (error) {
                break;
            }

            error = setRevocationMethod(revChecker,
                                        procParams, &flags->chainTests,
                                        cert_revocation_method_ocsp,
                                        PKIX_RevocationMethod_OCSP,
                                        validatingResponderCert,
                                        PKIX_FALSE, plContext);
            if (error) {
                break;
            }

        } break;

        case cert_pi_trustAnchors:
            certList = param->value.pointer.chain;
            if (!certList) {
                PORT_SetError(errCode);
                r = SECFailure;
                break;
            }
            error = PKIX_List_Create(&certListPkix, plContext);
            if (error != NULL) {
                break;
            }
            for (node = CERT_LIST_HEAD(certList); !CERT_LIST_END(node, certList);
                 node = CERT_LIST_NEXT(node)) {
                error = PKIX_PL_Cert_CreateFromCERTCertificate(node->cert,
                                                               &certPkix, plContext);
                if (error) {
                    break;
                }
                error = PKIX_TrustAnchor_CreateWithCert(certPkix, &trustAnchor,
                                                        plContext);
                if (error) {
                    break;
                }
                error = PKIX_List_AppendItem(certListPkix,
                                             (PKIX_PL_Object *)trustAnchor, plContext);
                if (error) {
                    break;
                }
                PKIX_PL_Object_DecRef((PKIX_PL_Object *)trustAnchor, plContext);
                trustAnchor = NULL;
                PKIX_PL_Object_DecRef((PKIX_PL_Object *)certPkix, plContext);
                certPkix = NULL;
            }
            error =
                PKIX_ProcessingParams_SetTrustAnchors(procParams, certListPkix,
                                                      plContext);
            break;

        case cert_pi_useAIACertFetch:
            error =
                PKIX_ProcessingParams_SetUseAIAForCertFetching(procParams,
                                                               (PRBool)(param->value.scalar.b !=
                                                                        0),
                                                               plContext);
            break;

        case cert_pi_chainVerifyCallback: {
            const CERTChainVerifyCallback *chainVerifyCallback =
                param->value.pointer.chainVerifyCallback;
            if (!chainVerifyCallback || !chainVerifyCallback->isChainValid) {
                PORT_SetError(errCode);
                r = SECFailure;
                break;
            }

            nssContext->chainVerifyCallback = *chainVerifyCallback;
        } break;

        case cert_pi_useOnlyTrustAnchors:
            error =
                PKIX_ProcessingParams_SetUseOnlyTrustAnchors(procParams,
                                                             (PRBool)(param->value.scalar.b !=
                                                                      0),
                                                             plContext);
            break;

        default:
            PORT_SetError(errCode);
            r = SECFailure;
            break;
    }

    if (policyOIDList != NULL)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)policyOIDList, plContext);

    if (date != NULL)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)date, plContext);

    if (revChecker != NULL)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)revChecker, plContext);

    if (certListPkix)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)certListPkix, plContext);

    if (trustAnchor)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)trustAnchor, plContext);

    if (certPkix)
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)certPkix, plContext);

    if (error != NULL) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object *)error, plContext);
        PORT_SetError(errCode);
        r = SECFailure;
    }

    return r;
}

void
cert_pkixDestroyValOutParam(CERTValOutParam *params)
{
    CERTValOutParam *i;

    if (params == NULL) {
        return;
    }
    for (i = params; i->type != cert_po_end; i++) {
        switch (i->type) {
            case cert_po_trustAnchor:
                if (i->value.pointer.cert) {
                    CERT_DestroyCertificate(i->value.pointer.cert);
                    i->value.pointer.cert = NULL;
                }
                break;

            case cert_po_certList:
                if (i->value.pointer.chain) {
                    CERT_DestroyCertList(i->value.pointer.chain);
                    i->value.pointer.chain = NULL;
                }
                break;

            default:
                break;
        }
    }
}

static PRUint64 certRev_NSS_3_11_Ocsp_Enabled_Soft_Policy_LeafFlags[2] = {
    /* crl */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FORBID_NETWORK_FETCHING |
        CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO,
    /* ocsp */
    CERT_REV_M_TEST_USING_THIS_METHOD
};

static PRUint64 certRev_NSS_3_11_Ocsp_Enabled_Soft_Policy_ChainFlags[2] = {
    /* crl */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FORBID_NETWORK_FETCHING |
        CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO,
    /* ocsp */
    0
};

static CERTRevocationMethodIndex
    certRev_NSS_3_11_Ocsp_Enabled_Soft_Policy_Method_Preference = {
        cert_revocation_method_crl
    };

static const CERTRevocationFlags certRev_NSS_3_11_Ocsp_Enabled_Soft_Policy = {
    { /* leafTests */
      2,
      certRev_NSS_3_11_Ocsp_Enabled_Soft_Policy_LeafFlags,
      1,
      &certRev_NSS_3_11_Ocsp_Enabled_Soft_Policy_Method_Preference,
      0 },
    { /* chainTests */
      2,
      certRev_NSS_3_11_Ocsp_Enabled_Soft_Policy_ChainFlags,
      0,
      0,
      0 }
};
#endif /* NSS_DISABLE_LIBPKIX */

extern const CERTRevocationFlags *
CERT_GetClassicOCSPEnabledSoftFailurePolicy()
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return NULL;
#else
    return &certRev_NSS_3_11_Ocsp_Enabled_Soft_Policy;
#endif /* NSS_DISABLE_LIBPKIX */
}

#ifndef NSS_DISABLE_LIBPKIX
static PRUint64 certRev_NSS_3_11_Ocsp_Enabled_Hard_Policy_LeafFlags[2] = {
    /* crl */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FORBID_NETWORK_FETCHING |
        CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO,
    /* ocsp */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO
};

static PRUint64 certRev_NSS_3_11_Ocsp_Enabled_Hard_Policy_ChainFlags[2] = {
    /* crl */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FORBID_NETWORK_FETCHING |
        CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO,
    /* ocsp */
    0
};

static CERTRevocationMethodIndex
    certRev_NSS_3_11_Ocsp_Enabled_Hard_Policy_Method_Preference = {
        cert_revocation_method_crl
    };

static const CERTRevocationFlags certRev_NSS_3_11_Ocsp_Enabled_Hard_Policy = {
    { /* leafTests */
      2,
      certRev_NSS_3_11_Ocsp_Enabled_Hard_Policy_LeafFlags,
      1,
      &certRev_NSS_3_11_Ocsp_Enabled_Hard_Policy_Method_Preference,
      0 },
    { /* chainTests */
      2,
      certRev_NSS_3_11_Ocsp_Enabled_Hard_Policy_ChainFlags,
      0,
      0,
      0 }
};
#endif /* NSS_DISABLE_LIBPKIX */

extern const CERTRevocationFlags *
CERT_GetClassicOCSPEnabledHardFailurePolicy()
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return NULL;
#else
    return &certRev_NSS_3_11_Ocsp_Enabled_Hard_Policy;
#endif /* NSS_DISABLE_LIBPKIX */
}

#ifndef NSS_DISABLE_LIBPKIX
static PRUint64 certRev_NSS_3_11_Ocsp_Disabled_Policy_LeafFlags[2] = {
    /* crl */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FORBID_NETWORK_FETCHING |
        CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO,
    /* ocsp */
    0
};

static PRUint64 certRev_NSS_3_11_Ocsp_Disabled_Policy_ChainFlags[2] = {
    /* crl */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FORBID_NETWORK_FETCHING |
        CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO,
    /* ocsp */
    0
};

static const CERTRevocationFlags certRev_NSS_3_11_Ocsp_Disabled_Policy = {
    { /* leafTests */
      2,
      certRev_NSS_3_11_Ocsp_Disabled_Policy_LeafFlags,
      0,
      0,
      0 },
    { /* chainTests */
      2,
      certRev_NSS_3_11_Ocsp_Disabled_Policy_ChainFlags,
      0,
      0,
      0 }
};
#endif /* NSS_DISABLE_LIBPKIX */

extern const CERTRevocationFlags *
CERT_GetClassicOCSPDisabledPolicy()
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return NULL;
#else
    return &certRev_NSS_3_11_Ocsp_Disabled_Policy;
#endif /* NSS_DISABLE_LIBPKIX */
}

#ifndef NSS_DISABLE_LIBPKIX
static PRUint64 certRev_PKIX_Verify_Nist_Policy_LeafFlags[2] = {
    /* crl */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO |
        CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE,
    /* ocsp */
    0
};

static PRUint64 certRev_PKIX_Verify_Nist_Policy_ChainFlags[2] = {
    /* crl */
    CERT_REV_M_TEST_USING_THIS_METHOD |
        CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO |
        CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE,
    /* ocsp */
    0
};

static const CERTRevocationFlags certRev_PKIX_Verify_Nist_Policy = {
    { /* leafTests */
      2,
      certRev_PKIX_Verify_Nist_Policy_LeafFlags,
      0,
      0,
      0 },
    { /* chainTests */
      2,
      certRev_PKIX_Verify_Nist_Policy_ChainFlags,
      0,
      0,
      0 }
};
#endif /* NSS_DISABLE_LIBPKIX */

extern const CERTRevocationFlags *
CERT_GetPKIXVerifyNistRevocationPolicy()
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return NULL;
#else
    return &certRev_PKIX_Verify_Nist_Policy;
#endif /* NSS_DISABLE_LIBPKIX */
}

CERTRevocationFlags *
CERT_AllocCERTRevocationFlags(
    PRUint32 number_leaf_methods, PRUint32 number_leaf_pref_methods,
    PRUint32 number_chain_methods, PRUint32 number_chain_pref_methods)
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return NULL;
#else
    CERTRevocationFlags *flags;

    flags = PORT_New(CERTRevocationFlags);
    if (!flags)
        return (NULL);

    flags->leafTests.number_of_defined_methods = number_leaf_methods;
    flags->leafTests.cert_rev_flags_per_method =
        PORT_NewArray(PRUint64, number_leaf_methods);

    flags->leafTests.number_of_preferred_methods = number_leaf_pref_methods;
    flags->leafTests.preferred_methods =
        PORT_NewArray(CERTRevocationMethodIndex, number_leaf_pref_methods);

    flags->chainTests.number_of_defined_methods = number_chain_methods;
    flags->chainTests.cert_rev_flags_per_method =
        PORT_NewArray(PRUint64, number_chain_methods);

    flags->chainTests.number_of_preferred_methods = number_chain_pref_methods;
    flags->chainTests.preferred_methods =
        PORT_NewArray(CERTRevocationMethodIndex, number_chain_pref_methods);

    if (!flags->leafTests.cert_rev_flags_per_method ||
        !flags->leafTests.preferred_methods ||
        !flags->chainTests.cert_rev_flags_per_method ||
        !flags->chainTests.preferred_methods) {
        CERT_DestroyCERTRevocationFlags(flags);
        return (NULL);
    }

    return flags;
#endif /* NSS_DISABLE_LIBPKIX */
}

void
CERT_DestroyCERTRevocationFlags(CERTRevocationFlags *flags)
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return;
#else
    if (!flags)
        return;

    if (flags->leafTests.cert_rev_flags_per_method)
        PORT_Free(flags->leafTests.cert_rev_flags_per_method);

    if (flags->leafTests.preferred_methods)
        PORT_Free(flags->leafTests.preferred_methods);

    if (flags->chainTests.cert_rev_flags_per_method)
        PORT_Free(flags->chainTests.cert_rev_flags_per_method);

    if (flags->chainTests.preferred_methods)
        PORT_Free(flags->chainTests.preferred_methods);

    PORT_Free(flags);
#endif /* NSS_DISABLE_LIBPKIX */
}

/*
 * CERT_PKIXVerifyCert
 *
 * Verify a Certificate using the PKIX library.
 *
 * Parameters:
 *  cert    - the target certificate to verify. Must be non-null
 *  params  - an array of type/value parameters which can be
 *            used to modify the behavior of the validation
 *            algorithm, or supply additional constraints.
 *
 *  outputTrustAnchor - the trust anchor which the certificate
 *                      chains to. The caller is responsible
 *                      for freeing this.
 *
 * Example Usage:
 *    CERTValParam args[3];
 *    args[0].type = cvpt_policyOID;
 *    args[0].value.si = oid;
 *    args[1].type = revCheckRequired;
 *    args[1].value.b = PR_TRUE;
 *    args[2].type = cvpt_end;
 *
 *    CERT_PKIXVerifyCert(cert, &output, args
 */
SECStatus
CERT_PKIXVerifyCert(
    CERTCertificate *cert,
    SECCertificateUsage usages,
    CERTValInParam *paramsIn,
    CERTValOutParam *paramsOut,
    void *wincx)
{
#ifdef NSS_DISABLE_LIBPKIX
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
#else
    SECStatus r = SECFailure;
    PKIX_Error *error = NULL;
    PKIX_ProcessingParams *procParams = NULL;
    PKIX_BuildResult *buildResult = NULL;
    void *nbioContext = NULL; /* for non-blocking IO */
    void *buildState = NULL;  /* for non-blocking IO */
    PKIX_CertSelector *certSelector = NULL;
    PKIX_List *certStores = NULL;
    PKIX_ValidateResult *valResult = NULL;
    PKIX_VerifyNode *verifyNode = NULL;
    PKIX_TrustAnchor *trustAnchor = NULL;
    PKIX_PL_Cert *trustAnchorCert = NULL;
    PKIX_List *builtCertList = NULL;
    CERTValOutParam *oparam = NULL;
    int i = 0;

    void *plContext = NULL;

#ifdef PKIX_OBJECT_LEAK_TEST
    int leakedObjNum = 0;
    int memLeakLoopCount = 0;
    int objCountTable[PKIX_NUMTYPES];
    int fnInvLocalCount = 0;
    PKIX_Boolean savedUsePkixEngFlag = usePKIXValidationEngine;

    if (usePKIXValidationEngine) {
        /* current memory leak testing implementation does not allow
         * to run simultaneous tests one the same or a different threads.
         * Setting the variable to false, to make additional chain
         * validations be handled by old nss. */
        usePKIXValidationEngine = PR_FALSE;
    }
    testStartFnStackPosition = 1;
    fnStackNameArr[0] = "CERT_PKIXVerifyCert";
    fnStackInvCountArr[0] = 0;
    PKIX_Boolean abortOnLeak =
        (PR_GetEnvSecure("PKIX_OBJECT_LEAK_TEST_ABORT_ON_LEAK") == NULL) ? PKIX_FALSE
                                                                         : PKIX_TRUE;
    runningLeakTest = PKIX_TRUE;

    /* Prevent multi-threaded run of object leak test */
    fnInvLocalCount = PR_ATOMIC_INCREMENT(&parallelFnInvocationCount);
    PORT_Assert(fnInvLocalCount == 1);

    do {
        r = SECFailure;
        error = NULL;
        procParams = NULL;
        buildResult = NULL;
        nbioContext = NULL; /* for non-blocking IO */
        buildState = NULL;  /* for non-blocking IO */
        certSelector = NULL;
        certStores = NULL;
        valResult = NULL;
        verifyNode = NULL;
        trustAnchor = NULL;
        trustAnchorCert = NULL;
        builtCertList = NULL;
        oparam = NULL;
        i = 0;
        errorGenerated = PKIX_FALSE;
        stackPosition = 0;

        if (leakedObjNum) {
            pkix_pl_lifecycle_ObjectTableUpdate(objCountTable);
        }
        memLeakLoopCount += 1;
#endif /* PKIX_OBJECT_LEAK_TEST */

        error = PKIX_PL_NssContext_Create(
            0, PR_FALSE /*use arena*/, wincx, &plContext);
        if (error != NULL) { /* need pkix->nss error map */
            PORT_SetError(SEC_ERROR_CERT_NOT_VALID);
            goto cleanup;
        }

        error = pkix_pl_NssContext_SetCertUsage(usages, plContext);
        if (error != NULL) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            goto cleanup;
        }

        error = PKIX_ProcessingParams_Create(&procParams, plContext);
        if (error != NULL) { /* need pkix->nss error map */
            PORT_SetError(SEC_ERROR_CERT_NOT_VALID);
            goto cleanup;
        }

        /* local cert store should be set into procParams before
         * filling in revocation settings. */
        certStores = cert_GetCertStores(plContext);
        if (certStores == NULL) {
            goto cleanup;
        }
        error = PKIX_ProcessingParams_SetCertStores(procParams, certStores, plContext);
        if (error != NULL) {
            goto cleanup;
        }

        /* now process the extensible input parameters structure */
        if (paramsIn != NULL) {
            i = 0;
            while (paramsIn[i].type != cert_pi_end) {
                if (paramsIn[i].type >= cert_pi_max) {
                    PORT_SetError(SEC_ERROR_INVALID_ARGS);
                    goto cleanup;
                }
                if (cert_pkixSetParam(procParams,
                                      &paramsIn[i], plContext) !=
                    SECSuccess) {
                    PORT_SetError(SEC_ERROR_INVALID_ARGS);
                    goto cleanup;
                }
                i++;
            }
        }

        certSelector = cert_GetTargetCertConstraints(cert, plContext);
        if (certSelector == NULL) {
            goto cleanup;
        }
        error = PKIX_ProcessingParams_SetTargetCertConstraints(procParams, certSelector, plContext);
        if (error != NULL) {
            goto cleanup;
        }

        error = PKIX_BuildChain(procParams, &nbioContext,
                                &buildState, &buildResult, &verifyNode,
                                plContext);
        if (error != NULL) {
            goto cleanup;
        }

        error = PKIX_BuildResult_GetValidateResult(buildResult, &valResult,
                                                   plContext);
        if (error != NULL) {
            goto cleanup;
        }

        error = PKIX_ValidateResult_GetTrustAnchor(valResult, &trustAnchor,
                                                   plContext);
        if (error != NULL) {
            goto cleanup;
        }

        if (trustAnchor != NULL) {
            error = PKIX_TrustAnchor_GetTrustedCert(trustAnchor, &trustAnchorCert,
                                                    plContext);
            if (error != NULL) {
                goto cleanup;
            }
        }

#ifdef PKIX_OBJECT_LEAK_TEST
        /* Can not continue if error was generated but not returned.
         * Jumping to cleanup. */
        if (errorGenerated)
            goto cleanup;
#endif /* PKIX_OBJECT_LEAK_TEST */

        oparam = cert_pkix_FindOutputParam(paramsOut, cert_po_trustAnchor);
        if (oparam != NULL) {
            if (trustAnchorCert != NULL) {
                oparam->value.pointer.cert =
                    cert_NSSCertFromPKIXCert(trustAnchorCert);
            } else {
                oparam->value.pointer.cert = NULL;
            }
        }

        error = PKIX_BuildResult_GetCertChain(buildResult, &builtCertList,
                                              plContext);
        if (error != NULL) {
            goto cleanup;
        }

        oparam = cert_pkix_FindOutputParam(paramsOut, cert_po_certList);
        if (oparam != NULL) {
            error = cert_PkixToNssCertsChain(builtCertList,
                                             &oparam->value.pointer.chain,
                                             plContext);
            if (error)
                goto cleanup;
        }

        r = SECSuccess;

    cleanup:
        if (verifyNode) {
            /* Return validation log only upon error. */
            oparam = cert_pkix_FindOutputParam(paramsOut, cert_po_errorLog);
#ifdef PKIX_OBJECT_LEAK_TEST
            if (!errorGenerated)
#endif /* PKIX_OBJECT_LEAK_TEST */
                if (r && oparam != NULL) {
                    PKIX_Error *tmpError =
                        cert_GetLogFromVerifyNode(oparam->value.pointer.log,
                                                  verifyNode, plContext);
                    if (tmpError) {
                        PKIX_PL_Object_DecRef((PKIX_PL_Object *)tmpError, plContext);
                    }
                }
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)verifyNode, plContext);
        }

        if (procParams != NULL)
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)procParams, plContext);

        if (trustAnchorCert != NULL)
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)trustAnchorCert, plContext);

        if (trustAnchor != NULL)
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)trustAnchor, plContext);

        if (valResult != NULL)
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)valResult, plContext);

        if (buildResult != NULL)
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)buildResult, plContext);

        if (certStores != NULL)
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)certStores, plContext);

        if (certSelector != NULL)
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)certSelector, plContext);

        if (builtCertList != NULL)
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)builtCertList, plContext);

        if (error != NULL) {
            SECErrorCodes nssErrorCode = 0;

            cert_PkixErrorToNssCode(error, &nssErrorCode, plContext);
            cert_pkixDestroyValOutParam(paramsOut);
            PORT_SetError(nssErrorCode);
            PKIX_PL_Object_DecRef((PKIX_PL_Object *)error, plContext);
        }

        PKIX_PL_NssContext_Destroy(plContext);

#ifdef PKIX_OBJECT_LEAK_TEST
        leakedObjNum =
            pkix_pl_lifecycle_ObjectLeakCheck(leakedObjNum ? objCountTable : NULL);

        if (pkixLog && leakedObjNum) {
            PR_LOG(pkixLog, 1, ("The generated error caused an object leaks. Loop %d."
                                "Stack %s\n",
                                memLeakLoopCount, errorFnStackString));
        }
        PR_Free(errorFnStackString);
        errorFnStackString = NULL;
        if (abortOnLeak) {
            PORT_Assert(leakedObjNum == 0);
        }

    } while (errorGenerated);

    runningLeakTest = PKIX_FALSE;
    PR_ATOMIC_DECREMENT(&parallelFnInvocationCount);
    usePKIXValidationEngine = savedUsePkixEngFlag;
#endif /* PKIX_OBJECT_LEAK_TEST */

    return r;
#endif /* NSS_DISABLE_LIBPKIX */
}
