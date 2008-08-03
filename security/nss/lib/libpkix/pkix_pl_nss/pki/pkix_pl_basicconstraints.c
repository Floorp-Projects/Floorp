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
 * pkix_pl_basicconstraints.c
 *
 * BasicConstraints Object Functions
 *
 */

#include "pkix_pl_basicconstraints.h"

/*
 * FUNCTION: pkix_pl_CertBasicConstraints_Create
 * DESCRIPTION:
 *
 *  Creates a new CertBasicConstraints object whose CA Flag has the value
 *  given by the Boolean value of "isCA" and whose path length field has the
 *  value given by the "pathLen" argument and stores it at "pObject".
 *
 * PARAMETERS
 *  "isCA"
 *      Boolean value with the desired value of CA Flag.
 *  "pathLen"
 *      a PKIX_Int32 with the desired value of path length
 *  "pObject"
 *      Address of object pointer's destination. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertBasicConstraints Error if the function fails
 *  in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CertBasicConstraints_Create(
        PKIX_Boolean isCA,
        PKIX_Int32 pathLen,
        PKIX_PL_CertBasicConstraints **pObject,
        void *plContext)
{
        PKIX_PL_CertBasicConstraints *basic = NULL;

        PKIX_ENTER(CERTBASICCONSTRAINTS,
                    "pkix_pl_CertBasicConstraints_Create");
        PKIX_NULLCHECK_ONE(pObject);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CERTBASICCONSTRAINTS_TYPE,
                    sizeof (PKIX_PL_CertBasicConstraints),
                    (PKIX_PL_Object **)&basic,
                    plContext),
                    PKIX_COULDNOTCREATECERTBASICCONSTRAINTSOBJECT);

        basic->isCA = isCA;

        /* pathLen has meaning only for CAs, but it's not worth checking */
        basic->pathLen = pathLen;

        *pObject = basic;

cleanup:

        PKIX_RETURN(CERTBASICCONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertBasicConstraints_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertBasicConstraints_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_CertBasicConstraints *certB = NULL;

        PKIX_ENTER(CERTBASICCONSTRAINTS,
                "pkix_pl_CertBasicConstraints_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_CERTBASICCONSTRAINTS_TYPE, plContext),
                    PKIX_OBJECTNOTCERTBASICCONSTRAINTS);

        certB = (PKIX_PL_CertBasicConstraints*)object;

        certB->isCA = PKIX_FALSE;
        certB->pathLen = 0;

cleanup:

        PKIX_RETURN(CERTBASICCONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertBasicConstraints_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertBasicConstraints_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *certBasicConstraintsString = NULL;
        PKIX_PL_CertBasicConstraints *certB = NULL;
        PKIX_Boolean isCA = PKIX_FALSE;
        PKIX_Int32 pathLen = 0;
        PKIX_PL_String *outString = NULL;
        char *fmtString = NULL;
        PKIX_Boolean pathlenArg = PKIX_FALSE;

        PKIX_ENTER(CERTBASICCONSTRAINTS,
                "pkix_pl_CertBasicConstraints_toString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_CERTBASICCONSTRAINTS_TYPE, plContext),
                    PKIX_FIRSTARGUMENTNOTCERTBASICCONSTRAINTSOBJECT);

        certB = (PKIX_PL_CertBasicConstraints *)object;

        /*
         * if CA == TRUE
         *      if pathLen == CERT_UNLIMITED_PATH_CONSTRAINT
         *              print "CA(-1)"
         *      else print "CA(nnn)"
         * if CA == FALSE, print "~CA"
         */

        isCA = certB->isCA;

        if (isCA) {
                pathLen = certB->pathLen;

                if (pathLen == CERT_UNLIMITED_PATH_CONSTRAINT) {
                        /* print "CA(-1)" */
                        fmtString = "CA(-1)";
                        pathlenArg = PKIX_FALSE;
                } else {
                        /* print "CA(pathLen)" */
                        fmtString = "CA(%d)";
                        pathlenArg = PKIX_TRUE;
                }
        } else {
                /* print "~CA" */
                fmtString = "~CA";
                pathlenArg = PKIX_FALSE;
        }

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    fmtString,
                    0,
                    &certBasicConstraintsString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        if (pathlenArg) {
                PKIX_CHECK(PKIX_PL_Sprintf
                            (&outString,
                            plContext,
                            certBasicConstraintsString,
                            pathLen),
                            PKIX_SPRINTFFAILED);
        } else {
                PKIX_CHECK(PKIX_PL_Sprintf
                            (&outString,
                            plContext,
                            certBasicConstraintsString),
                            PKIX_SPRINTFFAILED);
        }

        *pString = outString;

cleanup:

        PKIX_DECREF(certBasicConstraintsString);

        PKIX_RETURN(CERTBASICCONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertBasicConstraints_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertBasicConstraints_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_CertBasicConstraints *certB = NULL;
        PKIX_Boolean isCA = PKIX_FALSE;
        PKIX_Int32 pathLen = 0;
        PKIX_Int32 hashInput = 0;
        PKIX_UInt32 cbcHash = 0;

        PKIX_ENTER(CERTBASICCONSTRAINTS,
                "pkix_pl_CertBasicConstraints_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_CERTBASICCONSTRAINTS_TYPE, plContext),
                    PKIX_OBJECTNOTCERTBASICCONSTRAINTS);

        certB = (PKIX_PL_CertBasicConstraints *)object;

        /*
         * if CA == TRUE
         *      hash(pathLen + 1 - PKIX_UNLIMITED_PATH_CONSTRAINT)
         * if CA == FALSE, hash(0)
         */

        isCA = certB->isCA;

        if (isCA) {
                pathLen = certB->pathLen;

                hashInput = pathLen + 1 - PKIX_UNLIMITED_PATH_CONSTRAINT;
        }

        PKIX_CHECK(pkix_hash
                    ((const unsigned char *)&hashInput,
                    sizeof (hashInput),
                    &cbcHash,
                    plContext),
                    PKIX_HASHFAILED);

        *pHashcode = cbcHash;

cleanup:

        PKIX_RETURN(CERTBASICCONSTRAINTS);
}


/*
 * FUNCTION: pkix_pl_CertBasicConstraints_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertBasicConstraints_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_CertBasicConstraints *firstCBC = NULL;
        PKIX_PL_CertBasicConstraints *secondCBC = NULL;
        PKIX_UInt32 secondType;
        PKIX_Boolean firstIsCA = PKIX_FALSE;
        PKIX_Boolean secondIsCA = PKIX_FALSE;
        PKIX_Int32 firstPathLen = 0;
        PKIX_Int32 secondPathLen = 0;

        PKIX_ENTER(CERTBASICCONSTRAINTS,
                "pkix_pl_CertBasicConstraints_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a CertBasicConstraints */
        PKIX_CHECK(pkix_CheckType
                    (firstObject, PKIX_CERTBASICCONSTRAINTS_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTCERTBASICCONSTRAINTS);

        /*
         * Since we know firstObject is a CertBasicConstraints,
         * if both references are identical, they must be equal
         */
        if (firstObject == secondObject){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't a CertBasicConstraints, we
         * don't throw an error. We simply return FALSE.
         */
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObject, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_CERTBASICCONSTRAINTS_TYPE) {
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        firstCBC = (PKIX_PL_CertBasicConstraints *)firstObject;
        secondCBC = (PKIX_PL_CertBasicConstraints *)secondObject;

        /*
         * Compare the value of the CAFlag components
         */

        firstIsCA = firstCBC->isCA;

        /*
         * Failure here would be an error, not merely a miscompare,
         * since we know second is a CertBasicConstraints.
         */
        secondIsCA = secondCBC->isCA;

        /*
         * If isCA flags differ, the objects are not equal.
         */
        if (secondIsCA != firstIsCA) {
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        /*
         * If isCA was FALSE, the objects are equal, because
         * pathLen is meaningless in that case.
         */
        if (!firstIsCA) {
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        firstPathLen = firstCBC->pathLen;
        secondPathLen = secondCBC->pathLen;

        *pResult = (secondPathLen == firstPathLen);

cleanup:

        PKIX_RETURN(CERTBASICCONSTRAINTS);
}

/*
 * FUNCTION: pkix_pl_CertBasicConstraints_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_CERTBASICCONSTRAINTS_TYPE and its related
 *  functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize,
 *  which should only be called once, it is acceptable that
 *  this function is not thread-safe.
 */
PKIX_Error *
pkix_pl_CertBasicConstraints_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CERTBASICCONSTRAINTS,
                "pkix_pl_CertBasicConstraints_RegisterSelf");

        entry.description = "CertBasicConstraints";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_CertBasicConstraints);
        entry.destructor = pkix_pl_CertBasicConstraints_Destroy;
        entry.equalsFunction = pkix_pl_CertBasicConstraints_Equals;
        entry.hashcodeFunction = pkix_pl_CertBasicConstraints_Hashcode;
        entry.toStringFunction = pkix_pl_CertBasicConstraints_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_CERTBASICCONSTRAINTS_TYPE] = entry;

        PKIX_RETURN(CERTBASICCONSTRAINTS);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_BasicConstraints_GetCAFlag
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_BasicConstraints_GetCAFlag(
        PKIX_PL_CertBasicConstraints *basicConstraints,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_ENTER(CERTBASICCONSTRAINTS,
                "PKIX_PL_BasicConstraintsGetCAFlag");
        PKIX_NULLCHECK_TWO(basicConstraints, pResult);

        *pResult = basicConstraints->isCA;

        PKIX_RETURN(CERTBASICCONSTRAINTS);
}

/*
 * FUNCTION: PKIX_PL_BasicConstraints_GetPathLenConstraint
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_BasicConstraints_GetPathLenConstraint(
        PKIX_PL_CertBasicConstraints *basicConstraints,
        PKIX_Int32 *pPathLenConstraint,
        void *plContext)
{
        PKIX_ENTER(CERTBASICCONSTRAINTS,
                "PKIX_PL_BasicConstraintsGetPathLenConstraint");
        PKIX_NULLCHECK_TWO(basicConstraints, pPathLenConstraint);

        *pPathLenConstraint = basicConstraints->pathLen;

        PKIX_RETURN(CERTBASICCONSTRAINTS);
}
