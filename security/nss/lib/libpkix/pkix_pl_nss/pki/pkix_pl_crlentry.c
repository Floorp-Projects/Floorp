/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_crlentry.c
 *
 * CRLENTRY Function Definitions
 *
 */

#include "pkix_pl_crlentry.h"

/* --Private-CRLEntry-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_CRLEntry_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CRLEntry_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_CRLEntry *crlEntry = NULL;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRLENTRY_TYPE, plContext),
                    PKIX_OBJECTNOTCRLENTRY);

        crlEntry = (PKIX_PL_CRLEntry*)object;

        /* crlEntry->nssCrlEntry is freed by NSS when freeing CRL */
        crlEntry->userReasonCode = 0;
        crlEntry->userReasonCodeAbsent = PKIX_FALSE;
        crlEntry->nssCrlEntry = NULL;
        PKIX_DECREF(crlEntry->serialNumber);
        PKIX_DECREF(crlEntry->critExtOids);

cleanup:

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLEntry_ToString_Helper
 *
 * DESCRIPTION:
 *  Helper function that creates a string representation of the CRLEntry
 *  pointed to by "crlEntry" and stores it at "pString".
 *
 * PARAMETERS
 *  "crlEntry"
 *      Address of CRLEntry whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRLEntry Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CRLEntry_ToString_Helper(
        PKIX_PL_CRLEntry *crlEntry,
        PKIX_PL_String **pString,
        void *plContext)
{
        char *asciiFormat = NULL;
        PKIX_List *critExtOIDs = NULL;
        PKIX_PL_String *crlEntryString = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *crlSerialNumberString = NULL;
        PKIX_PL_String *crlRevocationDateString = NULL;
        PKIX_PL_String *critExtOIDsString = NULL;
        PKIX_Int32 reasonCode = 0;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_ToString_Helper");
        PKIX_NULLCHECK_FOUR
                (crlEntry,
                crlEntry->serialNumber,
                crlEntry->nssCrlEntry,
                pString);

        asciiFormat =
                "\n\t[\n"
                "\tSerialNumber:    %s\n"
                "\tReasonCode:      %d\n"
                "\tRevocationDate:  %s\n"
                "\tCritExtOIDs:     %s\n"
                "\t]\n\t";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        /* SerialNumber */
        PKIX_CHECK(PKIX_PL_Object_ToString
                    ((PKIX_PL_Object *)crlEntry->serialNumber,
                    &crlSerialNumberString,
                    plContext),
                    PKIX_BIGINTTOSTRINGHELPERFAILED);

        /* RevocationDate - No Date object created, use nss data directly */
        PKIX_CHECK(pkix_pl_Date_ToString_Helper
                    (&(crlEntry->nssCrlEntry->revocationDate),
                    &crlRevocationDateString,
                    plContext),
                    PKIX_DATETOSTRINGHELPERFAILED);

        /* CriticalExtensionOIDs */
        PKIX_CHECK(PKIX_PL_CRLEntry_GetCriticalExtensionOIDs
                    (crlEntry, &critExtOIDs, plContext),
                    PKIX_CRLENTRYGETCRITICALEXTENSIONOIDSFAILED);

        PKIX_TOSTRING(critExtOIDs, &critExtOIDsString, plContext,
                    PKIX_LISTTOSTRINGFAILED);

        /* Revocation Reason Code */
        PKIX_CHECK(PKIX_PL_CRLEntry_GetCRLEntryReasonCode
                            (crlEntry, &reasonCode, plContext),
                            PKIX_CRLENTRYGETCRLENTRYREASONCODEFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&crlEntryString,
                    plContext,
                    formatString,
                    crlSerialNumberString,
                    reasonCode,
                    crlRevocationDateString,
                    critExtOIDsString),
                    PKIX_SPRINTFFAILED);

        *pString = crlEntryString;

cleanup:

        PKIX_DECREF(critExtOIDs);
        PKIX_DECREF(crlSerialNumberString);
        PKIX_DECREF(crlRevocationDateString);
        PKIX_DECREF(critExtOIDsString);
        PKIX_DECREF(formatString);

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLEntry_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CRLEntry_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *crlEntryString = NULL;
        PKIX_PL_CRLEntry *crlEntry = NULL;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRLENTRY_TYPE, plContext),
                    PKIX_OBJECTNOTCRLENTRY);

        crlEntry = (PKIX_PL_CRLEntry *) object;

        PKIX_CHECK(pkix_pl_CRLEntry_ToString_Helper
                    (crlEntry, &crlEntryString, plContext),
                    PKIX_CRLENTRYTOSTRINGHELPERFAILED);

        *pString = crlEntryString;

cleanup:

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLEntry_Extensions_Hashcode
 * DESCRIPTION:
 *
 *  For each CRL Entry extension stored at NSS structure CERTCertExtension,
 *  get its derbyte data and do the hash.
 *
 * PARAMETERS
 *  "extensions"
 *      Address of arrray of CERTCertExtension whose hash value is desired.
 *      Must be non-NULL.
 *  "pHashValue"
 *      Address where the final hash value is returned. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditional Thread Safe
 *  Though the value of extensions once created is not supposed to change,
 *  it may be de-allocated while we are accessing it. But since we are
 *  validating the object, it is unlikely we or someone is de-allocating
 *  at the moment.
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OID Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CRLEntry_Extensions_Hashcode(
        CERTCertExtension **extensions,
        PKIX_UInt32 *pHashValue,
        void *plContext)
{
        CERTCertExtension *extension = NULL;
        PLArenaPool *arena = NULL;
        PKIX_UInt32 extHash = 0;
        PKIX_UInt32 hashValue = 0;
        SECItem *derBytes = NULL;
        SECItem *resultSecItem = NULL;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_Extensions_Hashcode");
        PKIX_NULLCHECK_TWO(extensions, pHashValue);

        if (extensions) {

                PKIX_CRLENTRY_DEBUG("\t\tCalling PORT_NewArena\n");
                arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                if (arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }

                while (*extensions) {

                        extension = *extensions++;

                        PKIX_NULLCHECK_ONE(extension);

                        PKIX_CRLENTRY_DEBUG("\t\tCalling PORT_ArenaZNew\n");
                        derBytes = PORT_ArenaZNew(arena, SECItem);
                        if (derBytes == NULL) {
                                PKIX_ERROR(PKIX_PORTARENAALLOCFAILED);
                        }

                        PKIX_CRLENTRY_DEBUG
                                ("\t\tCalling SEC_ASN1EncodeItem\n");
                        resultSecItem = SEC_ASN1EncodeItem
                                (arena,
                                derBytes,
                                extension,
                                CERT_CertExtensionTemplate);

                        if (resultSecItem == NULL){
                                PKIX_ERROR(PKIX_SECASN1ENCODEITEMFAILED);
                        }

                        PKIX_CHECK(pkix_hash
                                (derBytes->data,
                                derBytes->len,
                                &extHash,
                                plContext),
                                PKIX_HASHFAILED);

                        hashValue += (extHash << 7);

                }
        }

        *pHashValue = hashValue;

cleanup:

        if (arena){
                /* Note that freeing the arena also frees derBytes */
                PKIX_CRLENTRY_DEBUG("\t\tCalling PORT_FreeArena\n");
                PORT_FreeArena(arena, PR_FALSE);
                arena = NULL;
        }
        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLEntry_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CRLEntry_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        SECItem *nssDate = NULL;
        PKIX_PL_CRLEntry *crlEntry = NULL;
        PKIX_UInt32 crlEntryHash;
        PKIX_UInt32 hashValue;
        PKIX_Int32 reasonCode = 0;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRLENTRY_TYPE, plContext),
                    PKIX_OBJECTNOTCRLENTRY);

        crlEntry = (PKIX_PL_CRLEntry *)object;

        PKIX_NULLCHECK_ONE(crlEntry->nssCrlEntry);
        nssDate = &(crlEntry->nssCrlEntry->revocationDate);

        PKIX_NULLCHECK_ONE(nssDate->data);

        PKIX_CHECK(pkix_hash
                ((const unsigned char *)nssDate->data,
                nssDate->len,
                &crlEntryHash,
                plContext),
                PKIX_ERRORGETTINGHASHCODE);

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                ((PKIX_PL_Object *)crlEntry->serialNumber,
                &hashValue,
                plContext),
                PKIX_OBJECTHASHCODEFAILED);

        crlEntryHash += (hashValue << 7);

        hashValue = 0;

        if (crlEntry->nssCrlEntry->extensions) {

                PKIX_CHECK(pkix_pl_CRLEntry_Extensions_Hashcode
                    (crlEntry->nssCrlEntry->extensions, &hashValue, plContext),
                    PKIX_CRLENTRYEXTENSIONSHASHCODEFAILED);
        }

        crlEntryHash += (hashValue << 7);

        PKIX_CHECK(PKIX_PL_CRLEntry_GetCRLEntryReasonCode
                (crlEntry, &reasonCode, plContext),
                PKIX_CRLENTRYGETCRLENTRYREASONCODEFAILED);

        crlEntryHash += (reasonCode + 777) << 3;

        *pHashcode = crlEntryHash;

cleanup:

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLENTRY_Extensions_Equals
 * DESCRIPTION:
 *
 *  Compare each extension's DERbyte data in "firstExtensions" with extension
 *  in "secondExtensions" in sequential order and store the result in
 *  "pResult".
 *
 * PARAMETERS
 *  "firstExtensions"
 *      Address of first NSS structure CERTCertExtension to be compared.
 *      Must be non-NULL.
 *  "secondExtensions"
 *      Address of second NSS structure CERTCertExtension to be compared.
 *      Must be non-NULL.
 *  "pResult"
 *      Address where the comparison result is returned. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  Though the value of extensions once created is not supposed to change,
 *  it may be de-allocated while we are accessing it. But since we are
 *  validating the object, it is unlikely we or someone is de-allocating
 *  at the moment.
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OID Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CRLEntry_Extensions_Equals(
        CERTCertExtension **extensions1,
        CERTCertExtension **extensions2,
        PKIX_Boolean *pResult,
        void *plContext)
{
        CERTCertExtension **firstExtensions;
        CERTCertExtension **secondExtensions;
        CERTCertExtension *firstExtension = NULL;
        CERTCertExtension *secondExtension = NULL;
        PLArenaPool *arena = NULL;
        PKIX_Boolean cmpResult = PKIX_FALSE;
        SECItem *firstDerBytes = NULL;
        SECItem *secondDerBytes = NULL;
        SECItem *firstResultSecItem = NULL;
        SECItem *secondResultSecItem = NULL;
        PKIX_UInt32 firstNumExt = 0;
        PKIX_UInt32 secondNumExt = 0;
        SECComparison secResult;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_Extensions_Equals");
        PKIX_NULLCHECK_THREE(extensions1, extensions2, pResult);

        firstExtensions = extensions1;
        secondExtensions = extensions2;

        if (firstExtensions) {
                while (*firstExtensions) {
                        firstExtension = *firstExtensions++;
                        firstNumExt++;
                }
        }

        if (secondExtensions) {
                while (*secondExtensions) {
                        secondExtension = *secondExtensions++;
                        secondNumExt++;
                }
        }

        if (firstNumExt != secondNumExt) {
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        if (firstNumExt == 0 && secondNumExt == 0) {
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /* now have equal number, but non-zero extension items to compare */

        firstExtensions = extensions1;
        secondExtensions = extensions2;

        cmpResult = PKIX_TRUE;

        PKIX_CRLENTRY_DEBUG("\t\tCalling PORT_NewArena\n");
        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE*2);
        if (arena == NULL) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        while (firstNumExt--) {

                firstExtension = *firstExtensions++;
                secondExtension = *secondExtensions++;

                PKIX_NULLCHECK_TWO(firstExtension, secondExtension);

                PKIX_CRLENTRY_DEBUG("\t\tCalling PORT_ArenaZNew\n");
                firstDerBytes = PORT_ArenaZNew(arena, SECItem);
                if (firstDerBytes == NULL) {
                        PKIX_ERROR(PKIX_PORTARENAALLOCFAILED);
                }

                PKIX_CRLENTRY_DEBUG("\t\tCalling PORT_ArenaZNew\n");
                secondDerBytes = PORT_ArenaZNew(arena, SECItem);
                if (secondDerBytes == NULL) {
                        PKIX_ERROR(PKIX_PORTARENAALLOCFAILED);
                }

                PKIX_CRLENTRY_DEBUG
                        ("\t\tCalling SEC_ASN1EncodeItem\n");
                firstResultSecItem = SEC_ASN1EncodeItem
                        (arena,
                        firstDerBytes,
                        firstExtension,
                        CERT_CertExtensionTemplate);

                if (firstResultSecItem == NULL){
                        PKIX_ERROR(PKIX_SECASN1ENCODEITEMFAILED);
                }

                PKIX_CRLENTRY_DEBUG
                        ("\t\tCalling SEC_ASN1EncodeItem\n");
                secondResultSecItem = SEC_ASN1EncodeItem
                        (arena,
                        secondDerBytes,
                        secondExtension,
                        CERT_CertExtensionTemplate);

                if (secondResultSecItem == NULL){
                        PKIX_ERROR(PKIX_SECASN1ENCODEITEMFAILED);
                }

                PKIX_CRLENTRY_DEBUG("\t\tCalling SECITEM_CompareItem\n");
                secResult = SECITEM_CompareItem
                        (firstResultSecItem, secondResultSecItem);

                if (secResult != SECEqual) {
                        cmpResult = PKIX_FALSE;
                        break;
                }

        }

        *pResult = cmpResult;

cleanup:

        if (arena){
                /* Note that freeing the arena also frees derBytes */
                PKIX_CRLENTRY_DEBUG("\t\tCalling PORT_FreeArena\n");
                PORT_FreeArena(arena, PR_FALSE);
                arena = NULL;
        }

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLEntry_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CRLEntry_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_CRLEntry *firstCrlEntry = NULL;
        PKIX_PL_CRLEntry *secondCrlEntry = NULL;
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult = PKIX_FALSE;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a CRLEntry */
        PKIX_CHECK(pkix_CheckType(firstObject, PKIX_CRLENTRY_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTCRLENTRY);

        firstCrlEntry = (PKIX_PL_CRLEntry *)firstObject;
        secondCrlEntry = (PKIX_PL_CRLEntry *)secondObject;

        PKIX_NULLCHECK_TWO
                (firstCrlEntry->nssCrlEntry, secondCrlEntry->nssCrlEntry);

        /*
         * Since we know firstObject is a CRLEntry, if both references are
         * identical, they must be equal
         */
        if (firstCrlEntry == secondCrlEntry){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondCrlEntry isn't a CRL Entry, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    ((PKIX_PL_Object *)secondCrlEntry, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_CRLENTRY_TYPE) goto cleanup;

        /* Compare userSerialNumber */
        PKIX_CRLENTRY_DEBUG("\t\tCalling SECITEM_CompareItem\n");
        if (SECITEM_CompareItem(
            &(((PKIX_PL_CRLEntry *)firstCrlEntry)->nssCrlEntry->serialNumber),
            &(((PKIX_PL_CRLEntry *)secondCrlEntry)->nssCrlEntry->serialNumber))
            != SECEqual) {
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        /* Compare revocationDate */
        PKIX_CRLENTRY_DEBUG("\t\tCalling SECITEM_CompareItem\n");
        if (SECITEM_CompareItem
            (&(((PKIX_PL_CRLEntry *)firstCrlEntry)->nssCrlEntry->
                revocationDate),
            &(((PKIX_PL_CRLEntry *)secondCrlEntry)->nssCrlEntry->
                revocationDate))
            != SECEqual) {
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        /* Compare Critical Extension List */
        PKIX_CHECK(pkix_pl_CRLEntry_Extensions_Equals
                    (firstCrlEntry->nssCrlEntry->extensions,
                    secondCrlEntry->nssCrlEntry->extensions,
                    &cmpResult,
                    plContext),
                    PKIX_CRLENTRYEXTENSIONSEQUALSFAILED);

        if (cmpResult != PKIX_TRUE){
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        cmpResult = (firstCrlEntry->userReasonCode ==
                    secondCrlEntry->userReasonCode);

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLEntry_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_CRLEntry_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_CRLEntry_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_RegisterSelf");

        entry.description = "CRLEntry";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_CRLEntry);
        entry.destructor = pkix_pl_CRLEntry_Destroy;
        entry.equalsFunction = pkix_pl_CRLEntry_Equals;
        entry.hashcodeFunction = pkix_pl_CRLEntry_Hashcode;
        entry.toStringFunction = pkix_pl_CRLEntry_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_CRLENTRY_TYPE] = entry;

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLEntry_CreateEntry
 * DESCRIPTION:
 *
 *  Creates a new CRLEntry using the CertCrlEntry pointed to by "nssCrlEntry"
 *  and stores it at "pCrlEntry". Once created, a CRLEntry is immutable.
 *
 *  revokedCertificates SEQUENCE OF SEQUENCE  {
 *              userCertificate         CertificateSerialNumber,
 *              revocationDate          Time,
 *              crlEntryExtensions      Extensions OPTIONAL
 *                                      -- if present, MUST be v2
 *
 * PARAMETERS:
 *  "nssCrlEntry"
 *      Address of CERTCrlEntry representing an NSS CRL entry.
 *      Must be non-NULL.
 *  "pCrlEntry"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRLEntry Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_CRLEntry_CreateEntry(
        CERTCrlEntry *nssCrlEntry, /* entry data to be created from */
        PKIX_PL_CRLEntry **pCrlEntry,
        void *plContext)
{
        PKIX_PL_CRLEntry *crlEntry = NULL;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_CreateEntry");
        PKIX_NULLCHECK_TWO(nssCrlEntry, pCrlEntry);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CRLENTRY_TYPE,
                    sizeof (PKIX_PL_CRLEntry),
                    (PKIX_PL_Object **)&crlEntry,
                    plContext),
                    PKIX_COULDNOTCREATECRLENTRYOBJECT);

        crlEntry->nssCrlEntry = nssCrlEntry;
        crlEntry->serialNumber = NULL;
        crlEntry->critExtOids = NULL;
        crlEntry->userReasonCode = 0;
        crlEntry->userReasonCodeAbsent = PKIX_FALSE;

        *pCrlEntry = crlEntry;

cleanup:

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: pkix_pl_CRLEntry_Create
 * DESCRIPTION:
 *
 *  Creates a List of CRLEntries using the array of CERTCrlEntries pointed to
 *  by "nssCrlEntries" and stores it at "pCrlEntryList". If "nssCrlEntries" is
 *  NULL, this function stores an empty List at "pCrlEntryList".
 *                              }
 * PARAMETERS:
 *  "nssCrlEntries"
 *      Address of array of CERTCrlEntries representing NSS CRL entries.
 *      Can be NULL if CRL has no NSS CRL entries.
 *  "pCrlEntryList"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRLEntry Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_CRLEntry_Create(
        CERTCrlEntry **nssCrlEntries, /* head of entry list */
        PKIX_List **pCrlEntryList,
        void *plContext)
{
        PKIX_List *entryList = NULL;
        PKIX_PL_CRLEntry *crlEntry = NULL;
        CERTCrlEntry **entries = NULL;
        SECItem serialNumberItem;
        PKIX_PL_BigInt *serialNumber;
        char *bytes = NULL;
        PKIX_UInt32 length;

        PKIX_ENTER(CRLENTRY, "pkix_pl_CRLEntry_Create");
        PKIX_NULLCHECK_ONE(pCrlEntryList);

        entries = nssCrlEntries;

        PKIX_CHECK(PKIX_List_Create(&entryList, plContext),
                    PKIX_LISTCREATEFAILED);

        if (entries) {
            while (*entries){
                PKIX_CHECK(pkix_pl_CRLEntry_CreateEntry
                            (*entries, &crlEntry, plContext),
                            PKIX_COULDNOTCREATECRLENTRYOBJECT);

                /* Get Serial Number */
                serialNumberItem = (*entries)->serialNumber;
                length = serialNumberItem.len;
                bytes = (char *)serialNumberItem.data;

                PKIX_CHECK(pkix_pl_BigInt_CreateWithBytes
                            (bytes, length, &serialNumber, plContext),
                            PKIX_BIGINTCREATEWITHBYTESFAILED);

                crlEntry->serialNumber = serialNumber;
                crlEntry->nssCrlEntry = *entries;

                PKIX_CHECK(PKIX_List_AppendItem
                            (entryList, (PKIX_PL_Object *)crlEntry, plContext),
                            PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(crlEntry);

                entries++;
            }
        }

        *pCrlEntryList = entryList;

cleanup:
        PKIX_DECREF(crlEntry);

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(entryList);
        }

        PKIX_RETURN(CRLENTRY);
}

/* --Public-CRLENTRY-Functions------------------------------------- */

/*
 * FUNCTION: PKIX_PL_CRLEntry_GetCRLEntryReasonCode
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_CRLEntry_GetCRLEntryReasonCode (
        PKIX_PL_CRLEntry *crlEntry,
        PKIX_Int32 *pReason,
        void *plContext)
{
        SECStatus status;
        CERTCRLEntryReasonCode nssReasonCode;

        PKIX_ENTER(CRLENTRY, "PKIX_PL_CRLEntry_GetCRLEntryReasonCode");
        PKIX_NULLCHECK_TWO(crlEntry, pReason);

        if (!crlEntry->userReasonCodeAbsent && crlEntry->userReasonCode == 0) {

            PKIX_OBJECT_LOCK(crlEntry);

            if (!crlEntry->userReasonCodeAbsent &&
                crlEntry->userReasonCode == 0) {

                /* reason code has not been cached in */
                PKIX_CRLENTRY_DEBUG("\t\tCERT_FindCRLEntryReasonExten.\n");
                status = CERT_FindCRLEntryReasonExten
                        (crlEntry->nssCrlEntry, &nssReasonCode);

                if (status == SECSuccess) {
                        crlEntry->userReasonCode = (PKIX_Int32) nssReasonCode;
                } else {
                        crlEntry->userReasonCodeAbsent = PKIX_TRUE;
                }
            }

            PKIX_OBJECT_UNLOCK(crlEntry);

        }

        *pReason = crlEntry->userReasonCode;

cleanup:

        PKIX_RETURN(CRLENTRY);
}

/*
 * FUNCTION: PKIX_PL_CRLEntry_GetCriticalExtensionOIDs
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_CRLEntry_GetCriticalExtensionOIDs (
        PKIX_PL_CRLEntry *crlEntry,
        PKIX_List **pList,  /* list of PKIX_PL_OID */
        void *plContext)
{
        PKIX_List *oidsList = NULL;
        CERTCertExtension **extensions;

        PKIX_ENTER(CRLENTRY, "PKIX_PL_CRLEntry_GetCriticalExtensionOIDs");
        PKIX_NULLCHECK_THREE(crlEntry, crlEntry->nssCrlEntry, pList);

        /* if we don't have a cached copy from before, we create one */
        if (crlEntry->critExtOids == NULL) {

                PKIX_OBJECT_LOCK(crlEntry);

                if (crlEntry->critExtOids == NULL) {

                        extensions = crlEntry->nssCrlEntry->extensions;

                        PKIX_CHECK(pkix_pl_OID_GetCriticalExtensionOIDs
                                    (extensions, &oidsList, plContext),
                                    PKIX_GETCRITICALEXTENSIONOIDSFAILED);

                        crlEntry->critExtOids = oidsList;
                }

                PKIX_OBJECT_UNLOCK(crlEntry);

        }

        /* We should return a copy of the List since this list changes */
        PKIX_DUPLICATE(crlEntry->critExtOids, pList, plContext,
                PKIX_OBJECTDUPLICATELISTFAILED);

cleanup:

        PKIX_RETURN(CRLENTRY);
}
