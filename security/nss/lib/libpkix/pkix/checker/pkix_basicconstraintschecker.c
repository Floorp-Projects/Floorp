/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_basicconstraintschecker.c
 *
 * Functions for basic constraints validation
 *
 */

#include "pkix_basicconstraintschecker.h"

/* --Private-BasicConstraintsCheckerState-Functions------------------------- */

/*
 * FUNCTION: pkix_BasicConstraintsCheckerState_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BasicConstraintsCheckerState_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        pkix_BasicConstraintsCheckerState *state = NULL;

        PKIX_ENTER(BASICCONSTRAINTSCHECKERSTATE,
                    "pkix_BasicConstraintsCheckerState_Destroy");

        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a basic constraints checker state */
        PKIX_CHECK(pkix_CheckType
                (object, PKIX_BASICCONSTRAINTSCHECKERSTATE_TYPE, plContext),
                PKIX_OBJECTNOTBASICCONSTRAINTSCHECKERSTATE);

        state = (pkix_BasicConstraintsCheckerState *)object;

        PKIX_DECREF(state->basicConstraintsOID);

cleanup:

        PKIX_RETURN(BASICCONSTRAINTSCHECKERSTATE);
}

/*
 * FUNCTION: pkix_BasicConstraintsCheckerState_RegisterSelf
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
pkix_BasicConstraintsCheckerState_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(BASICCONSTRAINTSCHECKERSTATE,
                "pkix_BasicConstraintsCheckerState_RegisterSelf");

        entry.description = "BasicConstraintsCheckerState";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(pkix_BasicConstraintsCheckerState);
        entry.destructor = pkix_BasicConstraintsCheckerState_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_BASICCONSTRAINTSCHECKERSTATE_TYPE] = entry;

        PKIX_RETURN(BASICCONSTRAINTSCHECKERSTATE);
}

/*
 * FUNCTION: pkix_BasicConstraintsCheckerState_Create
 * DESCRIPTION:
 *
 *  Creates a new BasicConstraintsCheckerState using the number of certs in
 *  the chain represented by "certsRemaining" and stores it at "pState".
 *
 * PARAMETERS:
 *  "certsRemaining"
 *      Number of certificates in the chain.
 *  "pState"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a BasicConstraintsCheckerState Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_BasicConstraintsCheckerState_Create(
        PKIX_UInt32 certsRemaining,
        pkix_BasicConstraintsCheckerState **pState,
        void *plContext)
{
        pkix_BasicConstraintsCheckerState *state = NULL;

        PKIX_ENTER(BASICCONSTRAINTSCHECKERSTATE,
                    "pkix_BasicConstraintsCheckerState_Create");

        PKIX_NULLCHECK_ONE(pState);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_BASICCONSTRAINTSCHECKERSTATE_TYPE,
                    sizeof (pkix_BasicConstraintsCheckerState),
                    (PKIX_PL_Object **)&state,
                    plContext),
                    PKIX_COULDNOTCREATEBASICCONSTRAINTSSTATEOBJECT);

        /* initialize fields */
        state->certsRemaining = certsRemaining;
        state->maxPathLength = PKIX_UNLIMITED_PATH_CONSTRAINT;

        PKIX_CHECK(PKIX_PL_OID_Create
                    (PKIX_BASICCONSTRAINTS_OID,
                    &state->basicConstraintsOID,
                    plContext),
                    PKIX_OIDCREATEFAILED);

        *pState = state;
        state = NULL;

cleanup:

        PKIX_DECREF(state);

        PKIX_RETURN(BASICCONSTRAINTSCHECKERSTATE);
}

/* --Private-BasicConstraintsChecker-Functions------------------------------ */

/*
 * FUNCTION: pkix_BasicConstraintsChecker_Check
 * (see comments for PKIX_CertChainChecker_CheckCallback in pkix_checker.h)
 */
PKIX_Error *
pkix_BasicConstraintsChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticalExtensions,  /* list of PKIX_PL_OID */
        void **pNBIOContext,
        void *plContext)
{
        PKIX_PL_CertBasicConstraints *basicConstraints = NULL;
        pkix_BasicConstraintsCheckerState *state = NULL;
        PKIX_Boolean caFlag = PKIX_FALSE;
        PKIX_Int32 pathLength = 0;
        PKIX_Int32 maxPathLength_now;
        PKIX_Boolean isSelfIssued = PKIX_FALSE;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_BasicConstraintsChecker_Check");
        PKIX_NULLCHECK_THREE(checker, cert, pNBIOContext);

        *pNBIOContext = NULL; /* we never block on pending I/O */

        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                    (checker, (PKIX_PL_Object **)&state, plContext),
                    PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        state->certsRemaining--;

        if (state->certsRemaining != 0) {

                PKIX_CHECK(PKIX_PL_Cert_GetBasicConstraints
                    (cert, &basicConstraints, plContext),
                    PKIX_CERTGETBASICCONSTRAINTSFAILED);

                /* get CA Flag and path length */
                if (basicConstraints != NULL) {
                        PKIX_CHECK(PKIX_PL_BasicConstraints_GetCAFlag
                            (basicConstraints,
                            &caFlag,
                            plContext),
                            PKIX_BASICCONSTRAINTSGETCAFLAGFAILED);

                if (caFlag == PKIX_TRUE) {
                        PKIX_CHECK
                            (PKIX_PL_BasicConstraints_GetPathLenConstraint
                            (basicConstraints,
                            &pathLength,
                            plContext),
                            PKIX_BASICCONSTRAINTSGETPATHLENCONSTRAINTFAILED);
                }

                }else{
                        caFlag = PKIX_FALSE;
                        pathLength = PKIX_UNLIMITED_PATH_CONSTRAINT;
                }

                PKIX_CHECK(pkix_IsCertSelfIssued
                        (cert,
                        &isSelfIssued,
                        plContext),
                        PKIX_ISCERTSELFISSUEDFAILED);

                maxPathLength_now = state->maxPathLength;

                if (isSelfIssued != PKIX_TRUE) {

                    /* Not last CA Cert, but maxPathLength is down to zero */
                    if (maxPathLength_now == 0) {
                        PKIX_ERROR(PKIX_BASICCONSTRAINTSVALIDATIONFAILEDLN);
                    }

                    if (caFlag == PKIX_FALSE) {
                        PKIX_ERROR(PKIX_BASICCONSTRAINTSVALIDATIONFAILEDCA);
                    }

                    if (maxPathLength_now > 0) { /* can be unlimited (-1) */
                        maxPathLength_now--;
                    }

                }

                if (caFlag == PKIX_TRUE) {
                    if (maxPathLength_now == PKIX_UNLIMITED_PATH_CONSTRAINT){
                            maxPathLength_now = pathLength;
                    } else {
                            /* If pathLength is not specified, don't set */
                        if (pathLength != PKIX_UNLIMITED_PATH_CONSTRAINT) {
                            maxPathLength_now =
                                    (maxPathLength_now > pathLength)?
                                    pathLength:maxPathLength_now;
                        }
                    }
                }

                state->maxPathLength = maxPathLength_now;
        }

        /* Remove Basic Constraints Extension OID from list */
        if (unresolvedCriticalExtensions != NULL) {

                PKIX_CHECK(pkix_List_Remove
                            (unresolvedCriticalExtensions,
                            (PKIX_PL_Object *) state->basicConstraintsOID,
                            plContext),
                            PKIX_LISTREMOVEFAILED);
        }


        PKIX_CHECK(PKIX_CertChainChecker_SetCertChainCheckerState
                    (checker, (PKIX_PL_Object *)state, plContext),
                    PKIX_CERTCHAINCHECKERSETCERTCHAINCHECKERSTATEFAILED);


cleanup:
        PKIX_DECREF(state);
        PKIX_DECREF(basicConstraints);
        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: pkix_BasicConstraintsChecker_Initialize
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
pkix_BasicConstraintsChecker_Initialize(
        PKIX_UInt32 certsRemaining,
        PKIX_CertChainChecker **pChecker,
        void *plContext)
{
        pkix_BasicConstraintsCheckerState *state = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_BasicConstraintsChecker_Initialize");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(pkix_BasicConstraintsCheckerState_Create
                    (certsRemaining, &state, plContext),
                    PKIX_BASICCONSTRAINTSCHECKERSTATECREATEFAILED);

        PKIX_CHECK(PKIX_CertChainChecker_Create
                    (pkix_BasicConstraintsChecker_Check,
                    PKIX_FALSE,
                    PKIX_FALSE,
                    NULL,
                    (PKIX_PL_Object *)state,
                    pChecker,
                    plContext),
                    PKIX_CERTCHAINCHECKERCHECKFAILED);

cleanup:
        PKIX_DECREF(state);

        PKIX_RETURN(CERTCHAINCHECKER);
}
