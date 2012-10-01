/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_x500name.c
 *
 * X500Name Object Functions
 *
 */

#include "pkix_pl_x500name.h"

/* --Private-X500Name-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_X500Name_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the X500Name
 *  pointed to by "name" and stores it at "pString".
 *
 * PARAMETERS
 *  "name"
 *      Address of X500Name whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a X500Name Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_X500Name_ToString_Helper(
        PKIX_PL_X500Name *name,
        PKIX_PL_String **pString,
        void *plContext)
{
        CERTName *nssDN = NULL;
        char *utf8String = NULL;
        PKIX_UInt32 utf8Length;

        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_ToString_Helper");
        PKIX_NULLCHECK_TWO(name, pString);
        nssDN = &name->nssDN;

        /* this should really be called CERT_NameToUTF8 */
        utf8String = CERT_NameToAsciiInvertible(nssDN, CERT_N2A_INVERTIBLE);
        if (!utf8String){
                PKIX_ERROR(PKIX_CERTNAMETOASCIIFAILED);
        }

        PKIX_X500NAME_DEBUG("\t\tCalling PL_strlen).\n");
        utf8Length = PL_strlen(utf8String);

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_UTF8, utf8String, utf8Length, pString, plContext),
                    PKIX_STRINGCREATEFAILED);

cleanup:

        PR_Free(utf8String);

        PKIX_RETURN(X500NAME);
}

/*
 * FUNCTION: pkix_pl_X500Name_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_X500Name_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_X500Name *name = NULL;

        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_X500NAME_TYPE, plContext),
                    PKIX_OBJECTNOTANX500NAME);

        name = (PKIX_PL_X500Name *)object;

        /* PORT_FreeArena will destroy arena, and, allocated on it, CERTName
         * and SECItem */
        if (name->arena) {
            PORT_FreeArena(name->arena, PR_FALSE);
            name->arena = NULL;
        }

cleanup:

        PKIX_RETURN(X500NAME);
}

/*
 * FUNCTION: pkix_pl_X500Name_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_X500Name_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_X500Name *name = NULL;
        char *string = NULL;
        PKIX_UInt32 strLength = 0;

        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_toString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_X500NAME_TYPE, plContext),
                    PKIX_OBJECTNOTANX500NAME);

        name = (PKIX_PL_X500Name *)object;
        string = CERT_NameToAscii(&name->nssDN);
        if (!string){
                PKIX_ERROR(PKIX_CERTNAMETOASCIIFAILED);
        }
        strLength = PL_strlen(string);

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII, string, strLength, pString, plContext),
                    PKIX_STRINGCREATEFAILED);

cleanup:

        PKIX_RETURN(X500NAME);
}

/*
 * FUNCTION: pkix_pl_X500Name_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_X500Name_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_X500Name *name = NULL;
        SECItem *derBytes = NULL;
        PKIX_UInt32 nameHash;

        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_X500NAME_TYPE, plContext),
                    PKIX_OBJECTNOTANX500NAME);

        name = (PKIX_PL_X500Name *)object;

        /* we hash over the bytes in the DER encoding */

        derBytes = &name->derName;

        PKIX_CHECK(pkix_hash
                    (derBytes->data, derBytes->len, &nameHash, plContext),
                    PKIX_HASHFAILED);

        *pHashcode = nameHash;

cleanup:

        PKIX_RETURN(X500NAME);
}


/*
 * FUNCTION: pkix_pl_X500Name_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_X500Name_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;

        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is an X500Name */
        PKIX_CHECK(pkix_CheckType(firstObject, PKIX_X500NAME_TYPE, plContext),
                    PKIX_FIRSTOBJECTARGUMENTNOTANX500NAME);

        /*
         * Since we know firstObject is an X500Name, if both references are
         * identical, they must be equal
         */
        if (firstObject == secondObject){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't an X500Name, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObject, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_X500NAME_TYPE) goto cleanup;

        PKIX_CHECK(
            PKIX_PL_X500Name_Match((PKIX_PL_X500Name *)firstObject,
                                   (PKIX_PL_X500Name *)secondObject,
                                   pResult, plContext),
            PKIX_X500NAMEMATCHFAILED);

cleanup:

        PKIX_RETURN(X500NAME);
}

/*
 * FUNCTION: pkix_pl_X500Name_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_X500NAME_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_X500Name_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_RegisterSelf");

        entry.description = "X500Name";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_X500Name);
        entry.destructor = pkix_pl_X500Name_Destroy;
        entry.equalsFunction = pkix_pl_X500Name_Equals;
        entry.hashcodeFunction = pkix_pl_X500Name_Hashcode;
        entry.toStringFunction = pkix_pl_X500Name_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_X500NAME_TYPE] = entry;

        PKIX_RETURN(X500NAME);
}

#ifdef BUILD_LIBPKIX_TESTS
/*
 * FUNCTION: pkix_pl_X500Name_CreateFromUtf8
 *
 * DESCRIPTION:
 *  Creates an X500Name object from the RFC1485 string representation pointed
 *  to by "stringRep", and stores the result at "pName". If the string cannot
 *  be successfully converted, a non-fatal error is returned.
 *
 * NOTE: ifdefed BUILD_LIBPKIX_TESTS function: this function is allowed to be
 * called only by pkix tests programs.
 * 
 * PARAMETERS:
 *  "stringRep"
 *      Address of the RFC1485 string to be converted. Must be non-NULL.
 *  "pName"
 *      Address where the X500Name result will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an X500NAME Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_X500Name_CreateFromUtf8(
        char *stringRep,
        PKIX_PL_X500Name **pName,
        void *plContext)
{
        PKIX_PL_X500Name *x500Name = NULL;
        PRArenaPool *arena = NULL;
        CERTName *nssDN = NULL;
        SECItem *resultSecItem = NULL;
        
        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_CreateFromUtf8");
        PKIX_NULLCHECK_TWO(pName, stringRep);

        nssDN = CERT_AsciiToName(stringRep);
        if (nssDN == NULL) {
            PKIX_ERROR(PKIX_COULDNOTCREATENSSDN);
        }

        arena = nssDN->arena;

        /* create a PKIX_PL_X500Name object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_X500NAME_TYPE,
                    sizeof (PKIX_PL_X500Name),
                    (PKIX_PL_Object **)&x500Name,
                    plContext),
                    PKIX_COULDNOTCREATEX500NAMEOBJECT);

        /* populate the nssDN field */
        x500Name->arena = arena;
        x500Name->nssDN.arena = arena;
        x500Name->nssDN.rdns = nssDN->rdns;
        
        resultSecItem =
            SEC_ASN1EncodeItem(arena, &x500Name->derName, nssDN,
                               CERT_NameTemplate);
        
        if (resultSecItem == NULL){
            PKIX_ERROR(PKIX_SECASN1ENCODEITEMFAILED);
        }

        *pName = x500Name;

cleanup:

        if (PKIX_ERROR_RECEIVED){
            if (x500Name) {
                PKIX_PL_Object_DecRef((PKIX_PL_Object*)x500Name,
                                      plContext);
            } else if (nssDN) {
                CERT_DestroyName(nssDN);
            }
        }

        PKIX_RETURN(X500NAME);
}
#endif /* BUILD_LIBPKIX_TESTS */

/*
 * FUNCTION: pkix_pl_X500Name_GetCERTName
 *
 * DESCRIPTION:
 * 
 * Returns the pointer to CERTName member of X500Name structure.
 *
 * Returned pointed should not be freed.2
 *
 * PARAMETERS:
 *  "xname"
 *      Address of X500Name whose OrganizationName is to be extracted. Must be
 *      non-NULL.
 *  "pCERTName"
 *      Address where result will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_X500Name_GetCERTName(
        PKIX_PL_X500Name *xname,
        CERTName **pCERTName,
        void *plContext)
{
        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_GetCERTName");
        PKIX_NULLCHECK_TWO(xname, pCERTName);

        *pCERTName = &xname->nssDN;

        PKIX_RETURN(X500NAME);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_X500Name_CreateFromCERTName (see comments in pkix_pl_pki.h)
 */

PKIX_Error *
PKIX_PL_X500Name_CreateFromCERTName(
        SECItem *derName,
        CERTName *name, 
        PKIX_PL_X500Name **pName,
        void *plContext)
{
        PRArenaPool *arena = NULL;
        SECStatus rv = SECFailure;
        PKIX_PL_X500Name *x500Name = NULL;

        PKIX_ENTER(X500NAME, "PKIX_PL_X500Name_CreateFromCERTName");
        PKIX_NULLCHECK_ONE(pName);
        if (derName == NULL && name == NULL) {
            PKIX_ERROR(PKIX_NULLARGUMENT);
        }

        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (arena == NULL) {
            PKIX_ERROR(PKIX_OUTOFMEMORY);
        }
        
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_X500NAME_TYPE,
                    sizeof (PKIX_PL_X500Name),
                    (PKIX_PL_Object **)&x500Name,
                    plContext),
                    PKIX_COULDNOTCREATEX500NAMEOBJECT);

        x500Name->arena = arena;
        x500Name->nssDN.arena = NULL;

        if (derName != NULL) {
            rv = SECITEM_CopyItem(arena, &x500Name->derName, derName);
            if (rv == SECFailure) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
            }
        }            

        if (name != NULL) {
            rv = CERT_CopyName(arena, &x500Name->nssDN, name);
            if (rv == SECFailure) {
                PKIX_ERROR(PKIX_CERTCOPYNAMEFAILED);
            }
        } else {
            rv = SEC_QuickDERDecodeItem(arena, &x500Name->nssDN,
                                        CERT_NameTemplate,
                                        &x500Name->derName);
            if (rv == SECFailure) {
                PKIX_ERROR(PKIX_SECQUICKDERDECODERFAILED);
            }
        }

        *pName = x500Name;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
            if (x500Name) {
                PKIX_PL_Object_DecRef((PKIX_PL_Object*)x500Name,
                                      plContext);
            } else if (arena) {                
                PORT_FreeArena(arena, PR_FALSE);
            }
        }

        PKIX_RETURN(X500NAME);
}

#ifdef BUILD_LIBPKIX_TESTS
/*
 * FUNCTION: PKIX_PL_X500Name_Create (see comments in pkix_pl_pki.h)
 *
 * NOTE: ifdefed BUILD_LIBPKIX_TESTS function: this function is allowed
 * to be called only by pkix tests programs.
 */
PKIX_Error *
PKIX_PL_X500Name_Create(
        PKIX_PL_String *stringRep,
        PKIX_PL_X500Name **pName,
        void *plContext)
{
        char *utf8String = NULL;
        PKIX_UInt32 utf8Length = 0;

        PKIX_ENTER(X500NAME, "PKIX_PL_X500Name_Create");
        PKIX_NULLCHECK_TWO(pName, stringRep);

        /*
         * convert the input PKIX_PL_String to PKIX_UTF8_NULL_TERM.
         * we need to use this format specifier because
         * CERT_AsciiToName expects a NULL-terminated UTF8 string.
         * Since UTF8 allow NUL characters in the middle of the
         * string, this is buggy. However, as a workaround, using
         * PKIX_UTF8_NULL_TERM gives us a NULL-terminated UTF8 string.
         */

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                    (stringRep,
                    PKIX_UTF8_NULL_TERM,
                    (void **)&utf8String,
                    &utf8Length,
                    plContext),
                    PKIX_STRINGGETENCODEDFAILED);

        PKIX_CHECK(
            pkix_pl_X500Name_CreateFromUtf8(utf8String,
                                            pName, plContext),
            PKIX_X500NAMECREATEFROMUTF8FAILED);

cleanup:
        PKIX_FREE(utf8String);

        PKIX_RETURN(X500NAME);
}
#endif /* BUILD_LIBPKIX_TESTS */

/*
 * FUNCTION: PKIX_PL_X500Name_Match (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_X500Name_Match(
        PKIX_PL_X500Name *firstX500Name,
        PKIX_PL_X500Name *secondX500Name,
        PKIX_Boolean *pResult,
        void *plContext)
{
        SECItem *firstDerName = NULL;
        SECItem *secondDerName = NULL;
        SECComparison cmpResult;

        PKIX_ENTER(X500NAME, "PKIX_PL_X500Name_Match");
        PKIX_NULLCHECK_THREE(firstX500Name, secondX500Name, pResult);

        if (firstX500Name == secondX500Name){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        firstDerName = &firstX500Name->derName;
        secondDerName = &secondX500Name->derName;

        PKIX_NULLCHECK_TWO(firstDerName->data, secondDerName->data);

        cmpResult = SECITEM_CompareItem(firstDerName, secondDerName);
        if (cmpResult != SECEqual) {
            cmpResult = CERT_CompareName(&firstX500Name->nssDN,
                                         &secondX500Name->nssDN);
        }

        *pResult = (cmpResult == SECEqual);
                   
cleanup:

        PKIX_RETURN(X500NAME);
}

/*
 * FUNCTION: pkix_pl_X500Name_GetSECName
 *
 * DESCRIPTION:
 *  Returns a copy of CERTName DER representation allocated on passed in arena.
 *  If allocation on arena can not be done, NULL is stored at "pSECName".
 *
 * PARAMETERS:
 *  "xname"
 *      Address of X500Name whose CERTName flag is to be encoded. Must be
 *      non-NULL.
 *  "arena"
 *      Address of the PRArenaPool to be used in the encoding, and in which
 *      "pSECName" will be allocated. Must be non-NULL.
 *  "pSECName"
 *      Address where result will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_X500Name_GetDERName(
        PKIX_PL_X500Name *xname,
        PRArenaPool *arena,
        SECItem **pDERName,
        void *plContext)
{
        SECItem *derName = NULL;

        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_GetDERName");

        PKIX_NULLCHECK_THREE(xname, arena, pDERName);

        /* Return NULL is X500Name was not created from DER  */
        if (xname->derName.data == NULL) {
            *pDERName = NULL;
            goto cleanup;
        }

        derName = SECITEM_ArenaDupItem(arena, &xname->derName);
        if (derName == NULL) {
            PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        *pDERName = derName;
cleanup:

        PKIX_RETURN(X500NAME);
}

/*
 * FUNCTION: pkix_pl_X500Name_GetCommonName
 *
 * DESCRIPTION:
 *  Extracts the CommonName component of the X500Name object pointed to by
 *  "xname", and stores the result at "pCommonName". If the CommonName cannot
 *  be successfully extracted, NULL is stored at "pCommonName".
 *
 *  The returned string must be freed with PORT_Free.
 *
 * PARAMETERS:
 *  "xname"
 *      Address of X500Name whose CommonName is to be extracted. Must be
 *      non-NULL.
 *  "pCommonName"
 *      Address where result will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_X500Name_GetCommonName(
        PKIX_PL_X500Name *xname,
        unsigned char **pCommonName,
        void *plContext)
{
        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_GetCommonName");
        PKIX_NULLCHECK_TWO(xname, pCommonName);

        *pCommonName = (unsigned char *)CERT_GetCommonName(&xname->nssDN);

        PKIX_RETURN(X500NAME);
}

/*
 * FUNCTION: pkix_pl_X500Name_GetCountryName
 *
 * DESCRIPTION:
 *  Extracts the CountryName component of the X500Name object pointed to by
 *  "xname", and stores the result at "pCountryName". If the CountryName cannot
 *  be successfully extracted, NULL is stored at "pCountryName".
 *
 *  The returned string must be freed with PORT_Free.
 *
 * PARAMETERS:
 *  "xname"
 *      Address of X500Name whose CountryName is to be extracted. Must be
 *      non-NULL.
 *  "pCountryName"
 *      Address where result will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_X500Name_GetCountryName(
        PKIX_PL_X500Name *xname,
        unsigned char **pCountryName,
        void *plContext)
{
        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_GetCountryName");
        PKIX_NULLCHECK_TWO(xname, pCountryName);

        *pCountryName = (unsigned char*)CERT_GetCountryName(&xname->nssDN);

        PKIX_RETURN(X500NAME);
}

/*
 * FUNCTION: pkix_pl_X500Name_GetOrgName
 *
 * DESCRIPTION:
 *  Extracts the OrganizationName component of the X500Name object pointed to by
 *  "xname", and stores the result at "pOrgName". If the OrganizationName cannot
 *  be successfully extracted, NULL is stored at "pOrgName".
 *
 *  The returned string must be freed with PORT_Free.
 *
 * PARAMETERS:
 *  "xname"
 *      Address of X500Name whose OrganizationName is to be extracted. Must be
 *      non-NULL.
 *  "pOrgName"
 *      Address where result will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_X500Name_GetOrgName(
        PKIX_PL_X500Name *xname,
        unsigned char **pOrgName,
        void *plContext)
{
        PKIX_ENTER(X500NAME, "pkix_pl_X500Name_GetOrgName");
        PKIX_NULLCHECK_TWO(xname, pOrgName);

        *pOrgName = (unsigned char*)CERT_GetOrgName(&xname->nssDN);

        PKIX_RETURN(X500NAME);
}
