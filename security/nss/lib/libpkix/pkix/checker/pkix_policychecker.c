/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_policychecker.c
 *
 * Functions for Policy Checker
 *
 */
#include "pkix_policychecker.h"

/* --Forward declarations----------------------------------------------- */

static PKIX_Error *
pkix_PolicyChecker_MakeSingleton(
        PKIX_PL_Object *listItem,
        PKIX_Boolean immutability,
        PKIX_List **pList,
        void *plContext);

/* --Private-PolicyCheckerState-Functions---------------------------------- */

/*
 * FUNCTION:pkix_PolicyCheckerState_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_PolicyCheckerState_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PolicyCheckerState *checkerState = NULL;

        PKIX_ENTER(CERTPOLICYCHECKERSTATE, "pkix_PolicyCheckerState_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_CERTPOLICYCHECKERSTATE_TYPE, plContext),
                PKIX_OBJECTNOTPOLICYCHECKERSTATE);

        checkerState = (PKIX_PolicyCheckerState *)object;

        PKIX_DECREF(checkerState->certPoliciesExtension);
        PKIX_DECREF(checkerState->policyMappingsExtension);
        PKIX_DECREF(checkerState->policyConstraintsExtension);
        PKIX_DECREF(checkerState->inhibitAnyPolicyExtension);
        PKIX_DECREF(checkerState->anyPolicyOID);
        PKIX_DECREF(checkerState->validPolicyTree);
        PKIX_DECREF(checkerState->userInitialPolicySet);
        PKIX_DECREF(checkerState->mappedUserInitialPolicySet);

        checkerState->policyQualifiersRejected = PKIX_FALSE;
        checkerState->explicitPolicy = 0;
        checkerState->inhibitAnyPolicy = 0;
        checkerState->policyMapping = 0;
        checkerState->numCerts = 0;
        checkerState->certsProcessed = 0;
        checkerState->certPoliciesCritical = PKIX_FALSE;

        PKIX_DECREF(checkerState->anyPolicyNodeAtBottom);
        PKIX_DECREF(checkerState->newAnyPolicyNode);
        PKIX_DECREF(checkerState->mappedPolicyOIDs);

cleanup:

        PKIX_RETURN(CERTPOLICYCHECKERSTATE);
}

/*
 * FUNCTION: pkix_PolicyCheckerState_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_PolicyCheckerState_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pCheckerStateString,
        void *plContext)
{
        PKIX_PolicyCheckerState *state = NULL;
        PKIX_PL_String *resultString = NULL;
        PKIX_PL_String *policiesExtOIDString = NULL;
        PKIX_PL_String *policyMapOIDString = NULL;
        PKIX_PL_String *policyConstrOIDString = NULL;
        PKIX_PL_String *inhAnyPolOIDString = NULL;
        PKIX_PL_String *anyPolicyOIDString = NULL;
        PKIX_PL_String *validPolicyTreeString = NULL;
        PKIX_PL_String *userInitialPolicySetString = NULL;
        PKIX_PL_String *mappedUserPolicySetString = NULL;
        PKIX_PL_String *mappedPolicyOIDsString = NULL;
        PKIX_PL_String *anyAtBottomString = NULL;
        PKIX_PL_String *newAnyPolicyString = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *trueString = NULL;
        PKIX_PL_String *falseString = NULL;
        PKIX_PL_String *nullString = NULL;
        PKIX_Boolean initialPolicyMappingInhibit = PKIX_FALSE;
        PKIX_Boolean initialExplicitPolicy = PKIX_FALSE;
        PKIX_Boolean initialAnyPolicyInhibit = PKIX_FALSE;
        PKIX_Boolean initialIsAnyPolicy = PKIX_FALSE;
        PKIX_Boolean policyQualifiersRejected = PKIX_FALSE;
        PKIX_Boolean certPoliciesCritical = PKIX_FALSE;
        char *asciiFormat =
                "{\n"
                "\tcertPoliciesExtension:    \t%s\n"
                "\tpolicyMappingsExtension:  \t%s\n"
                "\tpolicyConstraintsExtension:\t%s\n"
                "\tinhibitAnyPolicyExtension:\t%s\n"
                "\tanyPolicyOID:             \t%s\n"
                "\tinitialIsAnyPolicy:       \t%s\n"
                "\tvalidPolicyTree:          \t%s\n"
                "\tuserInitialPolicySet:     \t%s\n"
                "\tmappedUserPolicySet:      \t%s\n"
                "\tpolicyQualifiersRejected: \t%s\n"
                "\tinitialPolMappingInhibit: \t%s\n"
                "\tinitialExplicitPolicy:    \t%s\n"
                "\tinitialAnyPolicyInhibit:  \t%s\n"
                "\texplicitPolicy:           \t%d\n"
                "\tinhibitAnyPolicy:         \t%d\n"
                "\tpolicyMapping:            \t%d\n"
                "\tnumCerts:                 \t%d\n"
                "\tcertsProcessed:           \t%d\n"
                "\tanyPolicyNodeAtBottom:    \t%s\n"
                "\tnewAnyPolicyNode:         \t%s\n"
                "\tcertPoliciesCritical:     \t%s\n"
                "\tmappedPolicyOIDs:         \t%s\n"
                "}";

        PKIX_ENTER(CERTPOLICYCHECKERSTATE, "pkix_PolicyCheckerState_ToString");

        PKIX_NULLCHECK_TWO(object, pCheckerStateString);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_CERTPOLICYCHECKERSTATE_TYPE, plContext),
                PKIX_OBJECTNOTPOLICYCHECKERSTATE);

        state = (PKIX_PolicyCheckerState *)object;
        PKIX_NULLCHECK_THREE
                (state->certPoliciesExtension,
                state->policyMappingsExtension,
                state->policyConstraintsExtension);
        PKIX_NULLCHECK_THREE
                (state->inhibitAnyPolicyExtension,
                state->anyPolicyOID,
                state->userInitialPolicySet);

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, asciiFormat, 0, &formatString, plContext),
                PKIX_STRINGCREATEFAILED);
        /*
         * Create TRUE, FALSE, and "NULL" PKIX_PL_Strings. But creating a
         * PKIX_PL_String is complicated enough, it's worth checking, for
         * each, to make sure the string is needed.
         */
        initialPolicyMappingInhibit = state->initialPolicyMappingInhibit;
        initialExplicitPolicy = state->initialExplicitPolicy;
        initialAnyPolicyInhibit = state->initialAnyPolicyInhibit;
        initialIsAnyPolicy = state->initialIsAnyPolicy;
        policyQualifiersRejected = state->policyQualifiersRejected;
        certPoliciesCritical = state->certPoliciesCritical;

        if (initialPolicyMappingInhibit || initialExplicitPolicy ||
            initialAnyPolicyInhibit || initialIsAnyPolicy ||
            policyQualifiersRejected || certPoliciesCritical) {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII, "TRUE", 0, &trueString, plContext),
                        PKIX_STRINGCREATEFAILED);
        }
        if (!initialPolicyMappingInhibit || !initialExplicitPolicy ||
            !initialAnyPolicyInhibit || !initialIsAnyPolicy ||
            !policyQualifiersRejected || !certPoliciesCritical) {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII, "FALSE", 0, &falseString, plContext),
                        PKIX_STRINGCREATEFAILED);
        }
        if (!(state->anyPolicyNodeAtBottom) || !(state->newAnyPolicyNode)) {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII, "(null)", 0, &nullString, plContext),
                        PKIX_STRINGCREATEFAILED);
        }

        PKIX_TOSTRING
                (state->certPoliciesExtension, &policiesExtOIDString, plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING
                (state->policyMappingsExtension,
                &policyMapOIDString,
                plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING
                (state->policyConstraintsExtension,
                &policyConstrOIDString,
                plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING
                (state->inhibitAnyPolicyExtension,
                &inhAnyPolOIDString,
                plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING(state->anyPolicyOID, &anyPolicyOIDString, plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING(state->validPolicyTree, &validPolicyTreeString, plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING
                (state->userInitialPolicySet,
                &userInitialPolicySetString,
                plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING
                (state->mappedUserInitialPolicySet,
                &mappedUserPolicySetString,
                plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        if (state->anyPolicyNodeAtBottom) {
                PKIX_CHECK(pkix_SinglePolicyNode_ToString
                        (state->anyPolicyNodeAtBottom,
                        &anyAtBottomString,
                        plContext),
                        PKIX_SINGLEPOLICYNODETOSTRINGFAILED);
        } else {
                PKIX_INCREF(nullString);
                anyAtBottomString = nullString;
        }

        if (state->newAnyPolicyNode) {
                PKIX_CHECK(pkix_SinglePolicyNode_ToString
                        (state->newAnyPolicyNode,
                        &newAnyPolicyString,
                        plContext),
                        PKIX_SINGLEPOLICYNODETOSTRINGFAILED);
        } else {
                PKIX_INCREF(nullString);
                newAnyPolicyString = nullString;
        }

        PKIX_TOSTRING
                (state->mappedPolicyOIDs,
                &mappedPolicyOIDsString,
                plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                (&resultString,
                plContext,
                formatString,
                policiesExtOIDString,
                policyMapOIDString,
                policyConstrOIDString,
                inhAnyPolOIDString,
                anyPolicyOIDString,
                initialIsAnyPolicy?trueString:falseString,
                validPolicyTreeString,
                userInitialPolicySetString,
                mappedUserPolicySetString,
                policyQualifiersRejected?trueString:falseString,
                initialPolicyMappingInhibit?trueString:falseString,
                initialExplicitPolicy?trueString:falseString,
                initialAnyPolicyInhibit?trueString:falseString,
                state->explicitPolicy,
                state->inhibitAnyPolicy,
                state->policyMapping,
                state->numCerts,
                state->certsProcessed,
                anyAtBottomString,
                newAnyPolicyString,
                certPoliciesCritical?trueString:falseString,
                mappedPolicyOIDsString),
                PKIX_SPRINTFFAILED);

        *pCheckerStateString = resultString;

cleanup:
        PKIX_DECREF(policiesExtOIDString);
        PKIX_DECREF(policyMapOIDString);
        PKIX_DECREF(policyConstrOIDString);
        PKIX_DECREF(inhAnyPolOIDString);
        PKIX_DECREF(anyPolicyOIDString);
        PKIX_DECREF(validPolicyTreeString);
        PKIX_DECREF(userInitialPolicySetString);
        PKIX_DECREF(mappedUserPolicySetString);
        PKIX_DECREF(anyAtBottomString);
        PKIX_DECREF(newAnyPolicyString);
        PKIX_DECREF(mappedPolicyOIDsString);
        PKIX_DECREF(formatString);
        PKIX_DECREF(trueString);
        PKIX_DECREF(falseString);
        PKIX_DECREF(nullString);

        PKIX_RETURN(CERTPOLICYCHECKERSTATE);
}

/*
 * FUNCTION: pkix_PolicyCheckerState_RegisterSelf
 * DESCRIPTION:
 *
 *  Registers PKIX_POLICYCHECKERSTATE_TYPE and its related functions
 *      with systemClasses[]
 *
 * PARAMETERS:
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_PolicyCheckerState_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER
                (CERTPOLICYCHECKERSTATE,
                "pkix_PolicyCheckerState_RegisterSelf");

        entry.description = "PolicyCheckerState";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PolicyCheckerState);
        entry.destructor = pkix_PolicyCheckerState_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = pkix_PolicyCheckerState_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_CERTPOLICYCHECKERSTATE_TYPE] = entry;

        PKIX_RETURN(CERTPOLICYCHECKERSTATE);
}

/*
 * FUNCTION:pkix_PolicyCheckerState_Create
 * DESCRIPTION:
 *
 *  Creates a PolicyCheckerState Object, using the List pointed to
 *  by "initialPolicies" for the user-initial-policy-set, the Boolean value
 *  of "policyQualifiersRejected" for the policyQualifiersRejected parameter,
 *  the Boolean value of "initialPolicyMappingInhibit" for the
 *  inhibitPolicyMappings parameter, the Boolean value of
 *  "initialExplicitPolicy" for the initialExplicitPolicy parameter, the
 *  Boolean value of "initialAnyPolicyInhibit" for the inhibitAnyPolicy
 *  parameter, and the UInt32 value of "numCerts" as the number of
 *  certificates in the chain; and stores the Object at "pCheckerState".
 *
 * PARAMETERS:
 *  "initialPolicies"
 *      Address of List of OIDs comprising the user-initial-policy-set; the List
 *      may be empty, but must be non-NULL
 *  "policyQualifiersRejected"
 *      Boolean value of the policyQualifiersRejected parameter
 *  "initialPolicyMappingInhibit"
 *      Boolean value of the inhibitPolicyMappings parameter
 *  "initialExplicitPolicy"
 *      Boolean value of the initialExplicitPolicy parameter
 *  "initialAnyPolicyInhibit"
 *      Boolean value of the inhibitAnyPolicy parameter
 *  "numCerts"
 *      Number of certificates in the chain to be validated
 *  "pCheckerState"
 *      Address where PolicyCheckerState will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertPolicyCheckerState Error if the functions fails in a
 *      non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyCheckerState_Create(
        PKIX_List *initialPolicies,
        PKIX_Boolean policyQualifiersRejected,
        PKIX_Boolean initialPolicyMappingInhibit,
        PKIX_Boolean initialExplicitPolicy,
        PKIX_Boolean initialAnyPolicyInhibit,
        PKIX_UInt32 numCerts,
        PKIX_PolicyCheckerState **pCheckerState,
        void *plContext)
{
        PKIX_PolicyCheckerState *checkerState = NULL;
        PKIX_PolicyNode *policyNode = NULL;
        PKIX_List *anyPolicyList = NULL;
        PKIX_Boolean initialPoliciesIsEmpty = PKIX_FALSE;

        PKIX_ENTER(CERTPOLICYCHECKERSTATE, "pkix_PolicyCheckerState_Create");
        PKIX_NULLCHECK_TWO(initialPolicies, pCheckerState);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_CERTPOLICYCHECKERSTATE_TYPE,
                sizeof (PKIX_PolicyCheckerState),
                (PKIX_PL_Object **)&checkerState,
                plContext),
                PKIX_COULDNOTCREATEPOLICYCHECKERSTATEOBJECT);

        /* Create constant PKIX_PL_OIDs: */

        PKIX_CHECK(PKIX_PL_OID_Create
                (PKIX_CERTIFICATEPOLICIES_OID,
                &(checkerState->certPoliciesExtension),
                plContext),
                PKIX_OIDCREATEFAILED);

        PKIX_CHECK(PKIX_PL_OID_Create
                (PKIX_POLICYMAPPINGS_OID,
                &(checkerState->policyMappingsExtension),
                plContext),
                PKIX_OIDCREATEFAILED);

        PKIX_CHECK(PKIX_PL_OID_Create
                (PKIX_POLICYCONSTRAINTS_OID,
                &(checkerState->policyConstraintsExtension),
                plContext),
                PKIX_OIDCREATEFAILED);

        PKIX_CHECK(PKIX_PL_OID_Create
                (PKIX_INHIBITANYPOLICY_OID,
                &(checkerState->inhibitAnyPolicyExtension),
                plContext),
                PKIX_OIDCREATEFAILED);

        PKIX_CHECK(PKIX_PL_OID_Create
                (PKIX_CERTIFICATEPOLICIES_ANYPOLICY_OID,
                &(checkerState->anyPolicyOID),
                plContext),
                PKIX_OIDCREATEFAILED);

        /* Create an initial policy set from argument supplied */
        PKIX_INCREF(initialPolicies);
        checkerState->userInitialPolicySet = initialPolicies;
        PKIX_INCREF(initialPolicies);
        checkerState->mappedUserInitialPolicySet = initialPolicies;

        PKIX_CHECK(PKIX_List_IsEmpty
                (initialPolicies,
                &initialPoliciesIsEmpty,
                plContext),
                PKIX_LISTISEMPTYFAILED);
        if (initialPoliciesIsEmpty) {
                checkerState->initialIsAnyPolicy = PKIX_TRUE;
        } else {
                PKIX_CHECK(pkix_List_Contains
                        (initialPolicies,
                        (PKIX_PL_Object *)(checkerState->anyPolicyOID),
                        &(checkerState->initialIsAnyPolicy),
                        plContext),
                        PKIX_LISTCONTAINSFAILED);
        }

        checkerState->policyQualifiersRejected =
                policyQualifiersRejected;
        checkerState->initialExplicitPolicy = initialExplicitPolicy;
        checkerState->explicitPolicy =
                (initialExplicitPolicy? 0: numCerts + 1);
        checkerState->initialAnyPolicyInhibit = initialAnyPolicyInhibit;
        checkerState->inhibitAnyPolicy =
                (initialAnyPolicyInhibit? 0: numCerts + 1);
        checkerState->initialPolicyMappingInhibit = initialPolicyMappingInhibit;
        checkerState->policyMapping =
                (initialPolicyMappingInhibit? 0: numCerts + 1);
                ;
        checkerState->numCerts = numCerts;
        checkerState->certsProcessed = 0;
        checkerState->certPoliciesCritical = PKIX_FALSE;

        /* Create a valid_policy_tree as in RFC3280 6.1.2(a) */
        PKIX_CHECK(pkix_PolicyChecker_MakeSingleton
                ((PKIX_PL_Object *)(checkerState->anyPolicyOID),
                PKIX_TRUE,
                &anyPolicyList,
                plContext),
                PKIX_POLICYCHECKERMAKESINGLETONFAILED);

        PKIX_CHECK(pkix_PolicyNode_Create
                (checkerState->anyPolicyOID,    /* validPolicy */
                NULL,                           /* qualifier set */
                PKIX_FALSE,                     /* criticality */
                anyPolicyList,                  /* expectedPolicySet */
                &policyNode,
                plContext),
                PKIX_POLICYNODECREATEFAILED);
        checkerState->validPolicyTree = policyNode;

        /*
         * Since the initial validPolicyTree specifies
         * ANY_POLICY, begin with a pointer to the root node.
         */
        PKIX_INCREF(policyNode);
        checkerState->anyPolicyNodeAtBottom = policyNode;

        checkerState->newAnyPolicyNode = NULL;

        checkerState->mappedPolicyOIDs = NULL;

        *pCheckerState = checkerState;
        checkerState = NULL;

cleanup:

        PKIX_DECREF(checkerState);

        PKIX_DECREF(anyPolicyList);

        PKIX_RETURN(CERTPOLICYCHECKERSTATE);
}

/* --Private-PolicyChecker-Functions--------------------------------------- */

/*
 * FUNCTION: pkix_PolicyChecker_MapContains
 * DESCRIPTION:
 *
 *  Checks the List of CertPolicyMaps pointed to by "certPolicyMaps", to
 *  determine whether the OID pointed to by "policy" is among the
 *  issuerDomainPolicies or subjectDomainPolicies of "certPolicyMaps", and
 *  stores the result in "pFound".
 *
 *  This function is intended to allow an efficient check that the proscription
 *  against anyPolicy being mapped, described in RFC3280 Section 6.1.4(a), is
 *  not violated.
 *
 * PARAMETERS:
 *  "certPolicyMaps"
 *      Address of List of CertPolicyMaps to be searched. May be empty, but
 *      must be non-NULL
 *  "policy"
 *      Address of OID to be checked for. Must be non-NULL
 *  "pFound"
 *      Address where the result of the search will be stored. Must be non-NULL.
 *  "plContext"
 *      platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_PolicyChecker_MapContains(
        PKIX_List *certPolicyMaps,
        PKIX_PL_OID *policy,
        PKIX_Boolean *pFound,
        void *plContext)
{
        PKIX_PL_CertPolicyMap *map = NULL;
        PKIX_UInt32 numEntries = 0;
        PKIX_UInt32 index = 0;
        PKIX_Boolean match = PKIX_FALSE;
        PKIX_PL_OID *issuerDomainPolicy = NULL;
        PKIX_PL_OID *subjectDomainPolicy = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_PolicyChecker_MapContains");
        PKIX_NULLCHECK_THREE(certPolicyMaps, policy, pFound);

        PKIX_CHECK(PKIX_List_GetLength(certPolicyMaps, &numEntries, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (index = 0; (!match) && (index < numEntries); index++) {
                PKIX_CHECK(PKIX_List_GetItem
                    (certPolicyMaps, index, (PKIX_PL_Object **)&map, plContext),
                    PKIX_LISTGETITEMFAILED);

                PKIX_NULLCHECK_ONE(map);

                PKIX_CHECK(PKIX_PL_CertPolicyMap_GetIssuerDomainPolicy
                        (map, &issuerDomainPolicy, plContext),
                        PKIX_CERTPOLICYMAPGETISSUERDOMAINPOLICYFAILED);

                PKIX_EQUALS
                        (policy, issuerDomainPolicy, &match, plContext,
                        PKIX_OBJECTEQUALSFAILED);

                if (!match) {
                        PKIX_CHECK(PKIX_PL_CertPolicyMap_GetSubjectDomainPolicy
                                (map, &subjectDomainPolicy, plContext),
                                PKIX_CERTPOLICYMAPGETSUBJECTDOMAINPOLICYFAILED);

                        PKIX_EQUALS
                                (policy, subjectDomainPolicy, &match, plContext,
                                PKIX_OBJECTEQUALSFAILED);
                }

                PKIX_DECREF(map);
                PKIX_DECREF(issuerDomainPolicy);
                PKIX_DECREF(subjectDomainPolicy);
        }

        *pFound = match;

cleanup:

        PKIX_DECREF(map);
        PKIX_DECREF(issuerDomainPolicy);
        PKIX_DECREF(subjectDomainPolicy);
        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_MapGetSubjectDomainPolicies
 * DESCRIPTION:
 *
 *  Checks the List of CertPolicyMaps pointed to by "certPolicyMaps", to create
 *  a list of all SubjectDomainPolicies for which the IssuerDomainPolicy is the
 *  policy pointed to by "policy", and stores the result in
 *  "pSubjectDomainPolicies".
 *
 *  If the List of CertPolicyMaps provided in "certPolicyMaps" is NULL, the
 *  resulting List will be NULL. If there are CertPolicyMaps, but none that
 *  include "policy" as an IssuerDomainPolicy, the returned List pointer will
 *  be NULL. Otherwise, the returned List will contain the SubjectDomainPolicies
 *  of all CertPolicyMaps for which "policy" is the IssuerDomainPolicy. If a
 *  List is returned it will be immutable.
 *
 * PARAMETERS:
 *  "certPolicyMaps"
 *      Address of List of CertPolicyMaps to be searched. May be empty or NULL.
 *  "policy"
 *      Address of OID to be checked for. Must be non-NULL
 *  "pSubjectDomainPolicies"
 *      Address where the result of the search will be stored. Must be non-NULL.
 *  "plContext"
 *      platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_PolicyChecker_MapGetSubjectDomainPolicies(
        PKIX_List *certPolicyMaps,
        PKIX_PL_OID *policy,
        PKIX_List **pSubjectDomainPolicies,
        void *plContext)
{
        PKIX_PL_CertPolicyMap *map = NULL;
        PKIX_List *subjectList = NULL;
        PKIX_UInt32 numEntries = 0;
        PKIX_UInt32 index = 0;
        PKIX_Boolean match = PKIX_FALSE;
        PKIX_PL_OID *issuerDomainPolicy = NULL;
        PKIX_PL_OID *subjectDomainPolicy = NULL;

        PKIX_ENTER
                (CERTCHAINCHECKER,
                "pkix_PolicyChecker_MapGetSubjectDomainPolicies");
        PKIX_NULLCHECK_TWO(policy, pSubjectDomainPolicies);

        if (certPolicyMaps) {
                PKIX_CHECK(PKIX_List_GetLength
                    (certPolicyMaps,
                    &numEntries,
                    plContext),
                    PKIX_LISTGETLENGTHFAILED);
        }

        for (index = 0; index < numEntries; index++) {
                PKIX_CHECK(PKIX_List_GetItem
                    (certPolicyMaps, index, (PKIX_PL_Object **)&map, plContext),
                    PKIX_LISTGETITEMFAILED);

                PKIX_NULLCHECK_ONE(map);

                PKIX_CHECK(PKIX_PL_CertPolicyMap_GetIssuerDomainPolicy
                        (map, &issuerDomainPolicy, plContext),
                        PKIX_CERTPOLICYMAPGETISSUERDOMAINPOLICYFAILED);

                PKIX_EQUALS
                    (policy, issuerDomainPolicy, &match, plContext,
                    PKIX_OBJECTEQUALSFAILED);

                if (match) {
                    if (!subjectList) {
                        PKIX_CHECK(PKIX_List_Create(&subjectList, plContext),
                                PKIX_LISTCREATEFAILED);
                    }

                    PKIX_CHECK(PKIX_PL_CertPolicyMap_GetSubjectDomainPolicy
                        (map, &subjectDomainPolicy, plContext),
                        PKIX_CERTPOLICYMAPGETSUBJECTDOMAINPOLICYFAILED);

                    PKIX_CHECK(PKIX_List_AppendItem
                        (subjectList,
                        (PKIX_PL_Object *)subjectDomainPolicy,
                        plContext),
                        PKIX_LISTAPPENDITEMFAILED);
                }

                PKIX_DECREF(map);
                PKIX_DECREF(issuerDomainPolicy);
                PKIX_DECREF(subjectDomainPolicy);
        }

        if (subjectList) {
                PKIX_CHECK(PKIX_List_SetImmutable(subjectList, plContext),
                        PKIX_LISTSETIMMUTABLEFAILED);
        }

        *pSubjectDomainPolicies = subjectList;

cleanup:

        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(subjectList);
        }

        PKIX_DECREF(map);
        PKIX_DECREF(issuerDomainPolicy);
        PKIX_DECREF(subjectDomainPolicy);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_MapGetMappedPolicies
 * DESCRIPTION:
 *
 *  Checks the List of CertPolicyMaps pointed to by "certPolicyMaps" to create a
 *  List of all IssuerDomainPolicies, and stores the result in
 * "pMappedPolicies".
 *
 *  The caller may not rely on the IssuerDomainPolicies to be in any particular
 *  order. IssuerDomainPolicies that appear in more than one CertPolicyMap will
 *  only appear once in "pMappedPolicies". If "certPolicyMaps" is empty the
 *  result will be an empty List. The created List is mutable.
 *
 * PARAMETERS:
 *  "certPolicyMaps"
 *      Address of List of CertPolicyMaps to be searched. May be empty, but
 *      must be non-NULL.
 *  "pMappedPolicies"
 *      Address where the result will be stored. Must be non-NULL.
 *  "plContext"
 *      platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_PolicyChecker_MapGetMappedPolicies(
        PKIX_List *certPolicyMaps,
        PKIX_List **pMappedPolicies,
        void *plContext)
{
        PKIX_PL_CertPolicyMap *map = NULL;
        PKIX_List *mappedList = NULL;
        PKIX_UInt32 numEntries = 0;
        PKIX_UInt32 index = 0;
        PKIX_Boolean isContained = PKIX_FALSE;
        PKIX_PL_OID *issuerDomainPolicy = NULL;

        PKIX_ENTER
                (CERTCHAINCHECKER, "pkix_PolicyChecker_MapGetMappedPolicies");
        PKIX_NULLCHECK_TWO(certPolicyMaps, pMappedPolicies);

        PKIX_CHECK(PKIX_List_Create(&mappedList, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_GetLength(certPolicyMaps, &numEntries, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (index = 0; index < numEntries; index++) {
                PKIX_CHECK(PKIX_List_GetItem
                    (certPolicyMaps, index, (PKIX_PL_Object **)&map, plContext),
                    PKIX_LISTGETITEMFAILED);

                PKIX_NULLCHECK_ONE(map);

                PKIX_CHECK(PKIX_PL_CertPolicyMap_GetIssuerDomainPolicy
                        (map, &issuerDomainPolicy, plContext),
                        PKIX_CERTPOLICYMAPGETISSUERDOMAINPOLICYFAILED);

                PKIX_CHECK(pkix_List_Contains
                        (mappedList,
                        (PKIX_PL_Object *)issuerDomainPolicy,
                        &isContained,
                        plContext),
                        PKIX_LISTCONTAINSFAILED);

                if (isContained == PKIX_FALSE) {
                        PKIX_CHECK(PKIX_List_AppendItem
                                (mappedList,
                                (PKIX_PL_Object *)issuerDomainPolicy,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);
                }

                PKIX_DECREF(map);
                PKIX_DECREF(issuerDomainPolicy);
        }

        *pMappedPolicies = mappedList;

cleanup:

        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(mappedList);
        }

        PKIX_DECREF(map);
        PKIX_DECREF(issuerDomainPolicy);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_MakeMutableCopy
 * DESCRIPTION:
 *
 *  Creates a mutable copy of the List pointed to by "list", which may or may
 *  not be immutable, and stores the address at "pMutableCopy".
 *
 * PARAMETERS:
 *  "list"
 *      Address of List to be copied. Must be non-NULL.
 *  "pMutableCopy"
 *      Address where mutable copy will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_MakeMutableCopy(
        PKIX_List *list,
        PKIX_List **pMutableCopy,
        void *plContext)
{
        PKIX_List *newList = NULL;
        PKIX_UInt32 listLen = 0;
        PKIX_UInt32 listIx = 0;
        PKIX_PL_Object *object = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_PolicyChecker_MakeMutableCopy");
        PKIX_NULLCHECK_TWO(list, pMutableCopy);

        PKIX_CHECK(PKIX_List_Create(&newList, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_GetLength(list, &listLen, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (listIx = 0; listIx < listLen; listIx++) {

                PKIX_CHECK(PKIX_List_GetItem(list, listIx, &object, plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(PKIX_List_AppendItem(newList, object, plContext),
                        PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(object);
        }

        *pMutableCopy = newList;
        newList = NULL;
        
cleanup:
        PKIX_DECREF(newList);
        PKIX_DECREF(object);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_MakeSingleton
 * DESCRIPTION:
 *
 *  Creates a new List containing the Object pointed to by "listItem", using
 *  the Boolean value of "immutability" to determine whether to set the List
 *  immutable, and stores the address at "pList".
 *
 * PARAMETERS:
 *  "listItem"
 *      Address of Object to be inserted into the new List. Must be non-NULL.
 *  "immutability"
 *      Boolean value indicating whether new List is to be immutable
 *  "pList"
 *      Address where List will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_MakeSingleton(
        PKIX_PL_Object *listItem,
        PKIX_Boolean immutability,
        PKIX_List **pList,
        void *plContext)
{
        PKIX_List *newList = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_PolicyChecker_MakeSingleton");
        PKIX_NULLCHECK_TWO(listItem, pList);

        PKIX_CHECK(PKIX_List_Create(&newList, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (newList, (PKIX_PL_Object *)listItem, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        if (immutability) {
                PKIX_CHECK(PKIX_List_SetImmutable(newList, plContext),
                        PKIX_LISTSETIMMUTABLEFAILED);
        }

        *pList = newList;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(newList);
        }

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_Spawn
 * DESCRIPTION:
 *
 *  Creates a new childNode for the parent pointed to by "parent", using
 *  the OID pointed to by "policyOID", the List of CertPolicyQualifiers
 *  pointed to by "qualifiers", the List of OIDs pointed to by
 *  "subjectDomainPolicies", and the PolicyCheckerState pointed to by
 *  "state". The new node will be added to "parent".
 *
 *  The validPolicy of the new node is set from the OID pointed to by
 *  "policyOID". The policy qualifiers for the new node is set from the
 *  List of qualifiers pointed to by "qualifiers", and may be NULL or
 *  empty if the argument provided was NULL or empty. The criticality is
 *  set according to the criticality obtained from the PolicyCheckerState.
 *  If "subjectDomainPolicies" is NULL, the expectedPolicySet of the
 *  child is set to contain the same policy as the validPolicy. If
 *  "subjectDomainPolicies" is not NULL, it is used as the value for
 *  the expectedPolicySet.
 *
 *  The PolicyCheckerState also contains a constant, anyPolicy, which is
 *  compared to "policyOID". If they match, the address of the childNode
 * is saved in the state's newAnyPolicyNode.
 *
 * PARAMETERS:
 *  "parent"
 *      Address of PolicyNode to which the child will be linked. Must be
 *      non-NULL.
 *  "policyOID"
 *      Address of OID of the new child's validPolicy and also, if
 *      subjectDomainPolicies is NULL, of the new child's expectedPolicySet.
 *      Must be non-NULL.
 *  "qualifiers"
 *      Address of List of CertPolicyQualifiers. May be NULL or empty.
 *  "subjectDomainPolicies"
 *      Address of List of OIDs indicating the policies to which "policy" is
 *      mapped. May be empty or NULL.
 *  "state"
 *      Address of the current PKIX_PolicyCheckerState. Must be non-NULL..
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_Spawn(
        PKIX_PolicyNode *parent,
        PKIX_PL_OID *policyOID,
        PKIX_List *qualifiers,  /* CertPolicyQualifiers */
        PKIX_List *subjectDomainPolicies,
        PKIX_PolicyCheckerState *state,
        void *plContext)
{
        PKIX_List *expectedSet = NULL; /* OIDs */
        PKIX_PolicyNode *childNode = NULL;
        PKIX_Boolean match = PKIX_FALSE;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_PolicyChecker_Spawn");
        PKIX_NULLCHECK_THREE(policyOID, parent, state);

        if (subjectDomainPolicies) {

                PKIX_INCREF(subjectDomainPolicies);
                expectedSet = subjectDomainPolicies;

        } else {
                /* Create the child's ExpectedPolicy Set */
                PKIX_CHECK(pkix_PolicyChecker_MakeSingleton
                        ((PKIX_PL_Object *)policyOID,
                        PKIX_TRUE,      /* make expectedPolicySet immutable */
                        &expectedSet,
                        plContext),
                        PKIX_POLICYCHECKERMAKESINGLETONFAILED);
        }

        PKIX_CHECK(pkix_PolicyNode_Create
                (policyOID,
                qualifiers,
                state->certPoliciesCritical,
                expectedSet,
                &childNode,
                plContext),
                PKIX_POLICYNODECREATEFAILED);

        /*
         * If we had a non-empty mapping, we know the new node could not
         * have been created with a validPolicy of anyPolicy. Otherwise,
         * check whether we just created a new node with anyPolicy, because
         * in that case we want to save the child pointer in newAnyPolicyNode.
         */
        if (!subjectDomainPolicies) {
                PKIX_EQUALS(policyOID, state->anyPolicyOID, &match, plContext,
                        PKIX_OBJECTEQUALSFAILED);

                if (match) {
                        PKIX_DECREF(state->newAnyPolicyNode);
                        PKIX_INCREF(childNode);
                        state->newAnyPolicyNode = childNode;
                }
        }

        PKIX_CHECK(pkix_PolicyNode_AddToParent(parent, childNode, plContext),
                PKIX_POLICYNODEADDTOPARENTFAILED);

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)state, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:
        PKIX_DECREF(childNode);
        PKIX_DECREF(expectedSet);
        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_CheckPolicyRecursive
 * DESCRIPTION:
 *
 *  Performs policy processing for the policy whose OID is pointed to by
 *  "policyOID" and whose List of CertPolicyQualifiers is pointed to by
 *  "policyQualifiers", using the List of policy OIDs pointed to by
 *  "subjectDomainPolicies" and the PolicyNode pointed to by "currentNode",
 *  in accordance with the current PolicyCheckerState pointed to by "state",
 *  and setting "pChildNodeCreated" to TRUE if a new childNode is created.
 *  Note: "pChildNodeCreated" is not set to FALSE if no childNode is created.
 *  The intent of the design is that the caller can set a variable to FALSE
 *  initially, prior to a recursive set of calls. At the end, the variable
 *  can be tested to see whether *any* of the calls created a child node.
 *
 *  If the currentNode is not at the bottom of the tree, this function
 *  calls itself recursively for each child of currentNode. At the bottom of
 *  the tree, it creates new child nodes as appropriate. This function will
 *  never be called with policy = anyPolicy.
 *
 *  This function implements the processing described in RFC3280
 *  Section 6.1.3(d)(1)(i).
 *
 * PARAMETERS:
 *  "policyOID"
 *      Address of OID of the policy to be checked for. Must be non-NULL.
 *  "policyQualifiers"
 *      Address of List of CertPolicyQualifiers of the policy to be checked for.
 *      May be empty or NULL.
 *  "subjectDomainPolicies"
 *      Address of List of OIDs indicating the policies to which "policy" is
 *      mapped. May be empty or NULL.
 *  "currentNode"
 *      Address of PolicyNode whose descendants will be checked, if not at the
 *      bottom of the tree; or whose expectedPolicySet will be compared to
 *      "policy", if at the bottom. Must be non-NULL.
 *  "state"
 *      Address of PolicyCheckerState of the current PolicyChecker. Must be
 *      non-NULL.
 *  "pChildNodeCreated"
 *      Address of the Boolean that will be set TRUE if this function
 *      creates a child node. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_CheckPolicyRecursive(
        PKIX_PL_OID *policyOID,
        PKIX_List *policyQualifiers,
        PKIX_List *subjectDomainPolicies,
        PKIX_PolicyNode *currentNode,
        PKIX_PolicyCheckerState *state,
        PKIX_Boolean *pChildNodeCreated,
        void *plContext)
{
        PKIX_UInt32 depth = 0;
        PKIX_UInt32 numChildren = 0;
        PKIX_UInt32 childIx = 0;
        PKIX_Boolean isIncluded = PKIX_FALSE;
        PKIX_List *children = NULL;     /* PolicyNodes */
        PKIX_PolicyNode *childNode = NULL;
        PKIX_List *expectedPolicies = NULL; /* OIDs */

        PKIX_ENTER
                (CERTCHAINCHECKER,
                "pkix_PolicyChecker_CheckPolicyRecursive");
        PKIX_NULLCHECK_FOUR(policyOID, currentNode, state, pChildNodeCreated);

        /* if not at the bottom of the tree */
        PKIX_CHECK(PKIX_PolicyNode_GetDepth
                (currentNode, &depth, plContext),
                PKIX_POLICYNODEGETDEPTHFAILED);

        if (depth < (state->certsProcessed)) {
                PKIX_CHECK(pkix_PolicyNode_GetChildrenMutable
                        (currentNode, &children, plContext),
                        PKIX_POLICYNODEGETCHILDRENMUTABLEFAILED);

                if (children) {
                        PKIX_CHECK(PKIX_List_GetLength
                                (children, &numChildren, plContext),
                                PKIX_LISTGETLENGTHFAILED);
                }

                for (childIx = 0; childIx < numChildren; childIx++) {

                        PKIX_CHECK(PKIX_List_GetItem
                            (children,
                            childIx,
                            (PKIX_PL_Object **)&childNode,
                            plContext),
                            PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(pkix_PolicyChecker_CheckPolicyRecursive
                            (policyOID,
                            policyQualifiers,
                            subjectDomainPolicies,
                            childNode,
                            state,
                            pChildNodeCreated,
                            plContext),
                            PKIX_POLICYCHECKERCHECKPOLICYRECURSIVEFAILED);

                        PKIX_DECREF(childNode);
                }
        } else { /* if at the bottom of the tree */

                /* Check whether policy is in this node's expectedPolicySet */
                PKIX_CHECK(PKIX_PolicyNode_GetExpectedPolicies
                        (currentNode, &expectedPolicies, plContext),
                        PKIX_POLICYNODEGETEXPECTEDPOLICIESFAILED);

                PKIX_NULLCHECK_ONE(expectedPolicies);

                PKIX_CHECK(pkix_List_Contains
                        (expectedPolicies,
                        (PKIX_PL_Object *)policyOID,
                        &isIncluded,
                        plContext),
                        PKIX_LISTCONTAINSFAILED);

                if (isIncluded) {
                        PKIX_CHECK(pkix_PolicyChecker_Spawn
                                (currentNode,
                                policyOID,
                                policyQualifiers,
                                subjectDomainPolicies,
                                state,
                                plContext),
                                PKIX_POLICYCHECKERSPAWNFAILED);

                        *pChildNodeCreated = PKIX_TRUE;
                }
        }

cleanup:

        PKIX_DECREF(children);
        PKIX_DECREF(childNode);
        PKIX_DECREF(expectedPolicies);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_CheckPolicy
 * DESCRIPTION:
 *
 *  Performs the non-recursive portion of the policy processing for the policy
 *  whose OID is pointed to by "policyOID" and whose List of
 *  CertPolicyQualifiers is pointed to by "policyQualifiers", for the
 *  Certificate pointed to by "cert" with the List of CertPolicyMaps pointed
 *  to by "maps", in accordance with the current PolicyCheckerState pointed
 *  to by "state".
 *
 *  This function implements the processing described in RFC3280
 *  Section 6.1.3(d)(1)(i).
 *
 * PARAMETERS:
 *  "policyOID"
 *      Address of OID of the policy to be checked for. Must be non-NULL.
 *  "policyQualifiers"
 *      Address of List of CertPolicyQualifiers of the policy to be checked for.
 *      May be empty or NULL.
 *  "cert"
 *      Address of the current certificate. Must be non-NULL.
 *  "maps"
 *      Address of List of CertPolicyMaps for the current certificate
 *  "state"
 *      Address of PolicyCheckerState of the current PolicyChecker. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_CheckPolicy(
        PKIX_PL_OID *policyOID,
        PKIX_List *policyQualifiers,
        PKIX_PL_Cert *cert,
        PKIX_List *maps,
        PKIX_PolicyCheckerState *state,
        void *plContext)
{
        PKIX_Boolean childNodeCreated = PKIX_FALSE;
        PKIX_Boolean okToSpawn = PKIX_FALSE;
        PKIX_Boolean found = PKIX_FALSE;
        PKIX_List *subjectDomainPolicies = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_PolicyChecker_CheckPolicy");
        PKIX_NULLCHECK_THREE(policyOID, cert, state);

        /*
         * If this is not the last certificate, get the set of
         * subjectDomainPolicies that "policy" maps to, according to the
         * current cert's policy mapping extension. That set will be NULL
         * if the current cert does not have a policy mapping extension,
         * or if the current policy is not mapped.
         */
        if (state->certsProcessed != (state->numCerts - 1)) {
            PKIX_CHECK(pkix_PolicyChecker_MapGetSubjectDomainPolicies
                (maps, policyOID, &subjectDomainPolicies, plContext),
                PKIX_POLICYCHECKERMAPGETSUBJECTDOMAINPOLICIESFAILED);
        }

        /*
         * Section 6.1.4(b)(2) tells us that if policyMapping is zero, we
         * will have to delete any nodes created with validPolicies equal to
         * policies that appear as issuerDomainPolicies in a policy mapping
         * extension. Let's avoid creating any such nodes.
         */
        if ((state->policyMapping) == 0) {
                if (subjectDomainPolicies) {
                        goto cleanup;
                }
        }

        PKIX_CHECK(pkix_PolicyChecker_CheckPolicyRecursive
                (policyOID,
                policyQualifiers,
                subjectDomainPolicies,
                state->validPolicyTree,
                state,
                &childNodeCreated,
                plContext),
                PKIX_POLICYCHECKERCHECKPOLICYRECURSIVEFAILED);

        if (!childNodeCreated) {
                /*
                 * Section 6.1.3(d)(1)(ii)
                 * There was no match. If there was a node at
                 * depth i-1 with valid policy anyPolicy,
                 * generate a node subordinate to that.
                 *
                 * But that means this created node would be in
                 * the valid-policy-node-set, and will be
                 * pruned in 6.1.5(g)(iii)(2) unless it is in
                 * the user-initial-policy-set or the user-
                 * initial-policy-set is {anyPolicy}. So check,
                 * and don't create it if it will be pruned.
                 */
                if (state->anyPolicyNodeAtBottom) {
                        if (state->initialIsAnyPolicy) {
                                okToSpawn = PKIX_TRUE;
                        } else {
                                PKIX_CHECK(pkix_List_Contains
                                        (state->mappedUserInitialPolicySet,
                                        (PKIX_PL_Object *)policyOID,
                                        &okToSpawn,
                                        plContext),
                                        PKIX_LISTCONTAINSFAILED);
                        }
                        if (okToSpawn) {
                                PKIX_CHECK(pkix_PolicyChecker_Spawn
                                        (state->anyPolicyNodeAtBottom,
                                        policyOID,
                                        policyQualifiers,
                                        subjectDomainPolicies,
                                        state,
                                        plContext),
                                        PKIX_POLICYCHECKERSPAWNFAILED);
                                childNodeCreated = PKIX_TRUE;
                        }
                }
        }

        if (childNodeCreated) {
                /*
                 * If this policy had qualifiers, and the certificate policies
                 * extension was marked critical, and the user cannot deal with
                 * policy qualifiers, throw an error.
                 */
                if (policyQualifiers &&
                    state->certPoliciesCritical &&
                    state->policyQualifiersRejected) {
                    PKIX_ERROR
                        (PKIX_QUALIFIERSINCRITICALCERTIFICATEPOLICYEXTENSION);
                }
                /*
                 * If the policy we just propagated was in the list of mapped
                 * policies, remove it from the list. That list is used, at the
                 * end, to determine policies that have not been propagated.
                 */
                if (state->mappedPolicyOIDs) {
                        PKIX_CHECK(pkix_List_Contains
                                (state->mappedPolicyOIDs,
                                (PKIX_PL_Object *)policyOID,
                                &found,
                                plContext),
                                PKIX_LISTCONTAINSFAILED);
                        if (found) {
                                PKIX_CHECK(pkix_List_Remove
                                        (state->mappedPolicyOIDs,
                                        (PKIX_PL_Object *)policyOID,
                                        plContext),
                                        PKIX_LISTREMOVEFAILED);
                        }
                }
        }

cleanup:

        PKIX_DECREF(subjectDomainPolicies);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_CheckAny
 * DESCRIPTION:
 *  Performs the creation of PolicyNodes, for the PolicyNode pointed to by
 *  "currentNode" and PolicyNodes subordinate to it, using the List of
 *  qualifiers pointed to by "qualsOfAny", in accordance with the current
 *  certificate's PolicyMaps pointed to by "policyMaps" and the current
 *  PolicyCheckerState pointed to by "state".
 *
 *  If the currentNode is not just above the bottom of the validPolicyTree, this
 *  function calls itself recursively for each child of currentNode. At the
 *  level just above the bottom, for each policy in the currentNode's
 *  expectedPolicySet not already present in a child node, it creates a new
 *  child node. The validPolicy of the child created, and its expectedPolicySet,
 *  will be the policy from the currentNode's expectedPolicySet. The policy
 *  qualifiers will be the qualifiers from the current certificate's anyPolicy,
 *  the "qualsOfAny" parameter. If the currentNode's expectedSet includes
 *  anyPolicy, a childNode will be created with a policy of anyPolicy. This is
 *  the only way such a node can be created.
 *
 *  This function is called only when anyPolicy is one of the current
 *  certificate's policies. This function implements the processing described
 *  in RFC3280 Section 6.1.3(d)(2).
 *
 * PARAMETERS:
 *  "currentNode"
 *      Address of PolicyNode whose descendants will be checked, if not at the
 *      bottom of the tree; or whose expectedPolicySet will be compared to those
 *      in "alreadyPresent", if at the bottom. Must be non-NULL.
 *  "qualsOfAny"
 *      Address of List of qualifiers of the anyPolicy in the current
 *      certificate. May be empty or NULL.
 *  "policyMaps"
 *      Address of the List of PolicyMaps of the current certificate. May be
 *      empty or NULL.
 *  "state"
 *      Address of the current state of the PKIX_PolicyChecker.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_CheckAny(
        PKIX_PolicyNode *currentNode,
        PKIX_List *qualsOfAny,  /* CertPolicyQualifiers */
        PKIX_List *policyMaps,  /* CertPolicyMaps */
        PKIX_PolicyCheckerState *state,
        void *plContext)
{
        PKIX_UInt32 depth = 0;
        PKIX_UInt32 numChildren = 0;
        PKIX_UInt32 childIx = 0;
        PKIX_UInt32 numPolicies = 0;
        PKIX_UInt32 polx = 0;
        PKIX_Boolean isIncluded = PKIX_FALSE;
        PKIX_List *children = NULL;     /* PolicyNodes */
        PKIX_PolicyNode *childNode = NULL;
        PKIX_List *expectedPolicies = NULL; /* OIDs */
        PKIX_PL_OID *policyOID = NULL;
        PKIX_PL_OID *childPolicy = NULL;
        PKIX_List *subjectDomainPolicies = NULL;  /* OIDs */

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_PolicyChecker_CheckAny");
        PKIX_NULLCHECK_TWO(currentNode, state);

        PKIX_CHECK(PKIX_PolicyNode_GetDepth
                (currentNode, &depth, plContext),
                PKIX_POLICYNODEGETDEPTHFAILED);

        PKIX_CHECK(pkix_PolicyNode_GetChildrenMutable
                (currentNode, &children, plContext),
                PKIX_POLICYNODEGETCHILDRENMUTABLEFAILED);

        if (children) {
                PKIX_CHECK(PKIX_List_GetLength
                        (children, &numChildren, plContext),
                        PKIX_LISTGETLENGTHFAILED);
        }

        if (depth < (state->certsProcessed)) {
                for (childIx = 0; childIx < numChildren; childIx++) {

                        PKIX_CHECK(PKIX_List_GetItem
                                (children,
                                childIx,
                                (PKIX_PL_Object **)&childNode,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                        PKIX_NULLCHECK_ONE(childNode);
                        PKIX_CHECK(pkix_PolicyChecker_CheckAny
                                (childNode,
                                qualsOfAny,
                                policyMaps,
                                state,
                                plContext),
                                PKIX_POLICYCHECKERCHECKANYFAILED);

                        PKIX_DECREF(childNode);
                }
        } else { /* if at the bottom of the tree */

            PKIX_CHECK(PKIX_PolicyNode_GetExpectedPolicies
                (currentNode, &expectedPolicies, plContext),
                PKIX_POLICYNODEGETEXPECTEDPOLICIESFAILED);

            /* Expected Policy Set is not allowed to be NULL */
            PKIX_NULLCHECK_ONE(expectedPolicies);

            PKIX_CHECK(PKIX_List_GetLength
                (expectedPolicies, &numPolicies, plContext),
                PKIX_LISTGETLENGTHFAILED);

            for (polx = 0; polx < numPolicies; polx++) {
                PKIX_CHECK(PKIX_List_GetItem
                    (expectedPolicies,
                    polx,
                    (PKIX_PL_Object **)&policyOID,
                    plContext),
                    PKIX_LISTGETITEMFAILED);

                PKIX_NULLCHECK_ONE(policyOID);

                isIncluded = PKIX_FALSE;

                for (childIx = 0;
                    (!isIncluded && (childIx < numChildren));
                    childIx++) {

                    PKIX_CHECK(PKIX_List_GetItem
                        (children,
                        childIx,
                        (PKIX_PL_Object **)&childNode,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                    PKIX_NULLCHECK_ONE(childNode);

                    PKIX_CHECK(PKIX_PolicyNode_GetValidPolicy
                        (childNode, &childPolicy, plContext),
                        PKIX_POLICYNODEGETVALIDPOLICYFAILED);

                    PKIX_NULLCHECK_ONE(childPolicy);

                    PKIX_EQUALS(policyOID, childPolicy, &isIncluded, plContext,
                        PKIX_OBJECTEQUALSFAILED);

                    PKIX_DECREF(childNode);
                    PKIX_DECREF(childPolicy);
                }

                if (!isIncluded) {
                    if (policyMaps) {
                        PKIX_CHECK
                          (pkix_PolicyChecker_MapGetSubjectDomainPolicies
                          (policyMaps,
                          policyOID,
                          &subjectDomainPolicies,
                          plContext),
                          PKIX_POLICYCHECKERMAPGETSUBJECTDOMAINPOLICIESFAILED);
                    }
                    PKIX_CHECK(pkix_PolicyChecker_Spawn
                        (currentNode,
                        policyOID,
                        qualsOfAny,
                        subjectDomainPolicies,
                        state,
                        plContext),
                        PKIX_POLICYCHECKERSPAWNFAILED);
                    PKIX_DECREF(subjectDomainPolicies);
                }

                PKIX_DECREF(policyOID);
            }
        }

cleanup:

        PKIX_DECREF(children);
        PKIX_DECREF(childNode);
        PKIX_DECREF(expectedPolicies);
        PKIX_DECREF(policyOID);
        PKIX_DECREF(childPolicy);
        PKIX_DECREF(subjectDomainPolicies);

        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: pkix_PolicyChecker_CalculateIntersection
 * DESCRIPTION:
 *
 *  Processes the PolicyNode pointed to by "currentNode", and its descendants,
 *  using the PolicyCheckerState pointed to by "state", using the List at
 *  the address pointed to by "nominees" the OIDs of policies that are in the
 *  user-initial-policy-set but are not represented among the nodes at the
 *  bottom of the tree, and storing at "pShouldBePruned" the value TRUE if
 *  currentNode is childless at the end of this processing, FALSE if it has
 *  children or is at the bottom of the tree.
 *
 *  When this function is called at the top level, "nominees" should be the List
 *  of all policies in the user-initial-policy-set. Policies that are
 *  represented in the valid-policy-node-set are removed from this List. As a
 *  result when nodes are created according to 6.1.5.(g)(iii)(3)(b), a node will
 *  be created for each policy remaining in this List.
 *
 *  This function implements the calculation of the intersection of the
 *  validPolicyTree with the user-initial-policy-set, as described in
 *  RFC 3280 6.1.5(g)(iii).
 *
 * PARAMETERS:
 *  "currentNode"
 *      Address of PolicyNode whose descendants will be processed as described.
 *      Must be non-NULL.
 *  "state"
 *      Address of the current state of the PKIX_PolicyChecker. Must be non-NULL
 *  "nominees"
 *      Address of List of the OIDs for which nodes should be created to replace
 *      anyPolicy nodes. Must be non-NULL but may be empty.
 *  "pShouldBePruned"
 *      Address where Boolean return value, set to TRUE if this PolicyNode
 *      should be deleted, is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_CalculateIntersection(
        PKIX_PolicyNode *currentNode,
        PKIX_PolicyCheckerState *state,
        PKIX_List *nominees, /* OIDs */
        PKIX_Boolean *pShouldBePruned,
        void *plContext)
{
        PKIX_Boolean currentPolicyIsAny = PKIX_FALSE;
        PKIX_Boolean parentPolicyIsAny = PKIX_FALSE;
        PKIX_Boolean currentPolicyIsValid = PKIX_FALSE;
        PKIX_Boolean shouldBePruned = PKIX_FALSE;
        PKIX_Boolean priorCriticality = PKIX_FALSE;
        PKIX_UInt32 depth = 0;
        PKIX_UInt32 numChildren = 0;
        PKIX_UInt32 childIndex = 0;
        PKIX_UInt32 numNominees = 0;
        PKIX_UInt32 polIx = 0;
        PKIX_PL_OID *currentPolicy = NULL;
        PKIX_PL_OID *parentPolicy = NULL;
        PKIX_PL_OID *substPolicy = NULL;
        PKIX_PolicyNode *parent = NULL;
        PKIX_PolicyNode *child = NULL;
        PKIX_List *children = NULL; /* PolicyNodes */
        PKIX_List *policyQualifiers = NULL;

        PKIX_ENTER
                (CERTCHAINCHECKER,
                "pkix_PolicyChecker_CalculateIntersection");

        /*
         * We call this function if the valid_policy_tree is not NULL and
         * the user-initial-policy-set is not any-policy.
         */
        if (!state->validPolicyTree || state->initialIsAnyPolicy) {
                PKIX_ERROR(PKIX_PRECONDITIONFAILED);
        }

        PKIX_NULLCHECK_FOUR(currentNode, state, nominees, pShouldBePruned);

        PKIX_CHECK(PKIX_PolicyNode_GetValidPolicy
                (currentNode, &currentPolicy, plContext),
                PKIX_POLICYNODEGETVALIDPOLICYFAILED);

        PKIX_NULLCHECK_TWO(state->anyPolicyOID, currentPolicy);

        PKIX_EQUALS
                (state->anyPolicyOID,
                currentPolicy,
                &currentPolicyIsAny,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        PKIX_CHECK(PKIX_PolicyNode_GetParent(currentNode, &parent, plContext),
                PKIX_POLICYNODEGETPARENTFAILED);

        if (currentPolicyIsAny == PKIX_FALSE) {

                /*
                 * If we are at the top of the tree, or if our
                 * parent's validPolicy is anyPolicy, we are in
                 * the valid policy node set.
                 */
                if (parent) {
                        PKIX_CHECK(PKIX_PolicyNode_GetValidPolicy
                                (parent, &parentPolicy, plContext),
                                PKIX_POLICYNODEGETVALIDPOLICYFAILED);

                        PKIX_NULLCHECK_ONE(parentPolicy);

                        PKIX_EQUALS
                                (state->anyPolicyOID,
                                parentPolicy,
                                &parentPolicyIsAny,
                                plContext,
                                PKIX_OBJECTEQUALSFAILED);
                }

                /*
                 * Section 6.1.5(g)(iii)(2)
                 * If this node's policy is not in the user-initial-policy-set,
                 * it is not in the intersection. Prune it.
                 */
                if (!parent || parentPolicyIsAny) {
                        PKIX_CHECK(pkix_List_Contains
                                (state->userInitialPolicySet,
                                (PKIX_PL_Object *)currentPolicy,
                                &currentPolicyIsValid,
                                plContext),
                                PKIX_LISTCONTAINSFAILED);
                        if (!currentPolicyIsValid) {
                                *pShouldBePruned = PKIX_TRUE;
                                goto cleanup;
                        }

                        /*
                         * If this node's policy is in the user-initial-policy-
                         * set, it will propagate that policy into the next
                         * level of the tree. Remove the policy from the list
                         * of policies that an anyPolicy will spawn.
                         */
                        PKIX_CHECK(pkix_List_Remove
                                (nominees,
                                (PKIX_PL_Object *)currentPolicy,
                                plContext),
                                PKIX_LISTREMOVEFAILED);
                }
        }


        /* Are we at the bottom of the tree? */

        PKIX_CHECK(PKIX_PolicyNode_GetDepth
                (currentNode, &depth, plContext),
                PKIX_POLICYNODEGETDEPTHFAILED);

        if (depth == (state->numCerts)) {
                /*
                 * Section 6.1.5(g)(iii)(3)
                 * Replace anyPolicy nodes...
                 */
                if (currentPolicyIsAny == PKIX_TRUE) {

                        /* replace this node */

                        PKIX_CHECK(PKIX_List_GetLength
                            (nominees, &numNominees, plContext),
                            PKIX_LISTGETLENGTHFAILED);

                        if (numNominees) {

                            PKIX_CHECK(PKIX_PolicyNode_GetPolicyQualifiers
                                (currentNode,
                                &policyQualifiers,
                                plContext),
                                PKIX_POLICYNODEGETPOLICYQUALIFIERSFAILED);

                            PKIX_CHECK(PKIX_PolicyNode_IsCritical
                                (currentNode, &priorCriticality, plContext),
                                PKIX_POLICYNODEISCRITICALFAILED);
                        }

                        PKIX_NULLCHECK_ONE(parent);

                        for (polIx = 0; polIx < numNominees; polIx++) {

                            PKIX_CHECK(PKIX_List_GetItem
                                (nominees,
                                polIx,
                                (PKIX_PL_Object **)&substPolicy,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                            PKIX_CHECK(pkix_PolicyChecker_Spawn
                                (parent,
                                substPolicy,
                                policyQualifiers,
                                NULL,
                                state,
                                plContext),
                                PKIX_POLICYCHECKERSPAWNFAILED);

                            PKIX_DECREF(substPolicy);

                        }
                        /* remove currentNode from parent */
                        *pShouldBePruned = PKIX_TRUE;
                        /*
                         * We can get away with augmenting the parent's List
                         * of children because we started at the end and went
                         * toward the beginning. New nodes are added at the end.
                         */
                }
        } else {
                /*
                 * Section 6.1.5(g)(iii)(4)
                 * Prune any childless nodes above the bottom level
                 */
                PKIX_CHECK(pkix_PolicyNode_GetChildrenMutable
                        (currentNode, &children, plContext),
                        PKIX_POLICYNODEGETCHILDRENMUTABLEFAILED);

                /* CurrentNode should have been pruned if childless. */
                PKIX_NULLCHECK_ONE(children);

                PKIX_CHECK(PKIX_List_GetLength
                        (children, &numChildren, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                for (childIndex = numChildren; childIndex > 0; childIndex--) {

                    PKIX_CHECK(PKIX_List_GetItem
                        (children,
                        childIndex - 1,
                        (PKIX_PL_Object **)&child,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                    PKIX_CHECK(pkix_PolicyChecker_CalculateIntersection
                        (child, state, nominees, &shouldBePruned, plContext),
                        PKIX_POLICYCHECKERCALCULATEINTERSECTIONFAILED);

                    if (PKIX_TRUE == shouldBePruned) {

                        PKIX_CHECK(PKIX_List_DeleteItem
                                (children, childIndex - 1, plContext),
                                PKIX_LISTDELETEITEMFAILED);
                        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                                ((PKIX_PL_Object *)state, plContext),
                                PKIX_OBJECTINVALIDATECACHEFAILED);
                    }

                    PKIX_DECREF(child);
                }

                PKIX_CHECK(PKIX_List_GetLength
                        (children, &numChildren, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                if (numChildren == 0) {
                        *pShouldBePruned = PKIX_TRUE;
                }
        }
cleanup:
        PKIX_DECREF(currentPolicy);
        PKIX_DECREF(parentPolicy);
        PKIX_DECREF(substPolicy);
        PKIX_DECREF(parent);
        PKIX_DECREF(child);
        PKIX_DECREF(children);
        PKIX_DECREF(policyQualifiers);

        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: pkix_PolicyChecker_PolicyMapProcessing
 * DESCRIPTION:
 *
 *  Performs the processing of Policies in the List of CertPolicyMaps pointed
 *  to by "policyMaps", using and updating the PolicyCheckerState pointed to by
 *  "state".
 *
 *  This function implements the policyMap processing described in RFC3280
 *  Section 6.1.4(b)(1), after certificate i has been processed, in preparation
 *  for certificate i+1. Section references are to that document.
 *
 * PARAMETERS:
 *  "policyMaps"
 *      Address of the List of CertPolicyMaps presented by certificate i.
 *      Must be non-NULL.
 *  "certPoliciesIncludeAny"
 *      Boolean value which is PKIX_TRUE if the current certificate asserts
 *      anyPolicy, PKIX_FALSE otherwise.
 *  "qualsOfAny"
 *      Address of List of qualifiers of the anyPolicy in the current
 *      certificate. May be empty or NULL.
 *  "state"
 *      Address of the current state of the PKIX_PolicyChecker.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_PolicyMapProcessing(
        PKIX_List *policyMaps,  /* CertPolicyMaps */
        PKIX_Boolean certPoliciesIncludeAny,
        PKIX_List *qualsOfAny,
        PKIX_PolicyCheckerState *state,
        void *plContext)
{
        PKIX_UInt32 numPolicies = 0;
        PKIX_UInt32 polX = 0;
        PKIX_PL_OID *policyOID = NULL;
        PKIX_List *newMappedPolicies = NULL;  /* OIDs */
        PKIX_List *subjectDomainPolicies = NULL;  /* OIDs */

        PKIX_ENTER
                (CERTCHAINCHECKER,
                "pkix_PolicyChecker_PolicyMapProcessing");
        PKIX_NULLCHECK_THREE
                (policyMaps,
                state,
                state->mappedUserInitialPolicySet);

        /*
         * For each policy in mappedUserInitialPolicySet, if it is not mapped,
         * append it to new policySet; if it is mapped, append its
         * subjectDomainPolicies to new policySet. When done, this new
         * policySet will replace mappedUserInitialPolicySet.
         */
        PKIX_CHECK(PKIX_List_Create
                (&newMappedPolicies, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_GetLength
                (state->mappedUserInitialPolicySet,
                &numPolicies,
                plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (polX = 0; polX < numPolicies; polX++) {

            PKIX_CHECK(PKIX_List_GetItem
                (state->mappedUserInitialPolicySet,
                polX,
                (PKIX_PL_Object **)&policyOID,
                plContext),
                PKIX_LISTGETITEMFAILED);

            PKIX_CHECK(pkix_PolicyChecker_MapGetSubjectDomainPolicies
                (policyMaps,
                policyOID,
                &subjectDomainPolicies,
                plContext),
                PKIX_POLICYCHECKERMAPGETSUBJECTDOMAINPOLICIESFAILED);

            if (subjectDomainPolicies) {

                PKIX_CHECK(pkix_List_AppendUnique
                        (newMappedPolicies,
                        subjectDomainPolicies,
                        plContext),
                        PKIX_LISTAPPENDUNIQUEFAILED);

                PKIX_DECREF(subjectDomainPolicies);

            } else {
                PKIX_CHECK(PKIX_List_AppendItem
                        (newMappedPolicies,
                        (PKIX_PL_Object *)policyOID,
                        plContext),
                        PKIX_LISTAPPENDITEMFAILED);
            }
            PKIX_DECREF(policyOID);
        }

        /*
         * For each policy ID-P remaining in mappedPolicyOIDs, it has not been
         * propagated to the bottom of the tree (depth i). If policyMapping
         * is greater than zero and this cert contains anyPolicy and the tree
         * contains an anyPolicy node at depth i-1, then we must create a node
         * with validPolicy ID-P, the policy qualifiers of anyPolicy in
         * this certificate, and expectedPolicySet the subjectDomainPolicies
         * that ID-P maps to. We also then add those subjectDomainPolicies to
         * the list of policies that will be accepted in the next certificate,
         * the mappedUserInitialPolicySet.
         */

        if ((state->policyMapping > 0) && (certPoliciesIncludeAny) &&
            (state->anyPolicyNodeAtBottom) && (state->mappedPolicyOIDs)) {

                PKIX_CHECK(PKIX_List_GetLength
                    (state->mappedPolicyOIDs,
                    &numPolicies,
                    plContext),
                    PKIX_LISTGETLENGTHFAILED);

                for (polX = 0; polX < numPolicies; polX++) {

                    PKIX_CHECK(PKIX_List_GetItem
                        (state->mappedPolicyOIDs,
                        polX,
                        (PKIX_PL_Object **)&policyOID,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                    PKIX_CHECK(pkix_PolicyChecker_MapGetSubjectDomainPolicies
                        (policyMaps,
                        policyOID,
                        &subjectDomainPolicies,
                        plContext),
                        PKIX_POLICYCHECKERMAPGETSUBJECTDOMAINPOLICIESFAILED);

                    PKIX_CHECK(pkix_PolicyChecker_Spawn
                        (state->anyPolicyNodeAtBottom,
                        policyOID,
                        qualsOfAny,
                        subjectDomainPolicies,
                        state,
                        plContext),
                        PKIX_POLICYCHECKERSPAWNFAILED);

                    PKIX_CHECK(pkix_List_AppendUnique
                        (newMappedPolicies,
                        subjectDomainPolicies,
                        plContext),
                        PKIX_LISTAPPENDUNIQUEFAILED);

                    PKIX_DECREF(subjectDomainPolicies);
                    PKIX_DECREF(policyOID);
                }
        }

        PKIX_CHECK(PKIX_List_SetImmutable(newMappedPolicies, plContext),
                PKIX_LISTSETIMMUTABLEFAILED);

        PKIX_DECREF(state->mappedUserInitialPolicySet);
        PKIX_INCREF(newMappedPolicies);

        state->mappedUserInitialPolicySet = newMappedPolicies;

cleanup:

        PKIX_DECREF(policyOID);
        PKIX_DECREF(newMappedPolicies);
        PKIX_DECREF(subjectDomainPolicies);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_WrapUpProcessing
 * DESCRIPTION:
 *
 *  Performs the wrap-up processing for the Cert pointed to by "cert",
 *  using and updating the PolicyCheckerState pointed to by "state".
 *
 *  This function implements the wrap-up processing described in RFC3280
 *  Section 6.1.5, after the final certificate has been processed. Section
 *  references in the comments are to that document.
 *
 * PARAMETERS:
 *  "cert"
 *      Address of the current (presumably the end entity) certificate.
 *      Must be non-NULL.
 *  "state"
 *      Address of the current state of the PKIX_PolicyChecker.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
static PKIX_Error *
pkix_PolicyChecker_WrapUpProcessing(
        PKIX_PL_Cert *cert,
        PKIX_PolicyCheckerState *state,
        void *plContext)
{
        PKIX_Int32 explicitPolicySkipCerts = 0;
        PKIX_Boolean isSelfIssued = PKIX_FALSE;
        PKIX_Boolean shouldBePruned = PKIX_FALSE;
        PKIX_List *nominees = NULL; /* OIDs */
#if PKIX_CERTPOLICYCHECKERSTATEDEBUG
        PKIX_PL_String *stateString = NULL;
        char *stateAscii = NULL;
        PKIX_UInt32 length;
#endif

        PKIX_ENTER
                (CERTCHAINCHECKER,
                "pkix_PolicyChecker_WrapUpProcessing");
        PKIX_NULLCHECK_THREE(cert, state, state->userInitialPolicySet);

#if PKIX_CERTPOLICYCHECKERSTATEDEBUG
        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)state, &stateString, plContext),
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                    (stateString,
                    PKIX_ESCASCII,
                    (void **)&stateAscii,
                    &length,
                    plContext),
                    PKIX_STRINGGETENCODEDFAILED);

        PKIX_DEBUG_ARG("%s\n", stateAscii);

        PKIX_FREE(stateAscii);
        PKIX_DECREF(stateString);
#endif

        /* Section 6.1.5(a) ... */
        PKIX_CHECK(pkix_IsCertSelfIssued
                (cert, &isSelfIssued, plContext),
                PKIX_ISCERTSELFISSUEDFAILED);

        if (!isSelfIssued) {
                if (state->explicitPolicy > 0) {

                        state->explicitPolicy--;

                        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                                ((PKIX_PL_Object *)state, plContext),
                                PKIX_OBJECTINVALIDATECACHEFAILED);
                }
        }

        /* Section 6.1.5(b) ... */
        PKIX_CHECK(PKIX_PL_Cert_GetRequireExplicitPolicy
                (cert, &explicitPolicySkipCerts, plContext),
                PKIX_CERTGETREQUIREEXPLICITPOLICYFAILED);

        if (explicitPolicySkipCerts  == 0) {
                state->explicitPolicy = 0;
        }

        /* Section 6.1.5(g)(i) ... */

        if (!(state->validPolicyTree)) {
                goto cleanup;
        }

        /* Section 6.1.5(g)(ii) ... */

        if (state->initialIsAnyPolicy) {
                goto cleanup;
        }

        /*
         * Section 6.1.5(g)(iii) ...
         * Create a list of policies which could be substituted for anyPolicy.
         * Start with a (mutable) copy of user-initial-policy-set.
         */
        PKIX_CHECK(pkix_PolicyChecker_MakeMutableCopy
                (state->userInitialPolicySet, &nominees, plContext),
                PKIX_POLICYCHECKERMAKEMUTABLECOPYFAILED);

        PKIX_CHECK(pkix_PolicyChecker_CalculateIntersection
                (state->validPolicyTree, /* node at top of tree */
                state,
                nominees,
                &shouldBePruned,
                plContext),
                PKIX_POLICYCHECKERCALCULATEINTERSECTIONFAILED);

        if (PKIX_TRUE == shouldBePruned) {
                PKIX_DECREF(state->validPolicyTree);
        }

        if (state->validPolicyTree) {
                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)state->validPolicyTree, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);
        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)state, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

#if PKIX_CERTPOLICYCHECKERSTATEDEBUG
        if (state->validPolicyTree) {
                PKIX_CHECK(PKIX_PL_Object_ToString
                        ((PKIX_PL_Object*)state, &stateString, plContext),
                        PKIX_OBJECTTOSTRINGFAILED);

                PKIX_CHECK(PKIX_PL_String_GetEncoded
                            (stateString,
                            PKIX_ESCASCII,
                            (void **)&stateAscii,
                            &length,
                            plContext),
                            PKIX_STRINGGETENCODEDFAILED);

                PKIX_DEBUG_ARG
                        ("After CalculateIntersection:\n%s\n", stateAscii);

                PKIX_FREE(stateAscii);
                PKIX_DECREF(stateString);
        } else {
                PKIX_DEBUG("validPolicyTree is NULL\n");
        }
#endif

        /* Section 6.1.5(g)(iii)(4) ... */

        if (state->validPolicyTree) {

                PKIX_CHECK(pkix_PolicyNode_Prune
                        (state->validPolicyTree,
                        state->numCerts,
                        &shouldBePruned,
                        plContext),
                        PKIX_POLICYNODEPRUNEFAILED);

                if (shouldBePruned) {
                        PKIX_DECREF(state->validPolicyTree);
                }
        }

        if (state->validPolicyTree) {
                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)state->validPolicyTree, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);
        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)state, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

#if PKIX_CERTPOLICYCHECKERSTATEDEBUG
        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)state, &stateString, plContext),
                PKIX_OBJECTTOSTRINGFAILED);
        PKIX_CHECK(PKIX_PL_String_GetEncoded
                    (stateString,
                    PKIX_ESCASCII,
                    (void **)&stateAscii,
                    &length,
                    plContext),
                    PKIX_STRINGGETENCODEDFAILED);
        PKIX_DEBUG_ARG("%s\n", stateAscii);

        PKIX_FREE(stateAscii);
        PKIX_DECREF(stateString);
#endif

cleanup:

        PKIX_DECREF(nominees);

        PKIX_RETURN(CERTCHAINCHECKER);
}


/*
 * FUNCTION: pkix_PolicyChecker_Check
 * (see comments in pkix_checker.h for PKIX_CertChainChecker_CheckCallback)
 *
 * Labels referring to sections, such as "Section 6.1.3(d)", refer to
 * sections of RFC3280, Section 6.1.3 Basic Certificate Processing.
 *
 * If a non-fatal error occurs, it is unlikely that policy processing can
 * continue. But it is still possible that chain validation could succeed if
 * policy processing is non-critical. So if this function receives a non-fatal
 * error from a lower level routine, it aborts policy processing by setting
 * the validPolicyTree to NULL and tries to continue.
 *
 */
static PKIX_Error *
pkix_PolicyChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticals,  /* OIDs */
        void **pNBIOContext,
        void *plContext)
{
        PKIX_UInt32 numPolicies = 0;
        PKIX_UInt32 polX = 0;
        PKIX_Boolean result = PKIX_FALSE;
        PKIX_Int32 inhibitMappingSkipCerts = 0;
        PKIX_Int32 explicitPolicySkipCerts = 0;
        PKIX_Int32 inhibitAnyPolicySkipCerts = 0;
        PKIX_Boolean shouldBePruned = PKIX_FALSE;
        PKIX_Boolean isSelfIssued = PKIX_FALSE;
        PKIX_Boolean certPoliciesIncludeAny = PKIX_FALSE;
        PKIX_Boolean doAnyPolicyProcessing = PKIX_FALSE;

        PKIX_PolicyCheckerState *state = NULL;
        PKIX_List *certPolicyInfos = NULL; /* CertPolicyInfos */
        PKIX_PL_CertPolicyInfo *policy = NULL;
        PKIX_PL_OID *policyOID = NULL;
        PKIX_List *qualsOfAny = NULL; /* CertPolicyQualifiers */
        PKIX_List *policyQualifiers = NULL; /* CertPolicyQualifiers */
        PKIX_List *policyMaps = NULL; /* CertPolicyMaps */
        PKIX_List *mappedPolicies = NULL; /* OIDs */
        PKIX_Error *subroutineErr = NULL;
#if PKIX_CERTPOLICYCHECKERSTATEDEBUG
        PKIX_PL_String *stateString = NULL;
        char *stateAscii = NULL;
        PKIX_PL_String *certString = NULL;
        char *certAscii = NULL;
        PKIX_UInt32 length;
#endif

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_PolicyChecker_Check");
        PKIX_NULLCHECK_FOUR(checker, cert, unresolvedCriticals, pNBIOContext);

        *pNBIOContext = NULL; /* we never block on pending I/O */

        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                    (checker, (PKIX_PL_Object **)&state, plContext),
                    PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        PKIX_NULLCHECK_TWO(state, state->certPoliciesExtension);

#if PKIX_CERTPOLICYCHECKERSTATEDEBUG
        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)state, &stateString, plContext),
                PKIX_OBJECTTOSTRINGFAILED);
        PKIX_CHECK(PKIX_PL_String_GetEncoded
                    (stateString,
                    PKIX_ESCASCII,
                    (void **)&stateAscii,
                    &length,
                    plContext),
                    PKIX_STRINGGETENCODEDFAILED);
        PKIX_DEBUG_ARG("On entry %s\n", stateAscii);
        PKIX_FREE(stateAscii);
        PKIX_DECREF(stateString);
#endif

        /*
         * Section 6.1.4(a)
         * If this is not the last certificate, and if
         * policyMapping extension is present, check that no
         * issuerDomainPolicy or subjectDomainPolicy is equal to the
         * special policy anyPolicy.
         */
        if (state->certsProcessed != (state->numCerts - 1)) {
                PKIX_CHECK(PKIX_PL_Cert_GetPolicyMappings
                        (cert, &policyMaps, plContext),
                        PKIX_CERTGETPOLICYMAPPINGSFAILED);
        }

        if (policyMaps) {

                PKIX_CHECK(pkix_PolicyChecker_MapContains
                        (policyMaps, state->anyPolicyOID, &result, plContext),
                        PKIX_POLICYCHECKERMAPCONTAINSFAILED);

                if (result) {
                        PKIX_ERROR(PKIX_INVALIDPOLICYMAPPINGINCLUDESANYPOLICY);
                }

                PKIX_CHECK(pkix_PolicyChecker_MapGetMappedPolicies
                        (policyMaps, &mappedPolicies, plContext),
                        PKIX_POLICYCHECKERMAPGETMAPPEDPOLICIESFAILED);

                PKIX_DECREF(state->mappedPolicyOIDs);
                PKIX_INCREF(mappedPolicies);
                state->mappedPolicyOIDs = mappedPolicies;
        }

        /* Section 6.1.3(d) */
        if (state->validPolicyTree) {

            PKIX_CHECK(PKIX_PL_Cert_GetPolicyInformation
                (cert, &certPolicyInfos, plContext),
                PKIX_CERTGETPOLICYINFORMATIONFAILED);

            if (certPolicyInfos) {
                PKIX_CHECK(PKIX_List_GetLength
                        (certPolicyInfos, &numPolicies, plContext),
                        PKIX_LISTGETLENGTHFAILED);
            }

            if (numPolicies > 0) {

                PKIX_CHECK(PKIX_PL_Cert_AreCertPoliciesCritical
                        (cert, &(state->certPoliciesCritical), plContext),
                        PKIX_CERTARECERTPOLICIESCRITICALFAILED);

                /* Section 6.1.3(d)(1) For each policy not equal to anyPolicy */
                for (polX = 0; polX < numPolicies; polX++) {

                    PKIX_CHECK(PKIX_List_GetItem
                        (certPolicyInfos,
                        polX,
                        (PKIX_PL_Object **)&policy,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                    PKIX_CHECK(PKIX_PL_CertPolicyInfo_GetPolicyId
                        (policy, &policyOID, plContext),
                        PKIX_CERTPOLICYINFOGETPOLICYIDFAILED);

                    PKIX_CHECK(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                        (policy, &policyQualifiers, plContext),
                        PKIX_CERTPOLICYINFOGETPOLQUALIFIERSFAILED);

                    PKIX_EQUALS
                        (state->anyPolicyOID,
                        policyOID,
                        &result,
                        plContext,
                        PKIX_OIDEQUALFAILED);

                    if (result == PKIX_FALSE) {

                        /* Section 6.1.3(d)(1)(i) */
                        subroutineErr = pkix_PolicyChecker_CheckPolicy
                                (policyOID,
                                policyQualifiers,
                                cert,
                                policyMaps,
                                state,
                                plContext);
                        if (subroutineErr) {
                                goto subrErrorCleanup;
                        }

                    } else {
                        /*
                         * No descent (yet) for anyPolicy, but we will need
                         * the policyQualifiers for anyPolicy in 6.1.3(d)(2)
                         */
                        PKIX_DECREF(qualsOfAny);
                        PKIX_INCREF(policyQualifiers);
                        qualsOfAny = policyQualifiers;
                        certPoliciesIncludeAny = PKIX_TRUE;
                    }
                    PKIX_DECREF(policy);
                    PKIX_DECREF(policyOID);
                    PKIX_DECREF(policyQualifiers);
                }

                /* Section 6.1.3(d)(2) */
                if (certPoliciesIncludeAny == PKIX_TRUE) {
                        if (state->inhibitAnyPolicy > 0) {
                                doAnyPolicyProcessing = PKIX_TRUE;
                        } else {
                            /* We haven't yet counted the current cert */
                            if (((state->certsProcessed) + 1) <
                                (state->numCerts)) {

                                PKIX_CHECK(pkix_IsCertSelfIssued
                                        (cert,
                                        &doAnyPolicyProcessing,
                                        plContext),
                                        PKIX_ISCERTSELFISSUEDFAILED);
                            }
                        }
                        if (doAnyPolicyProcessing) {
                            subroutineErr = pkix_PolicyChecker_CheckAny
                                (state->validPolicyTree,
                                qualsOfAny,
                                policyMaps,
                                state,
                                plContext);
                            if (subroutineErr) {
                                goto subrErrorCleanup;
                            }
                        }
                }

                /* Section 6.1.3(d)(3) */
                if (state->validPolicyTree) {
                        subroutineErr = pkix_PolicyNode_Prune
                                (state->validPolicyTree,
                                state->certsProcessed + 1,
                                &shouldBePruned,
                                plContext);
                        if (subroutineErr) {
                                goto subrErrorCleanup;
                        }
                        if (shouldBePruned) {
                                PKIX_DECREF(state->validPolicyTree);
                                PKIX_DECREF(state->anyPolicyNodeAtBottom);
                        }
                }

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)state, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);

            } else {
                /* Section 6.1.3(e) */
                PKIX_DECREF(state->validPolicyTree);
                PKIX_DECREF(state->anyPolicyNodeAtBottom);
                PKIX_DECREF(state->newAnyPolicyNode);

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)state, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);
            }
        }

        /* Section 6.1.3(f) */
        if ((0 == state->explicitPolicy) && (!state->validPolicyTree)) {
                PKIX_ERROR(PKIX_CERTCHAINFAILSCERTIFICATEPOLICYVALIDATION);
        }

        /*
         * Remove Policy OIDs from list of unresolved critical
         * extensions, if present.
         */
        PKIX_CHECK(pkix_List_Remove
                (unresolvedCriticals,
                (PKIX_PL_Object *)state->certPoliciesExtension,
                plContext),
                PKIX_LISTREMOVEFAILED);

        PKIX_CHECK(pkix_List_Remove
                (unresolvedCriticals,
                (PKIX_PL_Object *)state->policyMappingsExtension,
                plContext),
                PKIX_LISTREMOVEFAILED);

        PKIX_CHECK(pkix_List_Remove
                (unresolvedCriticals,
                (PKIX_PL_Object *)state->policyConstraintsExtension,
                plContext),
                PKIX_LISTREMOVEFAILED);

        PKIX_CHECK(pkix_List_Remove
                (unresolvedCriticals,
                (PKIX_PL_Object *)state->inhibitAnyPolicyExtension,
                plContext),
                PKIX_LISTREMOVEFAILED);

        state->certsProcessed++;

        /* If this was not the last certificate, do next-cert preparation */
        if (state->certsProcessed != state->numCerts) {

                if (policyMaps) {
                        subroutineErr = pkix_PolicyChecker_PolicyMapProcessing
                                (policyMaps,
                                certPoliciesIncludeAny,
                                qualsOfAny,
                                state,
                                plContext);
                        if (subroutineErr) {
                                goto subrErrorCleanup;
                        }
                }

                /* update anyPolicyNodeAtBottom pointer */
                PKIX_DECREF(state->anyPolicyNodeAtBottom);
                state->anyPolicyNodeAtBottom = state->newAnyPolicyNode;
                state->newAnyPolicyNode = NULL;

                /* Section 6.1.4(h) */
                PKIX_CHECK(pkix_IsCertSelfIssued
                        (cert, &isSelfIssued, plContext),
                        PKIX_ISCERTSELFISSUEDFAILED);

                if (!isSelfIssued) {
                        if (state->explicitPolicy > 0) {
                            state->explicitPolicy--;
                        }
                        if (state->policyMapping > 0) {
                            state->policyMapping--;
                        }
                        if (state->inhibitAnyPolicy > 0) {
                            state->inhibitAnyPolicy--;
                        }
                }

                /* Section 6.1.4(i) */
                PKIX_CHECK(PKIX_PL_Cert_GetRequireExplicitPolicy
                        (cert, &explicitPolicySkipCerts, plContext),
                        PKIX_CERTGETREQUIREEXPLICITPOLICYFAILED);

                if (explicitPolicySkipCerts != -1) {
                        if (((PKIX_UInt32)explicitPolicySkipCerts) <
                            (state->explicitPolicy)) {
                                state->explicitPolicy =
                                   ((PKIX_UInt32) explicitPolicySkipCerts);
                        }
                }

                PKIX_CHECK(PKIX_PL_Cert_GetPolicyMappingInhibited
                        (cert, &inhibitMappingSkipCerts, plContext),
                        PKIX_CERTGETPOLICYMAPPINGINHIBITEDFAILED);

                if (inhibitMappingSkipCerts != -1) {
                        if (((PKIX_UInt32)inhibitMappingSkipCerts) <
                            (state->policyMapping)) {
                                state->policyMapping =
                                    ((PKIX_UInt32)inhibitMappingSkipCerts);
                        }
                }

                PKIX_CHECK(PKIX_PL_Cert_GetInhibitAnyPolicy
                        (cert, &inhibitAnyPolicySkipCerts, plContext),
                        PKIX_CERTGETINHIBITANYPOLICYFAILED);

                if (inhibitAnyPolicySkipCerts != -1) {
                        if (((PKIX_UInt32)inhibitAnyPolicySkipCerts) <
                            (state->inhibitAnyPolicy)) {
                                state->inhibitAnyPolicy =
                                    ((PKIX_UInt32)inhibitAnyPolicySkipCerts);
                        }
                }

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)state, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);

        } else { /* If this was the last certificate, do wrap-up processing */

                /* Section 6.1.5 */
                subroutineErr = pkix_PolicyChecker_WrapUpProcessing
                        (cert, state, plContext);
                if (subroutineErr) {
                        goto subrErrorCleanup;
                }

                if ((0 == state->explicitPolicy) && (!state->validPolicyTree)) {
                    PKIX_ERROR(PKIX_CERTCHAINFAILSCERTIFICATEPOLICYVALIDATION);
                }

                PKIX_DECREF(state->anyPolicyNodeAtBottom);
                PKIX_DECREF(state->newAnyPolicyNode);
        }


        if (subroutineErr) {

subrErrorCleanup:
                /* We had an error. Was it a fatal error? */
                pkixErrorClass = subroutineErr->errClass;
                if (pkixErrorClass == PKIX_FATAL_ERROR) {
                    pkixErrorResult = subroutineErr;
                    subroutineErr = NULL;
                    goto cleanup;
                }
                /*
                 * Abort policy processing, and then determine whether
                 * we can continue without policy processing.
                 */
                PKIX_DECREF(state->validPolicyTree);
                PKIX_DECREF(state->anyPolicyNodeAtBottom);
                PKIX_DECREF(state->newAnyPolicyNode);
                if (state->explicitPolicy == 0) {
                    PKIX_ERROR
                        (PKIX_CERTCHAINFAILSCERTIFICATEPOLICYVALIDATION);
                }
        }

        /* Checking is complete. Save state for the next certificate. */
        PKIX_CHECK(PKIX_CertChainChecker_SetCertChainCheckerState
                (checker, (PKIX_PL_Object *)state, plContext),
                PKIX_CERTCHAINCHECKERSETCERTCHAINCHECKERSTATEFAILED);

cleanup:

#if PKIX_CERTPOLICYCHECKERSTATEDEBUG
        if (cert) {
                PKIX_CHECK(PKIX_PL_Object_ToString
                        ((PKIX_PL_Object*)cert, &certString, plContext),
                        PKIX_OBJECTTOSTRINGFAILED);
                PKIX_CHECK(PKIX_PL_String_GetEncoded
                            (certString,
                            PKIX_ESCASCII,
                            (void **)&certAscii,
                            &length,
                            plContext),
                            PKIX_STRINGGETENCODEDFAILED);
                PKIX_DEBUG_ARG("Cert was %s\n", certAscii);
                PKIX_FREE(certAscii);
                PKIX_DECREF(certString);
        }
        if (state) {
                PKIX_CHECK(PKIX_PL_Object_ToString
                        ((PKIX_PL_Object*)state, &stateString, plContext),
                        PKIX_OBJECTTOSTRINGFAILED);
                PKIX_CHECK(PKIX_PL_String_GetEncoded
                            (stateString,
                            PKIX_ESCASCII,
                            (void **)&stateAscii,
                            &length,
                            plContext),
                            PKIX_STRINGGETENCODEDFAILED);
                PKIX_DEBUG_ARG("On exit %s\n", stateAscii);
                PKIX_FREE(stateAscii);
                PKIX_DECREF(stateString);
        }
#endif

        PKIX_DECREF(state);
        PKIX_DECREF(certPolicyInfos);
        PKIX_DECREF(policy);
        PKIX_DECREF(qualsOfAny);
        PKIX_DECREF(policyQualifiers);
        PKIX_DECREF(policyOID);
        PKIX_DECREF(subroutineErr);
        PKIX_DECREF(policyMaps);
        PKIX_DECREF(mappedPolicies);

        PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: pkix_PolicyChecker_Initialize
 * DESCRIPTION:
 *
 *  Creates and initializes a PolicyChecker, using the List pointed to
 *  by "initialPolicies" for the user-initial-policy-set, the Boolean value
 *  of "policyQualifiersRejected" for the policyQualifiersRejected parameter,
 *  the Boolean value of "initialPolicyMappingInhibit" for the
 *  inhibitPolicyMappings parameter, the Boolean value of
 *  "initialExplicitPolicy" for the initialExplicitPolicy parameter, the
 *  Boolean value of "initialAnyPolicyInhibit" for the inhibitAnyPolicy
 *  parameter, and the UInt32 value of "numCerts" as the number of
 *  certificates in the chain; and stores the Checker at "pChecker".
 *
 * PARAMETERS:
 *  "initialPolicies"
 *      Address of List of OIDs comprising the user-initial-policy-set; the List
 *      may be empty or NULL
 *  "policyQualifiersRejected"
 *      Boolean value of the policyQualifiersRejected parameter
 *  "initialPolicyMappingInhibit"
 *      Boolean value of the inhibitPolicyMappings parameter
 *  "initialExplicitPolicy"
 *      Boolean value of the initialExplicitPolicy parameter
 *  "initialAnyPolicyInhibit"
 *      Boolean value of the inhibitAnyPolicy parameter
 *  "numCerts"
 *      Number of certificates in the chain to be validated
 *  "pChecker"
 *      Address to store the created PolicyChecker. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a CertChainChecker Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_PolicyChecker_Initialize(
        PKIX_List *initialPolicies,
        PKIX_Boolean policyQualifiersRejected,
        PKIX_Boolean initialPolicyMappingInhibit,
        PKIX_Boolean initialExplicitPolicy,
        PKIX_Boolean initialAnyPolicyInhibit,
        PKIX_UInt32 numCerts,
        PKIX_CertChainChecker **pChecker,
        void *plContext)
{
        PKIX_PolicyCheckerState *polCheckerState = NULL;
        PKIX_List *policyExtensions = NULL;     /* OIDs */
        PKIX_ENTER(CERTCHAINCHECKER, "pkix_PolicyChecker_Initialize");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(pkix_PolicyCheckerState_Create
                (initialPolicies,
                policyQualifiersRejected,
                initialPolicyMappingInhibit,
                initialExplicitPolicy,
                initialAnyPolicyInhibit,
                numCerts,
                &polCheckerState,
                plContext),
                PKIX_POLICYCHECKERSTATECREATEFAILED);

        /* Create the list of extensions that we handle */
        PKIX_CHECK(pkix_PolicyChecker_MakeSingleton
                ((PKIX_PL_Object *)(polCheckerState->certPoliciesExtension),
                PKIX_TRUE,
                &policyExtensions,
                plContext),
                PKIX_POLICYCHECKERMAKESINGLETONFAILED);

        PKIX_CHECK(PKIX_CertChainChecker_Create
                (pkix_PolicyChecker_Check,
                PKIX_FALSE,     /* forwardCheckingSupported */
                PKIX_FALSE,
                policyExtensions,
                (PKIX_PL_Object *)polCheckerState,
                pChecker,
                plContext),
                PKIX_CERTCHAINCHECKERCREATEFAILED);

cleanup:
        PKIX_DECREF(polCheckerState);
        PKIX_DECREF(policyExtensions);
        PKIX_RETURN(CERTCHAINCHECKER);

}
