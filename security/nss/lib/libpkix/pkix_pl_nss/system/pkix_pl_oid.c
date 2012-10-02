/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_oid.c
 *
 * OID Object Functions
 *
 */

#include "pkix_pl_oid.h"

/* --Private-OID-Functions---------------------------------------- */

 /*
 * FUNCTION: pkix_pl_OID_Comparator
 * (see comments for PKIX_PL_ComparatorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OID_Comparator(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Int32 *pRes,
        void *plContext)
{
        PKIX_PL_OID *firstOID = NULL;
        PKIX_PL_OID *secondOID = NULL;

        PKIX_ENTER(OID, "pkix_pl_OID_Comparator");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pRes);

        PKIX_CHECK(pkix_CheckTypes
                    (firstObject, secondObject, PKIX_OID_TYPE, plContext),
                    PKIX_ARGUMENTSNOTOIDS);

        firstOID = (PKIX_PL_OID*)firstObject;
        secondOID = (PKIX_PL_OID*)secondObject;

        *pRes = (PKIX_Int32)SECITEM_CompareItem(&firstOID->derOid,
                                                &secondOID->derOid);
cleanup:
        PKIX_RETURN(OID);
}

/*
 * FUNCTION: pkix_pl_OID_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OID_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_OID *oid = NULL;

        PKIX_ENTER(OID, "pkix_pl_OID_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_OID_TYPE, plContext),
                    PKIX_OBJECTNOTANOID);
        oid = (PKIX_PL_OID*)object;
        SECITEM_FreeItem(&oid->derOid, PR_FALSE);

cleanup:
        PKIX_RETURN(OID);
}

/*
 * FUNCTION: pkix_pl_OID_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OID_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_OID *oid = NULL;

        PKIX_ENTER(OID, "pkix_pl_OID_HashCode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_OID_TYPE, plContext),
                    PKIX_OBJECTNOTANOID);

        oid = (PKIX_PL_OID *)object;

        PKIX_CHECK(pkix_hash
                    ((unsigned char *)oid->derOid.data,
                    oid->derOid.len * sizeof (char),
                    pHashcode,
                    plContext),
                    PKIX_HASHFAILED);
cleanup:

        PKIX_RETURN(OID);
}

/*
 * FUNCTION: pkix_pl_OID_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OID_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        SECComparison cmpResult;

        PKIX_ENTER(OID, "pkix_pl_OID_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        PKIX_CHECK(pkix_CheckType(first, PKIX_OID_TYPE, plContext),
                    PKIX_FIRSTARGUMENTNOTANOID);

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        *pResult = PKIX_FALSE;

        /*
         * Do a quick check that the second object is an OID.
         * If so, check that their lengths are equal.
         */
        if (secondType != PKIX_OID_TYPE) {
                goto cleanup;
        }

        PKIX_CHECK(pkix_pl_OID_Comparator
                    (first, second, &cmpResult, plContext),
                    PKIX_OIDCOMPARATORFAILED);

        *pResult = (cmpResult == SECEqual);
cleanup:

        PKIX_RETURN(OID);
}

/*
 * FUNCTION: pkix_pl_OID_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 * Use this function only for printing OIDs and not to make any
 * critical security decision.
 */
static PKIX_Error *
pkix_pl_OID_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_OID *oid = NULL;
        char *oidString = NULL;

        PKIX_ENTER(OID, "pkix_pl_OID_toString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_OID_TYPE, plContext),
                    PKIX_OBJECTNOTANOID);
        oid = (PKIX_PL_OID*)object;
        oidString = CERT_GetOidString(&oid->derOid);
        
        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, oidString , 0, pString, plContext),
                PKIX_STRINGCREATEFAILED);
cleanup:
        PR_smprintf_free(oidString);
        
        PKIX_RETURN(OID);
}

/*
 * FUNCTION: pkix_pl_OID_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_OID_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_OID_RegisterSelf(
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry *entry = &systemClasses[PKIX_OID_TYPE];

        PKIX_ENTER(OID, "pkix_pl_OID_RegisterSelf");

        entry->description = "OID";
        entry->typeObjectSize = sizeof(PKIX_PL_OID);
        entry->destructor = pkix_pl_OID_Destroy;
        entry->equalsFunction = pkix_pl_OID_Equals;
        entry->hashcodeFunction = pkix_pl_OID_Hashcode;
        entry->toStringFunction = pkix_pl_OID_ToString;
        entry->comparator = pkix_pl_OID_Comparator;
        entry->duplicateFunction = pkix_duplicateImmutable;

        PKIX_RETURN(OID);
}

/*
 * FUNCTION: pkix_pl_OID_GetCriticalExtensionOIDs
 * DESCRIPTION:
 *
 *  Converts the extensions in "extensions" array that are critical to
 *  PKIX_PL_OID and returns the result as a PKIX_List in "pPidList".
 *  If there is no critical extension, an empty list is returned.
 *
 * PARAMETERS
 *  "extension"
 *      an array of extension pointers. May be NULL.
 *  "pOidsList"
 *      Address where the list of OIDs is returned. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRL Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_OID_GetCriticalExtensionOIDs(
        CERTCertExtension **extensions,
        PKIX_List **pOidsList,
        void *plContext)
{
        PKIX_List *oidsList = NULL;
        PKIX_PL_OID *pkixOID = NULL;

        PKIX_ENTER(OID, "pkix_pl_OID_GetCriticalExtensionOIDs");
        PKIX_NULLCHECK_ONE(pOidsList);

        PKIX_CHECK(PKIX_List_Create(&oidsList, plContext),
                    PKIX_LISTCREATEFAILED);

        if (extensions) {
            while (*extensions) {
                CERTCertExtension *extension = NULL;
                SECItem *critical = NULL;
                SECItem *oid = NULL;

                extension = *extensions++;
                /* extension is critical ? */
                critical = &extension->critical;
                if (critical->len == 0 || critical->data[0] == 0) {
                    continue;
                }
                oid = &extension->id;
                PKIX_CHECK(
                    PKIX_PL_OID_CreateBySECItem(oid, &pkixOID, plContext),
                    PKIX_OIDCREATEFAILED);
                PKIX_CHECK(
                    PKIX_List_AppendItem(oidsList, (PKIX_PL_Object *)pkixOID,
                                         plContext),
                    PKIX_LISTAPPENDITEMFAILED);
                PKIX_DECREF(pkixOID);
            }
        }

        *pOidsList = oidsList;
        oidsList = NULL;
        
cleanup:
        PKIX_DECREF(oidsList);
        PKIX_DECREF(pkixOID);
        PKIX_RETURN(OID);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_OID_CreateBySECItem (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_OID_CreateBySECItem(
        SECItem *derOid,
        PKIX_PL_OID **pOID,
        void *plContext)
{
        PKIX_PL_OID *oid = NULL;
        SECStatus rv;
        
        PKIX_ENTER(OID, "PKIX_PL_OID_CreateBySECItem");
        PKIX_NULLCHECK_TWO(pOID, derOid);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                   (PKIX_OID_TYPE,
                    sizeof (PKIX_PL_OID),
                    (PKIX_PL_Object **)&oid,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);
        rv = SECITEM_CopyItem(NULL, &oid->derOid, derOid);
        if (rv != SECSuccess) {
            PKIX_ERROR(PKIX_OUTOFMEMORY);
        }
        *pOID = oid;
        oid = NULL;
        
cleanup:
        PKIX_DECREF(oid);
        
        PKIX_RETURN(OID);
}

/*
 * FUNCTION: PKIX_PL_OID_Create (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_OID_Create(
        SECOidTag idtag,
        PKIX_PL_OID **pOID,
        void *plContext)
{
        SECOidData *oidData = NULL;
    
        PKIX_ENTER(OID, "PKIX_PL_OID_Create");
        PKIX_NULLCHECK_ONE(pOID);

        oidData = SECOID_FindOIDByTag((SECOidTag)idtag);
        if (!oidData) {
            PKIX_ERROR(PKIX_SECOIDFINDOIDTAGDESCRIPTIONFAILED);
        }
        
        pkixErrorResult = 
            PKIX_PL_OID_CreateBySECItem(&oidData->oid, pOID, plContext);
cleanup:
        PKIX_RETURN(OID);
}
