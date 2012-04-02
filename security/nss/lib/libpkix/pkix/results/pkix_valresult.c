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
 * pkix_valresult.c
 *
 * ValidateResult Object Functions
 *
 */

#include "pkix_valresult.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_ValidateResult_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ValidateResult_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ValidateResult *result = NULL;

        PKIX_ENTER(VALIDATERESULT, "pkix_ValidateResult_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a validate result object */
        PKIX_CHECK(pkix_CheckType(object, PKIX_VALIDATERESULT_TYPE, plContext),
                PKIX_OBJECTNOTVALIDATERESULT);

        result = (PKIX_ValidateResult *)object;

        PKIX_DECREF(result->anchor);
        PKIX_DECREF(result->pubKey);
        PKIX_DECREF(result->policyTree);

cleanup:

        PKIX_RETURN(VALIDATERESULT);
}

/*
 * FUNCTION: pkix_ValidateResult_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ValidateResult_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult;
        PKIX_ValidateResult *firstValResult = NULL;
        PKIX_ValidateResult *secondValResult = NULL;
        PKIX_TrustAnchor *firstAnchor = NULL;
        PKIX_TrustAnchor *secondAnchor = NULL;
        PKIX_PolicyNode *firstTree = NULL;
        PKIX_PolicyNode *secondTree = NULL;

        PKIX_ENTER(VALIDATERESULT, "pkix_ValidateResult_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        PKIX_CHECK(pkix_CheckType(first, PKIX_VALIDATERESULT_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTVALIDATERESULT);

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        *pResult = PKIX_FALSE;

        if (secondType != PKIX_VALIDATERESULT_TYPE) goto cleanup;

        firstValResult = (PKIX_ValidateResult *)first;
        secondValResult = (PKIX_ValidateResult *)second;

        PKIX_CHECK(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)firstValResult->pubKey,
                (PKIX_PL_Object *)secondValResult->pubKey,
                &cmpResult,
                plContext),
                PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        firstAnchor = firstValResult->anchor;
        secondAnchor = secondValResult->anchor;

        if ((firstAnchor != NULL) && (secondAnchor != NULL)) {
                PKIX_CHECK(PKIX_PL_Object_Equals
                        ((PKIX_PL_Object *)firstAnchor,
                        (PKIX_PL_Object *)secondAnchor,
                        &cmpResult,
                        plContext),
                        PKIX_OBJECTEQUALSFAILED);
        } else {
                cmpResult = (firstAnchor == secondAnchor);
        }

        if (!cmpResult) goto cleanup;

        firstTree = firstValResult->policyTree;
        secondTree = secondValResult->policyTree;

        if ((firstTree != NULL) && (secondTree != NULL)) {
                PKIX_CHECK(PKIX_PL_Object_Equals
                        ((PKIX_PL_Object *)firstTree,
                        (PKIX_PL_Object *)secondTree,
                        &cmpResult,
                        plContext),
                        PKIX_OBJECTEQUALSFAILED);
        } else {
                cmpResult = (firstTree == secondTree);
        }

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(VALIDATERESULT);
}

/*
 * FUNCTION: pkix_ValidateResult_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ValidateResult_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_ValidateResult *valResult = NULL;
        PKIX_UInt32 hash = 0;
        PKIX_UInt32 pubKeyHash = 0;
        PKIX_UInt32 anchorHash = 0;
        PKIX_UInt32 policyTreeHash = 0;

        PKIX_ENTER(VALIDATERESULT, "pkix_ValidateResult_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_VALIDATERESULT_TYPE, plContext),
                PKIX_OBJECTNOTVALIDATERESULT);

        valResult = (PKIX_ValidateResult*)object;

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                ((PKIX_PL_Object *)valResult->pubKey, &pubKeyHash, plContext),
                PKIX_OBJECTHASHCODEFAILED);

        if (valResult->anchor) {
                PKIX_CHECK(PKIX_PL_Object_Hashcode
                        ((PKIX_PL_Object *)valResult->anchor,
                        &anchorHash,
                        plContext),
                        PKIX_OBJECTHASHCODEFAILED);
        }

        if (valResult->policyTree) {
                PKIX_CHECK(PKIX_PL_Object_Hashcode
                        ((PKIX_PL_Object *)valResult->policyTree,
                        &policyTreeHash,
                        plContext),
                        PKIX_OBJECTHASHCODEFAILED);
        }

        hash = 31*(31 * pubKeyHash + anchorHash) + policyTreeHash;

        *pHashcode = hash;

cleanup:

        PKIX_RETURN(VALIDATERESULT);
}

/*
 * FUNCTION: pkix_ValidateResult_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ValidateResult_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_ValidateResult *valResult = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *valResultString = NULL;

        PKIX_TrustAnchor *anchor = NULL;
        PKIX_PL_PublicKey *pubKey = NULL;
        PKIX_PolicyNode *policyTree = NULL;

        PKIX_PL_String *anchorString = NULL;
        PKIX_PL_String *pubKeyString = NULL;
        PKIX_PL_String *treeString = NULL;
        char *asciiNullString = "(null)";
        char *asciiFormat =
                "[\n"
                "\tTrustAnchor: \t\t%s"
                "\tPubKey:    \t\t%s\n"
                "\tPolicyTree:  \t\t%s\n"
                "]\n";

        PKIX_ENTER(VALIDATERESULT, "pkix_ValidateResult_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_VALIDATERESULT_TYPE, plContext),
                PKIX_OBJECTNOTVALIDATERESULT);

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, asciiFormat, 0, &formatString, plContext),
                PKIX_STRINGCREATEFAILED);

        valResult = (PKIX_ValidateResult*)object;

        anchor = valResult->anchor;

        if (anchor) {
                PKIX_CHECK(PKIX_PL_Object_ToString
                        ((PKIX_PL_Object *)anchor, &anchorString, plContext),
                        PKIX_OBJECTTOSTRINGFAILED);
        } else {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII,
                        asciiNullString,
                        0,
                        &anchorString,
                        plContext),
                        PKIX_STRINGCREATEFAILED);
        }

        pubKey = valResult->pubKey;

        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object *)pubKey, &pubKeyString, plContext),
                PKIX_OBJECTTOSTRINGFAILED);

        policyTree = valResult->policyTree;

        if (policyTree) {
                PKIX_CHECK(PKIX_PL_Object_ToString
                        ((PKIX_PL_Object *)policyTree, &treeString, plContext),
                        PKIX_OBJECTTOSTRINGFAILED);
        } else {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII,
                        asciiNullString,
                        0,
                        &treeString,
                        plContext),
                        PKIX_STRINGCREATEFAILED);
        }

        PKIX_CHECK(PKIX_PL_Sprintf
                (&valResultString,
                plContext,
                formatString,
                anchorString,
                pubKeyString,
                treeString),
                PKIX_SPRINTFFAILED);

        *pString = valResultString;

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(anchorString);
        PKIX_DECREF(pubKeyString);
        PKIX_DECREF(treeString);

        PKIX_RETURN(VALIDATERESULT);
}

/*
 * FUNCTION: pkix_ValidateResult_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_VALIDATERESULT_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_ValidateResult_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(VALIDATERESULT, "pkix_ValidateResult_RegisterSelf");

        entry.description = "ValidateResult";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_ValidateResult);
        entry.destructor = pkix_ValidateResult_Destroy;
        entry.equalsFunction = pkix_ValidateResult_Equals;
        entry.hashcodeFunction = pkix_ValidateResult_Hashcode;
        entry.toStringFunction = pkix_ValidateResult_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_VALIDATERESULT_TYPE] = entry;

        PKIX_RETURN(VALIDATERESULT);
}

/*
 * FUNCTION: pkix_ValidateResult_Create
 * DESCRIPTION:
 *
 *  Creates a new ValidateResult Object using the PublicKey pointed to by
 *  "pubKey", the TrustAnchor pointed to by "anchor", and the PolicyNode
 *  pointed to by "policyTree", and stores it at "pResult".
 *
 * PARAMETERS
 *  "pubKey"
 *      PublicKey of the desired ValidateResult. Must be non-NULL.
 *  "anchor"
 *      TrustAnchor of the desired Validateresult. May be NULL.
 *  "policyTree"
 *      PolicyNode of the desired ValidateResult; may be NULL
 *  "pResult"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_ValidateResult_Create(
        PKIX_PL_PublicKey *pubKey,
        PKIX_TrustAnchor *anchor,
        PKIX_PolicyNode *policyTree,
        PKIX_ValidateResult **pResult,
        void *plContext)
{
        PKIX_ValidateResult *result = NULL;

        PKIX_ENTER(VALIDATERESULT, "pkix_ValidateResult_Create");
        PKIX_NULLCHECK_TWO(pubKey, pResult);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_VALIDATERESULT_TYPE,
                    sizeof (PKIX_ValidateResult),
                    (PKIX_PL_Object **)&result,
                    plContext),
                    PKIX_COULDNOTCREATEVALIDATERESULTOBJECT);

        /* initialize fields */

        PKIX_INCREF(pubKey);
        result->pubKey = pubKey;

        PKIX_INCREF(anchor);
        result->anchor = anchor;

        PKIX_INCREF(policyTree);
        result->policyTree = policyTree;

        *pResult = result;
        result = NULL;

cleanup:

        PKIX_DECREF(result);

        PKIX_RETURN(VALIDATERESULT);

}

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_ValidateResult_GetPublicKey
 *      (see comments in pkix_result.h)
 */
PKIX_Error *
PKIX_ValidateResult_GetPublicKey(
        PKIX_ValidateResult *result,
        PKIX_PL_PublicKey **pPublicKey,
        void *plContext)
{
        PKIX_ENTER(VALIDATERESULT, "PKIX_ValidateResult_GetPublicKey");
        PKIX_NULLCHECK_TWO(result, pPublicKey);

        PKIX_INCREF(result->pubKey);
        *pPublicKey = result->pubKey;

cleanup:
        PKIX_RETURN(VALIDATERESULT);
}

/*
 * FUNCTION: PKIX_ValidateResult_GetTrustAnchor
 *      (see comments in pkix_result.h)
 */
PKIX_Error *
PKIX_ValidateResult_GetTrustAnchor(
        PKIX_ValidateResult *result,
        PKIX_TrustAnchor **pTrustAnchor,
        void *plContext)
{
        PKIX_ENTER(VALIDATERESULT, "PKIX_ValidateResult_GetTrustAnchor");
        PKIX_NULLCHECK_TWO(result, pTrustAnchor);

        PKIX_INCREF(result->anchor);
        *pTrustAnchor = result->anchor;

cleanup:
        PKIX_RETURN(VALIDATERESULT);
}

/*
 * FUNCTION: PKIX_ValidateResult_GetPolicyTree
 *      (see comments in pkix_result.h)
 */
PKIX_Error *
PKIX_ValidateResult_GetPolicyTree(
        PKIX_ValidateResult *result,
        PKIX_PolicyNode **pPolicyTree,
        void *plContext)
{
        PKIX_ENTER(VALIDATERESULT, "PKIX_ValidateResult_GetPolicyTree");
        PKIX_NULLCHECK_TWO(result, pPolicyTree);

        PKIX_INCREF(result->policyTree);
        (*pPolicyTree) = result->policyTree;

cleanup:
        PKIX_RETURN(VALIDATERESULT);
}
