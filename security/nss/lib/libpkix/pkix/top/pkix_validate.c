/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_validate.c
 *
 * Top level validateChain function
 *
 */

#include "pkix_validate.h"
#include "pkix_pl_common.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_AddToVerifyLog
 * DESCRIPTION:
 *
 *  This function returns immediately if the address for the VerifyNode tree
 *  pointed to by "pVerifyTree" is NULL. Otherwise it creates a new VerifyNode
 *  from the Cert pointed to by "cert" and the Error pointed to by "error",
 *  and inserts it at the depth in the VerifyNode tree determined by "depth". A
 *  depth of zero means that this function creates the root node of a new tree.
 *
 *  Note: this function does not include the means of choosing among branches
 *  of a tree. It is intended for non-branching trees, that is, where each
 *  parent node has only a single child node.
 *
 * PARAMETERS:
 *  "cert"
 *      The address of the Cert to be included in the new VerifyNode. Must be
 *      non-NULL.
 *  "depth"
 *      The UInt32 value of the depth.
 *  "error"
 *      The address of the Error to be included in the new VerifyNode.
 *  "pVerifyTree"
 *      The address of the VerifyNode tree into which the created VerifyNode
 *      is to be inserted. The node is not created if VerifyTree is NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Validate Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_AddToVerifyLog(
        PKIX_PL_Cert *cert,
        PKIX_UInt32 depth,
        PKIX_Error *error,
        PKIX_VerifyNode **pVerifyTree,
        void *plContext)
{

        PKIX_VerifyNode *verifyNode = NULL;

        PKIX_ENTER(VALIDATE, "pkix_AddToVerifyLog");
        PKIX_NULLCHECK_ONE(cert);

        if (pVerifyTree) { /* nothing to do if no address given for log */

                PKIX_CHECK(pkix_VerifyNode_Create
                        (cert, depth, error, &verifyNode, plContext),
                        PKIX_VERIFYNODECREATEFAILED);

                if (depth == 0) {
                        /* We just created the root node */
                        *pVerifyTree = verifyNode;
                } else {
                        PKIX_CHECK(pkix_VerifyNode_AddToChain
                                (*pVerifyTree, verifyNode, plContext),
                                PKIX_VERIFYNODEADDTOCHAINFAILED);
                }
        }

cleanup:

        PKIX_RETURN(VALIDATE);

}

/*
 * FUNCTION: pkix_CheckCert
 * DESCRIPTION:
 *
 *  Checks whether the Cert pointed to by "cert" successfully validates
 *  using the List of CertChainCheckers pointed to by "checkers". If the
 *  certificate does not validate, an Error pointer is returned.
 *
 *  This function should be called initially with the UInt32 pointed to by
 *  "pCheckerIndex" containing zero, and the pointer at "pNBIOContext"
 *  containing NULL. If a checker does non-blocking I/O, this function will
 *  return with the index of that checker stored at "pCheckerIndex" and a
 *  platform-dependent non-blocking I/O context stored at "pNBIOContext".
 *  A subsequent call to this function with those values intact will allow the
 *  checking to resume where it left off. This should be repeated until the
 *  function returns with NULL stored at "pNBIOContext".
 *
 * PARAMETERS:
 *  "cert"
 *      Address of Cert to validate. Must be non-NULL.
 *  "checkers"
 *      List of CertChainCheckers which must each validate the certificate.
 *      Must be non-NULL.
 *  "checkedExtOIDs"
 *      List of PKIX_PL_OID that has been processed. If called from building
 *      chain, it is the list of critical extension OIDs that has been
 *      processed prior to validation. May be NULL.
 *  "pCheckerIndex"
 *      Address at which is stored the the index, within the List "checkers",
 *      of a checker whose processing was interrupted by non-blocking I/O.
 *      Must be non-NULL.
 *  "pNBIOContext"
 *      Address at which is stored platform-specific non-blocking I/O context.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Validate Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_CheckCert(
        PKIX_PL_Cert *cert,
        PKIX_List *checkers,
        PKIX_List *checkedExtOIDsList,
        PKIX_UInt32 *pCheckerIndex,
        void **pNBIOContext,
        void *plContext)
{
        PKIX_CertChainChecker_CheckCallback checkerCheck = NULL;
        PKIX_CertChainChecker *checker = NULL;
        PKIX_List *unresCritExtOIDs = NULL;
        PKIX_UInt32 numCheckers;
        PKIX_UInt32 numUnresCritExtOIDs = 0;
        PKIX_UInt32 checkerIndex = 0;
        void *nbioContext = NULL;

        PKIX_ENTER(VALIDATE, "pkix_CheckCert");
        PKIX_NULLCHECK_FOUR(cert, checkers, pCheckerIndex, pNBIOContext);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL; /* prepare for case of error exit */

        PKIX_CHECK(PKIX_PL_Cert_GetCriticalExtensionOIDs
                    (cert, &unresCritExtOIDs, plContext),
                    PKIX_CERTGETCRITICALEXTENSIONOIDSFAILED);

        PKIX_CHECK(PKIX_List_GetLength(checkers, &numCheckers, plContext),
                    PKIX_LISTGETLENGTHFAILED);

        for (checkerIndex = *pCheckerIndex;
                checkerIndex < numCheckers;
                checkerIndex++) {

                PKIX_CHECK(PKIX_List_GetItem
                        (checkers,
                        checkerIndex,
                        (PKIX_PL_Object **)&checker,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(PKIX_CertChainChecker_GetCheckCallback
                        (checker, &checkerCheck, plContext),
                        PKIX_CERTCHAINCHECKERGETCHECKCALLBACKFAILED);

                PKIX_CHECK(checkerCheck(checker, cert, unresCritExtOIDs,
                                        &nbioContext,  plContext),
                           PKIX_CERTCHAINCHECKERCHECKFAILED);

                if (nbioContext != NULL) {
                        *pCheckerIndex = checkerIndex;
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }

                PKIX_DECREF(checker);
        }

        if (unresCritExtOIDs){

#ifdef PKIX_VALIDATEDEBUG
                {
                        PKIX_PL_String *oidString = NULL;
                        PKIX_UInt32 length;
                        char *oidAscii = NULL;
                        PKIX_TOSTRING(unresCritExtOIDs, &oidString, plContext,
                                PKIX_LISTTOSTRINGFAILED);
                        PKIX_CHECK(PKIX_PL_String_GetEncoded
                                (oidString,
                                PKIX_ESCASCII,
                                (void **) &oidAscii,
                                &length,
                                plContext),
                                PKIX_STRINGGETENCODEDFAILED);
                        PKIX_VALIDATE_DEBUG_ARG
                                ("unrecognized critical extension OIDs:"
                                " %s\n", oidAscii);
                        PKIX_DECREF(oidString);
                        PKIX_PL_Free(oidAscii, plContext);
                }
#endif

                if (checkedExtOIDsList != NULL) {
                 /* Take out OID's that had been processed, if any */
                        PKIX_CHECK(pkix_List_RemoveItems
                                (unresCritExtOIDs,
                                checkedExtOIDsList,
                                plContext),
                                PKIX_LISTREMOVEITEMSFAILED);
                }

                PKIX_CHECK(PKIX_List_GetLength
                        (unresCritExtOIDs, &numUnresCritExtOIDs, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                if (numUnresCritExtOIDs != 0){
                        PKIX_ERROR(PKIX_UNRECOGNIZEDCRITICALEXTENSION);
                }

        }

cleanup:

        PKIX_DECREF(checker);
        PKIX_DECREF(unresCritExtOIDs);

        PKIX_RETURN(VALIDATE);

}

/*
 * FUNCTION: pkix_InitializeCheckers
 * DESCRIPTION:
 *
 *  Creates several checkers and initializes them with values derived from the
 *  TrustAnchor pointed to by "anchor", the ProcessingParams pointed to by
 *  "procParams", and the number of Certs in the Chain, represented by
 *  "numCerts". The List of checkers is stored at "pCheckers".
 *
 * PARAMETERS:
 *  "anchor"
 *      Address of TrustAnchor used to initialize the SignatureChecker and
 *      NameChainingChecker. Must be non-NULL.
 *  "procParams"
 *      Address of ProcessingParams used to initialize the ExpirationChecker
 *      and TargetCertChecker. Must be non-NULL.
 *  "numCerts"
 *      Number of certificates in the CertChain.
 *  "pCheckers"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Validate Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_InitializeCheckers(
        PKIX_TrustAnchor *anchor,
        PKIX_ProcessingParams *procParams,
        PKIX_UInt32 numCerts,
        PKIX_List **pCheckers,
        void *plContext)
{
        PKIX_CertChainChecker *targetCertChecker = NULL;
        PKIX_CertChainChecker *expirationChecker = NULL;
        PKIX_CertChainChecker *nameChainingChecker = NULL;
        PKIX_CertChainChecker *nameConstraintsChecker = NULL;
        PKIX_CertChainChecker *basicConstraintsChecker = NULL;
        PKIX_CertChainChecker *policyChecker = NULL;
        PKIX_CertChainChecker *sigChecker = NULL;
        PKIX_CertChainChecker *defaultCrlChecker = NULL;
        PKIX_CertChainChecker *userChecker = NULL;
        PKIX_PL_X500Name *trustedCAName = NULL;
        PKIX_PL_PublicKey *trustedPubKey = NULL;
        PKIX_List *checkers = NULL;
        PKIX_PL_Date *testDate = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_PL_Cert *trustedCert = NULL;
        PKIX_PL_CertNameConstraints *trustedNC = NULL;
        PKIX_List *initialPolicies = NULL;
        PKIX_Boolean policyQualifiersRejected = PKIX_FALSE;
        PKIX_Boolean initialPolicyMappingInhibit = PKIX_FALSE;
        PKIX_Boolean initialAnyPolicyInhibit = PKIX_FALSE;
        PKIX_Boolean initialExplicitPolicy = PKIX_FALSE;
        PKIX_List *userCheckersList = NULL;
        PKIX_List *certStores = NULL;
        PKIX_UInt32 numCertCheckers = 0;
        PKIX_UInt32 i;

        PKIX_ENTER(VALIDATE, "pkix_InitializeCheckers");
        PKIX_NULLCHECK_THREE(anchor, procParams, pCheckers);
        PKIX_CHECK(PKIX_List_Create(&checkers, plContext),
                    PKIX_LISTCREATEFAILED);

        /*
         * The TrustAnchor may have been created using CreateWithCert
         * (in which case GetCAPublicKey and GetCAName will return NULL)
         * or may have been created using CreateWithNameKeyPair (in which
         * case GetTrustedCert will return NULL. So we call GetTrustedCert
         * and populate trustedPubKey and trustedCAName accordingly.
         */

        PKIX_CHECK(PKIX_TrustAnchor_GetTrustedCert
                (anchor, &trustedCert, plContext),
                    PKIX_TRUSTANCHORGETTRUSTEDCERTFAILED);

        if (trustedCert){
                PKIX_CHECK(PKIX_PL_Cert_GetSubjectPublicKey
                            (trustedCert, &trustedPubKey, plContext),
                            PKIX_CERTGETSUBJECTPUBLICKEYFAILED);

                PKIX_CHECK(PKIX_PL_Cert_GetSubject
                            (trustedCert, &trustedCAName, plContext),
                            PKIX_CERTGETSUBJECTFAILED);
        } else {
                PKIX_CHECK(PKIX_TrustAnchor_GetCAPublicKey
                            (anchor, &trustedPubKey, plContext),
                            PKIX_TRUSTANCHORGETCAPUBLICKEYFAILED);

                PKIX_CHECK(PKIX_TrustAnchor_GetCAName
                            (anchor, &trustedCAName, plContext),
                            PKIX_TRUSTANCHORGETCANAMEFAILED);
        }

        PKIX_NULLCHECK_TWO(trustedPubKey, trustedCAName);

        PKIX_CHECK(PKIX_TrustAnchor_GetNameConstraints
                (anchor, &trustedNC, plContext),
                PKIX_TRUSTANCHORGETNAMECONSTRAINTSFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_GetTargetCertConstraints
                (procParams, &certSelector, plContext),
                PKIX_PROCESSINGPARAMSGETTARGETCERTCONSTRAINTSFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_GetDate
                (procParams, &testDate, plContext),
                PKIX_PROCESSINGPARAMSGETDATEFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_GetInitialPolicies
                (procParams, &initialPolicies, plContext),
                PKIX_PROCESSINGPARAMSGETINITIALPOLICIESFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_GetPolicyQualifiersRejected
                (procParams, &policyQualifiersRejected, plContext),
                PKIX_PROCESSINGPARAMSGETPOLICYQUALIFIERSREJECTEDFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_IsPolicyMappingInhibited
                (procParams, &initialPolicyMappingInhibit, plContext),
                PKIX_PROCESSINGPARAMSISPOLICYMAPPINGINHIBITEDFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_IsAnyPolicyInhibited
                (procParams, &initialAnyPolicyInhibit, plContext),
                PKIX_PROCESSINGPARAMSISANYPOLICYINHIBITEDFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_IsExplicitPolicyRequired
                (procParams, &initialExplicitPolicy, plContext),
                PKIX_PROCESSINGPARAMSISEXPLICITPOLICYREQUIREDFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_GetCertStores
                (procParams, &certStores, plContext),
                PKIX_PROCESSINGPARAMSGETCERTSTORESFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_GetCertChainCheckers
                (procParams, &userCheckersList, plContext),
                PKIX_PROCESSINGPARAMSGETCERTCHAINCHECKERSFAILED);

        /* now, initialize all the checkers */
        PKIX_CHECK(pkix_TargetCertChecker_Initialize
                (certSelector, numCerts, &targetCertChecker, plContext),
                PKIX_TARGETCERTCHECKERINITIALIZEFAILED);

        PKIX_CHECK(pkix_ExpirationChecker_Initialize
                (testDate, &expirationChecker, plContext),
                PKIX_EXPIRATIONCHECKERINITIALIZEFAILED);

        PKIX_CHECK(pkix_NameChainingChecker_Initialize
                (trustedCAName, &nameChainingChecker, plContext),
                PKIX_NAMECHAININGCHECKERINITIALIZEFAILED);

        PKIX_CHECK(pkix_NameConstraintsChecker_Initialize
                (trustedNC, numCerts, &nameConstraintsChecker, plContext),
                PKIX_NAMECONSTRAINTSCHECKERINITIALIZEFAILED);

        PKIX_CHECK(pkix_BasicConstraintsChecker_Initialize
                (numCerts, &basicConstraintsChecker, plContext),
                PKIX_BASICCONSTRAINTSCHECKERINITIALIZEFAILED);

        PKIX_CHECK(pkix_PolicyChecker_Initialize
                (initialPolicies,
                policyQualifiersRejected,
                initialPolicyMappingInhibit,
                initialExplicitPolicy,
                initialAnyPolicyInhibit,
                numCerts,
                &policyChecker,
                plContext),
                PKIX_POLICYCHECKERINITIALIZEFAILED);

        PKIX_CHECK(pkix_SignatureChecker_Initialize
                    (trustedPubKey, numCerts, &sigChecker, plContext),
                    PKIX_SIGNATURECHECKERINITIALIZEFAILED);

        if (userCheckersList != NULL) {

                PKIX_CHECK(PKIX_List_GetLength
                    (userCheckersList, &numCertCheckers, plContext),
                    PKIX_LISTGETLENGTHFAILED);

                for (i = 0; i < numCertCheckers; i++) {

                        PKIX_CHECK(PKIX_List_GetItem
                            (userCheckersList,
                            i,
                            (PKIX_PL_Object **) &userChecker,
                            plContext),
                            PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(PKIX_List_AppendItem
                            (checkers,
                            (PKIX_PL_Object *)userChecker,
                            plContext),
                            PKIX_LISTAPPENDITEMFAILED);

                        PKIX_DECREF(userChecker);
                }
        }

        PKIX_CHECK(PKIX_List_AppendItem
            (checkers, (PKIX_PL_Object *)targetCertChecker, plContext),
            PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
            (checkers, (PKIX_PL_Object *)expirationChecker, plContext),
            PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
            (checkers, (PKIX_PL_Object *)nameChainingChecker, plContext),
            PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
            (checkers, (PKIX_PL_Object *)nameConstraintsChecker, plContext),
            PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
            (checkers, (PKIX_PL_Object *)basicConstraintsChecker, plContext),
            PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
            (checkers, (PKIX_PL_Object *)policyChecker, plContext),
            PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
            (checkers, (PKIX_PL_Object *)sigChecker, plContext),
            PKIX_LISTAPPENDITEMFAILED);

        *pCheckers = checkers;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(checkers);
        }

        PKIX_DECREF(certSelector);
        PKIX_DECREF(testDate);
        PKIX_DECREF(initialPolicies);
        PKIX_DECREF(targetCertChecker);
        PKIX_DECREF(expirationChecker);
        PKIX_DECREF(nameChainingChecker);
        PKIX_DECREF(nameConstraintsChecker);
        PKIX_DECREF(basicConstraintsChecker);
        PKIX_DECREF(policyChecker);
        PKIX_DECREF(sigChecker);
        PKIX_DECREF(trustedCAName);
        PKIX_DECREF(trustedPubKey);
        PKIX_DECREF(trustedNC);
        PKIX_DECREF(trustedCert);
        PKIX_DECREF(defaultCrlChecker);
        PKIX_DECREF(userCheckersList);
        PKIX_DECREF(certStores);
        PKIX_DECREF(userChecker);

        PKIX_RETURN(VALIDATE);
}

/*
 * FUNCTION: pkix_RetrieveOutputs
 * DESCRIPTION:
 *
 *  This function queries the respective states of the List of checkers in
 *  "checkers" to to obtain the final public key from the SignatureChecker
 *  and the policy tree from the PolicyChecker, storing those values at
 *  "pFinalSubjPubKey" and "pPolicyTree", respectively.
 *
 * PARAMETERS:
 *  "checkers"
 *      Address of List of checkers to be queried. Must be non-NULL.
 *  "pFinalSubjPubKey"
 *      Address where final public key will be stored. Must be non-NULL.
 *  "pPolicyTree"
 *      Address where policy tree will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Validate Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_RetrieveOutputs(
        PKIX_List *checkers,
        PKIX_PL_PublicKey **pFinalSubjPubKey,
        PKIX_PolicyNode **pPolicyTree,
        void *plContext)
{
        PKIX_PL_PublicKey *finalSubjPubKey = NULL;
        PKIX_PolicyNode *validPolicyTree = NULL;
        PKIX_CertChainChecker *checker = NULL;
        PKIX_PL_Object *state = NULL;
        PKIX_UInt32 numCheckers = 0;
        PKIX_UInt32 type;
        PKIX_Int32 j;

        PKIX_ENTER(VALIDATE, "pkix_RetrieveOutputs");

        PKIX_NULLCHECK_TWO(checkers, pPolicyTree);

        /*
         * To optimize the search, we guess that the sigChecker is
         * last in the tree and is preceded by the policyChecker. We
         * search toward the front of the chain. Remember that List
         * items are indexed 0..(numItems - 1).
         */

        PKIX_CHECK(PKIX_List_GetLength(checkers, &numCheckers, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (j = numCheckers - 1; j >= 0; j--){
                PKIX_CHECK(PKIX_List_GetItem
                        (checkers, j, (PKIX_PL_Object **)&checker, plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                        (checker, &state, plContext),
                        PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

                /* user defined checker may have no state */
                if (state != NULL) {

                    PKIX_CHECK(PKIX_PL_Object_GetType(state, &type, plContext),
                            PKIX_OBJECTGETTYPEFAILED);

                    if (type == PKIX_SIGNATURECHECKERSTATE_TYPE){
                        /* final pubKey will include any inherited DSA params */
                        finalSubjPubKey =
                            ((pkix_SignatureCheckerState *)state)->
                                prevPublicKey;
                        PKIX_INCREF(finalSubjPubKey);
                        *pFinalSubjPubKey = finalSubjPubKey;
                    }

                    if (type == PKIX_CERTPOLICYCHECKERSTATE_TYPE) {
                        validPolicyTree =
                            ((PKIX_PolicyCheckerState *)state)->validPolicyTree;
                        break;
                    }
                }

                PKIX_DECREF(checker);
                PKIX_DECREF(state);
        }

        PKIX_INCREF(validPolicyTree);
        *pPolicyTree = validPolicyTree;

cleanup:

        PKIX_DECREF(checker);
        PKIX_DECREF(state);

        PKIX_RETURN(VALIDATE);

}

/*
 * FUNCTION: pkix_CheckChain
 * DESCRIPTION:
 *
 *  Checks whether the List of Certs pointed to by "certs", containing
 *  "numCerts" entries, successfully validates using each CertChainChecker in
 *  the List pointed to by "checkers" and has not been revoked, according to any
 *  of the Revocation Checkers in the List pointed to by "revChecker". Checkers
 *  are expected to remove from "removeCheckedExtOIDs" and extensions that they
 *  process. Indices to the certChain and the checkerChain are obtained and
 *  returned in "pCertCheckedIndex" and "pCheckerIndex", respectively. These
 *  should be set to zero prior to the initial call, but may be changed (and
 *  must be supplied on subsequent calls) if processing is suspended for non-
 *  blocking I/O. Each time a Cert passes from being validated by one of the
 *  CertChainCheckers to being checked by a Revocation Checker, the Boolean
 *  stored at "pRevChecking" is changed from FALSE to TRUE. If the Cert is
 *  rejected by a Revocation Checker, its reason code is returned at
 *  "pReasonCode. If the List of Certs successfully validates, the public key i
 *  the final certificate is obtained and stored at "pFinalSubjPubKey" and the
 *  validPolicyTree, which could be NULL, is stored at pPolicyTree. If the List
 *  of Certs fails to validate, an Error pointer is returned.
 *
 *  If "pVerifyTree" is non-NULL, a chain of VerifyNodes is created which
 *  tracks the results of the validation. That is, either each node in the
 *  chain has a NULL Error component, or the last node contains an Error
 *  which indicates why the validation failed.
 *
 *  The number of Certs in the List, represented by "numCerts", is used to
 *  determine which Cert is the final Cert.
 *
 * PARAMETERS:
 *  "certs"
 *      Address of List of Certs to validate. Must be non-NULL.
 *  "numCerts"
 *      Number of certificates in the List of certificates.
 *  "checkers"
 *      List of CertChainCheckers which must each validate the List of
 *      certificates. Must be non-NULL.
 *  "revChecker"
 *      List of RevocationCheckers which must each not reject the List of
 *      certificates. May be empty, but must be non-NULL.
 *  "removeCheckedExtOIDs"
 *      List of PKIX_PL_OID that has been processed. If called from building
 *      chain, it is the list of critical extension OIDs that has been
 *      processed prior to validation. Extension OIDs that may be processed by
 *      user defined checker processes are also in the list. May be NULL.
 *  "procParams"
 *      Address of ProcessingParams used to initialize various checkers. Must
 *      be non-NULL.
 *  "pCertCheckedIndex"
 *      Address where Int32 index to the Cert chain is obtained and
 *      returned. Must be non-NULL.
 *  "pCheckerIndex"
 *      Address where Int32 index to the CheckerChain is obtained and
 *      returned. Must be non-NULL.
 *  "pRevChecking"
 *      Address where Boolean is obtained and returned, indicating, if FALSE,
 *      that CertChainCheckers are being called; or, if TRUE, that RevChecker
 *      are being called. Must be non-NULL.
 *  "pReasonCode"
 *      Address where UInt32 results of revocation checking are stored. Must be
 *      non-NULL.
 *  "pNBIOContext"
 *      Address where platform-dependent context is stored if checking is
 *      suspended for non-blocking I/O. Must be non-NULL.
 *  "pFinalSubjPubKey"
 *      Address where the final public key will be stored. Must be non-NULL.
 *  "pPolicyTree"
 *      Address where the final validPolicyTree is stored. Must be non-NULL.
 *  "pVerifyTree"
 *      Address where a VerifyTree is stored, if non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Validate Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CheckChain(
        PKIX_List *certs,
        PKIX_UInt32 numCerts,
        PKIX_TrustAnchor *anchor,
        PKIX_List *checkers,
        PKIX_RevocationChecker *revChecker,
        PKIX_List *removeCheckedExtOIDs,
        PKIX_ProcessingParams *procParams,
        PKIX_UInt32 *pCertCheckedIndex,
        PKIX_UInt32 *pCheckerIndex,
        PKIX_Boolean *pRevChecking,
        PKIX_UInt32 *pReasonCode,
        void **pNBIOContext,
        PKIX_PL_PublicKey **pFinalSubjPubKey,
        PKIX_PolicyNode **pPolicyTree,
        PKIX_VerifyNode **pVerifyTree,
        void *plContext)
{
        PKIX_UInt32 j = 0;
        PKIX_Boolean revChecking = PKIX_FALSE;
        PKIX_Error *checkCertError = NULL;
        void *nbioContext = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_PL_Cert *issuer = NULL;
        PKIX_PL_NssContext *nssContext = NULL;
        CERTCertList *certList = NULL;
        const CERTChainVerifyCallback *chainVerifyCallback = NULL;
        CERTCertificate *nssCert = NULL;

        PKIX_ENTER(VALIDATE, "pkix_CheckChain");
        PKIX_NULLCHECK_FOUR(certs, checkers, revChecker, pCertCheckedIndex);
        PKIX_NULLCHECK_FOUR(pCheckerIndex, pRevChecking, pReasonCode, anchor);
        PKIX_NULLCHECK_THREE(pNBIOContext, pFinalSubjPubKey, pPolicyTree);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;
        revChecking = *pRevChecking;
        nssContext = (PKIX_PL_NssContext *)plContext;
        chainVerifyCallback = &nssContext->chainVerifyCallback;

        if (chainVerifyCallback->isChainValid != NULL) {
                PRBool chainOK = PR_FALSE; /*assume failure*/
                SECStatus rv;

                certList = CERT_NewCertList();
                if (certList == NULL) {
                        PKIX_ERROR_ALLOC_ERROR();
                }

                /* Add the trust anchor to the list */
                PKIX_CHECK(PKIX_TrustAnchor_GetTrustedCert
                        (anchor, &cert, plContext),
                        PKIX_TRUSTANCHORGETTRUSTEDCERTFAILED);

                PKIX_CHECK(
                        PKIX_PL_Cert_GetCERTCertificate(cert, &nssCert, plContext),
                        PKIX_CERTGETCERTCERTIFICATEFAILED);

                rv = CERT_AddCertToListHead(certList, nssCert);
                if (rv != SECSuccess) {
                        PKIX_ERROR_ALLOC_ERROR();
                }
                /* the certList takes ownership of nssCert on success */
                nssCert = NULL;
                PKIX_DECREF(cert);

                /* Add the rest of the chain to the list */
                for (j = *pCertCheckedIndex; j < numCerts; j++) {
                        PKIX_CHECK(PKIX_List_GetItem(
                                certs, j, (PKIX_PL_Object **)&cert, plContext),
                                PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(
                                PKIX_PL_Cert_GetCERTCertificate(cert, &nssCert, plContext),
                                PKIX_CERTGETCERTCERTIFICATEFAILED);

                        rv = CERT_AddCertToListHead(certList, nssCert);
                        if (rv != SECSuccess) {
                                PKIX_ERROR_ALLOC_ERROR();
                        }
                        /* the certList takes ownership of nssCert on success */
                        nssCert = NULL;
                        PKIX_DECREF(cert);
                }

                rv = (*chainVerifyCallback->isChainValid)
                     (chainVerifyCallback->isChainValidArg, certList, &chainOK);
                if (rv != SECSuccess) {
                       PKIX_ERROR_FATAL(PKIX_CHAINVERIFYCALLBACKFAILED);
                }

                if (!chainOK) {
                        PKIX_ERROR(PKIX_CHAINVERIFYCALLBACKFAILED);
                }

        }

        PKIX_CHECK(PKIX_TrustAnchor_GetTrustedCert
                (anchor, &cert, plContext),
                   PKIX_TRUSTANCHORGETTRUSTEDCERTFAILED);

        for (j = *pCertCheckedIndex; j < numCerts; j++) {

                PORT_Assert(cert);
                PKIX_DECREF(issuer);
                issuer = cert;
                cert = NULL;

                PKIX_CHECK(PKIX_List_GetItem(
                               certs, j, (PKIX_PL_Object **)&cert, plContext),
                           PKIX_LISTGETITEMFAILED);
                
                /* check if cert pointer is valid */
                PORT_Assert(cert);
                if (cert == NULL) {
                    continue;
                }

                if (revChecking == PKIX_FALSE) {

                        PKIX_CHECK(pkix_CheckCert
                                (cert,
                                checkers,
                                removeCheckedExtOIDs,
                                pCheckerIndex,
                                &nbioContext,
                                plContext),
                                PKIX_CHECKCERTFAILED);

                        if (nbioContext != NULL) {
                                *pCertCheckedIndex = j;
                                *pRevChecking = revChecking;
                                *pNBIOContext = nbioContext;
                                goto cleanup;
                        }

                        revChecking = PKIX_TRUE;
                        *pCheckerIndex = 0;
                }

                if (revChecking == PKIX_TRUE) {
                        PKIX_RevocationStatus revStatus;
                        pkixErrorResult =
                            PKIX_RevocationChecker_Check(
                                      cert, issuer, revChecker,
                                      procParams, PKIX_TRUE,
                                      (j == numCerts - 1) ? PKIX_TRUE : PKIX_FALSE,
                                      &revStatus, pReasonCode,
                                      &nbioContext, plContext);
                        if (nbioContext != NULL) {
                                *pCertCheckedIndex = j;
                                *pRevChecking = revChecking;
                                *pNBIOContext = nbioContext;
                                goto cleanup;
                        }
                        if (revStatus == PKIX_RevStatus_Revoked ||
                            pkixErrorResult) {
                            if (!pkixErrorResult) {
                                /* if pkixErrorResult is returned then
                                 * use it as it has a detailed revocation
                                 * error code. Otherwise create a new error */
                                PKIX_ERROR_CREATE(VALIDATE,
                                                  PKIX_CERTIFICATEREVOKED,
                                                  pkixErrorResult);
                            }
                            goto cleanup;
                        }
                        revChecking = PKIX_FALSE;
                        *pCheckerIndex = 0;
                }

                PKIX_CHECK(pkix_AddToVerifyLog
                        (cert, j, NULL, pVerifyTree, plContext),
                        PKIX_ADDTOVERIFYLOGFAILED);
        }

        PKIX_CHECK(pkix_RetrieveOutputs
                    (checkers, pFinalSubjPubKey, pPolicyTree, plContext),
                    PKIX_RETRIEVEOUTPUTSFAILED);

        *pNBIOContext = NULL;

cleanup:
        if (PKIX_ERROR_RECEIVED && cert) {
            checkCertError = pkixErrorResult;
            
            PKIX_CHECK_FATAL(
                pkix_AddToVerifyLog(cert, j, checkCertError, pVerifyTree,
                                    plContext),
                PKIX_ADDTOVERIFYLOGFAILED);
            pkixErrorResult = checkCertError;
            pkixErrorCode = pkixErrorResult->errCode;
            checkCertError = NULL;
        }

fatal:
        if (nssCert) {
                CERT_DestroyCertificate(nssCert);
        }

        if (certList) {
                CERT_DestroyCertList(certList);
        }

        PKIX_DECREF(checkCertError);
        PKIX_DECREF(cert);
        PKIX_DECREF(issuer);

        PKIX_RETURN(VALIDATE);
}

/*
 * FUNCTION: pkix_ExtractParameters
 * DESCRIPTION:
 *
 *  Extracts several parameters from the ValidateParams object pointed to by
 *  "valParams" and stores the CertChain at "pChain", the List of Certs at
 *  "pCerts", the number of Certs in the chain at "pNumCerts", the
 *  ProcessingParams object at "pProcParams", the List of TrustAnchors at
 *  "pAnchors", and the number of TrustAnchors at "pNumAnchors".
 *
 * PARAMETERS:
 *  "valParams"
 *      Address of ValidateParams from which the parameters are extracted.
 *      Must be non-NULL.
 *  "pCerts"
 *      Address where object pointer for List of Certs will be stored.
 *      Must be non-NULL.
 *  "pNumCerts"
 *      Address where number of Certs will be stored. Must be non-NULL.
 *  "pProcParams"
 *      Address where object pointer for ProcessingParams will be stored.
 *      Must be non-NULL.
 *  "pAnchors"
 *      Address where object pointer for List of Anchors will be stored.
 *      Must be non-NULL.
 *  "pNumAnchors"
 *      Address where number of Anchors will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Validate Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_ExtractParameters(
        PKIX_ValidateParams *valParams,
        PKIX_List **pCerts,
        PKIX_UInt32 *pNumCerts,
        PKIX_ProcessingParams **pProcParams,
        PKIX_List **pAnchors,
        PKIX_UInt32 *pNumAnchors,
        void *plContext)
{
        PKIX_ENTER(VALIDATE, "pkix_ExtractParameters");
        PKIX_NULLCHECK_THREE(valParams, pCerts, pNumCerts);
        PKIX_NULLCHECK_THREE(pProcParams, pAnchors, pNumAnchors);

        /* extract relevant parameters from chain */
        PKIX_CHECK(PKIX_ValidateParams_GetCertChain
                (valParams, pCerts, plContext),
                PKIX_VALIDATEPARAMSGETCERTCHAINFAILED);

        PKIX_CHECK(PKIX_List_GetLength(*pCerts, pNumCerts, plContext),
                PKIX_LISTGETLENGTHFAILED);

        /* extract relevant parameters from procParams */
        PKIX_CHECK(PKIX_ValidateParams_GetProcessingParams
                (valParams, pProcParams, plContext),
                PKIX_VALIDATEPARAMSGETPROCESSINGPARAMSFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_GetTrustAnchors
                (*pProcParams, pAnchors, plContext),
                PKIX_PROCESSINGPARAMSGETTRUSTANCHORSFAILED);

        PKIX_CHECK(PKIX_List_GetLength(*pAnchors, pNumAnchors, plContext),
                PKIX_LISTGETLENGTHFAILED);

cleanup:

        PKIX_RETURN(VALIDATE);
}

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_ValidateChain (see comments in pkix.h)
 */
PKIX_Error *
PKIX_ValidateChain(
        PKIX_ValidateParams *valParams,
        PKIX_ValidateResult **pResult,
        PKIX_VerifyNode **pVerifyTree,
        void *plContext)
{
        PKIX_Error *chainFailed = NULL;

        PKIX_ProcessingParams *procParams = NULL;
        PKIX_CertChainChecker *userChecker = NULL;
        PKIX_RevocationChecker *revChecker = NULL;
        PKIX_List *certs = NULL;
        PKIX_List *checkers = NULL;
        PKIX_List *anchors = NULL;
        PKIX_List *userCheckers = NULL;
        PKIX_List *userCheckerExtOIDs = NULL;
        PKIX_List *validateCheckedCritExtOIDsList = NULL;
        PKIX_TrustAnchor *anchor = NULL;
        PKIX_ValidateResult *valResult = NULL;
        PKIX_PL_PublicKey *finalPubKey = NULL;
        PKIX_PolicyNode *validPolicyTree = NULL;
        PKIX_Boolean supportForwarding = PKIX_FALSE;
        PKIX_Boolean revChecking = PKIX_FALSE;
        PKIX_UInt32 i, numCerts, numAnchors;
        PKIX_UInt32 numUserCheckers = 0;
        PKIX_UInt32 certCheckedIndex = 0;
        PKIX_UInt32 checkerIndex = 0;
        PKIX_UInt32 reasonCode = 0;
        void *nbioContext = NULL;

        PKIX_ENTER(VALIDATE, "PKIX_ValidateChain");
        PKIX_NULLCHECK_TWO(valParams, pResult);

        /* extract various parameters from valParams */
        PKIX_CHECK(pkix_ExtractParameters
                    (valParams,
                    &certs,
                    &numCerts,
                    &procParams,
                    &anchors,
                    &numAnchors,
                    plContext),
                    PKIX_EXTRACTPARAMETERSFAILED);

        /*
         * setup an extension OID list that user had defined for his checker
         * processing. User checker is not responsible for taking out OIDs
         * from unresolved critical extension list as the libpkix checker
         * is doing. Here we add those user checkers' OIDs to the removal
         * list to be taken out by CheckChain
         */
        PKIX_CHECK(PKIX_ProcessingParams_GetCertChainCheckers
                    (procParams, &userCheckers, plContext),
                    PKIX_PROCESSINGPARAMSGETCERTCHAINCHECKERSFAILED);

        if (userCheckers != NULL) {

                PKIX_CHECK(PKIX_List_Create
                    (&validateCheckedCritExtOIDsList,
                    plContext),
                    PKIX_LISTCREATEFAILED);

                PKIX_CHECK(PKIX_List_GetLength
                    (userCheckers, &numUserCheckers, plContext),
                    PKIX_LISTGETLENGTHFAILED);

                for (i = 0; i < numUserCheckers; i++) {

                    PKIX_CHECK(PKIX_List_GetItem
                        (userCheckers,
                        i,
                        (PKIX_PL_Object **) &userChecker,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                    PKIX_CHECK
                        (PKIX_CertChainChecker_IsForwardCheckingSupported
                        (userChecker, &supportForwarding, plContext),
                        PKIX_CERTCHAINCHECKERISFORWARDCHECKINGSUPPORTEDFAILED);

                    if (supportForwarding == PKIX_FALSE) {

                        PKIX_CHECK
                            (PKIX_CertChainChecker_GetSupportedExtensions
                            (userChecker, &userCheckerExtOIDs, plContext),
                            PKIX_CERTCHAINCHECKERGETSUPPORTEDEXTENSIONSFAILED);

                        if (userCheckerExtOIDs != NULL) {
                            PKIX_CHECK(pkix_List_AppendList
                                (validateCheckedCritExtOIDsList,
                                userCheckerExtOIDs,
                                plContext),
                                PKIX_LISTAPPENDLISTFAILED);
                        }
                    }

                    PKIX_DECREF(userCheckerExtOIDs);
                    PKIX_DECREF(userChecker);
                }
        }

        PKIX_CHECK(PKIX_ProcessingParams_GetRevocationChecker
                (procParams, &revChecker, plContext),
                PKIX_PROCESSINGPARAMSGETREVOCATIONCHECKERFAILED);

        /* try to validate the chain with each anchor */
        for (i = 0; i < numAnchors; i++){

                /* get trust anchor */
                PKIX_CHECK(PKIX_List_GetItem
                        (anchors, i, (PKIX_PL_Object **)&anchor, plContext),
                        PKIX_LISTGETITEMFAILED);

                /* initialize checkers using information from trust anchor */
                PKIX_CHECK(pkix_InitializeCheckers
                        (anchor, procParams, numCerts, &checkers, plContext),
                        PKIX_INITIALIZECHECKERSFAILED);

                /*
                 * Validate the chain using this trust anchor and these
                 * checkers. (WARNING: checkers that use non-blocking I/O
                 * are not currently supported.)
                 */
                certCheckedIndex = 0;
                checkerIndex = 0;
                revChecking = PKIX_FALSE;
                chainFailed = pkix_CheckChain
                        (certs,
                        numCerts,
                        anchor,
                        checkers,
                        revChecker,
                        validateCheckedCritExtOIDsList,
                        procParams,
                        &certCheckedIndex,
                        &checkerIndex,
                        &revChecking,
                        &reasonCode,
                        &nbioContext,
                        &finalPubKey,
                        &validPolicyTree,
                        pVerifyTree,
                        plContext);

                if (chainFailed || (reasonCode != 0)) {

                        /* cert chain failed to validate */

                        PKIX_DECREF(chainFailed);
                        PKIX_DECREF(anchor);
                        PKIX_DECREF(checkers);
                        PKIX_DECREF(validPolicyTree);

                        /* if last anchor, we fail; else, we try next anchor */
                        if (i == (numAnchors - 1)) { /* last anchor */
                                PKIX_ERROR(PKIX_VALIDATECHAINFAILED);
                        }

                } else {

                        /* cert chain successfully validated! */
                        PKIX_CHECK(pkix_ValidateResult_Create
                                (finalPubKey,
                                anchor,
                                validPolicyTree,
                                &valResult,
                                plContext),
                                PKIX_VALIDATERESULTCREATEFAILED);

                        *pResult = valResult;

                        /* no need to try any more anchors in the loop */
                        goto cleanup;
                }
        }

cleanup:

        PKIX_DECREF(finalPubKey);
        PKIX_DECREF(certs);
        PKIX_DECREF(anchors);
        PKIX_DECREF(anchor);
        PKIX_DECREF(checkers);
        PKIX_DECREF(revChecker);
        PKIX_DECREF(validPolicyTree);
        PKIX_DECREF(chainFailed);
        PKIX_DECREF(procParams);
        PKIX_DECREF(userCheckers);
        PKIX_DECREF(validateCheckedCritExtOIDsList);

        PKIX_RETURN(VALIDATE);
}

/*
 * FUNCTION: pkix_Validate_BuildUserOIDs
 * DESCRIPTION:
 *
 *  This function creates a List of the OIDs that are processed by the user
 *  checkers in the List pointed to by "userCheckers", storing the resulting
 *  List at "pUserCritOIDs". If the List of userCheckers is NULL, the output
 *  List will be NULL. Otherwise the output List will be non-NULL, but may be
 *  empty.
 *
 * PARAMETERS:
 *  "userCheckers"
 *      The address of the List of userCheckers.
 *  "pUserCritOIDs"
 *      The address at which the List is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a VALIDATE Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_Validate_BuildUserOIDs(
        PKIX_List *userCheckers,
        PKIX_List **pUserCritOIDs,
        void *plContext)
{
        PKIX_UInt32 numUserCheckers = 0;
        PKIX_UInt32 i = 0;
        PKIX_List *userCritOIDs = NULL;
        PKIX_List *userCheckerExtOIDs = NULL;
        PKIX_Boolean supportForwarding = PKIX_FALSE;
        PKIX_CertChainChecker *userChecker = NULL;

        PKIX_ENTER(VALIDATE, "pkix_Validate_BuildUserOIDs");
        PKIX_NULLCHECK_ONE(pUserCritOIDs);

        if (userCheckers != NULL) {
            PKIX_CHECK(PKIX_List_Create(&userCritOIDs, plContext),
                PKIX_LISTCREATEFAILED);

            PKIX_CHECK(PKIX_List_GetLength
                (userCheckers, &numUserCheckers, plContext),
                PKIX_LISTGETLENGTHFAILED);

            for (i = 0; i < numUserCheckers; i++) {
                PKIX_CHECK(PKIX_List_GetItem
                    (userCheckers,
                    i,
                    (PKIX_PL_Object **) &userChecker,
                    plContext),
                    PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(PKIX_CertChainChecker_IsForwardCheckingSupported
                    (userChecker, &supportForwarding, plContext),
                    PKIX_CERTCHAINCHECKERISFORWARDCHECKINGSUPPORTEDFAILED);

                if (supportForwarding == PKIX_FALSE) {

                    PKIX_CHECK(PKIX_CertChainChecker_GetSupportedExtensions
                        (userChecker, &userCheckerExtOIDs, plContext),
                        PKIX_CERTCHAINCHECKERGETSUPPORTEDEXTENSIONSFAILED);

                    if (userCheckerExtOIDs != NULL) {
                        PKIX_CHECK(pkix_List_AppendList
                            (userCritOIDs, userCheckerExtOIDs, plContext),
                            PKIX_LISTAPPENDLISTFAILED);
                    }
                }

                PKIX_DECREF(userCheckerExtOIDs);
                PKIX_DECREF(userChecker);
            }
        }

        *pUserCritOIDs = userCritOIDs;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(userCritOIDs);
        }

        PKIX_DECREF(userCheckerExtOIDs);
        PKIX_DECREF(userChecker);

        PKIX_RETURN(VALIDATE);
}

/*
 * FUNCTION: PKIX_ValidateChain_nb (see comments in pkix.h)
 */
PKIX_Error *
PKIX_ValidateChain_NB(
        PKIX_ValidateParams *valParams,
        PKIX_UInt32 *pCertIndex,
        PKIX_UInt32 *pAnchorIndex,
        PKIX_UInt32 *pCheckerIndex,
        PKIX_Boolean *pRevChecking,
        PKIX_List **pCheckers,
        void **pNBIOContext,
        PKIX_ValidateResult **pResult,
        PKIX_VerifyNode **pVerifyTree,
        void *plContext)
{
        PKIX_UInt32 numCerts = 0;
        PKIX_UInt32 numAnchors = 0;
        PKIX_UInt32 i = 0;
        PKIX_UInt32 certIndex = 0;
        PKIX_UInt32 anchorIndex = 0;
        PKIX_UInt32 checkerIndex = 0;
        PKIX_UInt32 reasonCode = 0;
        PKIX_Boolean revChecking = PKIX_FALSE;
        PKIX_List *certs = NULL;
        PKIX_List *anchors = NULL;
        PKIX_List *checkers = NULL;
        PKIX_List *userCheckers = NULL;
        PKIX_List *validateCheckedCritExtOIDsList = NULL;
        PKIX_TrustAnchor *anchor = NULL;
        PKIX_ValidateResult *valResult = NULL;
        PKIX_PL_PublicKey *finalPubKey = NULL;
        PKIX_PolicyNode *validPolicyTree = NULL;
        PKIX_ProcessingParams *procParams = NULL;
        PKIX_RevocationChecker *revChecker = NULL;
        PKIX_Error *chainFailed = NULL;
        void *nbioContext = NULL;

        PKIX_ENTER(VALIDATE, "PKIX_ValidateChain_NB");
        PKIX_NULLCHECK_FOUR
                (valParams, pCertIndex, pAnchorIndex, pCheckerIndex);
        PKIX_NULLCHECK_FOUR(pRevChecking, pCheckers, pNBIOContext, pResult);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        /* extract various parameters from valParams */
        PKIX_CHECK(pkix_ExtractParameters
                    (valParams,
                    &certs,
                    &numCerts,
                    &procParams,
                    &anchors,
                    &numAnchors,
                    plContext),
                    PKIX_EXTRACTPARAMETERSFAILED);

        /*
         * Create a List of the OIDs that will be processed by the user
         * checkers. User checkers are not responsible for removing OIDs from
         * the List of unresolved critical extensions, as libpkix checkers are.
         * So we add those user checkers' OIDs to the removal list to be taken
         * out by CheckChain.
         */
        PKIX_CHECK(PKIX_ProcessingParams_GetCertChainCheckers
                (procParams, &userCheckers, plContext),
                PKIX_PROCESSINGPARAMSGETCERTCHAINCHECKERSFAILED);

        PKIX_CHECK(pkix_Validate_BuildUserOIDs
                (userCheckers, &validateCheckedCritExtOIDsList, plContext),
                PKIX_VALIDATEBUILDUSEROIDSFAILED);

        PKIX_CHECK(PKIX_ProcessingParams_GetRevocationChecker
                (procParams, &revChecker, plContext),
                PKIX_PROCESSINGPARAMSGETREVOCATIONCHECKERFAILED);

        /* Are we resuming after a WOULDBLOCK return, or starting anew ? */
        if (nbioContext != NULL) {
                /* Resuming */
                certIndex = *pCertIndex;
                anchorIndex = *pAnchorIndex;
                checkerIndex = *pCheckerIndex;
                revChecking = *pRevChecking;
                checkers = *pCheckers;
                *pCheckers = NULL;
        }

        /* try to validate the chain with each anchor */
        for (i = anchorIndex; i < numAnchors; i++) {

                /* get trust anchor */
                PKIX_CHECK(PKIX_List_GetItem
                        (anchors, i, (PKIX_PL_Object **)&anchor, plContext),
                        PKIX_LISTGETITEMFAILED);

                /* initialize checkers using information from trust anchor */
                if (nbioContext == NULL) {
                        PKIX_CHECK(pkix_InitializeCheckers
                                (anchor,
                                procParams,
                                numCerts,
                                &checkers,
                                plContext),
                                PKIX_INITIALIZECHECKERSFAILED);
                }

                /*
                 * Validate the chain using this trust anchor and these
                 * checkers.
                 */
                chainFailed = pkix_CheckChain
                        (certs,
                        numCerts,
                        anchor,
                        checkers,
                        revChecker,
                        validateCheckedCritExtOIDsList,
                        procParams,
                        &certIndex,
                        &checkerIndex,
                        &revChecking,
                        &reasonCode,
                        &nbioContext,
                        &finalPubKey,
                        &validPolicyTree,
                        pVerifyTree,
                        plContext);

                if (nbioContext != NULL) {
                        *pCertIndex = certIndex;
                        *pAnchorIndex = anchorIndex;
                        *pCheckerIndex = checkerIndex;
                        *pRevChecking = revChecking;
                        PKIX_INCREF(checkers);
                        *pCheckers = checkers;
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }

                if (chainFailed || (reasonCode != 0)) {

                        /* cert chain failed to validate */

                        PKIX_DECREF(chainFailed);
                        PKIX_DECREF(anchor);
                        PKIX_DECREF(checkers);
                        PKIX_DECREF(validPolicyTree);

                        /* if last anchor, we fail; else, we try next anchor */
                        if (i == (numAnchors - 1)) { /* last anchor */
                                PKIX_ERROR(PKIX_VALIDATECHAINFAILED);
                        }

                } else {

                        /* cert chain successfully validated! */
                        PKIX_CHECK(pkix_ValidateResult_Create
                                (finalPubKey,
                                anchor,
                                validPolicyTree,
                                &valResult,
                                plContext),
                                PKIX_VALIDATERESULTCREATEFAILED);

                        *pResult = valResult;

                        /* no need to try any more anchors in the loop */
                        goto cleanup;
                }
        }

cleanup:

        PKIX_DECREF(finalPubKey);
        PKIX_DECREF(certs);
        PKIX_DECREF(anchors);
        PKIX_DECREF(anchor);
        PKIX_DECREF(checkers);
        PKIX_DECREF(revChecker);
        PKIX_DECREF(validPolicyTree);
        PKIX_DECREF(chainFailed);
        PKIX_DECREF(procParams);
        PKIX_DECREF(userCheckers);
        PKIX_DECREF(validateCheckedCritExtOIDsList);

        PKIX_RETURN(VALIDATE);
}
