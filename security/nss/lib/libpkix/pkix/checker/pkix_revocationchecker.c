/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_revocationchecker.c
 *
 * RevocationChecker Object Functions
 *
 */

#include "pkix_revocationchecker.h"
#include "pkix_tools.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_RevocationChecker_Destroy
 *      (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_RevocationChecker_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_RevocationChecker *checker = NULL;

        PKIX_ENTER(REVOCATIONCHECKER, "pkix_RevocationChecker_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a revocation checker */
        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_REVOCATIONCHECKER_TYPE, plContext),
                    PKIX_OBJECTNOTREVOCATIONCHECKER);

        checker = (PKIX_RevocationChecker *)object;

        PKIX_DECREF(checker->leafMethodList);
        PKIX_DECREF(checker->chainMethodList);
        
cleanup:

        PKIX_RETURN(REVOCATIONCHECKER);
}

/*
 * FUNCTION: pkix_RevocationChecker_Duplicate
 * (see comments for PKIX_PL_DuplicateCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_RevocationChecker_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_RevocationChecker *checker = NULL;
        PKIX_RevocationChecker *checkerDuplicate = NULL;
        PKIX_List *dupLeafList = NULL;
        PKIX_List *dupChainList = NULL;

        PKIX_ENTER(REVOCATIONCHECKER, "pkix_RevocationChecker_Duplicate");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_REVOCATIONCHECKER_TYPE, plContext),
                    PKIX_OBJECTNOTCERTCHAINCHECKER);

        checker = (PKIX_RevocationChecker *)object;

        if (checker->leafMethodList){
                PKIX_CHECK(PKIX_PL_Object_Duplicate
                            ((PKIX_PL_Object *)checker->leafMethodList,
                            (PKIX_PL_Object **)&dupLeafList,
                            plContext),
                            PKIX_OBJECTDUPLICATEFAILED);
        }
        if (checker->chainMethodList){
                PKIX_CHECK(PKIX_PL_Object_Duplicate
                            ((PKIX_PL_Object *)checker->chainMethodList,
                            (PKIX_PL_Object **)&dupChainList,
                            plContext),
                            PKIX_OBJECTDUPLICATEFAILED);
        }

        PKIX_CHECK(
            PKIX_RevocationChecker_Create(checker->leafMethodListFlags,
                                          checker->chainMethodListFlags,
                                          &checkerDuplicate,
                                          plContext),
            PKIX_REVOCATIONCHECKERCREATEFAILED);

        checkerDuplicate->leafMethodList = dupLeafList;
        checkerDuplicate->chainMethodList = dupChainList;
        dupLeafList = NULL;
        dupChainList = NULL;

        *pNewObject = (PKIX_PL_Object *)checkerDuplicate;

cleanup:
        PKIX_DECREF(dupLeafList);
        PKIX_DECREF(dupChainList);

        PKIX_RETURN(REVOCATIONCHECKER);
}

/*
 * FUNCTION: pkix_RevocationChecker_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_REVOCATIONCHECKER_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_RevocationChecker_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(REVOCATIONCHECKER, "pkix_RevocationChecker_RegisterSelf");

        entry.description = "RevocationChecker";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_RevocationChecker);
        entry.destructor = pkix_RevocationChecker_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_RevocationChecker_Duplicate;

        systemClasses[PKIX_REVOCATIONCHECKER_TYPE] = entry;

        PKIX_RETURN(REVOCATIONCHECKER);
}

/* Sort methods by their priorities (lower priority = higher preference) */
static PKIX_Error *
pkix_RevocationChecker_SortComparator(
        PKIX_PL_Object *obj1,
        PKIX_PL_Object *obj2,
        PKIX_Int32 *pResult,
        void *plContext)
{
    pkix_RevocationMethod *method1 = NULL, *method2 = NULL;
    
    PKIX_ENTER(BUILD, "pkix_RevocationChecker_SortComparator");
    
    method1 = (pkix_RevocationMethod *)obj1;
    method2 = (pkix_RevocationMethod *)obj2;
    
    if (method1->priority < method2->priority) {
      *pResult = -1;
    } else if (method1->priority > method2->priority) {
      *pResult = 1;
    } else {
      *pResult = 0;
    }
    
    PKIX_RETURN(BUILD);
}


/* --Public-Functions--------------------------------------------- */


/*
 * FUNCTION: PKIX_RevocationChecker_Create (see comments in pkix_revchecker.h)
 */
PKIX_Error *
PKIX_RevocationChecker_Create(
    PKIX_UInt32 leafMethodListFlags,
    PKIX_UInt32 chainMethodListFlags,
    PKIX_RevocationChecker **pChecker,
    void *plContext)
{
    PKIX_RevocationChecker *checker = NULL;
    
    PKIX_ENTER(REVOCATIONCHECKER, "PKIX_RevocationChecker_Create");
    PKIX_NULLCHECK_ONE(pChecker);
    
    PKIX_CHECK(
        PKIX_PL_Object_Alloc(PKIX_REVOCATIONCHECKER_TYPE,
                             sizeof (PKIX_RevocationChecker),
                             (PKIX_PL_Object **)&checker,
                             plContext),
        PKIX_COULDNOTCREATECERTCHAINCHECKEROBJECT);
    
    checker->leafMethodListFlags = leafMethodListFlags;
    checker->chainMethodListFlags = chainMethodListFlags;
    checker->leafMethodList = NULL;
    checker->chainMethodList = NULL;
    
    *pChecker = checker;
    checker = NULL;
    
cleanup:
    PKIX_DECREF(checker);
    
    PKIX_RETURN(REVOCATIONCHECKER);
}

/*
 * FUNCTION: PKIX_RevocationChecker_CreateAndAddMethod
 */
PKIX_Error *
PKIX_RevocationChecker_CreateAndAddMethod(
    PKIX_RevocationChecker *revChecker,
    PKIX_ProcessingParams *params,
    PKIX_RevocationMethodType methodType,
    PKIX_UInt32 flags,
    PKIX_UInt32 priority,
    PKIX_PL_VerifyCallback verificationFn,
    PKIX_Boolean isLeafMethod,
    void *plContext)
{
    PKIX_List **methodList = NULL;
    PKIX_List  *unsortedList = NULL;
    PKIX_List  *certStores = NULL;
    pkix_RevocationMethod *method = NULL;
    pkix_LocalRevocationCheckFn *localRevChecker = NULL;
    pkix_ExternalRevocationCheckFn *externRevChecker = NULL;
    PKIX_UInt32 miFlags;
    
    PKIX_ENTER(REVOCATIONCHECKER, "PKIX_RevocationChecker_CreateAndAddMethod");
    PKIX_NULLCHECK_ONE(revChecker);

    /* If the caller has said "Either one is sufficient, then don't let the 
     * absence of any one method's info lead to an overall failure.
     */
    miFlags = isLeafMethod ? revChecker->leafMethodListFlags 
	                   : revChecker->chainMethodListFlags;
    if (miFlags & PKIX_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE)
	flags &= ~PKIX_REV_M_FAIL_ON_MISSING_FRESH_INFO;

    switch (methodType) {
    case PKIX_RevocationMethod_CRL:
        localRevChecker = pkix_CrlChecker_CheckLocal;
        externRevChecker = pkix_CrlChecker_CheckExternal;
        PKIX_CHECK(
            PKIX_ProcessingParams_GetCertStores(params, &certStores,
                                                plContext),
            PKIX_PROCESSINGPARAMSGETCERTSTORESFAILED);
        PKIX_CHECK(
            pkix_CrlChecker_Create(methodType, flags, priority,
                                   localRevChecker, externRevChecker,
                                   certStores, verificationFn,
                                   &method,
                                   plContext),
            PKIX_COULDNOTCREATECRLCHECKEROBJECT);
        break;
    case PKIX_RevocationMethod_OCSP:
        localRevChecker = pkix_OcspChecker_CheckLocal;
        externRevChecker = pkix_OcspChecker_CheckExternal;
        PKIX_CHECK(
            pkix_OcspChecker_Create(methodType, flags, priority,
                                    localRevChecker, externRevChecker,
                                    verificationFn,
                                    &method,
                                    plContext),
            PKIX_COULDNOTCREATEOCSPCHECKEROBJECT);
        break;
    default:
        PKIX_ERROR(PKIX_INVALIDREVOCATIONMETHOD);
    }

    if (isLeafMethod) {
        methodList = &revChecker->leafMethodList;
    } else {
        methodList = &revChecker->chainMethodList;
    }
    
    if (*methodList == NULL) {
        PKIX_CHECK(
            PKIX_List_Create(methodList, plContext),
            PKIX_LISTCREATEFAILED);
    }
    unsortedList = *methodList;
    PKIX_CHECK(
        PKIX_List_AppendItem(unsortedList, (PKIX_PL_Object*)method, plContext),
        PKIX_LISTAPPENDITEMFAILED);
    PKIX_CHECK(
        pkix_List_BubbleSort(unsortedList, 
                             pkix_RevocationChecker_SortComparator,
                             methodList, plContext),
        PKIX_LISTBUBBLESORTFAILED);

cleanup:
    PKIX_DECREF(method);
    PKIX_DECREF(unsortedList);
    PKIX_DECREF(certStores);
    
    PKIX_RETURN(REVOCATIONCHECKER);
}

/*
 * FUNCTION: PKIX_RevocationChecker_Check
 */
PKIX_Error *
PKIX_RevocationChecker_Check(
    PKIX_PL_Cert *cert,
    PKIX_PL_Cert *issuer,
    PKIX_RevocationChecker *revChecker,
    PKIX_ProcessingParams *procParams,
    PKIX_Boolean chainVerificationState,
    PKIX_Boolean testingLeafCert,
    PKIX_RevocationStatus *pRevStatus,
    PKIX_UInt32 *pReasonCode,
    void **pNbioContext,
    void *plContext)
{
    PKIX_RevocationStatus overallStatus = PKIX_RevStatus_NoInfo;
    PKIX_RevocationStatus methodStatus[PKIX_RevocationMethod_MAX];
    PKIX_Boolean onlyUseRemoteMethods = PKIX_FALSE;
    PKIX_UInt32 revFlags = 0;
    PKIX_List *revList = NULL;
    PKIX_PL_Date *date = NULL;
    pkix_RevocationMethod *method = NULL;
    void *nbioContext;
    int tries;
    
    PKIX_ENTER(REVOCATIONCHECKER, "PKIX_RevocationChecker_Check");
    PKIX_NULLCHECK_TWO(revChecker, procParams);

    nbioContext = *pNbioContext;
    *pNbioContext = NULL;
    
    if (testingLeafCert) {
        revList = revChecker->leafMethodList;
        revFlags = revChecker->leafMethodListFlags;        
    } else {
        revList = revChecker->chainMethodList;
        revFlags = revChecker->chainMethodListFlags;
    }
    if (!revList) {
        /* Return NoInfo status */
        goto cleanup;
    }

    PORT_Memset(methodStatus, PKIX_RevStatus_NoInfo,
                sizeof(PKIX_RevocationStatus) * PKIX_RevocationMethod_MAX);

    date = procParams->date;

    /* Need to have two loops if we testing all local info first:
     *    first we are going to test all local(cached) info
     *    second, all remote info(fetching) */
    for (tries = 0;tries < 2;tries++) {
        unsigned int methodNum = 0;
        for (;methodNum < revList->length;methodNum++) {
            PKIX_UInt32 methodFlags = 0;

            PKIX_DECREF(method);
            PKIX_CHECK(
                PKIX_List_GetItem(revList, methodNum,
                                  (PKIX_PL_Object**)&method, plContext),
                PKIX_LISTGETITEMFAILED);
            methodFlags = method->flags;
            if (!(methodFlags & PKIX_REV_M_TEST_USING_THIS_METHOD)) {
                /* Will not check with this method. Skipping... */
                continue;
            }
            if (!onlyUseRemoteMethods &&
                methodStatus[methodNum] == PKIX_RevStatus_NoInfo) {
                PKIX_RevocationStatus revStatus = PKIX_RevStatus_NoInfo;
                PKIX_CHECK_NO_GOTO(
                    (*method->localRevChecker)(cert, issuer, date,
                                               method, procParams,
                                               methodFlags, 
                                               chainVerificationState,
                                               &revStatus,
                                               (CERTCRLEntryReasonCode *)pReasonCode,
                                               plContext),
                    PKIX_REVCHECKERCHECKFAILED);
                methodStatus[methodNum] = revStatus;
                if (revStatus == PKIX_RevStatus_Revoked) {
                    /* if error was generated use it as final error. */
                    overallStatus = PKIX_RevStatus_Revoked;
                    goto cleanup;
                }
                if (pkixErrorResult) {
                    /* Disregard errors. Only returned revStatus matters. */
                    PKIX_PL_Object_DecRef((PKIX_PL_Object*)pkixErrorResult,
                                          plContext);
                    pkixErrorResult = NULL;
                }
            }
            if ((!(revFlags & PKIX_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST) ||
                 onlyUseRemoteMethods) &&
                chainVerificationState &&
                methodStatus[methodNum] == PKIX_RevStatus_NoInfo) {
                if (!(methodFlags & PKIX_REV_M_FORBID_NETWORK_FETCHING)) {
                    PKIX_RevocationStatus revStatus = PKIX_RevStatus_NoInfo;
                    PKIX_CHECK_NO_GOTO(
                        (*method->externalRevChecker)(cert, issuer, date,
                                                      method,
                                                      procParams, methodFlags,
                                                      &revStatus,
                                                      (CERTCRLEntryReasonCode *)pReasonCode,
                                                      &nbioContext, plContext),
                        PKIX_REVCHECKERCHECKFAILED);
                    methodStatus[methodNum] = revStatus;
                    if (revStatus == PKIX_RevStatus_Revoked) {
                        /* if error was generated use it as final error. */
                        overallStatus = PKIX_RevStatus_Revoked;
                        goto cleanup;
                    }
                    if (pkixErrorResult) {
                        /* Disregard errors. Only returned revStatus matters. */
                        PKIX_PL_Object_DecRef((PKIX_PL_Object*)pkixErrorResult,
                                              plContext);
                        pkixErrorResult = NULL;
                    }
                } else if (methodFlags &
                           PKIX_REV_M_FAIL_ON_MISSING_FRESH_INFO) {
                    /* Info is not in the local cache. Network fetching is not
                     * allowed. If need to fail on missing fresh info for the
                     * the method, then we should fail right here.*/
                    overallStatus = PKIX_RevStatus_Revoked;
                    goto cleanup;
                }
            }
            /* If success and we should not check the next method, then
             * return a success. */
            if (methodStatus[methodNum] == PKIX_RevStatus_Success &&
                !(methodFlags & PKIX_REV_M_CONTINUE_TESTING_ON_FRESH_INFO)) {
                overallStatus = PKIX_RevStatus_Success;
                goto cleanup;
            }
        } /* inner loop */
        if (!onlyUseRemoteMethods &&
            revFlags & PKIX_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST &&
            chainVerificationState) {
            onlyUseRemoteMethods = PKIX_TRUE;
            continue;
        }
        break;
    } /* outer loop */
    
    if (overallStatus == PKIX_RevStatus_NoInfo &&
        chainVerificationState) {
        /* The following check makes sence only for chain
         * validation step, sinse we do not fetch info while
         * in the process of finding trusted anchor. 
         * For chain building step it is enough to know, that
         * the cert was not directly revoked by any of the
         * methods. */

        /* Still have no info. But one of the method could
         * have returned success status(possible if CONTINUE
         * TESTING ON FRESH INFO flag was used).
         * If any of the methods have returned Success status,
         * the overallStatus should be success. */
        int methodNum = 0;
        for (;methodNum < PKIX_RevocationMethod_MAX;methodNum++) {
            if (methodStatus[methodNum] == PKIX_RevStatus_Success) {
                overallStatus = PKIX_RevStatus_Success;
                goto cleanup;
            }
        }
        if (revFlags & PKIX_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE) {
            overallStatus = PKIX_RevStatus_Revoked;
        }
    }

cleanup:
    *pRevStatus = overallStatus;
    PKIX_DECREF(method);

    PKIX_RETURN(REVOCATIONCHECKER);
}

