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
 * The Original Code is the PKIX-C library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are
 * Copyright 2004-2007 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems, Inc.
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
 * pkix_defaultcrlchecker.c
 *
 * Functions for default CRL Checkers
 *
 */
#define CERTCHAINCHECKERDEBUG 1

#include "pkix_defaultcrlchecker.h"

static char *reasonCodeMsgString[] = {
	"Certificate is revoked by CRL for unspecified reason"
	"Certificate is revoked by CRL for key compromise",
	"Certificate is revoked by CRL for CA compromise",
	"Certificate is revoked by CRL for affiliation changed",
	"Certificate is revoked by CRL for being superseded",
	"Certificate is revoked by CRL for cessation of operation",
	"Certificate is revoked by CRL for certificate hold",
	"Certificate is revoked by CRL for undefined reason",
	"Certificate is revoked by CRL for being removed from CRL",
	"Certificate is revoked by CRL for privilege withdrawn",
	"Certificate is revoked by CRL for aACompromise",
};

static const int numReasonCodes = 
    sizeof(reasonCodeMsgString) / sizeof(reasonCodeMsgString[0]);

/* --Private-DefaultCRLCheckerState-Functions------------------------------- */

/*
 * FUNCTION: pkix_DefaultCRLCheckerstate_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_DefaultCRLCheckerState_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        pkix_DefaultCRLCheckerState *state = NULL;

        PKIX_ENTER(DEFAULTCRLCHECKERSTATE,
                    "pkix_DefaultCRLCheckerState_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a default CRL checker state */
        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_DEFAULTCRLCHECKERSTATE_TYPE, plContext),
                    PKIX_OBJECTNOTDEFAULTCRLCHECKERSTATE);

        state = (pkix_DefaultCRLCheckerState *)object;

        state->certHasValidCrl = PKIX_FALSE;
        state->prevCertCrlSign = PKIX_FALSE;
        state->reasonCodeMask = 0;

        PKIX_DECREF(state->certStores);
        PKIX_DECREF(state->testDate);
        PKIX_DECREF(state->prevPublicKey);
        PKIX_DECREF(state->prevPublicKeyList);
        PKIX_DECREF(state->crlReasonCodeOID);
        PKIX_DECREF(state->certIssuer);
        PKIX_DECREF(state->certSerialNumber);
        PKIX_DECREF(state->crlSelector);
        state->crlStoreIndex = 0;
        state->numCrlStores = 0;

cleanup:

        PKIX_RETURN(DEFAULTCRLCHECKERSTATE);
}

/*
 * FUNCTION: pkix_DefaultCRLCheckerState_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_DEFAULTCRLCHECKERSTATE_TYPE and its related functions
 *  with systemClasses[]
 *
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_DefaultCRLCheckerState_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(DEFAULTCRLCHECKERSTATE,
                    "pkix_DefaultCRLCheckerState_RegisterSelf");

        entry.description = "DefaultCRLCheckerState";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(pkix_DefaultCRLCheckerState);
        entry.destructor = pkix_DefaultCRLCheckerState_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_DEFAULTCRLCHECKERSTATE_TYPE] = entry;

        PKIX_RETURN(DEFAULTCRLCHECKERSTATE);
}

/*
 * FUNCTION: pkix_DefaultCRLCheckerState_Create
 *
 * DESCRIPTION:
 *  Allocate and initialize DefaultCRLChecker state data.
 *
 * PARAMETERS
 *  "certStores"
 *      Address of CertStore List to be stored in state. Must be non-NULL.
 *  "testDate"
 *      Address of PKIX_PL_Date to be checked. May be NULL.
 *  "trustedPubKey"
 *      Trusted Anchor Public Key for verifying first Cert in the chain.
 *      Must be non-NULL.
 *  "certsRemaining"
 *      Number of certificates remaining in the chain.
 *  "nistCRLPolicyEnabled"
 *      If enabled, enforce nist crl policy.
 *  "pCheckerState"
 *      Address of DefaultCRLCheckerState that is returned. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a DefaultCrlCheckerState Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error
 */
static PKIX_Error *
pkix_DefaultCRLCheckerState_Create(
    PKIX_List *certStores,
    PKIX_PL_Date *testDate,
    PKIX_PL_PublicKey *trustedPubKey,
    PKIX_UInt32 certsRemaining,
    PKIX_Boolean nistCRLPolicyEnabled,
    pkix_DefaultCRLCheckerState **pCheckerState,
    void *plContext)
{
        pkix_DefaultCRLCheckerState *state = NULL;

        PKIX_ENTER(DEFAULTCRLCHECKERSTATE,
                    "pkix_DefaultCRLCheckerState_Create");
        PKIX_NULLCHECK_TWO(certStores, pCheckerState);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_DEFAULTCRLCHECKERSTATE_TYPE,
                    sizeof (pkix_DefaultCRLCheckerState),
                    (PKIX_PL_Object **)&state,
                    plContext),
                    PKIX_COULDNOTCREATEDEFAULTCRLCHECKERSTATEOBJECT);

        /* Initialize fields */

        PKIX_INCREF(certStores);
        state->certStores = certStores;

        PKIX_INCREF(testDate);
        state->testDate = testDate;

        PKIX_INCREF(trustedPubKey);
        state->prevPublicKey = trustedPubKey;

        state->certHasValidCrl = PKIX_FALSE;
        state->nistCRLPolicyEnabled = nistCRLPolicyEnabled;
        state->prevCertCrlSign = PKIX_TRUE;
        state->prevPublicKeyList = NULL;
        state->reasonCodeMask = 0;
        state->certsRemaining = certsRemaining;

        PKIX_CHECK(PKIX_PL_OID_Create
                    (PKIX_CRLREASONCODE_OID,
                    &state->crlReasonCodeOID,
                    plContext),
                    PKIX_OIDCREATEFAILED);

        state->certIssuer = NULL;
        state->certSerialNumber = NULL;
        state->crlSelector = NULL;
        state->crlStoreIndex = 0;
        state->numCrlStores = 0;

        *pCheckerState = state;
        state = NULL;

cleanup:

        PKIX_DECREF(state);

        PKIX_RETURN(DEFAULTCRLCHECKERSTATE);
}

/* --Private-DefaultCRLChecker-Functions------------------------------------ */

/*
 * FUNCTION: pkix_DefaultCRLChecker_CheckCRLs
 *
 * DESCRIPTION:
 *  Check validity of "cert" based on CRLs at "crlList" that has correct
 *  signature verification with "publicKey".
 *
 * PARAMETERS
 *  "cert"
 *      Address of Cert which has the certificate data. Must be non-NULL.
 *  "certIssuer"
 *      Address of Issuer that associates with the Cert. Must be non-NULL.
 *  "certSerialNumber"
 *      Address of Serial Number that associates with the Cert. Must be 
 *      non-NULL.
 *  "publicKey"
 *      Address of Public Key that associates with the Cert Issuer.
 *      Must be non-NULL.
 *  "crlList"
 *      A List CRLs that the certificate is verified upon. Must be non-NULL.
 *  "state"
 *      Address of DefaultCRLCheckerState which keeps dynamic state data.
 *      Must be non-NULL.
 *  "pCrlEntryList"
 *      Address of PKIX_PL_CrlEntry List that contains valid CrlEntries for
 *      this Cert. May be NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error
 */
static PKIX_Error *
pkix_DefaultCRLChecker_CheckCRLs(
        PKIX_PL_Cert *cert,
        PKIX_PL_X500Name *certIssuer,
        PKIX_PL_BigInt *certSerialNumber,
        PKIX_PL_PublicKey *publicKey,
        PKIX_List *crlList,
        pkix_DefaultCRLCheckerState *state,
        PKIX_List **pCrlEntryList,
        void *plContext)
{
        PKIX_PL_CRL *crl = NULL;
        PKIX_PL_CRLEntry *crlEntry = NULL;
        PKIX_PL_PublicKey *pKey = NULL;
        PKIX_List *unresCrlCritExtOIDs = NULL;
        PKIX_List *unresCrlEntryCritExtOIDs = NULL;
        PKIX_List *crlEntryList = NULL;
        PKIX_Error *verifyFail = NULL;
        PKIX_UInt32 numCrls = 0;
        PKIX_UInt32 numKeys = 0;
        PKIX_UInt32 numCritExtOIDs = 0;
        PKIX_Boolean crlVerified = PKIX_FALSE;
        PKIX_Boolean crlRevoking = PKIX_FALSE;
        PKIX_Int32 reasonCode = 0;
        PKIX_UInt32 i;
        PKIX_Int32 j;

        PKIX_ENTER(CERTCHAINCHECKER,
                    "pkix_DefaultCRLChecker_CheckCRLs");
        PKIX_NULLCHECK_FOUR(cert, publicKey, crlList, state);

        PKIX_CHECK(PKIX_List_GetLength(crlList, &numCrls, plContext),
                    PKIX_LISTGETLENGTHFAILED);

        if (state->prevPublicKeyList != NULL) {

                PKIX_CHECK(PKIX_List_GetLength
                    (state->prevPublicKeyList, &numKeys, plContext),
                    PKIX_LISTGETLENGTHFAILED);
        }

        /* Check if Cert is not revoked by any the the CRLs */

        for (i = 0; i < numCrls; i++){

                PKIX_CHECK(PKIX_List_GetItem
                            (crlList, i, (PKIX_PL_Object **)&crl, plContext),
                            PKIX_LISTGETITEMFAILED);

                /*
                 * Checking serial number (issuer done in selector) then
                 * verify signature. If matches, get the CRL reason(s).
                 */

                if (state->prevCertCrlSign == PKIX_TRUE) {
                        verifyFail = PKIX_PL_CRL_VerifySignature
                                (crl, publicKey, plContext);
                        if (verifyFail == NULL) {
                                crlVerified = PKIX_TRUE;
                        } else {
                                crlVerified = PKIX_FALSE;
                                PKIX_DECREF(verifyFail);
                        }
                }

                if (crlVerified == PKIX_FALSE) {

                    /* Verify from old key(s) on the list */
                    for (j = numKeys - 1; j >= 0; j--) {

                            PKIX_CHECK(PKIX_List_GetItem
                                (state->prevPublicKeyList,
                                j,
                                (PKIX_PL_Object **) &pKey,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                            verifyFail = PKIX_PL_CRL_VerifySignature
                                (crl, pKey, plContext);

                            if (verifyFail == NULL) {
                                crlVerified = PKIX_TRUE;
                                break;
                            } else {
                                crlVerified = PKIX_FALSE;
                                PKIX_DECREF(verifyFail);
                            }

                            PKIX_DECREF(pKey);
                    }
                }

                if (crlVerified == PKIX_FALSE) {
                    /* try next one ... */
                    goto cleanup_loop;
                }

                state->certHasValidCrl = PKIX_TRUE;

                PKIX_CHECK(PKIX_PL_CRL_GetCriticalExtensionOIDs
                            (crl, &unresCrlCritExtOIDs, plContext),
                            PKIX_CRLGETCRITICALEXTENSIONOIDSFAILED);

                /*
                 * XXX Advanced CRL work - should put a
                 * Loop here to process and remove critical
                 * extension oids.
                 */

                if (unresCrlCritExtOIDs) {

                    PKIX_CHECK(PKIX_List_GetLength(unresCrlCritExtOIDs,
                        &numCritExtOIDs,
                        plContext),
                        PKIX_LISTGETLENGTHFAILED);

                    if (numCritExtOIDs != 0) {
                        PKIX_DEFAULTCRLCHECKERSTATE_DEBUG
                                (PKIX_CRLCRITICALEXTENSIONOIDSNOTPROCESSED);
                        /*
                         * Uncomment this after we have implemented
                         * checkers for all the critical extensions.
                         *
                         * PKIX_ERROR
                         *      ("Unrecognized CRL Critical Extension");
                         */
                    }
                }

                PKIX_CHECK(PKIX_PL_CRL_GetCRLEntryForSerialNumber
                            (crl, certSerialNumber, &crlEntry, plContext),
                            PKIX_CRLGETCRLENTRYFORSERIALNUMBERFAILED);

                if (crlEntry == NULL) {
                    goto cleanup_loop;
                }

                crlRevoking = PKIX_TRUE;

                PKIX_CHECK(PKIX_PL_CRLEntry_GetCRLEntryReasonCode
                            (crlEntry,
                            &reasonCode,
                            plContext),
                            PKIX_CRLENTRYGETCRLENTRYREASONCODEFAILED);

                /* This is a valid CRLEntry, return it for caching */
                if (crlEntryList == NULL) {
                    PKIX_CHECK(PKIX_List_Create(&crlEntryList, plContext),
                            PKIX_LISTCREATEFAILED);

                }

                PKIX_CHECK(PKIX_List_AppendItem
                        (crlEntryList, (PKIX_PL_Object *) crlEntry, plContext),
                        PKIX_LISTAPPENDITEMFAILED);

                /* Set reason code in state for advance CRL reviewing */

                if (reasonCode >= 0) {
                    if (reasonCode >= numReasonCodes) 
		        reasonCode = 0;

                    state->reasonCodeMask |= 1 << reasonCode;
                    PKIX_DEFAULTCRLCHECKERSTATE_DEBUG_ARG
                        ("CRL revocation Reason: %s\n ",
                        reasonCodeMsgString[reasonCode]);

                } else {
                    PKIX_DEFAULTCRLCHECKERSTATE_DEBUG
                        ("Revoked by Unknown CRL ReasonCode");
                }

                PKIX_CHECK(PKIX_PL_CRLEntry_GetCriticalExtensionOIDs
                            (crlEntry, &unresCrlEntryCritExtOIDs, plContext),
                            PKIX_CRLENTRYGETCRITICALEXTENSIONOIDSFAILED);
                if (unresCrlEntryCritExtOIDs) {

                    PKIX_CHECK(pkix_List_Remove
                            (unresCrlEntryCritExtOIDs,
                            (PKIX_PL_Object *) state->crlReasonCodeOID,
                            plContext),
                            PKIX_LISTREMOVEFAILED);

                    PKIX_CHECK(PKIX_List_GetLength(unresCrlEntryCritExtOIDs,
                        &numCritExtOIDs,
                        plContext),
                        PKIX_LISTGETLENGTHFAILED);

                    if (numCritExtOIDs != 0) {

                        PKIX_DEFAULTCRLCHECKERSTATE_DEBUG
                            (PKIX_CRLENTRYCRITICALEXTENSIONWASNOTPROCESSED);
                        PKIX_ERROR(PKIX_UNRECOGNIZEDCRLENTRYCRITICALEXTENSION);
                    }
                }

        cleanup_loop:

                PKIX_DECREF(pKey);
                PKIX_DECREF(verifyFail);
                PKIX_DECREF(pKey);
                PKIX_DECREF(crlEntry);
                PKIX_DECREF(crl);
                PKIX_DECREF(unresCrlCritExtOIDs);
                PKIX_DECREF(unresCrlEntryCritExtOIDs);
        }

        *pCrlEntryList = crlEntryList;

        if (crlRevoking == PKIX_TRUE) {

                PKIX_ERROR(PKIX_CERTIFICATEREVOKEDBYCRL);
        }

cleanup:

        PKIX_DECREF(pKey);
        PKIX_DECREF(verifyFail);
        PKIX_DECREF(crlEntry);
        PKIX_DECREF(crl);
        PKIX_DECREF(unresCrlCritExtOIDs);
        PKIX_DECREF(unresCrlEntryCritExtOIDs);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_DefaultCRLChecker_Check_SetSelector
 *
 * DESCRIPTION:
 *  This function creates a CRLSelector suitable for finding a CRL for
 *  the Cert pointed to by "cert", setting the result in the
 *  defaultCRLCheckerState pointed to by "state".
 *
 * PARAMETERS
 *  "cert"
 *      Address of Cert for which a CRLSelector is to be constructed. Must be
 *      non-NULL.
 *  "state"
 *      Address of defaultCRLCheckerState whose CRLSelector is to be set. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a DefaultCrlCheckerState Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error
 */
PKIX_Error *
pkix_DefaultCRLChecker_Check_SetSelector(
        PKIX_PL_Cert *cert,
        pkix_DefaultCRLCheckerState *state,
        void *plContext)
{
        PKIX_PL_X500Name *certIssuer = NULL;
        PKIX_PL_BigInt *certSerialNumber = NULL;
        PKIX_PL_Date *nowDate = NULL;
        PKIX_ComCRLSelParams *comCrlSelParams = NULL;
        PKIX_CRLSelector *crlSelector = NULL;

        PKIX_ENTER
                (CERTCHAINCHECKER, "pkix_DefaultCRLChecker_Check_SetSelector");
        PKIX_NULLCHECK_TWO(cert, state);

        PKIX_CHECK(PKIX_PL_Cert_GetIssuer(cert, &certIssuer, plContext),
                PKIX_CERTGETISSUERFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetSerialNumber
                (cert, &certSerialNumber, plContext),
                PKIX_CERTGETSERIALNUMBERFAILED);

        if (state->testDate != NULL) {

                PKIX_INCREF(state->testDate);
                nowDate = state->testDate;

        } else {

                PKIX_CHECK(PKIX_PL_Date_Create_UTCTime
                        (NULL, &nowDate, plContext),
                        PKIX_DATECREATEUTCTIMEFAILED);
        }

        PKIX_CHECK(PKIX_ComCRLSelParams_Create
                (&comCrlSelParams, plContext),
                PKIX_COMCRLSELPARAMSCREATEFAILED);

        PKIX_CHECK(PKIX_ComCRLSelParams_AddIssuerName
                (comCrlSelParams, certIssuer, plContext),
                PKIX_COMCRLSELPARAMSADDISSUERNAMEFAILED);

        PKIX_CHECK(PKIX_ComCRLSelParams_SetDateAndTime
                (comCrlSelParams, nowDate, plContext),
                PKIX_COMCRLSELPARAMSSETDATEANDTIMEFAILED);

        PKIX_CHECK(PKIX_ComCRLSelParams_SetNISTPolicyEnabled
                (comCrlSelParams, state->nistCRLPolicyEnabled, plContext),
                PKIX_COMCERTSELPARAMSSETNISTPOLICYENABLEDFAILED);

        PKIX_CHECK(PKIX_CRLSelector_Create
                (NULL,
                NULL, /* never used? (PKIX_PL_Object *)checker, */
                &crlSelector,
                plContext),
                PKIX_CRLSELECTORCREATEFAILED);

        PKIX_CHECK(PKIX_CRLSelector_SetCommonCRLSelectorParams
                (crlSelector, comCrlSelParams, plContext),
                PKIX_CRLSELECTORSETCOMMONCRLSELECTORPARAMSFAILED);

        PKIX_DECREF(state->certIssuer);
        PKIX_INCREF(certIssuer);
        state->certIssuer = certIssuer;
        PKIX_DECREF(state->certSerialNumber);
        PKIX_INCREF(certSerialNumber);
        state->certSerialNumber = certSerialNumber;
        PKIX_DECREF(state->crlSelector);
        PKIX_INCREF(crlSelector);
        state->crlSelector = crlSelector;

        state->crlStoreIndex = 0;

        PKIX_CHECK(PKIX_List_GetLength
                    (state->certStores, &(state->numCrlStores), plContext),
                    PKIX_LISTGETLENGTHFAILED);

        state->certHasValidCrl = PKIX_FALSE;

cleanup:

        PKIX_DECREF(certIssuer);
        PKIX_DECREF(certSerialNumber);
        PKIX_DECREF(nowDate);
        PKIX_DECREF(comCrlSelParams);
        PKIX_DECREF(crlSelector);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_DefaultCRLChecker_Check_Store
 *
 * DESCRIPTION:
 *  Checks the certStore pointed to by "certStore" for a CRL that may determine
 *  whether the Cert pointed to by "cert" has been revoked.
 *
 * PARAMETERS
 *  "checker"
 *      Address of CertChainChecker which has the state data.
 *      Must be non-NULL.
 *  "cert"
 *      Address of Certificate that is to be validated. Must be non-NULL.
 *  "prevPublicKey"
 *      Address of previous public key in the backward chain. May be NULL.
 *  "state"
 *      Address of DefaultCrlCheckerState. Must be non-NULL.
 *  "unresolvedCriticalExtensions"
 *      A List OIDs. Not **yet** used in this checker function.
 *  "certStore"
 *      Address of the CertStore to be queried for a relevant CRL. Must be
 *      non-NULL.
 *  "pNBIOContext"
 *      Address at which platform-dependent information is stored if processing
 *      is suspended for non-blocking I/O. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a DefaultCrlCheckerState Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error
 */
static PKIX_Error *
pkix_DefaultCRLChecker_Check_Store(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_PL_PublicKey *prevPublicKey,
        pkix_DefaultCRLCheckerState *state,
        PKIX_List *unresolvedCriticalExtensions,
        PKIX_CertStore *certStore,
        void **pNBIOContext,
        void *plContext)
{

        PKIX_Boolean cacheFlag = PKIX_FALSE;
        PKIX_Boolean cacheHit = PKIX_FALSE;
        PKIX_UInt32 numEntries = 0;
        PKIX_UInt32 i = 0;
        PKIX_Int32 reasonCode = 0;
        PKIX_UInt32 allReasonCodes = 0;
        PKIX_List *crlList = NULL;
        PKIX_List *crlEntryList = NULL;
        PKIX_PL_CRLEntry *crlEntry = NULL;
        PKIX_Error *checkCrlFail = NULL;
        PKIX_CertStore_CRLCallback getCrls = NULL;
        void *nbioContext = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_DefaultCRLChecker_Check_Store");
        PKIX_NULLCHECK_TWO(checker, cert);
        PKIX_NULLCHECK_THREE(state, certStore, pNBIOContext);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        /* Are this CertStore's entries in cache? */
        PKIX_CHECK(PKIX_CertStore_GetCertStoreCacheFlag
                (certStore, &cacheFlag, plContext),
                PKIX_CERTSTOREGETCERTSTORECACHEFLAGFAILED);

        if (cacheFlag) {

                PKIX_CHECK(pkix_CacheCrlEntry_Lookup
                        (certStore,
                        state->certIssuer,
                        state->certSerialNumber,
                        &cacheHit,
                        &crlEntryList,
                        plContext),
                        PKIX_CACHECRLENTRYLOOKUPFAILED);

        }

        if (cacheHit) {

                /* Use cached data */

                PKIX_CHECK(PKIX_List_GetLength
                        (crlEntryList, &numEntries, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                for (i = 0; i < numEntries; i++) {

                    PKIX_CHECK(PKIX_List_GetItem
                            (crlEntryList,
                            i,
                            (PKIX_PL_Object **)&crlEntry,
                            plContext),
                            PKIX_LISTGETITEMFAILED);

                    PKIX_CHECK(PKIX_PL_CRLEntry_GetCRLEntryReasonCode
                            (crlEntry, &reasonCode, plContext),
                            PKIX_CRLENTRYGETCRLENTRYREASONCODEFAILED);

		    if (reasonCode >= 0) {
			if (reasonCode >= numReasonCodes) 
			    reasonCode = 0;

			allReasonCodes |= (1 << reasonCode);

			PKIX_DEFAULTCRLCHECKERSTATE_DEBUG_ARG
                                    ("CRL revocation Reason: %s\n ",
                                    reasonCodeMsgString[reasonCode]);

                    }

                    PKIX_DECREF(crlEntry);
                }

                state->reasonCodeMask |= allReasonCodes;

                if (allReasonCodes != 0) {

                        PKIX_ERROR(PKIX_CERTIFICATEREVOKEDBYCRL);
                }
 
                PKIX_DECREF(crlEntryList);

       } else {

                if (nbioContext == NULL) {
                        PKIX_CHECK(PKIX_CertStore_GetCRLCallback
                                (certStore, &getCrls, plContext),
                                PKIX_CERTSTOREGETCRLCALLBACKFAILED);

                        PKIX_CHECK(getCrls
                                (certStore,
                                state->crlSelector,
                                &nbioContext,
                                &crlList,
                                plContext),
                                PKIX_GETCRLSFAILED);
                } else {
                        PKIX_CHECK(PKIX_CertStore_CrlContinue
                                (certStore,
                                state->crlSelector,
                                &nbioContext,
                                &crlList,
                                plContext),
                                PKIX_CERTSTORECRLCONTINUEFAILED);
                }

                /*
                 * Verify Certificate validity: if one CertStore provides
                 * reason code, we stop here. Instead of exhausting all
                 * CertStores to get all possible reason codes associated
                 *  with the Cert. May be expanded if desire otherwise.
                 */

                if (crlList == NULL) {

                        *pNBIOContext = nbioContext;
                } else {

                        *pNBIOContext = NULL;

                        checkCrlFail = pkix_DefaultCRLChecker_CheckCRLs
                                (cert,
                                state->certIssuer,
                                state->certSerialNumber,
                                prevPublicKey,
                                crlList,
                                state,
                                &crlEntryList,
                                plContext);

                        if (checkCrlFail) {
                                if (crlEntryList != NULL) {
                                        /* Add to cache */
                                        PKIX_CHECK(pkix_CacheCrlEntry_Add
                                               (certStore,
                                               state->certIssuer,
                                               state->certSerialNumber,
                                               crlEntryList,
                                               plContext),
                                               PKIX_CACHECRLENTRYADDFAILED);
                                }
                                PKIX_ERROR(PKIX_CERTIFICATEREVOKEDBYCRL);
                        }
                }

                PKIX_DECREF(crlList);

        }

cleanup:
        PKIX_DECREF(crlEntryList);
        PKIX_DECREF(crlEntry);
        PKIX_DECREF(crlList);
        PKIX_DECREF(checkCrlFail);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_DefaultCRLChecker_Check_Helper
 *
 * DESCRIPTION:
 *  Check if the Cert has been revoked based on the CRL's data. It exhausts all
 *  CertStores and match CRLs at those stores for the certificate. It set the
 *  current date and issuer in CRLSelector to fetching CRL data from cache.
 *
 * PARAMETERS
 *  "checker"
 *      Address of CertChainChecker which has the state data.
 *      Must be non-NULL.
 *  "cert"
 *      Address of Certificate that is to be validated. Must be non-NULL.
 *  "prevPublicKey"
 *      Address of previous public key in the backward chain. May be NULL.
 *  "state"
 *      Address of DefaultCrlCheckerState. Must be non-NULL.
 *  "unresolvedCriticalExtensions"
 *      A List OIDs. Not **yet** used in this checker function.
 *  "useOnlyLocal"
 *      Boolean value indicating whether to use or bypass remote CertStores
 *  "pNBIOContext"
 *      Address at which platform-dependent information is stored if processing
 *      is suspended for non-blocking I/O. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Not Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error
 */
PKIX_Error *
pkix_DefaultCRLChecker_Check_Helper(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_PL_PublicKey *prevPublicKey,
        pkix_DefaultCRLCheckerState *state,
        PKIX_List *unresolvedCriticalExtensions,
        PKIX_Boolean useOnlyLocal,
        void **pNBIOContext,
        void *plContext)
{

        void *nbioContext = NULL;
        PKIX_Boolean certStoreCanBeUsed = PKIX_FALSE;
        PKIX_CertStore *certStore = NULL;
        PKIX_Error *storeError = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_DefaultCRLChecker_Check_Helper");
        PKIX_NULLCHECK_THREE(checker, cert, state);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL; /* prepare for Error exit */

        while ((state->crlStoreIndex) < (state->numCrlStores)) {

                /*
                 * For Basic CRL work, exit the loop when there is a valid
                 * CRL. For advance CRL, need to exhaust CRL until all
                 * reason masks are checked or a revocation is found.
                 */

                if (state->certHasValidCrl == PKIX_TRUE) {
                        break;
                }

                PKIX_CHECK(PKIX_List_GetItem
                        (state->certStores,
                        state->crlStoreIndex,
                        (PKIX_PL_Object **)&certStore,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                if (useOnlyLocal == PKIX_FALSE) {
                        certStoreCanBeUsed = PKIX_TRUE;
                } else {
                        PKIX_CHECK(PKIX_CertStore_GetLocalFlag
                                (certStore, &certStoreCanBeUsed, plContext),
                                PKIX_CERTSTOREGETLOCALFLAGFAILED);
                }

                if (certStoreCanBeUsed == PKIX_TRUE)
                {
			/* Catch and re-throw error to preserve information */
			storeError = pkix_DefaultCRLChecker_Check_Store
			        (checker,
			        cert,
			        prevPublicKey,
			        state,
			        unresolvedCriticalExtensions,
			        certStore,
			        &nbioContext,
			        plContext);
			PKIX_CHECK
				(storeError,
				PKIX_DEFAULTCRLCHECKERCHECKSTOREFAILED);

                        if (nbioContext != NULL) {
                                /* I/O still pending. Exit and resume later. */
                                *pNBIOContext = nbioContext;
                                goto cleanup;
                        }
                }

                PKIX_DECREF(certStore);
                state->crlStoreIndex++;
        } /* while ((state->crlStoreIndex) < (state->numCrlStores)) */

        if (state->nistCRLPolicyEnabled != PKIX_FALSE &&
            state->certHasValidCrl == PKIX_FALSE) {
            PKIX_ERROR(PKIX_CERTIFICATEDOESNTHAVEVALIDCRL);
        }

cleanup:

        PKIX_DECREF(certStore);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_DefaultCRLChecker_Check
 *
 * DESCRIPTION:
 *  Check if the Cert has been revoked based the CRLs data.  This function
 *  maintains the checker state to be current.
 *
 * PARAMETERS
 *  "checker"
 *      Address of CertChainChecker which has the state data.
 *      Must be non-NULL.
 *  "cert"
 *      Address of Certificate that is to be validated. Must be non-NULL.
 *  "unresolvedCriticalExtensions"
 *      A List OIDs. Not **yet** used in this checker function.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Not Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error
 */
static PKIX_Error *
pkix_DefaultCRLChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticalExtensions,
        void **pNBIOContext,
        void *plContext)
{
        pkix_DefaultCRLCheckerState *state = NULL;
        PKIX_PL_PublicKey *publicKey = NULL;
        PKIX_PL_PublicKey *newPublicKey = NULL;
        PKIX_Error *checkKeyUsageFail = NULL;
        PKIX_Boolean selfIssued = PKIX_FALSE;
        void *nbioContext = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_DefaultCRLChecker_Check");
        PKIX_NULLCHECK_THREE(checker, cert, pNBIOContext);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL; /* prepare for Error exit */

        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                    (checker, (PKIX_PL_Object **)&state, plContext),
                    PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetSubjectPublicKey
                    (cert, &publicKey, plContext),
                    PKIX_CERTGETSUBJECTPUBLICKEYFAILED);

        /*
         * If we already have a selector, we were in the middle of checking
         * when a certStore returned with non-blocking I/O pendning.
         */
        if ((state->crlSelector) == NULL) {
                state->certsRemaining--;

                PKIX_NULLCHECK_ONE(state->prevPublicKey);

                if (state->prevCertCrlSign == PKIX_FALSE) {
                        PKIX_ERROR
                                (PKIX_KEYUSAGEKEYCRLSIGNBITNOTON);
                }

                /* Set up CRLSelector */
                PKIX_CHECK(pkix_DefaultCRLChecker_Check_SetSelector
                        (cert, state, plContext),
                        PKIX_DEFAULTCRLCHECKERCHECKSETSELECTORFAILED);

        }

        PKIX_CHECK(pkix_DefaultCRLChecker_Check_Helper
                    (checker,
                    cert,
                    state->prevPublicKey,
                    state,
                    unresolvedCriticalExtensions,
                    PKIX_FALSE,
                    &nbioContext,
                    plContext),
                    PKIX_DEFAULTCRLCHECKERCHECKHELPERFAILED);

        if (nbioContext != NULL) {
                *pNBIOContext = nbioContext;
                goto cleanup;
        }

        PKIX_DECREF(state->crlSelector);

        /*
         * Some NIST test case in 4.5.* use different publicKeys for
         * Cert and its CRL on the chain. Self-issued Certs are used
         * to speciy multiple keys for those cases. That is why we apply
         * the following algorithm:
         *
         * Check if Cert is self-issued. If so, the public key of the Cert
         * that issues this Cert (old key) can be used together with this
         * current key (new key) for key verification. If there are multiple
         * self-issued certs, keys of those Certs (old keys) can also be used
         * for key verification. Old key(s) is saved in a list (PrevPublickKey-
         * List) and cleared when a Cert is no longer self-issued.
         * PrevPublicKey keep key of the previous Cert.
         * PrevPublicKeyList keep key(s) of Cert before the previous one.
         */
        PKIX_CHECK(pkix_IsCertSelfIssued(cert, &selfIssued, plContext),
                    PKIX_ISCERTSELFISSUEFAILED);

        if (selfIssued == PKIX_TRUE) {

                if (state->prevPublicKeyList == NULL) {

                        PKIX_CHECK(PKIX_List_Create
                            (&state->prevPublicKeyList, plContext),
                            PKIX_LISTCREATEFAILED);

                }

                PKIX_CHECK(PKIX_List_AppendItem
                            (state->prevPublicKeyList,
                            (PKIX_PL_Object *) state->prevPublicKey,
                            plContext),
                            PKIX_LISTAPPENDITEMFAILED);

        } else {
                /* Not self-issued Cert any more, clear old key(s) saved */
                PKIX_DECREF(state->prevPublicKeyList);
        }

        /* Make inheritance and save current Public Key */
        PKIX_CHECK(PKIX_PL_PublicKey_MakeInheritedDSAPublicKey
                    (publicKey, state->prevPublicKey, &newPublicKey, plContext),
                    PKIX_PUBLICKEYMAKEINHERITEDDSAPUBLICKEYFAILED);

        if (newPublicKey == NULL){
                PKIX_INCREF(publicKey);
                newPublicKey = publicKey;
        }

        PKIX_DECREF(state->prevPublicKey);
        PKIX_INCREF(newPublicKey);
        state->prevPublicKey = newPublicKey;

        /* Save current Cert's crlSign bit for CRL checking later */
        if (state->certsRemaining != 0) {
                checkKeyUsageFail = PKIX_PL_Cert_VerifyKeyUsage
                        (cert, PKIX_CRL_SIGN, plContext);

                state->prevCertCrlSign = (checkKeyUsageFail == NULL)?
                        PKIX_TRUE : PKIX_FALSE;

                PKIX_DECREF(checkKeyUsageFail);
        }

/*
        PKIX_CHECK(PKIX_CertChainChecker_SetCertChainCheckerState
                (checker, (PKIX_PL_Object *)state, plContext),
                PKIX_CERTCHAINCHECKERSETCERTCHAINCHECKERSTATEFAILED);
 */

cleanup:

        PKIX_DECREF(state);
        PKIX_DECREF(publicKey);
        PKIX_DECREF(newPublicKey);
        PKIX_DECREF(checkKeyUsageFail);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_DefaultCRLChecker_Initialize
 *
 * DESCRIPTION:
 *  Create a CertChainChecker with DefaultCRLCheckerState.
 *
 * PARAMETERS
 *  "certStores"
 *      Address of CertStore List to be stored in state. Must be non-NULL.
 *  "testDate"
 *      Address of PKIX_PL_Date to be checked. May be NULL.
 *  "trustedPubKey"
 *      Address of Public Key of Trust Anchor. Must be non-NULL.
 *  "certsRemaining"
 *      Number of certificates remaining in the chain.
 *  "nistPolicyEnabled"
 *      Enable NIST crl policy.
 *  "pChecker"
 *      Address where object pointer will be stored. Must be non-NULL.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error
 */
PKIX_Error *
pkix_DefaultCRLChecker_Initialize(
        PKIX_List *certStores,
        PKIX_PL_Date *testDate,
        PKIX_PL_PublicKey *trustedPubKey,
        PKIX_UInt32 certsRemaining,
        PKIX_Boolean nistPolicyEnabled,
        PKIX_CertChainChecker **pChecker,
        void *plContext)
{
        pkix_DefaultCRLCheckerState *state = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_DefaultCRLChecker_Initialize");
        PKIX_NULLCHECK_TWO(certStores, pChecker);

        PKIX_CHECK(pkix_DefaultCRLCheckerState_Create
                    (certStores,
                    testDate,
                    trustedPubKey,
                    certsRemaining,
                    nistPolicyEnabled, 
                    &state,
                    plContext),
                    PKIX_DEFAULTCRLCHECKERSTATECREATEFAILED);

        PKIX_CHECK(PKIX_CertChainChecker_Create
                    (pkix_DefaultCRLChecker_Check,
                    PKIX_FALSE,
                    PKIX_FALSE,
                    NULL,
                    (PKIX_PL_Object *) state,
                    pChecker,
                    plContext),
                    PKIX_CERTCHAINCHECKERCREATEFAILED);

cleanup:

        PKIX_DECREF(state);

        PKIX_RETURN(CERTCHAINCHECKER);
}
