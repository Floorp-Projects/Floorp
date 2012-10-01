/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_generalname.c
 *
 * GeneralName Object Definitions
 *
 */

#include "pkix_pl_generalname.h"

/* --Private-GeneralName-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_GeneralName_GetNssGeneralName
 * DESCRIPTION:
 *
 *  Retrieves the NSS representation of the PKIX_PL_GeneralName pointed by
 *  "genName" and stores it at "pNssGenName". The NSS data type CERTGeneralName
 *  is stored in this object when the object was created.
 *
 * PARAMETERS:
 *  "genName"
 *      Address of PKIX_PL_GeneralName. Must be non-NULL.
 *  "pNssGenName"
 *      Address where CERTGeneralName will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a GeneralName Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_GeneralName_GetNssGeneralName(
        PKIX_PL_GeneralName *genName,
        CERTGeneralName **pNssGenName,
        void *plContext)
{
        CERTGeneralName *nssGenName = NULL;

        PKIX_ENTER(GENERALNAME, "pkix_pl_GeneralName_GetNssGeneralName");
        PKIX_NULLCHECK_THREE(genName, pNssGenName, genName->nssGeneralNameList);

        nssGenName = genName->nssGeneralNameList->name;

        *pNssGenName = nssGenName;

        PKIX_RETURN(GENERALNAME);
}

/*
 * FUNCTION: pkix_pl_OtherName_Create
 * DESCRIPTION:
 *
 *  Creates new OtherName which represents the CERTGeneralName pointed to by
 *  "nssAltName" and stores it at "pOtherName".
 *
 * PARAMETERS:
 *  "nssAltName"
 *      Address of CERTGeneralName. Must be non-NULL.
 *  "pOtherName"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a GeneralName Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_OtherName_Create(
        CERTGeneralName *nssAltName,
        OtherName **pOtherName,
        void *plContext)
{
        OtherName *otherName = NULL;
        SECItem secItemName;
        SECItem secItemOID;
        SECStatus rv;

        PKIX_ENTER(GENERALNAME, "pkix_pl_OtherName_Create");
        PKIX_NULLCHECK_TWO(nssAltName, pOtherName);

        PKIX_CHECK(PKIX_PL_Malloc
                    (sizeof (OtherName), (void **)&otherName, plContext),
                    PKIX_MALLOCFAILED);

        /* make a copy of the name field */
        PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_CopyItem).\n");
        rv = SECITEM_CopyItem
                (NULL, &otherName->name, &nssAltName->name.OthName.name);
        if (rv != SECSuccess) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        /* make a copy of the oid field */
        PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_CopyItem).\n");
        rv = SECITEM_CopyItem
                (NULL, &otherName->oid, &nssAltName->name.OthName.oid);
        if (rv != SECSuccess) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        *pOtherName = otherName;

cleanup:

        if (otherName && PKIX_ERROR_RECEIVED){
                secItemName = otherName->name;
                secItemOID = otherName->oid;

                PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_FreeItem).\n");
                SECITEM_FreeItem(&secItemName, PR_FALSE);

                PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_FreeItem).\n");
                SECITEM_FreeItem(&secItemOID, PR_FALSE);

                PKIX_FREE(otherName);
                otherName = NULL;
        }

        PKIX_RETURN(GENERALNAME);
}

/*
 * FUNCTION: pkix_pl_DirectoryName_Create
 * DESCRIPTION:
 *
 *  Creates a new X500Name which represents the directoryName component of the
 *  CERTGeneralName pointed to by "nssAltName" and stores it at "pX500Name".
 *
 * PARAMETERS:
 *  "nssAltName"
 *      Address of CERTGeneralName. Must be non-NULL.
 *  "pX500Name"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a GeneralName Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_DirectoryName_Create(
        CERTGeneralName *nssAltName,
        PKIX_PL_X500Name **pX500Name,
        void *plContext)
{
        PKIX_PL_X500Name *pkixDN = NULL;
        CERTName *dirName = NULL;
        PKIX_PL_String *pkixDNString = NULL;
        char *utf8String = NULL;

        PKIX_ENTER(GENERALNAME, "pkix_pl_DirectoryName_Create");
        PKIX_NULLCHECK_TWO(nssAltName, pX500Name);

        dirName = &nssAltName->name.directoryName;

        PKIX_CHECK(PKIX_PL_X500Name_CreateFromCERTName(NULL, dirName, 
                                                       &pkixDN, plContext),
                   PKIX_X500NAMECREATEFROMCERTNAMEFAILED);

        *pX500Name = pkixDN;

cleanup:

        PR_Free(utf8String);
        PKIX_DECREF(pkixDNString);

        PKIX_RETURN(GENERALNAME);
}

/*
 * FUNCTION: pkix_pl_GeneralName_Create
 * DESCRIPTION:
 *
 *  Creates new GeneralName which represents the CERTGeneralName pointed to by
 *  "nssAltName" and stores it at "pGenName".
 *
 * PARAMETERS:
 *  "nssAltName"
 *      Address of CERTGeneralName. Must be non-NULL.
 *  "pGenName"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a GeneralName Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_GeneralName_Create(
        CERTGeneralName *nssAltName,
        PKIX_PL_GeneralName **pGenName,
        void *plContext)
{
        PKIX_PL_GeneralName *genName = NULL;
        PKIX_PL_X500Name *pkixDN = NULL;
        PKIX_PL_OID *pkixOID = NULL;
        OtherName *otherName = NULL;
        CERTGeneralNameList *nssGenNameList = NULL;
        CERTGeneralNameType nameType;

        PKIX_ENTER(GENERALNAME, "pkix_pl_GeneralName_Create");
        PKIX_NULLCHECK_TWO(nssAltName, pGenName);

        /* create a PKIX_PL_GeneralName object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_GENERALNAME_TYPE,
                    sizeof (PKIX_PL_GeneralName),
                    (PKIX_PL_Object **)&genName,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        nameType = nssAltName->type;

        /*
         * We use CERT_CreateGeneralNameList to create just one CERTGeneralName
         * item for memory allocation reason. If we want to just create one
         * item, we have to use the calling path CERT_NewGeneralName, then
         * CERT_CopyOneGeneralName. With this calling path, if we pass
         * the arena argument as NULL, in CERT_CopyOneGeneralName's subsequent
         * call to CERT_CopyName, it assumes arena should be valid, hence
         * segmentation error (not sure this is a NSS bug, certainly it is
         * not consistent). But on the other hand, we don't want to keep an
         * arena record here explicitely for every PKIX_PL_GeneralName.
         * So I concluded it is better to use CERT_CreateGeneralNameList,
         * which keeps an arena pointer in its data structure and also masks
         * out details calls from this libpkix level.
         */

        PKIX_GENERALNAME_DEBUG("\t\tCalling CERT_CreateGeneralNameList).\n");
        nssGenNameList = CERT_CreateGeneralNameList(nssAltName);

        if (nssGenNameList == NULL) {
                PKIX_ERROR(PKIX_CERTCREATEGENERALNAMELISTFAILED);
        }

        genName->nssGeneralNameList = nssGenNameList;

        /* initialize fields */
        genName->type = nameType;
        genName->directoryName = NULL;
        genName->OthName = NULL;
        genName->other = NULL;
        genName->oid = NULL;

        switch (nameType){
        case certOtherName:

                PKIX_CHECK(pkix_pl_OtherName_Create
                            (nssAltName, &otherName, plContext),
                            PKIX_OTHERNAMECREATEFAILED);

                genName->OthName = otherName;
                break;

        case certDirectoryName:

                PKIX_CHECK(pkix_pl_DirectoryName_Create
                            (nssAltName, &pkixDN, plContext),
                            PKIX_DIRECTORYNAMECREATEFAILED);

                genName->directoryName = pkixDN;
                break;
        case certRegisterID:
                PKIX_CHECK(PKIX_PL_OID_CreateBySECItem(&nssAltName->name.other,
                                                       &pkixOID, plContext),
                            PKIX_OIDCREATEFAILED);

                genName->oid = pkixOID;
                break;
        case certDNSName:
        case certEDIPartyName:
        case certIPAddress:
        case certRFC822Name:
        case certX400Address:
        case certURI:
                genName->other = SECITEM_DupItem(&nssAltName->name.other);
                if (!genName->other) {
                    PKIX_ERROR(PKIX_OUTOFMEMORY);
                }     
                break;
        default:
                PKIX_ERROR(PKIX_NAMETYPENOTSUPPORTED);
        }

        *pGenName = genName;
        genName = NULL;

cleanup:
        PKIX_DECREF(genName);

        PKIX_RETURN(GENERALNAME);
}

/*
 * FUNCTION: pkix_pl_GeneralName_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the GeneralName
 *  pointed to by "name" and stores it at "pString" Different mechanisms are
 *  used to create the string, depending on the type of the GeneralName.
 *
 * PARAMETERS
 *  "name"
 *      Address of GeneralName whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a GeneralName Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_GeneralName_ToString_Helper(
        PKIX_PL_GeneralName *name,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_X500Name *pkixDN = NULL;
        PKIX_PL_OID *pkixOID = NULL;
        char *x400AsciiName = NULL;
        char *ediPartyName = NULL;
        char *asciiName = NULL;

        PKIX_ENTER(GENERALNAME, "pkix_pl_GeneralName_ToString_Helper");
        PKIX_NULLCHECK_TWO(name, pString);

        switch (name->type) {
        case certRFC822Name:
        case certDNSName:
        case certURI:
                /*
                 * Note that we can't use PKIX_ESCASCII here because
                 * name->other->data is not guaranteed to be null-terminated.
                 */

                PKIX_NULLCHECK_ONE(name->other);

                PKIX_CHECK(PKIX_PL_String_Create(PKIX_UTF8,
                                                (name->other)->data,
                                                (name->other)->len,
                                                pString,
                                                plContext),
                            PKIX_STRINGCREATEFAILED);
                break;
        case certEDIPartyName:
                /* XXX print out the actual bytes */
                ediPartyName = "EDIPartyName: <DER-encoded value>";
                PKIX_CHECK(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                ediPartyName,
                                                0,
                                                pString,
                                                plContext),
                            PKIX_STRINGCREATEFAILED);
                break;
        case certX400Address:
                /* XXX print out the actual bytes */
                x400AsciiName = "X400Address: <DER-encoded value>";
                PKIX_CHECK(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                x400AsciiName,
                                                0,
                                                pString,
                                                plContext),
                            PKIX_STRINGCREATEFAILED);
                break;
        case certIPAddress:
                PKIX_CHECK(pkix_pl_ipAddrBytes2Ascii
                            (name->other, &asciiName, plContext),
                            PKIX_IPADDRBYTES2ASCIIFAILED);

                PKIX_CHECK(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                asciiName,
                                                0,
                                                pString,
                                                plContext),
                            PKIX_STRINGCREATEFAILED);
                break;
        case certOtherName:
                PKIX_NULLCHECK_ONE(name->OthName);

                /* we only print type-id - don't know how to print value */
                /* XXX print out the bytes of the value */
                PKIX_CHECK(pkix_pl_oidBytes2Ascii
                            (&name->OthName->oid, &asciiName, plContext),
                            PKIX_OIDBYTES2ASCIIFAILED);

                PKIX_CHECK(PKIX_PL_String_Create
                            (PKIX_ESCASCII,
                            asciiName,
                            0,
                            pString,
                            plContext),
                            PKIX_STRINGCREATEFAILED);
                break;
        case certRegisterID:
                pkixOID = name->oid;
                PKIX_CHECK(PKIX_PL_Object_ToString
                            ((PKIX_PL_Object *)pkixOID, pString, plContext),
                            PKIX_OIDTOSTRINGFAILED);
                break;
        case certDirectoryName:
                pkixDN = name->directoryName;
                PKIX_CHECK(PKIX_PL_Object_ToString
                            ((PKIX_PL_Object *)pkixDN, pString, plContext),
                            PKIX_X500NAMETOSTRINGFAILED);
                break;
        default:
                PKIX_ERROR
                        (PKIX_TOSTRINGFORTHISGENERALNAMETYPENOTSUPPORTED);
        }

cleanup:

        PKIX_FREE(asciiName);

        PKIX_RETURN(GENERALNAME);
}

/*
 * FUNCTION: pkix_pl_GeneralName_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_GeneralName_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_GeneralName *name = NULL;
        SECItem secItemName;
        SECItem secItemOID;

        PKIX_ENTER(GENERALNAME, "pkix_pl_GeneralName_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_GENERALNAME_TYPE, plContext),
                    PKIX_OBJECTNOTGENERALNAME);

        name = (PKIX_PL_GeneralName *)object;

        PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_FreeItem).\n");
        SECITEM_FreeItem(name->other, PR_TRUE);
        name->other = NULL;

        if (name->OthName){
                secItemName = name->OthName->name;
                secItemOID = name->OthName->oid;

                PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_FreeItem).\n");
                SECITEM_FreeItem(&secItemName, PR_FALSE);

                PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_FreeItem).\n");
                SECITEM_FreeItem(&secItemOID, PR_FALSE);

                PKIX_FREE(name->OthName);
                name->OthName = NULL;
        }

        if (name->nssGeneralNameList != NULL) {
                PKIX_GENERALNAME_DEBUG
                        ("\t\tCalling CERT_DestroyGeneralNameList).\n");
                CERT_DestroyGeneralNameList(name->nssGeneralNameList);
        }

        PKIX_DECREF(name->directoryName);
        PKIX_DECREF(name->oid);

cleanup:

        PKIX_RETURN(GENERALNAME);
}

/*
 * FUNCTION: pkix_pl_GeneralName_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_GeneralName_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *nameString = NULL;
        PKIX_PL_GeneralName *name = NULL;

        PKIX_ENTER(GENERALNAME, "pkix_pl_GeneralName_toString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_GENERALNAME_TYPE, plContext),
                    PKIX_OBJECTNOTGENERALNAME);

        name = (PKIX_PL_GeneralName *)object;

        PKIX_CHECK(pkix_pl_GeneralName_ToString_Helper
                    (name, &nameString, plContext),
                    PKIX_GENERALNAMETOSTRINGHELPERFAILED);

        *pString = nameString;

cleanup:



        PKIX_RETURN(GENERALNAME);
}

/*
 * FUNCTION: pkix_pl_GeneralName_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_GeneralName_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_GeneralName *name = NULL;
        PKIX_UInt32 firstHash, secondHash, nameHash;

        PKIX_ENTER(GENERALNAME, "pkix_pl_GeneralName_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_GENERALNAME_TYPE, plContext),
                    PKIX_OBJECTNOTGENERALNAME);

        name = (PKIX_PL_GeneralName *)object;

        switch (name->type) {
        case certRFC822Name:
        case certDNSName:
        case certX400Address:
        case certEDIPartyName:
        case certURI:
        case certIPAddress:
                PKIX_NULLCHECK_ONE(name->other);
                PKIX_CHECK(pkix_hash
                            ((const unsigned char *)
                            name->other->data,
                            name->other->len,
                            &nameHash,
                            plContext),
                            PKIX_HASHFAILED);
                break;
        case certRegisterID:
                PKIX_CHECK(PKIX_PL_Object_Hashcode
                            ((PKIX_PL_Object *)name->oid,
                            &nameHash,
                            plContext),
                            PKIX_OIDHASHCODEFAILED);
                break;
        case certOtherName:
                PKIX_NULLCHECK_ONE(name->OthName);
                PKIX_CHECK(pkix_hash
                            ((const unsigned char *)
                            name->OthName->oid.data,
                            name->OthName->oid.len,
                            &firstHash,
                            plContext),
                            PKIX_HASHFAILED);

                PKIX_CHECK(pkix_hash
                            ((const unsigned char *)
                            name->OthName->name.data,
                            name->OthName->name.len,
                            &secondHash,
                            plContext),
                            PKIX_HASHFAILED);

                nameHash = firstHash + secondHash;
                break;
        case certDirectoryName:
                PKIX_CHECK(PKIX_PL_Object_Hashcode
                            ((PKIX_PL_Object *)
                            name->directoryName,
                            &nameHash,
                            plContext),
                            PKIX_X500NAMEHASHCODEFAILED);
                break;
        }

        *pHashcode = nameHash;

cleanup:

        PKIX_RETURN(GENERALNAME);

}

/*
 * FUNCTION: pkix_pl_GeneralName_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_GeneralName_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_GeneralName *firstName = NULL;
        PKIX_PL_GeneralName *secondName = NULL;
        PKIX_UInt32 secondType;

        PKIX_ENTER(GENERALNAME, "pkix_pl_GeneralName_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a GeneralName */
        PKIX_CHECK(pkix_CheckType
                    (firstObject, PKIX_GENERALNAME_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTGENERALNAME);

        /*
         * Since we know firstObject is a GeneralName, if both references are
         * identical, they must be equal
         */
        if (firstObject == secondObject){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't a GeneralName, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObject, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_GENERALNAME_TYPE){
                goto cleanup;
        }

        firstName = (PKIX_PL_GeneralName *)firstObject;
        secondName = (PKIX_PL_GeneralName *)secondObject;

        if (firstName->type != secondName->type){
                goto cleanup;
        }

        switch (firstName->type) {
        case certRFC822Name:
        case certDNSName:
        case certX400Address:
        case certEDIPartyName:
        case certURI:
        case certIPAddress:
                PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_CompareItem).\n");
                if (SECITEM_CompareItem(firstName->other,
                                        secondName->other) != SECEqual) {
                        goto cleanup;
                }
                break;
        case certRegisterID:
                PKIX_CHECK(PKIX_PL_Object_Equals
                            ((PKIX_PL_Object *)firstName->oid,
                            (PKIX_PL_Object *)secondName->oid,
                            pResult,
                            plContext),
                            PKIX_OIDEQUALSFAILED);
                goto cleanup;
        case certOtherName:
                PKIX_NULLCHECK_TWO(firstName->OthName, secondName->OthName);
                PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_CompareItem).\n");
                if (SECITEM_CompareItem(&firstName->OthName->oid,
                                        &secondName->OthName->oid)
                    != SECEqual ||
                    SECITEM_CompareItem(&firstName->OthName->name,
                                        &secondName->OthName->name)
                    != SECEqual) {
                        goto cleanup;
                }
                break;
        case certDirectoryName:
                PKIX_CHECK(PKIX_PL_Object_Equals
                            ((PKIX_PL_Object *)firstName->directoryName,
                            (PKIX_PL_Object *)secondName->directoryName,
                            pResult,
                            plContext),
                            PKIX_X500NAMEEQUALSFAILED);
                goto cleanup;
        }

        *pResult = PKIX_TRUE;

cleanup:

        PKIX_RETURN(GENERALNAME);
}

/*
 * FUNCTION: pkix_pl_GeneralName_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_GENERALNAME_TYPE and related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_GeneralName_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(GENERALNAME, "pkix_pl_GeneralName_RegisterSelf");

        entry.description = "GeneralName";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_GeneralName);
        entry.destructor = pkix_pl_GeneralName_Destroy;
        entry.equalsFunction = pkix_pl_GeneralName_Equals;
        entry.hashcodeFunction = pkix_pl_GeneralName_Hashcode;
        entry.toStringFunction = pkix_pl_GeneralName_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_GENERALNAME_TYPE] = entry;

        PKIX_RETURN(GENERALNAME);
}

/* --Public-Functions------------------------------------------------------- */

#ifdef BUILD_LIBPKIX_TESTS
/*
 * FUNCTION: PKIX_PL_GeneralName_Create (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_GeneralName_Create(
        PKIX_UInt32 nameType,
        PKIX_PL_String *stringRep,
        PKIX_PL_GeneralName **pGName,
        void *plContext)
{
        PKIX_PL_X500Name *pkixDN = NULL;
        PKIX_PL_OID *pkixOID = NULL;
        SECItem *secItem = NULL;
        char *asciiString = NULL;
        PKIX_UInt32 length = 0;
        PKIX_PL_GeneralName *genName = NULL;
        CERTGeneralName *nssGenName = NULL;
        CERTGeneralNameList *nssGenNameList = NULL;
        CERTName *nssCertName = NULL;
        PLArenaPool *arena = NULL;

        PKIX_ENTER(GENERALNAME, "PKIX_PL_GeneralName_Create");
        PKIX_NULLCHECK_TWO(pGName, stringRep);

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                    (stringRep,
                    PKIX_ESCASCII,
                    (void **)&asciiString,
                    &length,
                    plContext),
                    PKIX_STRINGGETENCODEDFAILED);

        /* Create a temporary CERTGeneralName */
        PKIX_GENERALNAME_DEBUG("\t\tCalling PL_strlen).\n");
        length = PL_strlen(asciiString);
        PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_AllocItem).\n");
        secItem = SECITEM_AllocItem(NULL, NULL, length);
        PKIX_GENERALNAME_DEBUG("\t\tCalling PORT_Memcpy).\n");
        (void) PORT_Memcpy(secItem->data, asciiString, length);
        PKIX_CERT_DEBUG("\t\tCalling PORT_NewArena).\n");
        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (arena == NULL) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }
        PKIX_GENERALNAME_DEBUG("\t\tCalling CERT_NewGeneralName).\n");
        nssGenName = CERT_NewGeneralName(arena, nameType);
        if (nssGenName == NULL) {
                PKIX_ERROR(PKIX_ALLOCATENEWCERTGENERALNAMEFAILED);
        }

        switch (nameType) {
        case certRFC822Name:
        case certDNSName:
        case certURI:
                nssGenName->name.other = *secItem;
                break;

        case certDirectoryName:

                PKIX_CHECK(PKIX_PL_X500Name_Create
                            (stringRep, &pkixDN, plContext),
                            PKIX_X500NAMECREATEFAILED);

                PKIX_GENERALNAME_DEBUG("\t\tCalling CERT_AsciiToName).\n");
                nssCertName = CERT_AsciiToName(asciiString);
                nssGenName->name.directoryName = *nssCertName;
                break;

        case certRegisterID:
                PKIX_CHECK(PKIX_PL_OID_Create
                            (asciiString, &pkixOID, plContext),
                            PKIX_OIDCREATEFAILED);
                nssGenName->name.other = *secItem;
                break;
        default:
                /* including IPAddress, EDIPartyName, OtherName, X400Address */
                PKIX_ERROR(PKIX_UNABLETOCREATEGENERALNAMEOFTHISTYPE);
        }

        /* create a PKIX_PL_GeneralName object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_GENERALNAME_TYPE,
                    sizeof (PKIX_PL_GeneralName),
                    (PKIX_PL_Object **)&genName,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        /* create a CERTGeneralNameList */
        nssGenName->type = nameType;
        PKIX_GENERALNAME_DEBUG("\t\tCalling CERT_CreateGeneralNameList).\n");
        nssGenNameList = CERT_CreateGeneralNameList(nssGenName);
        if (nssGenNameList == NULL) {
                PKIX_ERROR(PKIX_CERTCREATEGENERALNAMELISTFAILED);
        }
        genName->nssGeneralNameList = nssGenNameList;

        /* initialize fields */
        genName->type = nameType;
        genName->directoryName = pkixDN;
        genName->OthName = NULL;
        genName->other = secItem;
        genName->oid = pkixOID;

        *pGName = genName;
cleanup:

        PKIX_FREE(asciiString);

        if (nssCertName != NULL) {
                PKIX_CERT_DEBUG("\t\tCalling CERT_DestroyName).\n");
                CERT_DestroyName(nssCertName);
        }

        if (arena){ /* will free nssGenName */
                PKIX_CERT_DEBUG("\t\tCalling PORT_FreeArena).\n");
                PORT_FreeArena(arena, PR_FALSE);
        }

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(pkixDN);
                PKIX_DECREF(pkixOID);

                PKIX_GENERALNAME_DEBUG("\t\tCalling SECITEM_FreeItem).\n");
                if (secItem){
                        SECITEM_FreeItem(secItem, PR_TRUE);
                        secItem = NULL;
                }
        }

        PKIX_RETURN(GENERALNAME);
}

#endif /* BUILD_LIBPKIX_TESTS */
