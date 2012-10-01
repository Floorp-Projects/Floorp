/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_certchainchecker.c
 *
 * CertChainChecker Object Functions
 *
 */

#include "pkix_certchainchecker.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_CertChainChecker_Destroy
 *      (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_CertChainChecker_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_CertChainChecker *checker = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_CertChainChecker_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a cert chain checker */
        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_CERTCHAINCHECKER_TYPE, plContext),
                    PKIX_OBJECTNOTCERTCHAINCHECKER);

        checker = (PKIX_CertChainChecker *)object;

        PKIX_DECREF(checker->extensions);
        PKIX_DECREF(checker->state);

cleanup:

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_CertChainChecker_Duplicate
 * (see comments for PKIX_PL_DuplicateCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_CertChainChecker_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_CertChainChecker *checker = NULL;
        PKIX_CertChainChecker *checkerDuplicate = NULL;
        PKIX_List *extensionsDuplicate = NULL;
        PKIX_PL_Object *stateDuplicate = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_CertChainChecker_Duplicate");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_CERTCHAINCHECKER_TYPE, plContext),
                    PKIX_OBJECTNOTCERTCHAINCHECKER);

        checker = (PKIX_CertChainChecker *)object;

        if (checker->extensions){
                PKIX_CHECK(PKIX_PL_Object_Duplicate
                            ((PKIX_PL_Object *)checker->extensions,
                            (PKIX_PL_Object **)&extensionsDuplicate,
                            plContext),
                            PKIX_OBJECTDUPLICATEFAILED);
        }

        if (checker->state){
                PKIX_CHECK(PKIX_PL_Object_Duplicate
                            ((PKIX_PL_Object *)checker->state,
                            (PKIX_PL_Object **)&stateDuplicate,
                            plContext),
                            PKIX_OBJECTDUPLICATEFAILED);
        }

        PKIX_CHECK(PKIX_CertChainChecker_Create
                    (checker->checkCallback,
                    checker->forwardChecking,
                    checker->isForwardDirectionExpected,
                    extensionsDuplicate,
                    stateDuplicate,
                    &checkerDuplicate,
                    plContext),
                    PKIX_CERTCHAINCHECKERCREATEFAILED);

        *pNewObject = (PKIX_PL_Object *)checkerDuplicate;

cleanup:

        PKIX_DECREF(extensionsDuplicate);
        PKIX_DECREF(stateDuplicate);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_CertChainChecker_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_CERTCHAINCHECKER_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_CertChainChecker_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_CertChainChecker_RegisterSelf");

        entry.description = "CertChainChecker";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_CertChainChecker);
        entry.destructor = pkix_CertChainChecker_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_CertChainChecker_Duplicate;

        systemClasses[PKIX_CERTCHAINCHECKER_TYPE] = entry;

        PKIX_RETURN(CERTCHAINCHECKER);
}

/* --Public-Functions--------------------------------------------- */


/*
 * FUNCTION: PKIX_CertChainChecker_Create (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_CertChainChecker_Create(
    PKIX_CertChainChecker_CheckCallback callback,
    PKIX_Boolean forwardCheckingSupported,
    PKIX_Boolean isForwardDirectionExpected,
    PKIX_List *list,  /* list of PKIX_PL_OID */
    PKIX_PL_Object *initialState,
    PKIX_CertChainChecker **pChecker,
    void *plContext)
{
        PKIX_CertChainChecker *checker = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "PKIX_CertChainChecker_Create");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CERTCHAINCHECKER_TYPE,
                    sizeof (PKIX_CertChainChecker),
                    (PKIX_PL_Object **)&checker,
                    plContext),
                    PKIX_COULDNOTCREATECERTCHAINCHECKEROBJECT);

        /* initialize fields */
        checker->checkCallback = callback;
        checker->forwardChecking = forwardCheckingSupported;
        checker->isForwardDirectionExpected = isForwardDirectionExpected;

        PKIX_INCREF(list);
        checker->extensions = list;

        PKIX_INCREF(initialState);
        checker->state = initialState;

        *pChecker = checker;
        checker = NULL;
cleanup:
        
        PKIX_DECREF(checker);

        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: PKIX_CertChainChecker_GetCheckCallback
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_CertChainChecker_GetCheckCallback(
        PKIX_CertChainChecker *checker,
        PKIX_CertChainChecker_CheckCallback *pCallback,
        void *plContext)
{
        PKIX_ENTER(CERTCHAINCHECKER, "PKIX_CertChainChecker_GetCheckCallback");
        PKIX_NULLCHECK_TWO(checker, pCallback);

        *pCallback = checker->checkCallback;

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: PKIX_CertChainChecker_IsForwardCheckingSupported
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_CertChainChecker_IsForwardCheckingSupported(
        PKIX_CertChainChecker *checker,
        PKIX_Boolean *pForwardCheckingSupported,
        void *plContext)
{
        PKIX_ENTER
                (CERTCHAINCHECKER,
                "PKIX_CertChainChecker_IsForwardCheckingSupported");
        PKIX_NULLCHECK_TWO(checker, pForwardCheckingSupported);

        *pForwardCheckingSupported = checker->forwardChecking;

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: PKIX_CertChainChecker_IsForwardDirectionExpected
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_CertChainChecker_IsForwardDirectionExpected(
        PKIX_CertChainChecker *checker,
        PKIX_Boolean *pForwardDirectionExpected,
        void *plContext)
{
        PKIX_ENTER
                (CERTCHAINCHECKER,
                "PKIX_CertChainChecker_IsForwardDirectionExpected");
        PKIX_NULLCHECK_TWO(checker, pForwardDirectionExpected);

        *pForwardDirectionExpected = checker->isForwardDirectionExpected;

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: PKIX_CertChainChecker_GetCertChainCheckerState
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_CertChainChecker_GetCertChainCheckerState(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Object **pCertChainCheckerState,
        void *plContext)
{
        PKIX_ENTER(CERTCHAINCHECKER,
                    "PKIX_CertChainChecker_GetCertChainCheckerState");

        PKIX_NULLCHECK_TWO(checker, pCertChainCheckerState);

        PKIX_INCREF(checker->state);

        *pCertChainCheckerState = checker->state;

cleanup:
        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: PKIX_CertChainChecker_SetCertChainCheckerState
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_CertChainChecker_SetCertChainCheckerState(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Object *certChainCheckerState,
        void *plContext)
{
        PKIX_ENTER(CERTCHAINCHECKER,
                    "PKIX_CertChainChecker_SetCertChainCheckerState");

        PKIX_NULLCHECK_ONE(checker);

        /* DecRef old contents */
        PKIX_DECREF(checker->state);

        PKIX_INCREF(certChainCheckerState);
        checker->state = certChainCheckerState;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)checker, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: PKIX_CertChainChecker_GetSupportedExtensions
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_CertChainChecker_GetSupportedExtensions(
        PKIX_CertChainChecker *checker,
        PKIX_List **pExtensions, /* list of PKIX_PL_OID */
        void *plContext)
{
        PKIX_ENTER(CERTCHAINCHECKER,
                    "PKIX_CertChainChecker_GetSupportedExtensions");

        PKIX_NULLCHECK_TWO(checker, pExtensions);

        PKIX_INCREF(checker->extensions);

        *pExtensions = checker->extensions;

cleanup:
        PKIX_RETURN(CERTCHAINCHECKER);

}
