/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_crl.c
 *
 * CRL Function Definitions
 *
 */

#include "pkix_pl_crl.h"
#include "certxutl.h"

extern PKIX_PL_HashTable *cachedCrlSigTable;

/* --Private-CRL-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_CRL_GetVersion
 * DESCRIPTION:
 *
 *  Retrieves the version of the CRL pointed to by "crl" and stores it at
 *  "pVersion". The version number will either be 0 or 1 (corresponding to
 *  v1 or v2, respectively).
 *
 *  Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
 *
 * PARAMETERS:
 *  "crl"
 *      Address of CRL whose version is to be stored. Must be non-NULL.
 *  "pVersion"
 *      Address where a version will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRL Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CRL_GetVersion(
        PKIX_PL_CRL *crl,
        PKIX_UInt32 *pVersion,
        void *plContext)
{
        PKIX_UInt32 myVersion;

        PKIX_ENTER(CRL, "pkix_pl_CRL_GetVersion");
        PKIX_NULLCHECK_THREE(crl, crl->nssSignedCrl, pVersion);
      
        PKIX_NULLCHECK_ONE(crl->nssSignedCrl->crl.version.data);

        myVersion = *(crl->nssSignedCrl->crl.version.data);

        if (myVersion > 1) {
                PKIX_ERROR(PKIX_VERSIONVALUEMUSTBEV1ORV2);
        }

        *pVersion = myVersion;

cleanup:

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: PKIX_PL_CRL_GetCRLNumber (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_CRL_GetCRLNumber(
        PKIX_PL_CRL *crl,
        PKIX_PL_BigInt **pCrlNumber,
        void *plContext)
{
        PKIX_PL_BigInt *crlNumber = NULL;
        SECItem nssCrlNumber;
        PLArenaPool *arena = NULL;
        SECStatus status;
        PKIX_UInt32 length = 0;
        char *bytes = NULL;

        PKIX_ENTER(CRL, "PKIX_PL_CRL_GetCRLNumber");
        PKIX_NULLCHECK_THREE(crl, crl->nssSignedCrl, pCrlNumber);

        /* Can call this function only with der been adopted. */
        PORT_Assert(crl->adoptedDerCrl);

        if (!crl->crlNumberAbsent && crl->crlNumber == NULL) {

            PKIX_OBJECT_LOCK(crl);

            if (!crl->crlNumberAbsent && crl->crlNumber == NULL) {

                nssCrlNumber.type = 0;
                nssCrlNumber.len = 0;
                nssCrlNumber.data = NULL;

                PKIX_CRL_DEBUG("\t\tCalling PORT_NewArena).\n");
                arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                if (arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }

                PKIX_CRL_DEBUG("\t\tCalling CERT_FindCRLNumberExten\n");
                status = CERT_FindCRLNumberExten
                        (arena, &crl->nssSignedCrl->crl, &nssCrlNumber);

                if (status == SECSuccess) {
                        /* Get data in bytes then convert to bigint */
                        length = nssCrlNumber.len;
                        bytes = (char *)nssCrlNumber.data;

                        PKIX_CHECK(pkix_pl_BigInt_CreateWithBytes
                                    (bytes, length, &crlNumber, plContext),
                                    PKIX_BIGINTCREATEWITHBYTESFAILED);

                        /* arena release does the job 
                        PKIX_CRL_DEBUG("\t\tCalling SECITEM_FreeItem\n");
                        SECITEM_FreeItem(&nssCrlNumber, PKIX_FALSE);
                        */
                        crl->crlNumber = crlNumber;

                } else {

                        crl->crlNumberAbsent = PKIX_TRUE;
                }
            }

            PKIX_OBJECT_UNLOCK(crl);

        }

        PKIX_INCREF(crl->crlNumber);

        *pCrlNumber = crl->crlNumber;

cleanup:

        if (arena){
                PKIX_CRL_DEBUG("\t\tCalling PORT_FreeArena).\n");
                PORT_FreeArena(arena, PR_FALSE);
        }

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_GetSignatureAlgId
 *
 * DESCRIPTION:
 *  Retrieves a pointer to the OID that represents the signature algorithm of
 *  the CRL pointed to by "crl" and stores it at "pSignatureAlgId".
 *
 *  AlgorithmIdentifier  ::=  SEQUENCE  {
 *      algorithm               OBJECT IDENTIFIER,
 *      parameters              ANY DEFINED BY algorithm OPTIONAL  }
 *
 * PARAMETERS:
 *  "crl"
 *      Address of CRL whose signature algorithm OID is to be stored.
 *      Must be non-NULL.
 *  "pSignatureAlgId"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRL Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CRL_GetSignatureAlgId(
        PKIX_PL_CRL *crl,
        PKIX_PL_OID **pSignatureAlgId,
        void *plContext)
{
        PKIX_PL_OID *signatureAlgId = NULL;

        PKIX_ENTER(CRL, "pkix_pl_CRL_GetSignatureAlgId");
        PKIX_NULLCHECK_THREE(crl, crl->nssSignedCrl, pSignatureAlgId);

        /* if we don't have a cached copy from before, we create one */
        if (crl->signatureAlgId == NULL){
                PKIX_OBJECT_LOCK(crl);
                if (crl->signatureAlgId == NULL){
                    CERTCrl *nssCrl = &(crl->nssSignedCrl->crl);
                    SECAlgorithmID *algorithm = &nssCrl->signatureAlg;
                    SECItem *algBytes = &algorithm->algorithm;

                    if (!algBytes->data || !algBytes->len) {
                        PKIX_ERROR(PKIX_OIDBYTESLENGTH0);
                    }
                    PKIX_CHECK(PKIX_PL_OID_CreateBySECItem
                               (algBytes, &signatureAlgId, plContext),
                               PKIX_OIDCREATEFAILED);
                    
                    /* save a cached copy in case it is asked for again */
                    crl->signatureAlgId = signatureAlgId;
                    signatureAlgId = NULL;
                }
                PKIX_OBJECT_UNLOCK(crl);
        }
        PKIX_INCREF(crl->signatureAlgId);
        *pSignatureAlgId = crl->signatureAlgId;
cleanup:
        PKIX_DECREF(signatureAlgId);
        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_GetCRLEntries
 * DESCRIPTION:
 *
 *  Retrieves a pointer to the List of CRLEntries found in the CRL pointed to
 *  by "crl" and stores it at "pCRLEntries". If there are no CRLEntries,
 *  this functions stores NULL at "pCRLEntries".
 *
 * PARAMETERS:
 *  "crl"
 *      Address of CRL whose CRL Entries are to be retrieved. Must be non-NULL.
 *  "pCRLEntries"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRL Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CRL_GetCRLEntries(
        PKIX_PL_CRL *crl,
        PKIX_List **pCrlEntries,
        void *plContext)
{
        PKIX_List *entryList = NULL;
        CERTCrl *nssCrl = NULL;

        PKIX_ENTER(CRL, "pkix_pl_CRL_GetCRLEntries");
        PKIX_NULLCHECK_THREE(crl, crl->nssSignedCrl, pCrlEntries);

        /* if we don't have a cached copy from before, we create one */
        if (crl->crlEntryList == NULL) {

                PKIX_OBJECT_LOCK(crl);

                if (crl->crlEntryList == NULL){

                        nssCrl = &(crl->nssSignedCrl->crl);

                        PKIX_CHECK(pkix_pl_CRLEntry_Create
                                    (nssCrl->entries, &entryList, plContext),
                                    PKIX_CRLENTRYCREATEFAILED);

                        PKIX_CHECK(PKIX_List_SetImmutable
                                    (entryList, plContext),
                                    PKIX_LISTSETIMMUTABLEFAILED);

                        crl->crlEntryList = entryList;
                }

                PKIX_OBJECT_UNLOCK(crl);

        }

        PKIX_INCREF(crl->crlEntryList);

        *pCrlEntries = crl->crlEntryList;

cleanup:

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CRL_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_CRL *crl = NULL;

        PKIX_ENTER(CRL, "pkix_pl_CRL_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRL_TYPE, plContext),
                    PKIX_OBJECTNOTCRL);

        crl = (PKIX_PL_CRL*)object;

        PKIX_CRL_DEBUG("\t\tCalling CERT_DestroyCrl\n");
        if (crl->nssSignedCrl) {
            CERT_DestroyCrl(crl->nssSignedCrl);
        }
        if (crl->adoptedDerCrl) {
            SECITEM_FreeItem(crl->adoptedDerCrl, PR_TRUE);
        }
        crl->nssSignedCrl = NULL;
        crl->adoptedDerCrl = NULL;
        crl->crlNumberAbsent = PKIX_FALSE;

        PKIX_DECREF(crl->issuer);
        PKIX_DECREF(crl->signatureAlgId);
        PKIX_DECREF(crl->crlNumber);
        PKIX_DECREF(crl->crlEntryList);
        PKIX_DECREF(crl->critExtOids);
        if (crl->derGenName) {
            SECITEM_FreeItem(crl->derGenName, PR_TRUE);
        }

cleanup:

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the CRL pointed
 *  to by "crl" and stores it at "pString".
 *
 * PARAMETERS
 *  "crl"
 *      Address of CRL whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRL Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CRL_ToString_Helper(
        PKIX_PL_CRL *crl,
        PKIX_PL_String **pString,
        void *plContext)
{
        char *asciiFormat = NULL;
        PKIX_UInt32 crlVersion;
        PKIX_PL_X500Name *crlIssuer = NULL;
        PKIX_PL_OID *nssSignatureAlgId = NULL;
        PKIX_PL_BigInt *crlNumber = NULL;
        PKIX_List *crlEntryList = NULL;
        PKIX_List *critExtOIDs = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *crlIssuerString = NULL;
        PKIX_PL_String *lastUpdateString = NULL;
        PKIX_PL_String *nextUpdateString = NULL;
        PKIX_PL_String *nssSignatureAlgIdString = NULL;
        PKIX_PL_String *crlNumberString = NULL;
        PKIX_PL_String *crlEntryListString = NULL;
        PKIX_PL_String *critExtOIDsString = NULL;
        PKIX_PL_String *crlString = NULL;

        PKIX_ENTER(CRL, "pkix_pl_CRL_ToString_Helper");
        PKIX_NULLCHECK_THREE(crl, crl->nssSignedCrl, pString);

        asciiFormat =
                "[\n"
                "\tVersion:         v%d\n"
                "\tIssuer:          %s\n"
                "\tUpdate:   [Last: %s\n"
                "\t           Next: %s]\n"
                "\tSignatureAlgId:  %s\n"
                "\tCRL Number     : %s\n"
                "\n"
                "\tEntry List:      %s\n"
                "\n"
                "\tCritExtOIDs:     %s\n"
                "]\n";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        /* Version */
        PKIX_CHECK(pkix_pl_CRL_GetVersion(crl, &crlVersion, plContext),
                    PKIX_CRLGETVERSIONFAILED);

        /* Issuer */
        PKIX_CHECK(PKIX_PL_CRL_GetIssuer(crl, &crlIssuer, plContext),
                    PKIX_CRLGETISSUERFAILED);

        PKIX_CHECK(PKIX_PL_Object_ToString
                    ((PKIX_PL_Object *)crlIssuer, &crlIssuerString, plContext),
                    PKIX_X500NAMETOSTRINGFAILED);

        /* This update - No Date object created, use nss data directly */
        PKIX_CHECK(pkix_pl_Date_ToString_Helper
                    (&(crl->nssSignedCrl->crl.lastUpdate),
                    &lastUpdateString,
                    plContext),
                    PKIX_DATETOSTRINGHELPERFAILED);

        /* Next update - No Date object created, use nss data directly */
        PKIX_CHECK(pkix_pl_Date_ToString_Helper
                    (&(crl->nssSignedCrl->crl.nextUpdate),
                    &nextUpdateString,
                    plContext),
                    PKIX_DATETOSTRINGHELPERFAILED);

        /* Signature Algorithm Id */
        PKIX_CHECK(pkix_pl_CRL_GetSignatureAlgId
                    (crl, &nssSignatureAlgId, plContext),
                    PKIX_CRLGETSIGNATUREALGIDFAILED);

        PKIX_CHECK(PKIX_PL_Object_ToString
                    ((PKIX_PL_Object *)nssSignatureAlgId,
                    &nssSignatureAlgIdString,
                    plContext),
                    PKIX_OIDTOSTRINGFAILED);

        /* CRL Number */
        PKIX_CHECK(PKIX_PL_CRL_GetCRLNumber
                    (crl, &crlNumber, plContext),
                    PKIX_CRLGETCRLNUMBERFAILED);

        PKIX_TOSTRING(crlNumber, &crlNumberString, plContext,
                    PKIX_BIGINTTOSTRINGFAILED);

        /* CRL Entries */
        PKIX_CHECK(pkix_pl_CRL_GetCRLEntries(crl, &crlEntryList, plContext),
                    PKIX_CRLGETCRLENTRIESFAILED);

        PKIX_TOSTRING(crlEntryList, &crlEntryListString, plContext,
                    PKIX_LISTTOSTRINGFAILED);

        /* CriticalExtensionOIDs */
        PKIX_CHECK(PKIX_PL_CRL_GetCriticalExtensionOIDs
                    (crl, &critExtOIDs, plContext),
                    PKIX_CRLGETCRITICALEXTENSIONOIDSFAILED);

        PKIX_TOSTRING(critExtOIDs, &critExtOIDsString, plContext,
                    PKIX_LISTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&crlString,
                    plContext,
                    formatString,
                    crlVersion + 1,
                    crlIssuerString,
                    lastUpdateString,
                    nextUpdateString,
                    nssSignatureAlgIdString,
                    crlNumberString,
                    crlEntryListString,
                    critExtOIDsString),
                    PKIX_SPRINTFFAILED);

        *pString = crlString;

cleanup:

        PKIX_DECREF(crlIssuer);
        PKIX_DECREF(nssSignatureAlgId);
        PKIX_DECREF(crlNumber);
        PKIX_DECREF(crlEntryList);
        PKIX_DECREF(critExtOIDs);
        PKIX_DECREF(crlIssuerString);
        PKIX_DECREF(lastUpdateString);
        PKIX_DECREF(nextUpdateString);
        PKIX_DECREF(nssSignatureAlgIdString);
        PKIX_DECREF(crlNumberString);
        PKIX_DECREF(crlEntryListString);
        PKIX_DECREF(critExtOIDsString);
        PKIX_DECREF(formatString);

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CRL_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *crlString = NULL;
        PKIX_PL_CRL *crl = NULL;

        PKIX_ENTER(CRL, "pkix_pl_CRL_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRL_TYPE, plContext),
                    PKIX_OBJECTNOTCRL);

        crl = (PKIX_PL_CRL *) object;

        PKIX_CHECK(pkix_pl_CRL_ToString_Helper(crl, &crlString, plContext),
                    PKIX_CRLTOSTRINGHELPERFAILED);

        *pString = crlString;

cleanup:

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CRL_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_CRL *crl = NULL;
        PKIX_UInt32 certHash;
        SECItem *crlDer = NULL;
        
        PKIX_ENTER(CRL, "pkix_pl_CRL_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRL_TYPE, plContext),
                    PKIX_OBJECTNOTCRL);

        crl = (PKIX_PL_CRL *)object;
        if (crl->adoptedDerCrl) {
            crlDer = crl->adoptedDerCrl;
        } else if (crl->nssSignedCrl && crl->nssSignedCrl->derCrl) { 
            crlDer = crl->nssSignedCrl->derCrl;
        }
        if (!crlDer || !crlDer->data) {
            PKIX_ERROR(PKIX_CANNOTAQUIRECRLDER);
        }

        PKIX_CHECK(pkix_hash(crlDer->data, crlDer->len,
                             &certHash, plContext),
                   PKIX_ERRORINHASH);

        *pHashcode = certHash;

cleanup:

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CRL_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_CRL *firstCrl = NULL;
        PKIX_PL_CRL *secondCrl = NULL;
        SECItem *crlDerOne = NULL, *crlDerTwo = NULL;
        PKIX_UInt32 secondType;

        PKIX_ENTER(CRL, "pkix_pl_CRL_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a CRL */
        PKIX_CHECK(pkix_CheckType(firstObject, PKIX_CRL_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTCRL);

        firstCrl = (PKIX_PL_CRL *)firstObject;
        secondCrl = (PKIX_PL_CRL *)secondObject;

        /*
         * Since we know firstObject is a CRL, if both references are
         * identical, they must be equal
         */
        if (firstCrl == secondCrl){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondCrl isn't a CRL, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    ((PKIX_PL_Object *)secondCrl, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_CRL_TYPE) goto cleanup;

        if (firstCrl->adoptedDerCrl) {
            crlDerOne = firstCrl->adoptedDerCrl;
        } else if (firstCrl->nssSignedCrl && firstCrl->nssSignedCrl->derCrl) {
            crlDerOne = firstCrl->nssSignedCrl->derCrl;
        }

        if (secondCrl->adoptedDerCrl) {
            crlDerTwo = secondCrl->adoptedDerCrl;
        } else if (secondCrl->nssSignedCrl && secondCrl->nssSignedCrl->derCrl) {
            crlDerTwo = secondCrl->nssSignedCrl->derCrl;
        }

        if (SECITEM_CompareItem(crlDerOne, crlDerTwo) == SECEqual) {
            *pResult = PKIX_TRUE;
        }

cleanup:

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_CRL_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_CRL_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry *entry = &systemClasses[PKIX_CRL_TYPE];

        PKIX_ENTER(CRL, "pkix_pl_CRL_RegisterSelf");

        entry->description = "CRL";
        entry->typeObjectSize = sizeof(PKIX_PL_CRL);
        entry->destructor = pkix_pl_CRL_Destroy;
        entry->equalsFunction = pkix_pl_CRL_Equals;
        entry->hashcodeFunction = pkix_pl_CRL_Hashcode;
        entry->toStringFunction = pkix_pl_CRL_ToString;
        entry->duplicateFunction = pkix_duplicateImmutable;

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: PKIX_PL_CRL_VerifyUpdateTime (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_CRL_VerifyUpdateTime(
        PKIX_PL_CRL *crl,
        PKIX_PL_Date *date,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PRTime timeToCheck;
        PRTime nextUpdate;
        PRTime lastUpdate;
        SECStatus status;
        CERTCrl *nssCrl = NULL;
        SECItem *nextUpdateDer = NULL;
        PKIX_Boolean haveNextUpdate = PR_FALSE;

        PKIX_ENTER(CRL, "PKIX_PL_CRL_VerifyUpdateTime");
        PKIX_NULLCHECK_FOUR(crl, crl->nssSignedCrl, date, pResult);

        /* Can call this function only with der been adopted. */
        PORT_Assert(crl->adoptedDerCrl);

        nssCrl = &(crl->nssSignedCrl->crl);
        timeToCheck = date->nssTime;

        /* nextUpdate can be NULL. Checking before using it */
        nextUpdateDer = &nssCrl->nextUpdate;
        if (nextUpdateDer->data && nextUpdateDer->len) {
                haveNextUpdate = PR_TRUE;
                status = DER_DecodeTimeChoice(&nextUpdate, nextUpdateDer);
                if (status != SECSuccess) {
                        PKIX_ERROR(PKIX_DERDECODETIMECHOICEFORNEXTUPDATEFAILED);
                }
        }

        status = DER_DecodeTimeChoice(&lastUpdate, &(nssCrl->lastUpdate));
        if (status != SECSuccess) {
                PKIX_ERROR(PKIX_DERDECODETIMECHOICEFORLASTUPDATEFAILED);
        }

        if (!haveNextUpdate || nextUpdate < timeToCheck) {
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        if (lastUpdate <= timeToCheck) {
                *pResult = PKIX_TRUE;
        } else {
                *pResult = PKIX_FALSE;
        }

cleanup:

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: pkix_pl_CRL_CreateWithSignedCRL
 * DESCRIPTION:
 *
 *  Creates a new CRL using the CERTSignedCrl pointed to by "nssSignedCrl"
 *  and stores it at "pCRL". If the decoding of the CERTSignedCrl fails,
 *  a PKIX_Error is returned.
 *
 * PARAMETERS:
 *  "nssSignedCrl"
 *      Address of CERTSignedCrl. Must be non-NULL.
 *  "adoptedDerCrl"
 *      SECItem ponter that if not NULL is indicating that memory used
 *      for der should be adopted by crl that is about to be created.
 *  "pCRL"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRL Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CRL_CreateWithSignedCRL(
        CERTSignedCrl *nssSignedCrl,
        SECItem *adoptedDerCrl,
        SECItem *derGenName,
        PKIX_PL_CRL **pCrl,
        void *plContext)
{
        PKIX_PL_CRL *crl = NULL;

        PKIX_ENTER(CRL, "pkix_pl_CRL_CreateWithSignedCRL");
        PKIX_NULLCHECK_ONE(pCrl);

        /* create a PKIX_PL_CRL object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CRL_TYPE,
                    sizeof (PKIX_PL_CRL),
                    (PKIX_PL_Object **)&crl,
                    plContext),
                    PKIX_COULDNOTCREATECRLOBJECT);

        /* populate the nssSignedCrl field */
        crl->nssSignedCrl = nssSignedCrl;
        crl->adoptedDerCrl = adoptedDerCrl;
        crl->issuer = NULL;
        crl->signatureAlgId = NULL;
        crl->crlNumber = NULL;
        crl->crlNumberAbsent = PKIX_FALSE;
        crl->crlEntryList = NULL;
        crl->critExtOids = NULL;
        if (derGenName) {
            crl->derGenName =
                SECITEM_DupItem(derGenName);
            if (!crl->derGenName) {
                PKIX_ERROR(PKIX_ALLOCERROR);
            }
        }

        *pCrl = crl;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(crl);
        }

        PKIX_RETURN(CRL);
}

/* --Public-CRL-Functions------------------------------------- */

/*
 * FUNCTION: PKIX_PL_CRL_Create (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_CRL_Create(
        PKIX_PL_ByteArray *byteArray,
        PKIX_PL_CRL **pCrl,
        void *plContext)
{
        CERTSignedCrl *nssSignedCrl = NULL;
        SECItem derItem, *derCrl = NULL;
        PKIX_PL_CRL *crl = NULL;

        PKIX_ENTER(CRL, "PKIX_PL_CRL_Create");
        PKIX_NULLCHECK_TWO(byteArray, pCrl);

        if (byteArray->length == 0){
            PKIX_ERROR(PKIX_ZEROLENGTHBYTEARRAYFORCRLENCODING);
        }
        derItem.type = siBuffer;
        derItem.data = byteArray->array;
        derItem.len = byteArray->length;
        derCrl = SECITEM_DupItem(&derItem);
        if (!derCrl) {
            PKIX_ERROR(PKIX_ALLOCERROR);
        }
        nssSignedCrl =
            CERT_DecodeDERCrlWithFlags(NULL, derCrl, SEC_CRL_TYPE,
                                       CRL_DECODE_DONT_COPY_DER |
                                       CRL_DECODE_SKIP_ENTRIES);
        if (!nssSignedCrl) {
            PKIX_ERROR(PKIX_CERTDECODEDERCRLFAILED);
        }
        PKIX_CHECK(
            pkix_pl_CRL_CreateWithSignedCRL(nssSignedCrl, derCrl, NULL,
                                            &crl, plContext),
            PKIX_CRLCREATEWITHSIGNEDCRLFAILED);
        nssSignedCrl = NULL;
        derCrl = NULL;
        *pCrl = crl;

cleanup:
        if (derCrl) {
            SECITEM_FreeItem(derCrl, PR_TRUE);
        }
        if (nssSignedCrl) {
            SEC_DestroyCrl(nssSignedCrl);
        } 

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: PKIX_PL_CRL_GetIssuer (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_CRL_GetIssuer(
        PKIX_PL_CRL *crl,
        PKIX_PL_X500Name **pCRLIssuer,
        void *plContext)
{
        PKIX_PL_String *crlString = NULL;
        PKIX_PL_X500Name *issuer = NULL;
        SECItem  *derIssuerName = NULL;
        CERTName *issuerName = NULL;

        PKIX_ENTER(CRL, "PKIX_PL_CRL_GetIssuer");
        PKIX_NULLCHECK_THREE(crl, crl->nssSignedCrl, pCRLIssuer);

        /* Can call this function only with der been adopted. */
        PORT_Assert(crl->adoptedDerCrl);

        /* if we don't have a cached copy from before, we create one */
        if (crl->issuer == NULL){

                PKIX_OBJECT_LOCK(crl);

                if (crl->issuer == NULL) {

                        issuerName = &crl->nssSignedCrl->crl.name;
                        derIssuerName = &crl->nssSignedCrl->crl.derName;

                        PKIX_CHECK(
                            PKIX_PL_X500Name_CreateFromCERTName(derIssuerName,
                                                                issuerName,
                                                                &issuer,
                                                                plContext),
                            PKIX_X500NAMECREATEFROMCERTNAMEFAILED);
                        
                        /* save a cached copy in case it is asked for again */
                        crl->issuer = issuer;
                }

                PKIX_OBJECT_UNLOCK(crl);

        }

        PKIX_INCREF(crl->issuer);

        *pCRLIssuer = crl->issuer;

cleanup:

        PKIX_DECREF(crlString);

        PKIX_RETURN(CRL);
}


/*
 * FUNCTION: PKIX_PL_CRL_GetCriticalExtensionOIDs
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_CRL_GetCriticalExtensionOIDs(
        PKIX_PL_CRL *crl,
        PKIX_List **pExtensions,   /* list of PKIX_PL_OID */
        void *plContext)
{
        PKIX_List *oidsList = NULL;
        CERTCertExtension **extensions = NULL;
        CERTCrl *nssSignedCrl = NULL;

        PKIX_ENTER(CRL, "PKIX_PL_CRL_GetCriticalExtensionOIDs");
        PKIX_NULLCHECK_THREE(crl, crl->nssSignedCrl, pExtensions);

        /* Can call this function only with der been adopted. */
        PORT_Assert(crl->adoptedDerCrl);

        /* if we don't have a cached copy from before, we create one */
        if (crl->critExtOids == NULL) {

                PKIX_OBJECT_LOCK(crl);

                nssSignedCrl = &(crl->nssSignedCrl->crl);
                extensions = nssSignedCrl->extensions;

                if (crl->critExtOids == NULL) {

                        PKIX_CHECK(pkix_pl_OID_GetCriticalExtensionOIDs
                                    (extensions, &oidsList, plContext),
                                    PKIX_GETCRITICALEXTENSIONOIDSFAILED);

                        crl->critExtOids = oidsList;
                }

                PKIX_OBJECT_UNLOCK(crl);

        }

        /* We should return a copy of the List since this list changes */
        PKIX_DUPLICATE(crl->critExtOids, pExtensions, plContext,
                PKIX_OBJECTDUPLICATELISTFAILED);

cleanup:

        PKIX_RETURN(CRL);
}

/*
 * FUNCTION: PKIX_PL_CRL_VerifySignature (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_CRL_VerifySignature(
        PKIX_PL_CRL *crl,
        PKIX_PL_PublicKey *pubKey,
        void *plContext)
{
        PKIX_PL_CRL *cachedCrl = NULL;
        PKIX_Error *verifySig = NULL;
        PKIX_Error *cachedSig = NULL;
        PKIX_Boolean crlEqual = PKIX_FALSE;
        PKIX_Boolean crlInHash= PKIX_FALSE;
        CERTSignedCrl *nssSignedCrl = NULL;
        SECKEYPublicKey *nssPubKey = NULL;
        CERTSignedData *tbsCrl = NULL;
        void* wincx = NULL;
        SECStatus status;

        PKIX_ENTER(CRL, "PKIX_PL_CRL_VerifySignature");
        PKIX_NULLCHECK_THREE(crl, crl->nssSignedCrl, pubKey);

        /* Can call this function only with der been adopted. */
        PORT_Assert(crl->adoptedDerCrl);

        verifySig = PKIX_PL_HashTable_Lookup
                (cachedCrlSigTable,
                (PKIX_PL_Object *) pubKey,
                (PKIX_PL_Object **) &cachedCrl,
                plContext);

        if (cachedCrl != NULL && verifySig == NULL) {
                /* Cached Signature Table lookup succeed */
                PKIX_EQUALS(crl, cachedCrl, &crlEqual, plContext,
                            PKIX_OBJECTEQUALSFAILED);
                if (crlEqual == PKIX_TRUE) {
                        goto cleanup;
                }
                /* Different PubKey may hash to same value, skip add */
                crlInHash = PKIX_TRUE;
        }

        nssSignedCrl = crl->nssSignedCrl;
        tbsCrl = &nssSignedCrl->signatureWrap;

        PKIX_CRL_DEBUG("\t\tCalling SECKEY_ExtractPublicKey\n");
        nssPubKey = SECKEY_ExtractPublicKey(pubKey->nssSPKI);
        if (!nssPubKey){
                PKIX_ERROR(PKIX_SECKEYEXTRACTPUBLICKEYFAILED);
        }

        PKIX_CHECK(pkix_pl_NssContext_GetWincx
                   ((PKIX_PL_NssContext *)plContext, &wincx),
                   PKIX_NSSCONTEXTGETWINCXFAILED);

        PKIX_CRL_DEBUG("\t\tCalling CERT_VerifySignedDataWithPublicKey\n");
        status = CERT_VerifySignedDataWithPublicKey(tbsCrl, nssPubKey, wincx);

        if (status != SECSuccess) {
                PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                PKIX_ERROR(PKIX_SIGNATUREDIDNOTVERIFYWITHTHEPUBLICKEY);
        }

        if (crlInHash == PKIX_FALSE) {
                cachedSig = PKIX_PL_HashTable_Add
                        (cachedCrlSigTable,
                        (PKIX_PL_Object *) pubKey,
                        (PKIX_PL_Object *) crl,
                        plContext);

                if (cachedSig != NULL) {
                        PKIX_DEBUG("PKIX_PL_HashTable_Add skipped: entry existed\n");
                }
        }

cleanup:

        if (nssPubKey){
                PKIX_CRL_DEBUG("\t\tCalling SECKEY_DestroyPublicKey\n");
                SECKEY_DestroyPublicKey(nssPubKey);
                nssPubKey = NULL;
        }

        PKIX_DECREF(cachedCrl);
        PKIX_DECREF(verifySig);
        PKIX_DECREF(cachedSig);

        PKIX_RETURN(CRL);
}

PKIX_Error*
PKIX_PL_CRL_ReleaseDerCrl(PKIX_PL_CRL *crl,
                         SECItem **derCrl,
                         void *plContext)
{
    PKIX_ENTER(CRL, "PKIX_PL_CRL_ReleaseDerCrl");
    *derCrl = crl->adoptedDerCrl;
    crl->adoptedDerCrl = NULL;
        
    PKIX_RETURN(CRL);
}

PKIX_Error*
PKIX_PL_CRL_AdoptDerCrl(PKIX_PL_CRL *crl,
                         SECItem *derCrl,
                         void *plContext)
{
    PKIX_ENTER(CRL, "PKIX_PL_CRL_AquireDerCrl");
    if (crl->adoptedDerCrl) {
        PKIX_ERROR(PKIX_CANNOTAQUIRECRLDER);
    }
    crl->adoptedDerCrl = derCrl;
cleanup:        
    PKIX_RETURN(CRL);
}
