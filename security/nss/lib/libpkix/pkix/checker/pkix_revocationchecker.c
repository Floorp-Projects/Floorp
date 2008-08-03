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
 * pkix_revocationchecker.c
 *
 * RevocationChecker Object Functions
 *
 */

#include "pkix_revocationchecker.h"

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

        PKIX_DECREF(checker->revCheckerContext);

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
        PKIX_PL_Object *contextDuplicate = NULL;

        PKIX_ENTER(REVOCATIONCHECKER, "pkix_RevocationChecker_Duplicate");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_REVOCATIONCHECKER_TYPE, plContext),
                    PKIX_OBJECTNOTCERTCHAINCHECKER);

        checker = (PKIX_RevocationChecker *)object;

        if (checker->revCheckerContext){
                PKIX_CHECK(PKIX_PL_Object_Duplicate
                            ((PKIX_PL_Object *)checker->revCheckerContext,
                            (PKIX_PL_Object **)&contextDuplicate,
                            plContext),
                            PKIX_OBJECTDUPLICATEFAILED);
        }

        PKIX_CHECK(PKIX_RevocationChecker_Create
                    (checker->checkCallback,
                    contextDuplicate,
                    &checkerDuplicate,
                    plContext),
                    PKIX_REVOCATIONCHECKERCREATEFAILED);

        *pNewObject = (PKIX_PL_Object *)checkerDuplicate;

cleanup:

        PKIX_DECREF(contextDuplicate);

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

/* --Public-Functions--------------------------------------------- */


/*
 * FUNCTION: PKIX_RevocationChecker_Create (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_RevocationChecker_Create(
    PKIX_RevocationChecker_RevCallback callback,
    PKIX_PL_Object *revCheckerContext,
    PKIX_RevocationChecker **pChecker,
    void *plContext)
{
        PKIX_RevocationChecker *checker = NULL;

        PKIX_ENTER(REVOCATIONCHECKER, "PKIX_RevocationChecker_Create");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_REVOCATIONCHECKER_TYPE,
                    sizeof (PKIX_RevocationChecker),
                    (PKIX_PL_Object **)&checker,
                    plContext),
                    PKIX_COULDNOTCREATECERTCHAINCHECKEROBJECT);

        /* initialize fields */
        checker->checkCallback = callback;

        PKIX_INCREF(revCheckerContext);
        checker->revCheckerContext = revCheckerContext;

        *pChecker = checker;

cleanup:

        PKIX_RETURN(REVOCATIONCHECKER);

}

/*
 * FUNCTION: PKIX_RevocationChecker_GetCheckCallback
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_RevocationChecker_GetRevCallback(
        PKIX_RevocationChecker *checker,
        PKIX_RevocationChecker_RevCallback *pCallback,
        void *plContext)
{
        PKIX_ENTER
                (REVOCATIONCHECKER, "PKIX_RevocationChecker_GetRevCallback");
        PKIX_NULLCHECK_TWO(checker, pCallback);

        *pCallback = checker->checkCallback;

        PKIX_RETURN(REVOCATIONCHECKER);
}

/*
 * FUNCTION: PKIX_RevocationChecker_GetRevCheckerContext
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_RevocationChecker_GetRevCheckerContext(
        PKIX_RevocationChecker *checker,
        PKIX_PL_Object **pRevCheckerContext,
        void *plContext)
{
        PKIX_ENTER(REVOCATIONCHECKER,
                    "PKIX_RevocationChecker_GetRevCheckerContext");

        PKIX_NULLCHECK_TWO(checker, pRevCheckerContext);

        PKIX_INCREF(checker->revCheckerContext);

        *pRevCheckerContext = checker->revCheckerContext;

cleanup:
        PKIX_RETURN(REVOCATIONCHECKER);

}
