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
 * pkix_nameconstraintschecker.c
 *
 * Functions for Name Constraints Checkers
 *
 */

#include "pkix_nameconstraintschecker.h"

/* --Private-NameConstraintsCheckerState-Functions---------------------- */

/*
 * FUNCTION: pkix_NameConstraintsCheckerstate_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_NameConstraintsCheckerState_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        pkix_NameConstraintsCheckerState *state = NULL;

        PKIX_ENTER(CERTNAMECONSTRAINTSCHECKERSTATE,
                    "pkix_NameConstraintsCheckerState_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that object type */
        PKIX_CHECK(pkix_CheckType
            (object, PKIX_CERTNAMECONSTRAINTSCHECKERSTATE_TYPE, plContext),
            PKIX_OBJECTNOTNAMECONSTRAINTSCHECKERSTATE);

        state = (pkix_NameConstraintsCheckerState *)object;

        PKIX_DECREF(state->nameConstraints);
        PKIX_DECREF(state->nameConstraintsOID);

cleanup:

        PKIX_RETURN(CERTNAMECONSTRAINTSCHECKERSTATE);
}

/*
 * FUNCTION: pkix_NameConstraintsCheckerState_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_CERTNAMECONSTRAINTSCHECKERSTATE_TYPE and its related
 *  functions with systemClasses[]
 *
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_NameConstraintsCheckerState_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CERTNAMECONSTRAINTSCHECKERSTATE,
                    "pkix_NameConstraintsCheckerState_RegisterSelf");

        entry.description = "NameConstraintsCheckerState";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(pkix_NameConstraintsCheckerState);
        entry.destructor = pkix_NameConstraintsCheckerState_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_CERTNAMECONSTRAINTSCHECKERSTATE_TYPE] = entry;

        PKIX_RETURN(CERTNAMECONSTRAINTSCHECKERSTATE);
}

/*
 * FUNCTION: pkix_NameConstraintsCheckerState_Create
 *
 * DESCRIPTION:
 *  Allocate and initialize NameConstraintsChecker state data.
 *
 * PARAMETERS
 *  "nameConstraints"
 *      Address of NameConstraints to be stored in state. May be NULL.
 *  "numCerts"
 *      Number of certificates in the validation chain. This data is used
 *      to identify end-entity.
 *  "pCheckerState"
 *      Address of NameConstraintsCheckerState that is returned. Must be
 *      non-NULL.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CERTNAMECONSTRAINTSCHECKERSTATE Error if the function fails in
 *  a non-fatal way.
 *  Returns a Fatal Error
 */
static PKIX_Error *
pkix_NameConstraintsCheckerState_Create(
    PKIX_PL_CertNameConstraints *nameConstraints,
    PKIX_UInt32 numCerts,
    pkix_NameConstraintsCheckerState **pCheckerState,
    void *plContext)
{
        pkix_NameConstraintsCheckerState *state = NULL;

        PKIX_ENTER(CERTNAMECONSTRAINTSCHECKERSTATE,
                    "pkix_NameConstraintsCheckerState_Create");
        PKIX_NULLCHECK_ONE(pCheckerState);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CERTNAMECONSTRAINTSCHECKERSTATE_TYPE,
                    sizeof (pkix_NameConstraintsCheckerState),
                    (PKIX_PL_Object **)&state,
                    plContext),
                    PKIX_COULDNOTCREATENAMECONSTRAINTSCHECKERSTATEOBJECT);

        /* Initialize fields */

        PKIX_CHECK(PKIX_PL_OID_Create
                    (PKIX_NAMECONSTRAINTS_OID,
                    &state->nameConstraintsOID,
                    plContext),
                    PKIX_OIDCREATEFAILED);

        PKIX_INCREF(nameConstraints);

        state->nameConstraints = nameConstraints;
        state->certsRemaining = numCerts;

        *pCheckerState = state;
        state = NULL;

cleanup:

        PKIX_DECREF(state);

        PKIX_RETURN(CERTNAMECONSTRAINTSCHECKERSTATE);
}

/* --Private-NameConstraintsChecker-Functions------------------------- */

/*
 * FUNCTION: pkix_NameConstraintsChecker_Check
 * (see comments for PKIX_CertChainChecker_CheckCallback in pkix_checker.h)
 */
static PKIX_Error *
pkix_NameConstraintsChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticalExtensions,
        void **pNBIOContext,
        void *plContext)
{
        pkix_NameConstraintsCheckerState *state = NULL;
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;
        PKIX_PL_CertNameConstraints *mergedNameConstraints = NULL;
        PKIX_Boolean selfIssued = PKIX_FALSE;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_NameConstraintsChecker_Check");
        PKIX_NULLCHECK_THREE(checker, cert, pNBIOContext);

        *pNBIOContext = NULL; /* we never block on pending I/O */

        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                    (checker, (PKIX_PL_Object **)&state, plContext),
                    PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        state->certsRemaining--;

        /* Get status of self issued */
        PKIX_CHECK(pkix_IsCertSelfIssued(cert, &selfIssued, plContext),
                    PKIX_ISCERTSELFISSUEDFAILED);

        /* Check on non self-issued and if so only for last cert */
        if (selfIssued == PKIX_FALSE ||
            (selfIssued == PKIX_TRUE && state->certsRemaining == 0)) {
                PKIX_CHECK(PKIX_PL_Cert_CheckNameConstraints
                    (cert, state->nameConstraints, plContext),
                    PKIX_CERTCHECKNAMECONSTRAINTSFAILED);
        }

        if (state->certsRemaining != 0) {

            PKIX_CHECK(PKIX_PL_Cert_GetNameConstraints
                    (cert, &nameConstraints, plContext),
                    PKIX_CERTGETNAMECONSTRAINTSFAILED);

            /* Merge with previous name constraints kept in state */

            if (nameConstraints != NULL) {

                if (state->nameConstraints == NULL) {

                        state->nameConstraints = nameConstraints;

                } else {

                        PKIX_CHECK(PKIX_PL_Cert_MergeNameConstraints
                                (nameConstraints,
                                state->nameConstraints,
                                &mergedNameConstraints,
                                plContext),
                                PKIX_CERTMERGENAMECONSTRAINTSFAILED);

                        PKIX_DECREF(nameConstraints);
                        PKIX_DECREF(state->nameConstraints);

                        state->nameConstraints = mergedNameConstraints;
                }

                /* Remove Name Constraints Extension OID from list */
                if (unresolvedCriticalExtensions != NULL) {
                        PKIX_CHECK(pkix_List_Remove
                                    (unresolvedCriticalExtensions,
                                    (PKIX_PL_Object *)state->nameConstraintsOID,
                                    plContext),
                                    PKIX_LISTREMOVEFAILED);
                }
            }
        }

        PKIX_CHECK(PKIX_CertChainChecker_SetCertChainCheckerState
                    (checker, (PKIX_PL_Object *)state, plContext),
                    PKIX_CERTCHAINCHECKERSETCERTCHAINCHECKERSTATEFAILED);

cleanup:

        PKIX_DECREF(state);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_NameConstraintsChecker_Initialize
 *
 * DESCRIPTION:
 *  Create a CertChainChecker with a NameConstraintsCheckerState. The
 *  NameConstraintsCheckerState is created with "trustedNC" and "numCerts"
 *  as its initial state. The CertChainChecker for the NameConstraints is
 *  returned at address of "pChecker".
 *
 * PARAMETERS
 *  "trustedNC"
 *      The NameConstraints from trusted anchor Cert is stored at "trustedNC"
 *      for initialization. May be NULL.
 *  "numCerts"
 *      Number of certificates in the validation chain. This data is used
 *      to identify end-entity.
 *  "pChecker"
 *      Address of CertChainChecker to bo created and returned.
 *      Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CERTCHAINCHECKER Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error
 */
PKIX_Error *
pkix_NameConstraintsChecker_Initialize(
        PKIX_PL_CertNameConstraints *trustedNC,
        PKIX_UInt32 numCerts,
        PKIX_CertChainChecker **pChecker,
        void *plContext)
{
        pkix_NameConstraintsCheckerState *state = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_NameConstraintsChecker_Initialize");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(pkix_NameConstraintsCheckerState_Create
                    (trustedNC, numCerts, &state, plContext),
                    PKIX_NAMECONSTRAINTSCHECKERSTATECREATEFAILED);

        PKIX_CHECK(PKIX_CertChainChecker_Create
                    (pkix_NameConstraintsChecker_Check,
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
