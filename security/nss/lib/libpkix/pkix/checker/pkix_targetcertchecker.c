/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_targetcertchecker.c
 *
 * Functions for target cert validation
 *
 */


#include "pkix_targetcertchecker.h"

/* --Private-TargetCertCheckerState-Functions------------------------------- */

/*
 * FUNCTION: pkix_TargetCertCheckerState_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_TargetCertCheckerState_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        pkix_TargetCertCheckerState *state = NULL;

        PKIX_ENTER(TARGETCERTCHECKERSTATE,
                    "pkix_TargetCertCheckerState_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a target cert checker state */
        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_TARGETCERTCHECKERSTATE_TYPE, plContext),
                    PKIX_OBJECTNOTTARGETCERTCHECKERSTATE);

        state = (pkix_TargetCertCheckerState *)object;

        PKIX_DECREF(state->certSelector);
        PKIX_DECREF(state->extKeyUsageOID);
        PKIX_DECREF(state->subjAltNameOID);
        PKIX_DECREF(state->pathToNameList);
        PKIX_DECREF(state->extKeyUsageList);
        PKIX_DECREF(state->subjAltNameList);

cleanup:

        PKIX_RETURN(TARGETCERTCHECKERSTATE);
}

/*
 * FUNCTION: pkix_TargetCertCheckerState_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_TARGETCERTCHECKERSTATE_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_TargetCertCheckerState_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(TARGETCERTCHECKERSTATE,
                    "pkix_TargetCertCheckerState_RegisterSelf");

        entry.description = "TargetCertCheckerState";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(pkix_TargetCertCheckerState);
        entry.destructor = pkix_TargetCertCheckerState_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_TARGETCERTCHECKERSTATE_TYPE] = entry;

        PKIX_RETURN(TARGETCERTCHECKERSTATE);
}

/*
 * FUNCTION: pkix_TargetCertCheckerState_Create
 * DESCRIPTION:
 *
 *  Creates a new TargetCertCheckerState using the CertSelector pointed to
 *  by "certSelector" and the number of certs represented by "certsRemaining"
 *  and stores it at "pState".
 *
 * PARAMETERS:
 *  "certSelector"
 *      Address of CertSelector representing the criteria against which the
 *      final certificate in a chain is to be matched. Must be non-NULL.
 *  "certsRemaining"
 *      Number of certificates remaining in the chain.
 *  "pState"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a TargetCertCheckerState Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_TargetCertCheckerState_Create(
    PKIX_CertSelector *certSelector,
    PKIX_UInt32 certsRemaining,
    pkix_TargetCertCheckerState **pState,
    void *plContext)
{
        pkix_TargetCertCheckerState *state = NULL;
        PKIX_ComCertSelParams *certSelectorParams = NULL;
        PKIX_List *pathToNameList = NULL;
        PKIX_List *extKeyUsageList = NULL;
        PKIX_List *subjAltNameList = NULL;
        PKIX_PL_OID *extKeyUsageOID = NULL;
        PKIX_PL_OID *subjAltNameOID = NULL;
        PKIX_Boolean subjAltNameMatchAll = PKIX_TRUE;

        PKIX_ENTER(TARGETCERTCHECKERSTATE,
                    "pkix_TargetCertCheckerState_Create");
        PKIX_NULLCHECK_ONE(pState);

        PKIX_CHECK(PKIX_PL_OID_Create
                    (PKIX_EXTENDEDKEYUSAGE_OID,
                    &extKeyUsageOID,
                    plContext),
                    PKIX_OIDCREATEFAILED);

        PKIX_CHECK(PKIX_PL_OID_Create
                    (PKIX_CERTSUBJALTNAME_OID,
                    &subjAltNameOID,
                    plContext),
                    PKIX_OIDCREATEFAILED);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_TARGETCERTCHECKERSTATE_TYPE,
                    sizeof (pkix_TargetCertCheckerState),
                    (PKIX_PL_Object **)&state,
                    plContext),
                    PKIX_COULDNOTCREATETARGETCERTCHECKERSTATEOBJECT);

        /* initialize fields */

        if (certSelector != NULL) {

                PKIX_CHECK(PKIX_CertSelector_GetCommonCertSelectorParams
                        (certSelector, &certSelectorParams, plContext),
                        PKIX_CERTSELECTORGETCOMMONCERTSELECTORPARAMFAILED);

                if (certSelectorParams != NULL) {

                        PKIX_CHECK(PKIX_ComCertSelParams_GetPathToNames
                            (certSelectorParams,
                            &pathToNameList,
                            plContext),
                            PKIX_COMCERTSELPARAMSGETPATHTONAMESFAILED);

                        PKIX_CHECK(PKIX_ComCertSelParams_GetExtendedKeyUsage
                            (certSelectorParams,
                            &extKeyUsageList,
                            plContext),
                            PKIX_COMCERTSELPARAMSGETEXTENDEDKEYUSAGEFAILED);

                        PKIX_CHECK(PKIX_ComCertSelParams_GetSubjAltNames
                            (certSelectorParams,
                            &subjAltNameList,
                            plContext),
                            PKIX_COMCERTSELPARAMSGETSUBJALTNAMESFAILED);

                        PKIX_CHECK(PKIX_ComCertSelParams_GetMatchAllSubjAltNames
                            (certSelectorParams,
                            &subjAltNameMatchAll,
                            plContext),
                            PKIX_COMCERTSELPARAMSGETSUBJALTNAMESFAILED);
                }
        }

        state->certsRemaining = certsRemaining;
        state->subjAltNameMatchAll = subjAltNameMatchAll;

        PKIX_INCREF(certSelector);
        state->certSelector = certSelector;

        state->pathToNameList = pathToNameList;
        pathToNameList = NULL;

        state->extKeyUsageList = extKeyUsageList;
        extKeyUsageList = NULL;

        state->subjAltNameList = subjAltNameList;
        subjAltNameList = NULL;

        state->extKeyUsageOID = extKeyUsageOID;
        extKeyUsageOID = NULL;

        state->subjAltNameOID = subjAltNameOID;
        subjAltNameOID = NULL;

        *pState = state;
        state = NULL;

cleanup:
        
        PKIX_DECREF(extKeyUsageOID);
        PKIX_DECREF(subjAltNameOID);
        PKIX_DECREF(pathToNameList);
        PKIX_DECREF(extKeyUsageList);
        PKIX_DECREF(subjAltNameList);
        PKIX_DECREF(state);

        PKIX_DECREF(certSelectorParams);

        PKIX_RETURN(TARGETCERTCHECKERSTATE);

}

/* --Private-TargetCertChecker-Functions------------------------------- */

/*
 * FUNCTION: pkix_TargetCertChecker_Check
 * (see comments for PKIX_CertChainChecker_CheckCallback in pkix_checker.h)
 */
PKIX_Error *
pkix_TargetCertChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticalExtensions,
        void **pNBIOContext,
        void *plContext)
{
        pkix_TargetCertCheckerState *state = NULL;
        PKIX_CertSelector_MatchCallback certSelectorMatch = NULL;
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;
        PKIX_List *certSubjAltNames = NULL;
        PKIX_List *certExtKeyUsageList = NULL;
        PKIX_PL_GeneralName *name = NULL;
        PKIX_PL_X500Name *certSubjectName = NULL;
        PKIX_Boolean checkPassed = PKIX_FALSE;
        PKIX_UInt32 numItems, i;
        PKIX_UInt32 matchCount = 0;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_TargetCertChecker_Check");
        PKIX_NULLCHECK_THREE(checker, cert, pNBIOContext);

        *pNBIOContext = NULL; /* we never block on pending I/O */

        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                    (checker, (PKIX_PL_Object **)&state, plContext),
                    PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        (state->certsRemaining)--;

        if (state->pathToNameList != NULL) {

                PKIX_CHECK(PKIX_PL_Cert_GetNameConstraints
                    (cert, &nameConstraints, plContext),
                    PKIX_CERTGETNAMECONSTRAINTSFAILED);

                /*
                 * XXX We should either make the following call a public one
                 * so it is legal to call from the portability layer or we
                 * should try to create pathToNameList as CertNameConstraints
                 * then call the existing check function.
                 */
                PKIX_CHECK(PKIX_PL_CertNameConstraints_CheckNamesInNameSpace
                    (state->pathToNameList,
                    nameConstraints,
                    &checkPassed,
                    plContext),
                    PKIX_CERTNAMECONSTRAINTSCHECKNAMEINNAMESPACEFAILED);

                if (checkPassed != PKIX_TRUE) {
                    PKIX_ERROR(PKIX_VALIDATIONFAILEDPATHTONAMECHECKFAILED);
                }

        }

        PKIX_CHECK(PKIX_PL_Cert_GetSubjectAltNames
                    (cert, &certSubjAltNames, plContext),
                    PKIX_CERTGETSUBJALTNAMESFAILED);

        if (state->subjAltNameList != NULL && certSubjAltNames != NULL) {

                PKIX_CHECK(PKIX_List_GetLength
                        (state->subjAltNameList, &numItems, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                for (i = 0; i < numItems; i++) {

                        PKIX_CHECK(PKIX_List_GetItem
                            (state->subjAltNameList,
                            i,
                            (PKIX_PL_Object **) &name,
                            plContext),
                            PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(pkix_List_Contains
                            (certSubjAltNames,
                            (PKIX_PL_Object *) name,
                            &checkPassed,
                            plContext),
                            PKIX_LISTCONTAINSFAILED);

                        PKIX_DECREF(name);

                        if (checkPassed == PKIX_TRUE) {

                            if (state->subjAltNameMatchAll == PKIX_FALSE) {
                                matchCount = numItems;
                                break;
                            } else {
                                /* else continue checking next */
                                matchCount++;
                            }

                        }
                }

                if (matchCount != numItems) {
                        PKIX_ERROR(PKIX_SUBJALTNAMECHECKFAILED);

                }
        }

        if (state->certsRemaining == 0) {

            if (state->certSelector != NULL) {
                PKIX_CHECK(PKIX_CertSelector_GetMatchCallback
                           (state->certSelector,
                            &certSelectorMatch,
                            plContext),
                           PKIX_CERTSELECTORGETMATCHCALLBACKFAILED);

                PKIX_CHECK(certSelectorMatch
                           (state->certSelector,
                            cert,
                            plContext),
                           PKIX_CERTSELECTORMATCHFAILED);
            } else {
                /* Check at least cert/key usages if target cert selector
                 * is not set. */
                PKIX_CHECK(PKIX_PL_Cert_VerifyCertAndKeyType(cert,
                                         PKIX_FALSE  /* is chain cert*/,
                                         plContext),
                           PKIX_CERTVERIFYCERTTYPEFAILED);
            }
            /*
             * There are two Extended Key Usage Checkings
             * available :
             * 1) here at the targetcertchecker where we
             *    verify the Extended Key Usage OIDs application
             *    specifies via ComCertSelParams are included
             *    in Cert's Extended Key Usage OID's. Note,
             *    this is an OID to OID comparison and only last
             *    Cert is checked.
             * 2) at user defined ekuchecker where checking
             *    is applied to all Certs on the chain and
             *    the NSS Extended Key Usage algorithm is
             *    used. In order to invoke this checking, not
             *    only does the ComCertSelparams needs to be
             *    set, the EKU initialize call is required to
             *    activate the checking.
             *
             * XXX We use the same ComCertSelParams Set/Get
             * functions to set the parameters for both cases.
             * We may want to separate them in the future.
             */
            
            PKIX_CHECK(PKIX_PL_Cert_GetExtendedKeyUsage
                       (cert, &certExtKeyUsageList, plContext),
                       PKIX_CERTGETEXTENDEDKEYUSAGEFAILED);
            
            
            if (state->extKeyUsageList != NULL &&
                certExtKeyUsageList != NULL) {
                
                PKIX_CHECK(PKIX_List_GetLength
                           (state->extKeyUsageList, &numItems, plContext),
                           PKIX_LISTGETLENGTHFAILED);
                
                for (i = 0; i < numItems; i++) {
                    
                    PKIX_CHECK(PKIX_List_GetItem
                               (state->extKeyUsageList,
                                i,
                                (PKIX_PL_Object **) &name,
                                plContext),
                               PKIX_LISTGETITEMFAILED);
                    
                    PKIX_CHECK(pkix_List_Contains
                               (certExtKeyUsageList,
                                (PKIX_PL_Object *) name,
                                &checkPassed,
                                plContext),
                               PKIX_LISTCONTAINSFAILED);
                    
                    PKIX_DECREF(name);
                    
                    if (checkPassed != PKIX_TRUE) {
                        PKIX_ERROR
                            (PKIX_EXTENDEDKEYUSAGECHECKINGFAILED);
                        
                    }
                }
            }
        } else {
            /* Check key usage and cert type based on certificate usage. */
            PKIX_CHECK(PKIX_PL_Cert_VerifyCertAndKeyType(cert, PKIX_TRUE,
                                                         plContext),
                       PKIX_CERTVERIFYCERTTYPEFAILED);
        }

        /* Remove Critical Extension OID from list */
        if (unresolvedCriticalExtensions != NULL) {

                PKIX_CHECK(pkix_List_Remove
                            (unresolvedCriticalExtensions,
                            (PKIX_PL_Object *) state->extKeyUsageOID,
                            plContext),
                            PKIX_LISTREMOVEFAILED);

                PKIX_CHECK(PKIX_PL_Cert_GetSubject
                            (cert, &certSubjectName, plContext),
                            PKIX_CERTGETSUBJECTFAILED);

                if (certSubjAltNames != NULL) {
                        PKIX_CHECK(pkix_List_Remove
                            (unresolvedCriticalExtensions,
                            (PKIX_PL_Object *) state->subjAltNameOID,
                            plContext),
                            PKIX_LISTREMOVEFAILED);
                }

        }

cleanup:

        PKIX_DECREF(name);
        PKIX_DECREF(nameConstraints);
        PKIX_DECREF(certSubjAltNames);
        PKIX_DECREF(certExtKeyUsageList);
        PKIX_DECREF(certSubjectName);
        PKIX_DECREF(state);

        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: pkix_TargetCertChecker_Initialize
 * DESCRIPTION:
 *
 *  Creates a new CertChainChecker and stores it at "pChecker", where it will
 *  used by pkix_TargetCertChecker_Check to check that the final certificate
 *  of a chain meets the criteria of the CertSelector pointed to by
 *  "certSelector". The number of certs remaining in the chain, represented by
 *  "certsRemaining" is used to initialize the checker's state.
 *
 * PARAMETERS:
 *  "certSelector"
 *      Address of CertSelector representing the criteria against which the
 *      final certificate in a chain is to be matched. May be NULL.
 *  "certsRemaining"
 *      Number of certificates remaining in the chain.
 *  "pChecker"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_TargetCertChecker_Initialize(
        PKIX_CertSelector *certSelector,
        PKIX_UInt32 certsRemaining,
        PKIX_CertChainChecker **pChecker,
        void *plContext)
{
        pkix_TargetCertCheckerState *state = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_TargetCertChecker_Initialize");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(pkix_TargetCertCheckerState_Create
                    (certSelector, certsRemaining, &state, plContext),
                    PKIX_TARGETCERTCHECKERSTATECREATEFAILED);

        PKIX_CHECK(PKIX_CertChainChecker_Create
                    (pkix_TargetCertChecker_Check,
                    PKIX_FALSE,
                    PKIX_FALSE,
                    NULL,
                    (PKIX_PL_Object *)state,
                    pChecker,
                    plContext),
                    PKIX_CERTCHAINCHECKERCREATEFAILED);

cleanup:

        PKIX_DECREF(state);

        PKIX_RETURN(CERTCHAINCHECKER);
}
