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
 * pkix_defaultrevchecker.c
 *
 * Functions for default Revocation Checker
 *
 */

#include "pkix_defaultrevchecker.h"

/* --Private-DefaultRevChecker-Functions------------------------------- */

/*
 * FUNCTION: pkix_DefaultRevChecker_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_DefaultRevChecker_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_DefaultRevocationChecker *revChecker = NULL;

        PKIX_ENTER(DEFAULTREVOCATIONCHECKER,
                    "pkix_DefaultRevChecker_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a DefaultRevocationChecker */
        PKIX_CHECK(pkix_CheckType
                (object, PKIX_DEFAULTREVOCATIONCHECKER_TYPE, plContext),
                PKIX_OBJECTNOTDEFAULTREVOCATIONCHECKER);

        revChecker = (PKIX_DefaultRevocationChecker *)object;

        PKIX_DECREF(revChecker->certChainChecker);
        PKIX_DECREF(revChecker->certStores);
        PKIX_DECREF(revChecker->testDate);
        PKIX_DECREF(revChecker->trustedPubKey);

cleanup:

        PKIX_RETURN(DEFAULTREVOCATIONCHECKER);
}

/*
 * FUNCTION: pkix_DefaultRevocationChecker_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_DEFAULTREVOCATIONCHECKER_TYPE and its related functions
 *  with systemClasses[]
 *
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_DefaultRevocationChecker_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(DEFAULTREVOCATIONCHECKER,
                    "pkix_DefaultRevocationChecker_RegisterSelf");

        entry.description = "DefaultRevocationChecker";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_DefaultRevocationChecker);
        entry.destructor = pkix_DefaultRevChecker_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_DEFAULTREVOCATIONCHECKER_TYPE] = entry;

        PKIX_RETURN(DEFAULTREVOCATIONCHECKER);
}

/*
 * FUNCTION: pkix_DefaultRevChecker_Create
 *
 * DESCRIPTION:
 *  This function uses the List of certStores given by "certStores", the Date
 *  given by "testDate", the PublicKey given by "trustedPubKey", and the number
 *  of certs remaining in the chain given by "certsRemaining" to create a
 *  DefaultRevocationChecker, which is stored at "pRevChecker".
 *
 * PARAMETERS
 *  "certStores"
 *      Address of CertStore List to be stored in state. Must be non-NULL.
 *  "testDate"
 *      Address of PKIX_PL_Date to be checked. May be NULL.
 *  "trustedPubKey"
 *      Address of Public Key of Trust Anchor. Must be non-NULL.
 *  "certsRemaining"
 *      Number of certificates remaining in the chain.
 *  "pRevChecker"
 *      Address of DefaultRevocationChecker that is returned. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a DefaultRevocationChecker Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error
 */
static PKIX_Error *
pkix_DefaultRevChecker_Create(
        PKIX_List *certStores,
        PKIX_PL_Date *testDate,
        PKIX_PL_PublicKey *trustedPubKey,
        PKIX_UInt32 certsRemaining,
        PKIX_DefaultRevocationChecker **pRevChecker,
        void *plContext)
{
        PKIX_DefaultRevocationChecker *revChecker = NULL;

        PKIX_ENTER(DEFAULTREVOCATIONCHECKER, "pkix_DefaultRevChecker_Create");
        PKIX_NULLCHECK_THREE(certStores, trustedPubKey, pRevChecker);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_DEFAULTREVOCATIONCHECKER_TYPE,
                sizeof (PKIX_DefaultRevocationChecker),
                (PKIX_PL_Object **)&revChecker,
                plContext),
                PKIX_COULDNOTCREATEDEFAULTREVOCATIONCHECKEROBJECT);

        /* Initialize fields */

        revChecker->certChainChecker = NULL;
        revChecker->check = NULL;

        PKIX_INCREF(certStores);
        revChecker->certStores = certStores;

        PKIX_INCREF(testDate);
        revChecker->testDate = testDate;

        PKIX_INCREF(trustedPubKey);
        revChecker->trustedPubKey = trustedPubKey;

        revChecker->certsRemaining = certsRemaining;

        *pRevChecker = revChecker;
        revChecker = NULL;

cleanup:

        PKIX_DECREF(revChecker);

        PKIX_RETURN(DEFAULTREVOCATIONCHECKER);
}

/* --Private-DefaultRevChecker-Functions------------------------------------ */

/*
 * FUNCTION: pkix_DefaultRevChecker_Check
 *
 * DESCRIPTION:
 *  Check if the Cert has been revoked based on the CRLs data.  This function
 *  maintains the checker state to be current.
 *
 * PARAMETERS
 *  "checkerContext"
 *      Address of RevocationCheckerContext which has the state data.
 *      Must be non-NULL.
 *  "cert"
 *      Address of Certificate that is to be validated. Must be non-NULL.
 *  "procParams"
 *      Address of ProcessingParams used to initialize the ExpirationChecker
 *      and TargetCertChecker. Must be non-NULL.
 *  "pNBIOContext"
 *      Address at which platform-dependent non-blocking I/O context is stored.
 *      Must be non-NULL.
 *  "pResultCode"
 *      Address where revocation status will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Not Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a RevocationChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error
 */
static PKIX_Error *
pkix_DefaultRevChecker_Check(
        PKIX_PL_Object *checkerContext,
        PKIX_PL_Cert *cert,
        PKIX_ProcessingParams *procParams,
        void **pNBIOContext,
        PKIX_UInt32 *pReasonCode,
        void *plContext)
{
        PKIX_DefaultRevocationChecker *defaultRevChecker = NULL;
        PKIX_CertChainChecker *crlChecker = NULL;
        PKIX_PL_Object *crlCheckerState = NULL;
        PKIX_CertChainChecker_CheckCallback check = NULL;
        void *nbioContext = NULL;

        PKIX_ENTER(REVOCATIONCHECKER, "pkix_DefaultRevChecker_Check");
        PKIX_NULLCHECK_FOUR(checkerContext, cert, pNBIOContext, pReasonCode);

        /* Check that this object is a DefaultRevocationChecker */
        PKIX_CHECK(pkix_CheckType
                ((PKIX_PL_Object *)checkerContext,
                PKIX_DEFAULTREVOCATIONCHECKER_TYPE,
                plContext),
                PKIX_OBJECTNOTDEFAULTREVOCATIONCHECKER);

        defaultRevChecker = (PKIX_DefaultRevocationChecker *)checkerContext;

        nbioContext = *pNBIOContext;
        *pNBIOContext = 0;
        *pReasonCode = 0;

        /*
         * If we haven't yet created a defaultCrlChecker to do the actual work,
         * create one now.
         */
        if (defaultRevChecker->certChainChecker == NULL) {
                PKIX_Boolean nistCRLPolicyEnabled = PR_TRUE;
                if (procParams) {
                    PKIX_CHECK(
                        pkix_ProcessingParams_GetNISTRevocationPolicyEnabled
                        (procParams, &nistCRLPolicyEnabled, plContext),
                        PKIX_PROCESSINGPARAMSGETNISTREVPOLICYENABLEDFAILED);
                }

                PKIX_CHECK(pkix_DefaultCRLChecker_Initialize
                        (defaultRevChecker->certStores,
                        defaultRevChecker->testDate,
                        defaultRevChecker->trustedPubKey,
                        defaultRevChecker->certsRemaining,
                        nistCRLPolicyEnabled,
                        &crlChecker,
                        plContext),
                        PKIX_DEFAULTCRLCHECKERINITIALIZEFAILED);

                PKIX_CHECK(PKIX_CertChainChecker_GetCheckCallback
                        (crlChecker, &check, plContext),
                        PKIX_CERTCHAINCHECKERGETCHECKCALLBACKFAILED);

                defaultRevChecker->certChainChecker = crlChecker;
                defaultRevChecker->check = check;
        }

        /*
         * The defaultCRLChecker, which we are using, wants a CRLSelector
         * (in its state) to select the Issuer of the target Cert.
         */
        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                (defaultRevChecker->certChainChecker,
                &crlCheckerState,
                plContext),
                PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        PKIX_CHECK(pkix_CheckType
                (crlCheckerState, PKIX_DEFAULTCRLCHECKERSTATE_TYPE, plContext),
                PKIX_OBJECTNOTDEFAULTCRLCHECKERSTATE);

        /* Set up CRLSelector */
        PKIX_CHECK(pkix_DefaultCRLChecker_Check_SetSelector
                (cert,
                (pkix_DefaultCRLCheckerState *)crlCheckerState,
                plContext),
                PKIX_DEFAULTCRLCHECKERCHECKSETSELECTORFAILED);

        PKIX_CHECK
                (PKIX_CertChainChecker_SetCertChainCheckerState
                (defaultRevChecker->certChainChecker,
                crlCheckerState,
                plContext),
                PKIX_CERTCHAINCHECKERSETCERTCHAINCHECKERSTATEFAILED);

        PKIX_CHECK(defaultRevChecker->check
                (defaultRevChecker->certChainChecker,
                cert,
                NULL,
                &nbioContext,
                plContext),
                PKIX_CERTCHAINCHECKERCHECKCALLBACKFAILED);

        *pNBIOContext = nbioContext;

cleanup:

        PKIX_DECREF(crlCheckerState);

        PKIX_RETURN(REVOCATIONCHECKER);
}

/*
 * FUNCTION: pkix_DefaultRevChecker_Initialize
 *
 * DESCRIPTION:
 *  Create a CertChainChecker with DefaultRevChecker.
 *
 * PARAMETERS
 *  "certStores"
 *      Address of CertStore List to be stored in state. Must be non-NULL.
 *  "testDate"
 *      Address of PKIX_PL_Date to be checked. May be NULL.
 *  "trustedPubKey"
 *      Address of Public Key of Trust Anchor. Must be non-NULL.
 *  "certsRemaining"
 *      Number of certificates remaining in the chain.
 *  "pChecker"
 *      Address where object pointer will be stored. Must be non-NULL.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error
 */
PKIX_Error *
pkix_DefaultRevChecker_Initialize(
        PKIX_List *certStores,
        PKIX_PL_Date *testDate,
        PKIX_PL_PublicKey *trustedPubKey,
        PKIX_UInt32 certsRemaining,
        PKIX_RevocationChecker **pChecker,
        void *plContext)
{
        PKIX_DefaultRevocationChecker *revChecker = NULL;

        PKIX_ENTER(REVOCATIONCHECKER, "pkix_DefaultRevChecker_Initialize");
        PKIX_NULLCHECK_TWO(certStores, pChecker);

        PKIX_CHECK(pkix_DefaultRevChecker_Create
                (certStores,
                testDate,
                trustedPubKey,
                certsRemaining,
                &revChecker,
                plContext),
                PKIX_DEFAULTREVCHECKERCREATEFAILED);

        PKIX_CHECK(PKIX_RevocationChecker_Create
                (pkix_DefaultRevChecker_Check,
                (PKIX_PL_Object *)revChecker,
                pChecker,
                plContext),
                PKIX_REVOCATIONCHECKERCREATEFAILED);

cleanup:

        PKIX_DECREF(revChecker);

        PKIX_RETURN(REVOCATIONCHECKER);
}
