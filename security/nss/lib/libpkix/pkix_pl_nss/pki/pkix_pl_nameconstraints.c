/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_nameconstraints.c
 *
 * Name Constraints Object Functions Definitions
 *
 */

#include "pkix_pl_nameconstraints.h"


/* --Private-NameConstraints-Functions----------------------------- */

/*
 * FUNCTION: pkix_pl_CertNameConstraints_GetPermitted
 * DESCRIPTION:
 *
 *  This function retrieve name constraints permitted list from NSS
 *  data in "nameConstraints" and returns a PKIX_PL_GeneralName list
 *  in "pPermittedList".
 *
 * PARAMETERS
 *  "nameConstraints"
 *      Address of CertNameConstraints which has a pointer to
 *      CERTNameConstraints data. Must be non-NULL.
 *  "pPermittedList"
 *      Address where returned permitted name list is stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_GetPermitted(
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_List **pPermittedList,
        void *plContext)
{
        CERTNameConstraints *nssNameConstraints = NULL;
        CERTNameConstraints **nssNameConstraintsList = NULL;
        CERTNameConstraint *nssPermitted = NULL;
        CERTNameConstraint *firstPermitted = NULL;
        PKIX_List *permittedList = NULL;
        PKIX_PL_GeneralName *name = NULL;
        PKIX_UInt32 numItems = 0;
        PKIX_UInt32 i;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                "pkix_pl_CertNameConstraints_GetPermitted");
        PKIX_NULLCHECK_TWO(nameConstraints, pPermittedList);

        /*
         * nssNameConstraints is an array of CERTNameConstraints
         * pointers where CERTNameConstraints keep its permitted and excluded
         * lists as pointer array of CERTNameConstraint.
         */

        if (nameConstraints->permittedList == NULL) {

            PKIX_OBJECT_LOCK(nameConstraints);

            if (nameConstraints->permittedList == NULL) {

                PKIX_CHECK(PKIX_List_Create(&permittedList, plContext),
                        PKIX_LISTCREATEFAILED);

                numItems = nameConstraints->numNssNameConstraints;
                nssNameConstraintsList =
                        nameConstraints->nssNameConstraintsList;

                for (i = 0; i < numItems; i++) {

                    PKIX_NULLCHECK_ONE(nssNameConstraintsList);
                    nssNameConstraints = *(nssNameConstraintsList + i);
                    PKIX_NULLCHECK_ONE(nssNameConstraints);

                    if (nssNameConstraints->permited != NULL) {

                        nssPermitted = nssNameConstraints->permited;
                        firstPermitted = nssPermitted;

                        do {

                            PKIX_CHECK(pkix_pl_GeneralName_Create
                                (&nssPermitted->name, &name, plContext),
                                PKIX_GENERALNAMECREATEFAILED);

                            PKIX_CHECK(PKIX_List_AppendItem
                                (permittedList,
                                (PKIX_PL_Object *)name,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);

                            PKIX_DECREF(name);

                            PKIX_CERTNAMECONSTRAINTS_DEBUG
                                ("\t\tCalling CERT_GetNextNameConstraint\n");
                            nssPermitted = CERT_GetNextNameConstraint
                                (nssPermitted);

                        } while (nssPermitted != firstPermitted);

                    }
                }

                PKIX_CHECK(PKIX_List_SetImmutable(permittedList, plContext),
                            PKIX_LISTSETIMMUTABLEFAILED);

                nameConstraints->permittedList = permittedList;

            }

            PKIX_OBJECT_UNLOCK(nameConstraints);

        }

        PKIX_INCREF(nameConstraints->permittedList);

        *pPermittedList = nameConstraints->permittedList;

cleanup:

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_GetExcluded
 * DESCRIPTION:
 *
 *  This function retrieve name constraints excluded list from NSS
 *  data in "nameConstraints" and returns a PKIX_PL_GeneralName list
 *  in "pExcludedList".
 *
 * PARAMETERS
 *  "nameConstraints"
 *      Address of CertNameConstraints which has a pointer to NSS data.
 *      Must be non-NULL.
 *  "pPermittedList"
 *      Address where returned excluded name list is stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_GetExcluded(
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_List **pExcludedList,
        void *plContext)
{
        CERTNameConstraints *nssNameConstraints = NULL;
        CERTNameConstraints **nssNameConstraintsList = NULL;
        CERTNameConstraint *nssExcluded = NULL;
        CERTNameConstraint *firstExcluded = NULL;
        PKIX_List *excludedList = NULL;
        PKIX_PL_GeneralName *name = NULL;
        PKIX_UInt32 numItems = 0;
        PKIX_UInt32 i;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                "pkix_pl_CertNameConstraints_GetExcluded");
        PKIX_NULLCHECK_TWO(nameConstraints, pExcludedList);

        if (nameConstraints->excludedList == NULL) {

            PKIX_OBJECT_LOCK(nameConstraints);

            if (nameConstraints->excludedList == NULL) {

                PKIX_CHECK(PKIX_List_Create(&excludedList, plContext),
                            PKIX_LISTCREATEFAILED);

                numItems = nameConstraints->numNssNameConstraints;
                nssNameConstraintsList =
                        nameConstraints->nssNameConstraintsList;

                for (i = 0; i < numItems; i++) {

                    PKIX_NULLCHECK_ONE(nssNameConstraintsList);
                    nssNameConstraints = *(nssNameConstraintsList + i);
                    PKIX_NULLCHECK_ONE(nssNameConstraints);

                    if (nssNameConstraints->excluded != NULL) {

                        nssExcluded = nssNameConstraints->excluded;
                        firstExcluded = nssExcluded;

                        do {

                            PKIX_CHECK(pkix_pl_GeneralName_Create
                                (&nssExcluded->name, &name, plContext),
                                PKIX_GENERALNAMECREATEFAILED);

                            PKIX_CHECK(PKIX_List_AppendItem
                                (excludedList,
                                (PKIX_PL_Object *)name,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);

                            PKIX_DECREF(name);

                            PKIX_CERTNAMECONSTRAINTS_DEBUG
                                ("\t\tCalling CERT_GetNextNameConstraint\n");
                            nssExcluded = CERT_GetNextNameConstraint
                                (nssExcluded);

                        } while (nssExcluded != firstExcluded);

                    }

                }
                PKIX_CHECK(PKIX_List_SetImmutable(excludedList, plContext),
                            PKIX_LISTSETIMMUTABLEFAILED);

                nameConstraints->excludedList = excludedList;

            }

            PKIX_OBJECT_UNLOCK(nameConstraints);
        }

        PKIX_INCREF(nameConstraints->excludedList);

        *pExcludedList = nameConstraints->excludedList;

cleanup:

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_CheckNameSpaceNssNames
 * DESCRIPTION:
 *
 *  This function checks if CERTGeneralNames in "nssSubjectNames" comply
 *  with the permitted and excluded names in "nameConstraints". It returns
 *  PKIX_TRUE in "pCheckPass", if the Names satify the name space of the
 *  permitted list and if the Names are not in the excluded list. Otherwise,
 *  it returns PKIX_FALSE.
 *
 * PARAMETERS
 *  "nssSubjectNames"
 *      List of CERTGeneralName that nameConstraints verification is based on.
 *  "nameConstraints"
 *      Address of CertNameConstraints that provides lists of permitted
 *      and excluded names. Must be non-NULL.
 *  "pCheckPass"
 *      Address where PKIX_TRUE is returned if the all names in "nameList" are
 *      valid.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CertNameConstraints_CheckNameSpaceNssNames(
        CERTGeneralName *nssSubjectNames,
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_Boolean *pCheckPass,
        void *plContext)
{
        CERTNameConstraints **nssNameConstraintsList = NULL;
        CERTNameConstraints *nssNameConstraints = NULL;
        CERTGeneralName *nssMatchName = NULL;
        PLArenaPool *arena = NULL;
        PKIX_UInt32 numItems = 0;
        PKIX_UInt32 i;
        SECStatus status = SECSuccess;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                "pkix_pl_CertNameConstraints_CheckNameSpaceNssNames");
        PKIX_NULLCHECK_THREE(nssSubjectNames, nameConstraints, pCheckPass);

        *pCheckPass = PKIX_TRUE;

        PKIX_CERTNAMECONSTRAINTS_DEBUG("\t\tCalling PORT_NewArena\n");
        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (arena == NULL) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        nssMatchName = nssSubjectNames;
        nssNameConstraintsList = nameConstraints->nssNameConstraintsList;

        /*
         * CERTNameConstraint items in each permitted or excluded list
         * is verified as OR condition. That means, if one item matched,
         * then the checking on the remaining items on the list is skipped.
         * (see NSS cert_CompareNameWithConstraints(...)).
         * Items on PKIX_PL_NameConstraint's nssNameConstraints are verified
         * as AND condition. PKIX_PL_NameConstraint keeps an array of pointers
         * of CERTNameConstraints resulting from merging multiple
         * PKIX_PL_NameConstraints. Since each CERTNameConstraint are created
         * for different entity, a union condition of these entities then is
         * performed.
         */

        do {

            numItems = nameConstraints->numNssNameConstraints;

            for (i = 0; i < numItems; i++) {

                PKIX_NULLCHECK_ONE(nssNameConstraintsList);
                nssNameConstraints = *(nssNameConstraintsList + i);
                PKIX_NULLCHECK_ONE(nssNameConstraints);

                PKIX_CERTNAMECONSTRAINTS_DEBUG
                        ("\t\tCalling CERT_CheckNameSpace\n");
                status = CERT_CheckNameSpace
                        (arena, nssNameConstraints, nssMatchName);
                if (status != SECSuccess) {
                        break;
                }

            }

            if (status != SECSuccess) {
                    break;
            }

            PKIX_CERTNAMECONSTRAINTS_DEBUG
                    ("\t\tCalling CERT_GetNextGeneralName\n");
            nssMatchName = CERT_GetNextGeneralName(nssMatchName);

        } while (nssMatchName != nssSubjectNames);

        if (status == SECFailure) {

                *pCheckPass = PKIX_FALSE;
        }

cleanup:

        if (arena){
                PKIX_CERTNAMECONSTRAINTS_DEBUG
                        ("\t\tCalling PORT_FreeArena).\n");
                PORT_FreeArena(arena, PR_FALSE);
        }

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_NameConstraints_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;

        PKIX_ENTER(CERTNAMECONSTRAINTS, "pkix_pl_CertNameConstraints_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_CERTNAMECONSTRAINTS_TYPE, plContext),
                PKIX_OBJECTNOTCERTNAMECONSTRAINTS);

        nameConstraints = (PKIX_PL_CertNameConstraints *)object;

        PKIX_CHECK(PKIX_PL_Free
                    (nameConstraints->nssNameConstraintsList, plContext),
                    PKIX_FREEFAILED);

        if (nameConstraints->arena){
                PKIX_CERTNAMECONSTRAINTS_DEBUG
                        ("\t\tCalling PORT_FreeArena).\n");
                PORT_FreeArena(nameConstraints->arena, PR_FALSE);
                nameConstraints->arena = NULL;
        }

        PKIX_DECREF(nameConstraints->permittedList);
        PKIX_DECREF(nameConstraints->excludedList);

cleanup:

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the object
 *  NameConstraints and stores it at "pString".
 *
 * PARAMETERS
 *  "nameConstraints"
 *      Address of CertNameConstraints whose string representation is
 *      desired. Must be non-NULL.
 *  "pString"
 *      Address where string object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_ToString_Helper(
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_PL_String **pString,
        void *plContext)
{
        char *asciiFormat = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_List *permittedList = NULL;
        PKIX_List *excludedList = NULL;
        PKIX_PL_String *permittedListString = NULL;
        PKIX_PL_String *excludedListString = NULL;
        PKIX_PL_String *nameConstraintsString = NULL;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                    "pkix_pl_CertNameConstraints_ToString_Helper");
        PKIX_NULLCHECK_TWO(nameConstraints, pString);

        asciiFormat =
                "[\n"
                "\t\tPermitted Name:  %s\n"
                "\t\tExcluded Name:   %s\n"
                "\t]\n";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        PKIX_CHECK(pkix_pl_CertNameConstraints_GetPermitted
                    (nameConstraints, &permittedList, plContext),
                    PKIX_CERTNAMECONSTRAINTSGETPERMITTEDFAILED);

        PKIX_TOSTRING(permittedList, &permittedListString, plContext,
                    PKIX_LISTTOSTRINGFAILED);

        PKIX_CHECK(pkix_pl_CertNameConstraints_GetExcluded
                    (nameConstraints, &excludedList, plContext),
                    PKIX_CERTNAMECONSTRAINTSGETEXCLUDEDFAILED);

        PKIX_TOSTRING(excludedList, &excludedListString, plContext,
                    PKIX_LISTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&nameConstraintsString,
                    plContext,
                    formatString,
                    permittedListString,
                    excludedListString),
                    PKIX_SPRINTFFAILED);

        *pString = nameConstraintsString;

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(permittedList);
        PKIX_DECREF(excludedList);
        PKIX_DECREF(permittedListString);
        PKIX_DECREF(excludedListString);

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *nameConstraintsString = NULL;
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;

        PKIX_ENTER(CERTNAMECONSTRAINTS, "pkix_pl_CertNameConstraints_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(
                    object, PKIX_CERTNAMECONSTRAINTS_TYPE, plContext),
                    PKIX_OBJECTNOTCERTNAMECONSTRAINTS);

        nameConstraints = (PKIX_PL_CertNameConstraints *)object;

        PKIX_CHECK(pkix_pl_CertNameConstraints_ToString_Helper
                    (nameConstraints, &nameConstraintsString, plContext),
                    PKIX_CERTNAMECONSTRAINTSTOSTRINGHELPERFAILED);

        *pString = nameConstraintsString;

cleanup:

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;
        PKIX_List *permittedList = NULL;
        PKIX_List *excludedList = NULL;
        PKIX_UInt32 permitHash = 0;
        PKIX_UInt32 excludeHash = 0;

        PKIX_ENTER(CERTNAMECONSTRAINTS, "pkix_pl_CertNameConstraints_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_CERTNAMECONSTRAINTS_TYPE, plContext),
                    PKIX_OBJECTNOTCERTNAMECONSTRAINTS);

        nameConstraints = (PKIX_PL_CertNameConstraints *)object;

        PKIX_CHECK(pkix_pl_CertNameConstraints_GetPermitted
                    (nameConstraints, &permittedList, plContext),
                    PKIX_CERTNAMECONSTRAINTSGETPERMITTEDFAILED);

        PKIX_HASHCODE(permittedList, &permitHash, plContext,
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_CHECK(pkix_pl_CertNameConstraints_GetExcluded
                    (nameConstraints, &excludedList, plContext),
                    PKIX_CERTNAMECONSTRAINTSGETEXCLUDEDFAILED);

        PKIX_HASHCODE(excludedList, &excludeHash, plContext,
                    PKIX_OBJECTHASHCODEFAILED);

        *pHashcode = (((permitHash << 7) + excludeHash) << 7) +
                nameConstraints->numNssNameConstraints;

cleanup:

        PKIX_DECREF(permittedList);
        PKIX_DECREF(excludedList);
        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *firstNC = NULL;
        PKIX_PL_CertNameConstraints *secondNC = NULL;
        PKIX_List *firstPermittedList = NULL;
        PKIX_List *secondPermittedList = NULL;
        PKIX_List *firstExcludedList = NULL;
        PKIX_List *secondExcludedList = NULL;
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult = PKIX_FALSE;

        PKIX_ENTER(CERTNAMECONSTRAINTS, "pkix_pl_CertNameConstraints_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a CertNameConstraints */
        PKIX_CHECK(pkix_CheckType
                (firstObject, PKIX_CERTNAMECONSTRAINTS_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTCERTNAMECONSTRAINTS);

        firstNC = (PKIX_PL_CertNameConstraints *)firstObject;
        secondNC = (PKIX_PL_CertNameConstraints *)secondObject;

        /*
         * Since we know firstObject is a CertNameConstraints, if both
         * references are identical, they must be equal
         */
        if (firstNC == secondNC){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondNC isn't a CertNameConstraints, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;

        PKIX_CHECK(PKIX_PL_Object_GetType
                    ((PKIX_PL_Object *)secondNC, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        if (secondType != PKIX_CERTNAMECONSTRAINTS_TYPE) {
                goto cleanup;
        }

        PKIX_CHECK(pkix_pl_CertNameConstraints_GetPermitted
                    (firstNC, &firstPermittedList, plContext),
                    PKIX_CERTNAMECONSTRAINTSGETPERMITTEDFAILED);

        PKIX_CHECK(pkix_pl_CertNameConstraints_GetPermitted
                    (secondNC, &secondPermittedList, plContext),
                    PKIX_CERTNAMECONSTRAINTSGETPERMITTEDFAILED);

        PKIX_EQUALS
                (firstPermittedList, secondPermittedList, &cmpResult, plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (cmpResult != PKIX_TRUE) {
                goto cleanup;
        }

        PKIX_CHECK(pkix_pl_CertNameConstraints_GetExcluded
                    (firstNC, &firstExcludedList, plContext),
                    PKIX_CERTNAMECONSTRAINTSGETEXCLUDEDFAILED);

        PKIX_CHECK(pkix_pl_CertNameConstraints_GetExcluded
                    (secondNC, &secondExcludedList, plContext),
                    PKIX_CERTNAMECONSTRAINTSGETEXCLUDEDFAILED);

        PKIX_EQUALS
                (firstExcludedList, secondExcludedList, &cmpResult, plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (cmpResult != PKIX_TRUE) {
                goto cleanup;
        }

        /*
         * numNssNameConstraints is not checked because it is basically a
         * merge count, it cannot determine the data equality.
         */

        *pResult = PKIX_TRUE;

cleanup:

        PKIX_DECREF(firstPermittedList);
        PKIX_DECREF(secondPermittedList);
        PKIX_DECREF(firstExcludedList);
        PKIX_DECREF(secondExcludedList);

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_CERTNAMECONSTRAINTS_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_CertNameConstraints_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                    "pkix_pl_CertNameConstraints_RegisterSelf");

        entry.description = "CertNameConstraints";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_CertNameConstraints);
        entry.destructor = pkix_pl_CertNameConstraints_Destroy;
        entry.equalsFunction = pkix_pl_CertNameConstraints_Equals;
        entry.hashcodeFunction = pkix_pl_CertNameConstraints_Hashcode;
        entry.toStringFunction = pkix_pl_CertNameConstraints_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_CERTNAMECONSTRAINTS_TYPE] = entry;

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_Create_Helper
 *
 * DESCRIPTION:
 *  This function retrieves name constraints in "nssNameConstraints",
 *  converts and stores the result in a PKIX_PL_CertNameConstraints object.
 *
 * PARAMETERS
 *  "nssNameConstraints"
 *      Address of CERTNameConstraints that contains this object's data.
 *      Must be non-NULL.
 *  "pNameConstraints"
 *      Address where object pointer will be stored. Must be non-NULL.
 *      A NULL value will be returned if there is no Name Constraints extension.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_Create_Helper(
        CERTNameConstraints *nssNameConstraints,
        PKIX_PL_CertNameConstraints **pNameConstraints,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;
        CERTNameConstraints **nssNameConstraintPtr = NULL;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                    "pkix_pl_CertNameConstraints_Create_Helper");
        PKIX_NULLCHECK_TWO(nssNameConstraints, pNameConstraints);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CERTNAMECONSTRAINTS_TYPE,
                    sizeof (PKIX_PL_CertNameConstraints),
                    (PKIX_PL_Object **)&nameConstraints,
                    plContext),
                    PKIX_COULDNOTCREATECERTNAMECONSTRAINTSOBJECT);

        PKIX_CHECK(PKIX_PL_Malloc
                    (sizeof (CERTNameConstraint *),
                    (void *)&nssNameConstraintPtr,
                    plContext),
                    PKIX_MALLOCFAILED);

        nameConstraints->numNssNameConstraints = 1;
        nameConstraints->nssNameConstraintsList = nssNameConstraintPtr;
        *nssNameConstraintPtr = nssNameConstraints;

        nameConstraints->permittedList = NULL;
        nameConstraints->excludedList = NULL;
        nameConstraints->arena = NULL;

        *pNameConstraints = nameConstraints;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(nameConstraints);
        }

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_Create
 *
 * DESCRIPTION:
 *  function that allocates and initialize the object CertNameConstraints.
 *
 * PARAMETERS
 *  "nssCert"
 *      Address of CERT that contains this object's data.
 *      Must be non-NULL.
 *  "pNameConstraints"
 *      Address where object pointer will be stored. Must be non-NULL.
 *      A NULL value will be returned if there is no Name Constraints extension.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CertNameConstraints_Create(
        CERTCertificate *nssCert,
        PKIX_PL_CertNameConstraints **pNameConstraints,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;
        CERTNameConstraints *nssNameConstraints = NULL;
        PLArenaPool *arena = NULL;
        SECStatus status;

        PKIX_ENTER(CERTNAMECONSTRAINTS, "pkix_pl_CertNameConstraints_Create");
        PKIX_NULLCHECK_THREE(nssCert, pNameConstraints, nssCert->arena);

        PKIX_CERTNAMECONSTRAINTS_DEBUG("\t\tCalling PORT_NewArena).\n");
        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (arena == NULL) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        PKIX_CERTNAMECONSTRAINTS_DEBUG
                ("\t\tCalling CERT_FindNameConstraintsExten\n");
        status = CERT_FindNameConstraintsExten
                (arena, nssCert, &nssNameConstraints);

        if (status != SECSuccess) {
                PKIX_ERROR(PKIX_DECODINGCERTNAMECONSTRAINTSFAILED);
        }

        if (nssNameConstraints == NULL) {
                *pNameConstraints = NULL;
                if (arena){
                        PKIX_CERTNAMECONSTRAINTS_DEBUG
                                ("\t\tCalling PORT_FreeArena).\n");
                        PORT_FreeArena(arena, PR_FALSE);
                }
                goto cleanup;
        }

        PKIX_CHECK(pkix_pl_CertNameConstraints_Create_Helper
                    (nssNameConstraints, &nameConstraints, plContext),
                    PKIX_CERTNAMECONSTRAINTSCREATEHELPERFAILED);

        nameConstraints->arena = arena;

        *pNameConstraints = nameConstraints;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                if (arena){
                        PKIX_CERTNAMECONSTRAINTS_DEBUG
                                ("\t\tCalling PORT_FreeArena).\n");
                        PORT_FreeArena(arena, PR_FALSE);
                }
        }

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_CreateByMerge
 *
 * DESCRIPTION:
 *
 *  This function allocates and creates a PKIX_PL_NameConstraint object
 *  for merging. It also allocates CERTNameConstraints data space for the
 *  merged NSS NameConstraints data.
 *
 * PARAMETERS
 *  "pNameConstraints"
 *      Address where object pointer will be stored and returned.
 *      Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_CreateByMerge(
        PKIX_PL_CertNameConstraints **pNameConstraints,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;
        CERTNameConstraints *nssNameConstraints = NULL;
        PLArenaPool *arena = NULL;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                    "pkix_pl_CertNameConstraints_CreateByMerge");
        PKIX_NULLCHECK_ONE(pNameConstraints);

        PKIX_CERTNAMECONSTRAINTS_DEBUG("\t\tCalling PORT_NewArena).\n");
        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (arena == NULL) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        PKIX_CERTNAMECONSTRAINTS_DEBUG("\t\tCalling PORT_ArenaZNew).\n");
        nssNameConstraints = PORT_ArenaZNew(arena, CERTNameConstraints);
        if (nssNameConstraints == NULL) {
                PKIX_ERROR(PKIX_PORTARENAALLOCFAILED);
        }

        nssNameConstraints->permited = NULL;
        nssNameConstraints->excluded = NULL;
        nssNameConstraints->DERPermited = NULL;
        nssNameConstraints->DERExcluded = NULL;

        PKIX_CHECK(pkix_pl_CertNameConstraints_Create_Helper
                    (nssNameConstraints, &nameConstraints, plContext),
                    PKIX_CERTNAMECONSTRAINTSCREATEHELPERFAILED);

        nameConstraints->arena = arena;

        *pNameConstraints = nameConstraints;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                if (arena){
                        PKIX_CERTNAMECONSTRAINTS_DEBUG
                                ("\t\tCalling PORT_FreeArena).\n");
                        PORT_FreeArena(arena, PR_FALSE);
                }
        }

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_CopyNssNameConstraints
 *
 * DESCRIPTION:
 *
 *  This function allocates and copies data to a NSS CERTNameConstraints from
 *  the NameConstraints given by "srcNC" and stores the result at "pDestNC". It
 *  copies items on both the permitted and excluded lists, but not the
 *  DERPermited and DERExcluded.
 *
 * PARAMETERS
 *  "arena"
 *      Memory pool where object data is allocated from. Must be non-NULL.
 *  "srcNC"
 *      Address of the NameConstraints to copy from. Must be non-NULL.
 *  "pDestNC"
 *      Address where new copied object is stored and returned.
 *      Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CertNameConstraints_CopyNssNameConstraints(
        PLArenaPool *arena,
        CERTNameConstraints *srcNC,
        CERTNameConstraints **pDestNC,
        void *plContext)
{
        CERTNameConstraints *nssNameConstraints = NULL;
        CERTNameConstraint *nssNameConstraintHead = NULL;
        CERTNameConstraint *nssCurrent = NULL;
        CERTNameConstraint *nssCopyTo = NULL;
        CERTNameConstraint *nssCopyFrom = NULL;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                    "pkix_pl_CertNameConstraints_CopyNssNameConstraints");
        PKIX_NULLCHECK_THREE(arena, srcNC, pDestNC);

        PKIX_CERTNAMECONSTRAINTS_DEBUG("\t\tCalling PORT_ArenaZNew).\n");
        nssNameConstraints = PORT_ArenaZNew(arena, CERTNameConstraints);
        if (nssNameConstraints == NULL) {
                PKIX_ERROR(PKIX_PORTARENAALLOCFAILED);
        }

        if (srcNC->permited) {

            nssCopyFrom = srcNC->permited;

            do {

                nssCopyTo = NULL;
                PKIX_CERTNAMECONSTRAINTS_DEBUG
                        ("\t\tCalling CERT_CopyNameConstraint).\n");
                nssCopyTo = CERT_CopyNameConstraint
                        (arena, nssCopyTo, nssCopyFrom);
                if (nssCopyTo == NULL) {
                        PKIX_ERROR(PKIX_CERTCOPYNAMECONSTRAINTFAILED);
                }
                if (nssCurrent == NULL) {
                        nssCurrent = nssNameConstraintHead = nssCopyTo;
                } else {
                        PKIX_CERTNAMECONSTRAINTS_DEBUG
                                ("\t\tCalling CERT_AddNameConstraint).\n");
                        nssCurrent = CERT_AddNameConstraint
                                (nssCurrent, nssCopyTo);
                }

                PKIX_CERTNAMECONSTRAINTS_DEBUG
                        ("\t\tCalling CERT_GetNextNameConstrain).\n");
                nssCopyFrom = CERT_GetNextNameConstraint(nssCopyFrom);

            } while (nssCopyFrom != srcNC->permited);

            nssNameConstraints->permited = nssNameConstraintHead;
        }

        if (srcNC->excluded) {

            nssCurrent = NULL;
            nssCopyFrom = srcNC->excluded;

            do {

                /*
                 * Cannot use CERT_DupGeneralNameList, which just increments
                 * refcount. We need our own copy since arena is for each
                 * PKIX_PL_NameConstraints. Perhaps contribute this code
                 * as CERT_CopyGeneralNameList (in the future).
                 */
                nssCopyTo = NULL;
                PKIX_CERTNAMECONSTRAINTS_DEBUG
                        ("\t\tCalling CERT_CopyNameConstraint).\n");
                nssCopyTo = CERT_CopyNameConstraint
                        (arena, nssCopyTo, nssCopyFrom);
                if (nssCopyTo == NULL) {
                        PKIX_ERROR(PKIX_CERTCOPYNAMECONSTRAINTFAILED);
                }
                if (nssCurrent == NULL) {
                        nssCurrent = nssNameConstraintHead = nssCopyTo;
                } else {
                        PKIX_CERTNAMECONSTRAINTS_DEBUG
                                ("\t\tCalling CERT_AddNameConstraint).\n");
                        nssCurrent = CERT_AddNameConstraint
                                (nssCurrent, nssCopyTo);
                }

                PKIX_CERTNAMECONSTRAINTS_DEBUG
                        ("\t\tCalling CERT_GetNextNameConstrain).\n");
                nssCopyFrom = CERT_GetNextNameConstraint(nssCopyFrom);

            } while (nssCopyFrom != srcNC->excluded);

            nssNameConstraints->excluded = nssNameConstraintHead;
        }

        *pDestNC = nssNameConstraints;

cleanup:

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertNameConstraints_Merge
 *
 * DESCRIPTION:
 *
 *  This function merges two NameConstraints pointed to by "firstNC" and
 *  "secondNC" and stores the result in "pMergedNC".
 *
 * PARAMETERS
 *  "firstNC"
 *      Address of the first NameConstraints to be merged. Must be non-NULL.
 *  "secondNC"
 *      Address of the second NameConstraints to be merged. Must be non-NULL.
 *  "pMergedNC"
 *      Address where the merge result is stored and returned. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a NameConstraints Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CertNameConstraints_Merge(
        PKIX_PL_CertNameConstraints *firstNC,
        PKIX_PL_CertNameConstraints *secondNC,
        PKIX_PL_CertNameConstraints **pMergedNC,
        void *plContext)
{
        PKIX_PL_CertNameConstraints *nameConstraints = NULL;
        CERTNameConstraints **nssNCto = NULL;
        CERTNameConstraints **nssNCfrom = NULL;
        CERTNameConstraints *nssNameConstraints = NULL;
        PKIX_UInt32 numNssItems = 0;
        PKIX_UInt32 i;

        PKIX_ENTER(CERTNAMECONSTRAINTS, "pkix_pl_CertNameConstraints_Merge");
        PKIX_NULLCHECK_THREE(firstNC, secondNC, pMergedNC);

        PKIX_CHECK(pkix_pl_CertNameConstraints_CreateByMerge
                    (&nameConstraints, plContext),
                    PKIX_CERTNAMECONSTRAINTSCREATEBYMERGEFAILED);

        /* Merge NSSCertConstraint lists */

        numNssItems = firstNC->numNssNameConstraints +
                    secondNC->numNssNameConstraints;

        /* Free the default space (only one entry) allocated by create */
        PKIX_CHECK(PKIX_PL_Free
                    (nameConstraints->nssNameConstraintsList, plContext),
                    PKIX_FREEFAILED);

        /* Reallocate the size we need */
        PKIX_CHECK(PKIX_PL_Malloc
                    (numNssItems * sizeof (CERTNameConstraint *),
                    (void *)&nssNCto,
                    plContext),
                    PKIX_MALLOCFAILED);

        nameConstraints->nssNameConstraintsList = nssNCto;

        nssNCfrom = firstNC->nssNameConstraintsList;

        for (i = 0; i < firstNC->numNssNameConstraints; i++) {

                PKIX_CHECK(pkix_pl_CertNameConstraints_CopyNssNameConstraints
                        (nameConstraints->arena,
                        *nssNCfrom,
                        &nssNameConstraints,
                        plContext),
                        PKIX_CERTNAMECONSTRAINTSCOPYNSSNAMECONSTRAINTSFAILED);

                *nssNCto = nssNameConstraints;

                nssNCto++;
                nssNCfrom++;
        }

        nssNCfrom = secondNC->nssNameConstraintsList;

        for (i = 0; i < secondNC->numNssNameConstraints; i++) {

                PKIX_CHECK(pkix_pl_CertNameConstraints_CopyNssNameConstraints
                        (nameConstraints->arena,
                        *nssNCfrom,
                        &nssNameConstraints,
                        plContext),
                        PKIX_CERTNAMECONSTRAINTSCOPYNSSNAMECONSTRAINTSFAILED);

                *nssNCto = nssNameConstraints;

                nssNCto++;
                nssNCfrom++;
        }

        nameConstraints->numNssNameConstraints = numNssItems;
        nameConstraints->permittedList = NULL;
        nameConstraints->excludedList = NULL;

        *pMergedNC = nameConstraints;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(nameConstraints);
        }

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}

/* --Public-NameConstraints-Functions-------------------------------- */

/*
 * FUNCTION:  PKIX_PL_CertNameConstraints_CheckNamesInNameSpace
 * (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_CertNameConstraints_CheckNamesInNameSpace(
        PKIX_List *nameList, /* List of PKIX_PL_GeneralName */
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_Boolean *pCheckPass,
        void *plContext)
{
        CERTNameConstraints **nssNameConstraintsList = NULL;
        CERTNameConstraints *nssNameConstraints = NULL;
        CERTGeneralName *nssMatchName = NULL;
        PLArenaPool *arena = NULL;
        PKIX_PL_GeneralName *name = NULL;
        PKIX_UInt32 numNameItems = 0;
        PKIX_UInt32 numNCItems = 0;
        PKIX_UInt32 i, j;
        SECStatus status = SECSuccess;

        PKIX_ENTER(CERTNAMECONSTRAINTS,
                "PKIX_PL_CertNameConstraints_CheckNamesInNameSpace");
        PKIX_NULLCHECK_TWO(nameConstraints, pCheckPass);

        *pCheckPass = PKIX_TRUE;

        if (nameList != NULL) {

                PKIX_CERTNAMECONSTRAINTS_DEBUG("\t\tCalling PORT_NewArena\n");
                arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                if (arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }

                nssNameConstraintsList =
                        nameConstraints->nssNameConstraintsList;
                PKIX_NULLCHECK_ONE(nssNameConstraintsList);
                numNCItems = nameConstraints->numNssNameConstraints;

                PKIX_CHECK(PKIX_List_GetLength
                        (nameList, &numNameItems, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                for (i = 0; i < numNameItems; i++) {

                        PKIX_CHECK(PKIX_List_GetItem
                                (nameList,
                                i,
                                (PKIX_PL_Object **) &name,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(pkix_pl_GeneralName_GetNssGeneralName
                                (name, &nssMatchName, plContext),
                                PKIX_GENERALNAMEGETNSSGENERALNAMEFAILED);

                        PKIX_DECREF(name);

                        for (j = 0; j < numNCItems; j++) {

                            nssNameConstraints = *(nssNameConstraintsList + j);
                            PKIX_NULLCHECK_ONE(nssNameConstraints);

                            PKIX_CERTNAMECONSTRAINTS_DEBUG
                                ("\t\tCalling CERT_CheckNameSpace\n");
                            status = CERT_CheckNameSpace
                                (arena, nssNameConstraints, nssMatchName);
                            if (status != SECSuccess) {
                                break;
                            }

                        }

                        if (status != SECSuccess) {
                            break;
                        }

                }
        }

        if (status == SECFailure) {
                *pCheckPass = PKIX_FALSE;
        }

cleanup:

        if (arena){
                PKIX_CERTNAMECONSTRAINTS_DEBUG
                        ("\t\tCalling PORT_FreeArena).\n");
                PORT_FreeArena(arena, PR_FALSE);
        }

        PKIX_RETURN(CERTNAMECONSTRAINTS);
}
