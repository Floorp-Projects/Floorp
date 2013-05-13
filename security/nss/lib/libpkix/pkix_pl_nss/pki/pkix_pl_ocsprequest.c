/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_ocsprequest.c
 *
 */

#include "pkix_pl_ocsprequest.h"

/* --Private-OcspRequest-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_OcspRequest_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OcspRequest_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_OcspRequest *ocspReq = NULL;

        PKIX_ENTER(OCSPREQUEST, "pkix_pl_OcspRequest_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_OCSPREQUEST_TYPE, plContext),
                    PKIX_OBJECTNOTOCSPREQUEST);

        ocspReq = (PKIX_PL_OcspRequest *)object;

        if (ocspReq->decoded != NULL) {
                CERT_DestroyOCSPRequest(ocspReq->decoded);
        }

        if (ocspReq->encoded != NULL) {
                SECITEM_FreeItem(ocspReq->encoded, PR_TRUE);
        }

        if (ocspReq->location != NULL) {
                PORT_Free(ocspReq->location);
        }

        PKIX_DECREF(ocspReq->cert);
        PKIX_DECREF(ocspReq->validity);
        PKIX_DECREF(ocspReq->signerCert);

cleanup:

        PKIX_RETURN(OCSPREQUEST);
}

/*
 * FUNCTION: pkix_pl_OcspRequest_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OcspRequest_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_UInt32 certHash = 0;
        PKIX_UInt32 dateHash = 0;
        PKIX_UInt32 extensionHash = 0;
        PKIX_UInt32 signerHash = 0;
        PKIX_PL_OcspRequest *ocspRq = NULL;

        PKIX_ENTER(OCSPREQUEST, "pkix_pl_OcspRequest_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_OCSPREQUEST_TYPE, plContext),
                    PKIX_OBJECTNOTOCSPREQUEST);

        ocspRq = (PKIX_PL_OcspRequest *)object;

        *pHashcode = 0;

        PKIX_HASHCODE(ocspRq->cert, &certHash, plContext,
                PKIX_CERTHASHCODEFAILED);

        PKIX_HASHCODE(ocspRq->validity, &dateHash, plContext,
                PKIX_DATEHASHCODEFAILED);

        if (ocspRq->addServiceLocator == PKIX_TRUE) {
                extensionHash = 0xff;
        }

        PKIX_HASHCODE(ocspRq->signerCert, &signerHash, plContext,
                PKIX_CERTHASHCODEFAILED);

        *pHashcode = (((((extensionHash << 8) || certHash) << 8) ||
                dateHash) << 8) || signerHash;

cleanup:

        PKIX_RETURN(OCSPREQUEST);

}

/*
 * FUNCTION: pkix_pl_OcspRequest_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OcspRequest_Equals(
        PKIX_PL_Object *firstObj,
        PKIX_PL_Object *secondObj,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_Boolean match = PKIX_FALSE;
        PKIX_UInt32 secondType = 0;
        PKIX_PL_OcspRequest *firstReq = NULL;
        PKIX_PL_OcspRequest *secondReq = NULL;

        PKIX_ENTER(OCSPREQUEST, "pkix_pl_OcspRequest_Equals");
        PKIX_NULLCHECK_THREE(firstObj, secondObj, pResult);

        /* test that firstObj is a OcspRequest */
        PKIX_CHECK(pkix_CheckType(firstObj, PKIX_OCSPREQUEST_TYPE, plContext),
                    PKIX_FIRSTOBJARGUMENTNOTOCSPREQUEST);

        /*
         * Since we know firstObj is a OcspRequest, if both references are
         * identical, they must be equal
         */
        if (firstObj == secondObj){
                match = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObj isn't a OcspRequest, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObj, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_OCSPREQUEST_TYPE) {
                goto cleanup;
        }

        firstReq = (PKIX_PL_OcspRequest *)firstObj;
        secondReq = (PKIX_PL_OcspRequest *)secondObj;

        if (firstReq->addServiceLocator != secondReq->addServiceLocator) {
                goto cleanup;
        }

        PKIX_EQUALS(firstReq->cert, secondReq->cert, &match, plContext,
                PKIX_CERTEQUALSFAILED);

        if (match == PKIX_FALSE) {
                goto cleanup;
        }

        PKIX_EQUALS(firstReq->validity, secondReq->validity, &match, plContext,
                PKIX_DATEEQUALSFAILED);

        if (match == PKIX_FALSE) {
                goto cleanup;
        }

        PKIX_EQUALS
                (firstReq->signerCert, secondReq->signerCert, &match, plContext,
                PKIX_CERTEQUALSFAILED);

cleanup:

        *pResult = match;

        PKIX_RETURN(OCSPREQUEST);
}

/*
 * FUNCTION: pkix_pl_OcspRequest_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_OCSPREQUEST_TYPE and its related functions with
 *  systemClasses[]
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
pkix_pl_OcspRequest_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(OCSPREQUEST, "pkix_pl_OcspRequest_RegisterSelf");

        entry.description = "OcspRequest";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_OcspRequest);
        entry.destructor = pkix_pl_OcspRequest_Destroy;
        entry.equalsFunction = pkix_pl_OcspRequest_Equals;
        entry.hashcodeFunction = pkix_pl_OcspRequest_Hashcode;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_OCSPREQUEST_TYPE] = entry;

        PKIX_RETURN(OCSPREQUEST);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: pkix_pl_OcspRequest_Create
 * DESCRIPTION:
 *
 *  This function creates an OcspRequest to be used in validating the Cert
 *  pointed to by "cert" and storing the result at "pRequest". If a URI
 *  is found for an OCSP responder, PKIX_TRUE is stored at "pURIFound". If no
 *  URI is found, PKIX_FALSE is stored.
 *
 *  If a Date is provided in "validity" it may be used in the search for the
 *  issuer of "cert" but has no effect on the request itself. If
 *  "addServiceLocator" is TRUE, the AddServiceLocator extension will be
 *  included in the Request. If "signerCert" is provided it will be used to sign
 *  the Request. (Note: this signed request feature is not currently supported.)
 *
 * PARAMETERS:
 *  "cert"
 *     Address of the Cert for which an OcspRequest is to be created. Must be
 *     non-NULL.
 *  "validity"
 *     Address of the Date for which the Cert's validity is to be determined.
 *     May be NULL.
 *  "signerCert"
 *     Address of the Cert to be used, if present, in signing the request.
 *     May be NULL.
 *  "pRequest"
 *     Address at which the result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspRequest Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_OcspRequest_Create(
        PKIX_PL_Cert *cert,
        PKIX_PL_OcspCertID *cid,
        PKIX_PL_Date *validity,
        PKIX_PL_Cert *signerCert,
        PKIX_UInt32 methodFlags,
        PKIX_Boolean *pURIFound,
        PKIX_PL_OcspRequest **pRequest,
        void *plContext)
{
        PKIX_PL_OcspRequest *ocspRequest = NULL;

        CERTCertDBHandle *handle = NULL;
        SECStatus rv = SECFailure;
        SECItem *encoding = NULL;
        CERTOCSPRequest *certRequest = NULL;
        PRTime time = 0;
        PRBool addServiceLocatorExtension = PR_FALSE;
        CERTCertificate *nssCert = NULL;
        CERTCertificate *nssSignerCert = NULL;
        char *location = NULL;
        PRErrorCode locError = 0;
        PKIX_Boolean canUseDefaultSource = PKIX_FALSE;

        PKIX_ENTER(OCSPREQUEST, "pkix_pl_OcspRequest_Create");
        PKIX_NULLCHECK_TWO(cert, pRequest);

        /* create a PKIX_PL_OcspRequest object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_OCSPREQUEST_TYPE,
                    sizeof (PKIX_PL_OcspRequest),
                    (PKIX_PL_Object **)&ocspRequest,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        PKIX_INCREF(cert);
        ocspRequest->cert = cert;

        PKIX_INCREF(validity);
        ocspRequest->validity = validity;

        PKIX_INCREF(signerCert);
        ocspRequest->signerCert = signerCert;

        ocspRequest->decoded = NULL;
        ocspRequest->encoded = NULL;

        ocspRequest->location = NULL;

        nssCert = cert->nssCert;

        /*
         * Does this Cert have an Authority Information Access extension with
         * the URI of an OCSP responder?
         */
        handle = CERT_GetDefaultCertDB();

        if (!(methodFlags & PKIX_REV_M_IGNORE_IMPLICIT_DEFAULT_SOURCE)) {
            canUseDefaultSource = PKIX_TRUE;
        }
        location = ocsp_GetResponderLocation(handle, nssCert,
                                             canUseDefaultSource,
                                             &addServiceLocatorExtension);
        if (location == NULL) {
                locError = PORT_GetError();
                if (locError == SEC_ERROR_EXTENSION_NOT_FOUND ||
                    locError == SEC_ERROR_CERT_BAD_ACCESS_LOCATION) {
                    PORT_SetError(0);
                    *pURIFound = PKIX_FALSE;
                    goto cleanup;
                }
                PKIX_ERROR(PKIX_ERRORFINDINGORPROCESSINGURI);
        }

        ocspRequest->location = location;
        *pURIFound = PKIX_TRUE;

        if (signerCert != NULL) {
                nssSignerCert = signerCert->nssCert;
        }

        if (validity != NULL) {
		PKIX_CHECK(pkix_pl_Date_GetPRTime(validity, &time, plContext),
			PKIX_DATEGETPRTIMEFAILED);
        } else {
                time = PR_Now();
	}

        certRequest = cert_CreateSingleCertOCSPRequest(
                cid->certID, cert->nssCert, time, 
                addServiceLocatorExtension, nssSignerCert);

        ocspRequest->decoded = certRequest;

        if (certRequest == NULL) {
                PKIX_ERROR(PKIX_UNABLETOCREATECERTOCSPREQUEST);
        }

        rv = CERT_AddOCSPAcceptableResponses(
                certRequest, SEC_OID_PKIX_OCSP_BASIC_RESPONSE);

        if (rv == SECFailure) {
                PKIX_ERROR(PKIX_UNABLETOADDACCEPTABLERESPONSESTOREQUEST);
        }

        encoding = CERT_EncodeOCSPRequest(NULL, certRequest, NULL);

        ocspRequest->encoded = encoding;

        *pRequest = ocspRequest;
        ocspRequest = NULL;

cleanup:
        PKIX_DECREF(ocspRequest);

        PKIX_RETURN(OCSPREQUEST);
}

/*
 * FUNCTION: pkix_pl_OcspRequest_GetEncoded
 * DESCRIPTION:
 *
 *  This function obtains the encoded message from the OcspRequest pointed to
 *  by "request", storing the result at "pRequest".
 *
 * PARAMETERS
 *  "request"
 *      The address of the OcspRequest whose encoded message is to be
 *      retrieved. Must be non-NULL.
 *  "pRequest"
 *      The address at which is stored the address of the encoded message. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_OcspRequest_GetEncoded(
        PKIX_PL_OcspRequest *request,
        SECItem **pRequest,
        void *plContext)
{
        PKIX_ENTER(OCSPREQUEST, "pkix_pl_OcspRequest_GetEncoded");
        PKIX_NULLCHECK_TWO(request, pRequest);

        *pRequest = request->encoded;

        PKIX_RETURN(OCSPREQUEST);
}

/*
 * FUNCTION: pkix_pl_OcspRequest_GetLocation
 * DESCRIPTION:
 *
 *  This function obtains the location from the OcspRequest pointed to
 *  by "request", storing the result at "pLocation".
 *
 * PARAMETERS
 *  "request"
 *      The address of the OcspRequest whose encoded message is to be
 *      retrieved. Must be non-NULL.
 *  "pLocation"
 *      The address at which is stored the address of the location. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_OcspRequest_GetLocation(
        PKIX_PL_OcspRequest *request,
        char **pLocation,
        void *plContext)
{
        PKIX_ENTER(OCSPREQUEST, "pkix_pl_OcspRequest_GetLocation");
        PKIX_NULLCHECK_TWO(request, pLocation);

        *pLocation = request->location;

        PKIX_RETURN(OCSPREQUEST);
}
