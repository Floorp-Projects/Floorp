/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_ocspcertid.c
 *
 * Certificate ID Object for OCSP
 *
 */

#include "pkix_pl_ocspcertid.h"

/* --Private-Cert-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_OcspCertID_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OcspCertID_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_OcspCertID *certID = NULL;

        PKIX_ENTER(OCSPCERTID, "pkix_pl_OcspCertID_Destroy");

        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_OCSPCERTID_TYPE, plContext),
                    PKIX_OBJECTNOTOCSPCERTID);

        certID = (PKIX_PL_OcspCertID *)object;

        if (certID->certID) {
                CERT_DestroyOCSPCertID(certID->certID);
        }

cleanup:

        PKIX_RETURN(OCSPCERTID);
}

/*
 * FUNCTION: pkix_pl_OcspCertID_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_PUBLICKEY_TYPE and its related functions 
 *  with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_OcspCertID_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(OCSPCERTID, "pkix_pl_OcspCertID_RegisterSelf");

        entry.description = "OcspCertID";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_OcspCertID);
        entry.destructor = pkix_pl_OcspCertID_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;
        systemClasses[PKIX_OCSPCERTID_TYPE] = entry;

        PKIX_RETURN(OCSPCERTID);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_OcspCertID_Create
 * DESCRIPTION:
 *
 *  This function creates an OcspCertID for a given certificate,
 *  to be used with OCSP transactions.
 *
 *  If a Date is provided in "validity" it may be used in the search for the
 *  issuer of "cert" but has no effect on the request itself.
 *
 * PARAMETERS:
 *  "cert"
 *     Address of the Cert for which an OcspCertID is to be created. Must be
 *     non-NULL.
 *  "validity"
 *     Address of the Date for which the Cert's validity is to be determined.
 *     May be NULL.
 *  "object"
 *     Address at which the result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspCertID Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_PL_OcspCertID_Create(
        PKIX_PL_Cert *cert,
        PKIX_PL_Date *validity,
        PKIX_PL_OcspCertID **object,
        void *plContext)
{
        PKIX_PL_OcspCertID *cid = NULL;
        int64 time = 0;

        PKIX_ENTER(DATE, "PKIX_PL_OcspCertID_Create");
        PKIX_NULLCHECK_TWO(cert, object);
    
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_OCSPCERTID_TYPE,
                    sizeof (PKIX_PL_OcspCertID),
                    (PKIX_PL_Object **)&cid,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        if (validity != NULL) {
                PKIX_CHECK(pkix_pl_Date_GetPRTime(validity, &time, plContext),
                        PKIX_DATEGETPRTIMEFAILED);
        } else {
                time = PR_Now();
        }

        cid->certID = CERT_CreateOCSPCertID(cert->nssCert, time);
        if (!cid->certID) {
                PKIX_ERROR(PKIX_COULDNOTCREATEOBJECT);
        }

        *object = cid;
        cid = NULL;
cleanup:
        PKIX_DECREF(cid);
        PKIX_RETURN(OCSPCERTID);
}

/*
 * FUNCTION: PKIX_PL_OcspCertID_GetFreshCacheStatus
 * DESCRIPTION:
 *
 *  This function may return cached OCSP results for the provided
 *  certificate, but only if stored information is still considered to be
 *  fresh.
 *
 * PARAMETERS
 *  "cid"
 *      A certificate ID as used by OCSP
 *  "validity"
 *      Optional date parameter to request validity for a specifc time.
 *  "hasFreshStatus"
 *      Output parameter, if the function successed to find fresh cached
 *      information, this will be set to true. Must be non-NULL.
 *  "statusIsGood"
 *      The good/bad result stored in the cache. Must be non-NULL.
 *  "missingResponseError"
 *      If OCSP status is "bad", this variable may indicate the exact
 *      reason why the previous OCSP request had failed.
 *  "plContext"
 *      Platform-specific context pointer.
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspCertID Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_PL_OcspCertID_GetFreshCacheStatus(
        PKIX_PL_OcspCertID *cid, 
        PKIX_PL_Date *validity,
        PKIX_Boolean *hasFreshStatus,
        PKIX_Boolean *statusIsGood,
        SECErrorCodes *missingResponseError,
        void *plContext)
{
        int64 time = 0;
        SECStatus rv;
        SECStatus rvOcsp;

        PKIX_ENTER(DATE, "PKIX_PL_OcspCertID_GetFreshCacheStatus");
        PKIX_NULLCHECK_THREE(cid, hasFreshStatus, statusIsGood);

        if (validity != NULL) {
                PKIX_CHECK(pkix_pl_Date_GetPRTime(validity, &time, plContext),
                        PKIX_DATEGETPRTIMEFAILED);
        } else {
                time = PR_Now();
        }

        rv = ocsp_GetCachedOCSPResponseStatusIfFresh(
                cid->certID, time, PR_TRUE, /*ignoreGlobalOcspFailureSetting*/
                &rvOcsp, missingResponseError);

        *hasFreshStatus = (rv == SECSuccess);
        if (*hasFreshStatus) {
                *statusIsGood = (rvOcsp == SECSuccess);
        }
cleanup:
        PKIX_RETURN(OCSPCERTID);
}

/*
 * FUNCTION: PKIX_PL_OcspCertID_RememberOCSPProcessingFailure
 * DESCRIPTION:
 *
 *  Information about the current failure associated to the given certID
 *  will be remembered in the cache, potentially allowing future calls
 *  to prevent repetitive OCSP requests.
 *  After this function got called, it may no longer be safe to
 *  use the provided cid parameter, because ownership might have been
 *  transfered to the cache. This status will be recorded inside the
 *  cid object.
 *
 * PARAMETERS
 *  "cid"
 *      The certificate ID associated to a failed OCSP processing.
 *  "plContext"
 *      Platform-specific context pointer.
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspCertID Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_PL_OcspCertID_RememberOCSPProcessingFailure(
        PKIX_PL_OcspCertID *cid, 
        void *plContext)
{
        PRBool certIDWasConsumed = PR_FALSE;

        PKIX_ENTER(DATE, "PKIX_PL_OcspCertID_RememberOCSPProcessingFailure");
        PKIX_NULLCHECK_TWO(cid, cid->certID);

        cert_RememberOCSPProcessingFailure(cid->certID, &certIDWasConsumed);

        if (certIDWasConsumed) {
                cid->certID = NULL;
        }

        PKIX_RETURN(OCSPCERTID);
}

