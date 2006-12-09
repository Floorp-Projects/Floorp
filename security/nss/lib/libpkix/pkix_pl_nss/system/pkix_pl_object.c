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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
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
 * pkix_pl_object.c
 *
 * Object Construction, Destruction and Callback Functions
 *
 */

#include "pkix_pl_object.h"

/* --Debugging---------------------------------------------------- */

static PKIX_UInt32 refCountTotal = 0;

/* --Class-Table-Initializers------------------------------------ */

/*
 * Create storage space for 20 Class Table buckets.
 * These are only for user-defined types. System types are registered
 * separately by PKIX_PL_Initialize.
 */

static pkix_pl_HT_Elem*
pkix_Raw_ClassTable_Buckets[] = {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/*
 * Allocate static memory for a ClassTable.
 * XXX This assumes the bucket pointer will fit into a PKIX_UInt32
 */
static pkix_pl_PrimHashTable pkix_Raw_ClassTable = {
        (void *)pkix_Raw_ClassTable_Buckets, /* Buckets */
        20 /* Number of Buckets */
};
static pkix_pl_PrimHashTable * classTable = &pkix_Raw_ClassTable;

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_pl_Object_GetHeader
 * DESCRIPTION:
 *
 *  Shifts Object pointed to by "object" by the sizeof(PKIX_PL_Object) and
 *  stores the value at "pObjectHeader".
 *
 * PARAMETERS:
 *  "object"
 *      Address of Object to shift. Must be non-NULL.
 *  "pObjectHeader"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Object_GetHeader(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pObjectHeader,
        void *plContext)
{
        PKIX_PL_Object *header = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_UInt32 myType;

        PKIX_ENTER(OBJECT, "pkix_pl_Object_GetHeader");
        PKIX_NULLCHECK_TWO(object, pObjectHeader);

        PKIX_OBJECT_DEBUG("\tShifting object pointer).\n");

        /* The header is sizeof(PKIX_PL_Object) before the object pointer */
        header = (PKIX_PL_Object *)((char *)object - sizeof(PKIX_PL_Object));

        myType = header->type;

        if (myType >= PKIX_NUMTYPES) { /* if this is a user-defined type */
                PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                PR_Lock(classTableLock);

                PKIX_CHECK(pkix_pl_PrimHashTable_Lookup
                            (classTable,
                            (void *)&myType,
                            header->type,
                            NULL,
                            (void **)&ctEntry,
                            plContext),
                            PKIX_ERRORGETTINGCLASSTABLEENTRY);

                PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                PR_Unlock(classTableLock);

                if (ctEntry == NULL) {
                        PKIX_ERROR_FATAL(PKIX_UNKNOWNOBJECTTYPE);
                }
        }

        if ((header == NULL)||
            (header->magicHeader != PKIX_MAGIC_HEADER)) {
                return (PKIX_ALLOC_ERROR());
        }

        *pObjectHeader = header;

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: pkix_Destroy_Object
 * DESCRIPTION:
 *
 *  Destroys and deallocates Object pointed to by "object". The caller is
 *  assumed to hold the Object's lock, which is acquired in
 *  PKIX_PL_Object_DecRef().
 *
 * PARAMETERS:
 *  "object"
 *      Address of Object to destroy. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Object_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_Object *objectHeader = NULL;

        PKIX_ENTER(OBJECT, "pkix_pl_Object_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_pl_Object_GetHeader(object, &objectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* Attempt to delete an object still being used */
        if (objectHeader->references != 0) {
                PKIX_ERROR_FATAL(PKIX_OBJECTSTILLREFERENCED);
        }

        objectHeader->magicHeader = 0;
        PKIX_DECREF(objectHeader->stringRep);

        PKIX_CHECK(pkix_UnlockObject(object, plContext),
                    PKIX_ERRORUNLOCKINGOBJECT);

        /* Destroy this object's lock */
        PKIX_OBJECT_DEBUG("\tCalling PR_DestroyLock).\n");
        PR_DestroyLock(objectHeader->lock);
        objectHeader->lock = NULL;
        object = NULL;

        PKIX_FREE(objectHeader);

cleanup:

        PKIX_RETURN(OBJECT);
}

/* --Default-Callbacks-------------------------------------------- */

/*
 * FUNCTION: pkix_pl_Object_Equals_Default
 * DESCRIPTION:
 *
 *  Default Object_Equals callback: Compares the address of the Object pointed
 *  to by "firstObject" with the address of the Object pointed to by
 *  "secondObject" and stores the Boolean result at "pResult".
 *
 * PARAMETERS:
 *  "firstObject"
 *      Address of first Object to compare. Must be non-NULL.
 *  "secondObject"
 *      Address of second Object to compare. Must be non-NULL.
 *  "pResult"
 *      Address where Boolean result will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Object_Equals_Default(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_ENTER(OBJECT, "pkix_pl_Object_Equals_Default");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* Just compare pointer values */
        *pResult = (firstObject == secondObject)?PKIX_TRUE:PKIX_FALSE;

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: pkix_pl_Object_ToString_Default
 * DESCRIPTION:
 *
 *  Default Object_ToString callback: Creates a string consisting of the
 *  typename and address of the Object pointed to by "object" and stores
 *  the result at "pString". The format for the string is
 *  "TypeName@Address: <address>", where the default typename is "Object".
 *
 * PARAMETERS:
 *  "object"
 *      Address of Object to convert to a string. Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Object Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Object_ToString_Default(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *descString = NULL;
        char *format = "%s@Address: %x";
        char *description = NULL;
        PKIX_UInt32 type;

        PKIX_ENTER(OBJECT, "pkix_pl_Object_ToString_Default");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(PKIX_PL_Object_GetType(object, &type, plContext),
                    PKIX_OBJECTGETTYPEFAILED);

        /* Ensure that type code is known. Otherwise, default to Object */
        /* See pkixt.h for all known types. */
        /* XXX Throw an illegal type error here? */
        if (type >= PKIX_NUMTYPES) {
                type = 0;
        }

        /* first, special handling for system types */
        if (type < PKIX_NUMTYPES){
                description = systemClasses[type].description;
        } else {
                PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                PR_Lock(classTableLock);
                pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                        (classTable,
                        (void *)&type,
                        type,
                        NULL,
                        (void **)&ctEntry,
                        plContext);
                PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                PR_Unlock(classTableLock);
                if (pkixErrorResult){
                        PKIX_ERROR_FATAL(PKIX_ERRORGETTINGCLASSTABLEENTRY);
                }

                if (ctEntry == NULL){
                        PKIX_ERROR_FATAL(PKIX_UNDEFINEDCLASSTABLEENTRY);
                } else {
                        description = ctEntry->description;
                }
        }

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    (void *)format,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    (void *)description,
                    0,
                    &descString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (pString,
                    plContext,
                    formatString,
                    descString,
                    object),
                    PKIX_SPRINTFFAILED);

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(descString);

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: pkix_pl_Object_Hashcode_Default
 * DESCRIPTION:
 *
 *  Default Object_Hashcode callback. Creates the a hashcode value using the
 *  address of the Object pointed to by "object" and stores the result at
 *  "pValue".
 *
 *  XXX This isn't great since addresses are not uniformly distributed.
 *
 * PARAMETERS:
 *  "object"
 *      Address of Object to compute hashcode for. Must be non-NULL.
 *  "pValue"
 *      Address where PKIX_UInt32 will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Object_Hashcode_Default(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pValue,
        void *plContext)
{
#ifdef NSS_USE_64
	union {
		void *pointer;
		PKIX_UInt32 hilo[2];
	} extracter;
#endif

        PKIX_ENTER(OBJECT, "pkix_pl_Object_Hashcode_Default");
        PKIX_NULLCHECK_TWO(object, pValue);

#ifdef NSS_USE_64
	extracter.pointer = object;
        *pValue = extracter.hilo[1];
#else
        *pValue = (PKIX_UInt32)object;
#endif

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: pkix_pl_Object_RetrieveEqualsCallback
 * DESCRIPTION:
 *
 *  Retrieves Equals callback function of Object pointed to by "object and
 *  stores it at "pEqualsCallback". If the object's type is one of the system
 *  types, its callback function is retrieved from the systemClasses array;
 *  otherwise, its callback function is retrieve from the classTable hash
 *  table where user-defined types are stored.
 *
 * PARAMETERS:
 *  "object"
 *      Address of Object whose equals callback is desired. Must be non-NULL.
 *  "pEqualsCallback"
 *      Address where EqualsCallback function pointer will be stored.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Object Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Object_RetrieveEqualsCallback(
        PKIX_PL_Object *object,
        PKIX_PL_EqualsCallback *pEqualsCallback,
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        PKIX_PL_Object *objectHeader = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_PL_EqualsCallback func = NULL;
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(OBJECT, "pkix_pl_Object_RetrieveEqualsCallback");
        PKIX_NULLCHECK_TWO(object, pEqualsCallback);

        PKIX_CHECK(pkix_pl_Object_GetHeader
                    (object, &objectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* first, special handling for system types */
        if (objectHeader->type < PKIX_NUMTYPES){
                entry = systemClasses[objectHeader->type];
                func = entry.equalsFunction;
                if (func == NULL){
                        func = pkix_pl_Object_Equals_Default;
                }
                *pEqualsCallback = func;
        } else {
                PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                PR_Lock(classTableLock);
                pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                        (classTable,
                        (void *)&objectHeader->type,
                        objectHeader->type,
                        NULL,
                        (void **)&ctEntry,
                        plContext);
                PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                PR_Unlock(classTableLock);
                if (pkixErrorResult){
                        PKIX_ERROR(PKIX_ERRORGETTINGCLASSTABLEENTRY);
                }

                if ((ctEntry == NULL) || (ctEntry->equalsFunction == NULL)) {
                        PKIX_ERROR(PKIX_UNDEFINEDEQUALSCALLBACK);
                } else {
                        *pEqualsCallback = ctEntry->equalsFunction;
                }
        }

cleanup:

        PKIX_RETURN(OBJECT);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_Object_Alloc (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_Alloc(
        PKIX_UInt32 type,
        PKIX_UInt32 size,
        PKIX_PL_Object **pObject,
        void *plContext)
{
        PKIX_PL_Object *object = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_Boolean typeRegistered;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_Alloc");
        PKIX_NULLCHECK_ONE(pObject);

        /*
         * We need to ensure that user-defined types have been registered.
         * All system types have already been registered by PKIX_PL_Initialize.
         */

        if (type >= PKIX_NUMTYPES) { /* i.e. if this is a user-defined type */
                PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                PR_Lock(classTableLock);
                pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                        (classTable,
                        (void *)&type,
                        type,
                        NULL,
                        (void **)&ctEntry,
                        plContext);
                PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                PR_Unlock(classTableLock);
                if (pkixErrorResult){
                        PKIX_ERROR_FATAL(PKIX_COULDNOTLOOKUPINHASHTABLE);
                }

                typeRegistered = (ctEntry != NULL);

                if (!typeRegistered) {
                        PKIX_ERROR_FATAL(PKIX_UNKNOWNTYPEARGUMENT);
                }
        }

        /* Allocate space for the object header and the requested size */
        PKIX_CHECK(PKIX_PL_Malloc
                    (((PKIX_UInt32)sizeof (PKIX_PL_Object))+size,
                    (void **)&object,
                    plContext),
                    PKIX_MALLOCFAILED);

        /* Initialize all object fields */
        object->magicHeader = PKIX_MAGIC_HEADER;
        object->type = type;
        object->references = 1; /* Default to a single reference */
        object->stringRep = NULL;
        object->hashcode = 0;
        object->hashcodeCached = 0;

        /* XXX Debugging - not thread safe */
        refCountTotal++;

        PKIX_REF_COUNT_DEBUG_ARG("PKIX_PL_Object_Alloc: refCountTotal == %d \n",
                            refCountTotal);

        /* Cannot use PKIX_PL_Mutex because it depends on Object */
        /* Using NSPR Locks instead */
        PKIX_OBJECT_DEBUG("\tCalling PR_NewLock).\n");
        object->lock = PR_NewLock();
        if (object->lock == NULL) {
                PKIX_FREE(pObject);
                return (PKIX_ALLOC_ERROR());
        }

        PKIX_OBJECT_DEBUG("\tShifting object pointer).\n");

        /* Return a pointer to the user data. Need to offset by object size */
        *pObject = object + 1;

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_IsTypeRegistered (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_IsTypeRegistered(
        PKIX_UInt32 type,
        PKIX_Boolean *pBool,
        void *plContext)
{
        pkix_ClassTable_Entry *ctEntry = NULL;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_IsTypeRegistered");
        PKIX_NULLCHECK_ONE(pBool);

        /* first, we handle the system types */
        if (type < PKIX_NUMTYPES) {
                *pBool = PKIX_TRUE;
                goto cleanup;
        }

        PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
        PR_Lock(classTableLock);
        pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                (classTable,
                (void *)&type,
                type,
                NULL,
                (void **)&ctEntry,
                plContext);
        PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
        PR_Unlock(classTableLock);

        if (pkixErrorResult){
                PKIX_ERROR_FATAL(PKIX_COULDNOTLOOKUPINHASHTABLE);
        }

        *pBool = (ctEntry != NULL);

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_RegisterType (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_RegisterType(
        PKIX_UInt32 type,
        char *description,
        PKIX_PL_DestructorCallback destructor,
        PKIX_PL_EqualsCallback equalsFunction,
        PKIX_PL_HashcodeCallback hashcodeFunction,
        PKIX_PL_ToStringCallback toStringFunction,
        PKIX_PL_ComparatorCallback comparator,
        PKIX_PL_DuplicateCallback duplicateFunction,
        void *plContext)
{
        pkix_ClassTable_Entry *ctEntry = NULL;
        pkix_pl_Integer *key = NULL;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_RegisterType");

        /*
         * System types are registered on startup by PKIX_PL_Initialize.
         * These can not be overwritten.
         */

        if (type < PKIX_NUMTYPES) { /* if this is a system type */
                PKIX_ERROR(PKIX_CANTREREGISTERSYSTEMTYPE);
        }

        PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
        PR_Lock(classTableLock);
        PKIX_CHECK(pkix_pl_PrimHashTable_Lookup
                    (classTable,
                    (void *)&type,
                    type,
                    NULL,
                    (void **)&ctEntry,
                    plContext),
                    PKIX_PRIMHASHTABLELOOKUPFAILED);

        /* If the type is already registered, throw an error */
        if (ctEntry) {
                PKIX_ERROR(PKIX_TYPEALREADYREGISTERED);
        }

        PKIX_CHECK(PKIX_PL_Malloc
                    (((PKIX_UInt32)sizeof (pkix_ClassTable_Entry)),
                    (void **)&ctEntry,
                    plContext),
                    PKIX_MALLOCFAILED);

        /* Set Default Values if none specified */

        if (description == NULL){
                description = "Object";
        }

        if (equalsFunction == NULL) {
                equalsFunction = pkix_pl_Object_Equals_Default;
        }

        if (toStringFunction == NULL) {
                toStringFunction = pkix_pl_Object_ToString_Default;
        }

        if (hashcodeFunction == NULL) {
                hashcodeFunction = pkix_pl_Object_Hashcode_Default;
        }

        ctEntry->destructor = destructor;
        ctEntry->equalsFunction = equalsFunction;
        ctEntry->toStringFunction = toStringFunction;
        ctEntry->hashcodeFunction = hashcodeFunction;
        ctEntry->comparator = comparator;
        ctEntry->duplicateFunction = duplicateFunction;
        ctEntry->description = description;

        PKIX_CHECK(PKIX_PL_Malloc
                    (((PKIX_UInt32)sizeof (pkix_pl_Integer)),
                    (void **)&key,
                    plContext),
                    PKIX_COULDNOTMALLOCNEWKEY);

        key->ht_int = type;

        PKIX_CHECK(pkix_pl_PrimHashTable_Add
                    (classTable,
                    (void *)key,
                    (void *)ctEntry,
                    type,
                    NULL,
                    plContext),
                    PKIX_PRIMHASHTABLEADDFAILED);

cleanup:
        PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
        PR_Unlock(classTableLock);

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_IncRef (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_IncRef(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_Boolean refCountError = PKIX_FALSE;
        PKIX_PL_Object *objectHeader = NULL;
        PKIX_PL_NssContext *context = NULL;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_IncRef");
        PKIX_NULLCHECK_ONE(object);

        if (plContext){
                /* 
                 * PKIX_PL_NssContext is not a complete PKIX Type, it doesn't
                 * have a header therefore we cannot verify its type before
                 * casting.
                 */  
                context = (PKIX_PL_NssContext *) plContext;
                if (context->arena != NULL) {
                        goto cleanup;
                }
        }

        if (object == (PKIX_PL_Object*)PKIX_ALLOC_ERROR()) {
                PKIX_ERROR_FATAL(PKIX_ATTEMPTTOINCREFALLOCERROR);
        }

        /* Shift pointer from user data to object header */
        PKIX_CHECK(pkix_pl_Object_GetHeader(object, &objectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* Lock the test and increment */
        PKIX_OBJECT_DEBUG("\tAcquiring object lock).\n");
        PKIX_CHECK(pkix_LockObject(object, plContext),
                    PKIX_ERRORLOCKINGOBJECT);

        /* This object should never have zero references */
        refCountError = (objectHeader->references == 0);

        if (!refCountError) {
                objectHeader->references++;
                refCountTotal++;
                PKIX_REF_COUNT_DEBUG_ARG("PKIX_PL_Object_IncRef: "
                                    "refCountTotal == %d \n", refCountTotal);
        }

        PKIX_CHECK(pkix_UnlockObject(object, plContext),
                    PKIX_ERRORUNLOCKINGOBJECT);

        if (refCountError) {
                PKIX_THROW
                    (FATAL,
                    PKIX_ErrorText[PKIX_OBJECTWITHNONPOSITIVEREFERENCES]);
        }

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_DecRef (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_DecRef(
        PKIX_PL_Object *object,
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        PKIX_PL_DestructorCallback destructor = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_PL_DestructorCallback func = NULL;
        PKIX_PL_Object *objectHeader = NULL;
        pkix_ClassTable_Entry entry;
        PKIX_PL_NssContext *context = NULL;
        PKIX_Boolean refCountError = PKIX_FALSE;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_DecRef");
        PKIX_NULLCHECK_ONE(object);

        if (plContext){
                /* 
                 * PKIX_PL_NssContext is not a complete PKIX Type, it doesn't
                 * have a header therefore we cannot verify its type before
                 * casting.
                 */  
                context = (PKIX_PL_NssContext *) plContext;
                if (context->arena != NULL) {
                        goto cleanup;
                }
        }

        if (object == (PKIX_PL_Object*)PKIX_ALLOC_ERROR()) {
                PKIX_ERROR_FATAL(PKIX_ATTEMPTTODECREFALLOCERROR);
        }

        /* Shift pointer from user data to object header */
        PKIX_CHECK(pkix_pl_Object_GetHeader(object, &objectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* Lock the test for the reference count and the decrement */
        PKIX_OBJECT_DEBUG("\tAcquiring object lock).\n");
        PKIX_CHECK(pkix_LockObject(object, plContext),
                    PKIX_ERRORLOCKINGOBJECT);

        refCountError = (objectHeader->references == 0);

        if (!refCountError) {
                objectHeader->references--;
                refCountTotal--;

                PKIX_REF_COUNT_DEBUG_ARG("PKIX_PL_Object_DecRef: "
                                    "refCountTotal == %d \n", refCountTotal);

                if (objectHeader->references == 0) {
                        /* first, special handling for system types */
                        if (objectHeader->type < PKIX_NUMTYPES){
                                entry = systemClasses[objectHeader->type];
                                func = entry.destructor;
                                if (func != NULL){
                                /* Call destructor on user data if necessary */
                                    PKIX_CHECK_FATAL(func(object, plContext),
                                             PKIX_ERRORINOBJECTDEFINEDESTROY);
                                }
                        } else {
                                PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                                PR_Lock(classTableLock);
                                pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                                        (classTable,
                                        (void *)&objectHeader->type,
                                        objectHeader->type,
                                        NULL,
                                        (void **)&ctEntry,
                                        plContext);
                                PKIX_OBJECT_DEBUG
                                        ("\tCalling PR_Unlock).\n");
                                PR_Unlock(classTableLock);
                                if (pkixErrorResult){
                                    PKIX_ERROR_FATAL
                                            (PKIX_ERRORINGETTINGDESTRUCTOR);
                                }

                                if (ctEntry != NULL){
                                        destructor = ctEntry->destructor;
                                }

                                if (destructor != NULL) {
                                /* Call destructor on user data if necessary */
                                        PKIX_CHECK_FATAL(destructor
                                                (object, plContext),
                                                PKIX_ERRORINOBJECTDEFINEDESTROY);
                                }
                        }

                        /* pkix_pl_Object_Destroy assumes the lock is held */
                        /* It will call unlock and destroy the object */
                        return (pkix_pl_Object_Destroy(object, plContext));

                }
        }

        PKIX_OBJECT_DEBUG("\tReleasing object lock).\n");
        PKIX_CHECK(pkix_UnlockObject(object, plContext),
                    PKIX_ERRORUNLOCKINGOBJECT);

        /* if a reference count was already zero, throw an error */
        if (refCountError) {
                return (PKIX_ALLOC_ERROR());
        }

cleanup:

        PKIX_RETURN(OBJECT);
}



/*
 * FUNCTION: PKIX_PL_Object_Equals (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        PKIX_PL_Object *firstObjectHeader = NULL;
        PKIX_PL_Object *secondObjectHeader = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_PL_EqualsCallback func = NULL;
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        PKIX_CHECK(pkix_pl_Object_GetHeader
                    (firstObject, &firstObjectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        PKIX_CHECK(pkix_pl_Object_GetHeader
                    (secondObject, &secondObjectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* if hashcodes are cached but not equal, objects can't be equal */
        if (firstObjectHeader->hashcodeCached &&
            secondObjectHeader->hashcodeCached){
                if (firstObjectHeader->hashcode !=
                    secondObjectHeader->hashcode){
                        *pResult = PKIX_FALSE;
                        goto cleanup;
                }
        }

        /* first, special handling for system types */
        if (firstObjectHeader->type < PKIX_NUMTYPES){
                entry = systemClasses[firstObjectHeader->type];
                func = entry.equalsFunction;
                if (func == NULL){
                        func = pkix_pl_Object_Equals_Default;
                }
        } else {
                PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                PR_Lock(classTableLock);
                pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                        (classTable,
                        (void *)&firstObjectHeader->type,
                        firstObjectHeader->type,
                        NULL,
                        (void **)&ctEntry,
                        plContext);
                PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                PR_Unlock(classTableLock);

                if (pkixErrorResult){
                        PKIX_ERROR_FATAL(PKIX_ERRORGETTINGCLASSTABLEENTRY);
                }

                if ((ctEntry == NULL) || (ctEntry->equalsFunction == NULL)) {
                        PKIX_ERROR_FATAL(PKIX_UNDEFINEDCALLBACK);
                } else {
                        func = ctEntry->equalsFunction;
                }
        }

        PKIX_CHECK(func(firstObject, secondObject, pResult, plContext),
                    PKIX_OBJECTSPECIFICFUNCTIONFAILED);

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_Duplicate (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_Duplicate(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        PKIX_PL_Object *firstObjectHeader = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_PL_DuplicateCallback func = NULL;
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_Duplicate");
        PKIX_NULLCHECK_TWO(firstObject, pNewObject);

        PKIX_CHECK(pkix_pl_Object_GetHeader
                    (firstObject, &firstObjectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* first, special handling for system types */
        if (firstObjectHeader->type < PKIX_NUMTYPES){
                entry = systemClasses[firstObjectHeader->type];
                func = entry.duplicateFunction;
                if (!func){
                        PKIX_ERROR_FATAL(PKIX_UNDEFINEDDUPLICATEFUNCTION);
                }
        } else {
                PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                PR_Lock(classTableLock);
                pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                        (classTable,
                        (void *)&firstObjectHeader->type,
                        firstObjectHeader->type,
                        NULL,
                        (void **)&ctEntry,
                        plContext);
                PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                PR_Unlock(classTableLock);

                if (pkixErrorResult){
                        PKIX_ERROR_FATAL(PKIX_ERRORGETTINGCLASSTABLEENTRY);
                }

                if ((ctEntry == NULL) || (ctEntry->duplicateFunction == NULL)) {
                        PKIX_ERROR_FATAL(PKIX_UNDEFINEDCALLBACK);
                } else {
                        func = ctEntry->duplicateFunction;
                }
        }

        PKIX_CHECK(func(firstObject, pNewObject, plContext),
                    PKIX_OBJECTSPECIFICFUNCTIONFAILED);

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_Hashcode (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pValue,
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        PKIX_PL_Object *objectHeader = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_PL_HashcodeCallback func = NULL;
        pkix_ClassTable_Entry entry;
        PKIX_Boolean objectLocked = PKIX_FALSE;
        PKIX_UInt32 objectHash;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_Hashcode");
        PKIX_NULLCHECK_TWO(object, pValue);

        /* Shift pointer from user data to object header */
        PKIX_CHECK(pkix_pl_Object_GetHeader(object, &objectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* if we don't have a cached copy from before, we create one */
        if (!objectHeader->hashcodeCached){

                /* first, special handling for system types */
                if (objectHeader->type < PKIX_NUMTYPES){
                        entry = systemClasses[objectHeader->type];
                        func = entry.hashcodeFunction;
                        if (func == NULL){
                                func = pkix_pl_Object_Hashcode_Default;
                        }
                } else {
                        PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                        PR_Lock(classTableLock);
                        pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                                (classTable,
                                (void *)&objectHeader->type,
                                objectHeader->type,
                                NULL,
                                (void **)&ctEntry,
                                plContext);
                        PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                        PR_Unlock(classTableLock);

                        if (pkixErrorResult){
                                PKIX_ERROR_FATAL
                                        (PKIX_ERRORGETTINGCLASSTABLEENTRY);
                        }

                        if ((ctEntry == NULL) ||
                            (ctEntry->hashcodeFunction == NULL)) {
                                PKIX_ERROR_FATAL(PKIX_UNDEFINEDCALLBACK);
                        }

                        func = ctEntry->hashcodeFunction;
                }

                PKIX_CHECK(func(object, &objectHash, plContext),
                            PKIX_OBJECTSPECIFICFUNCTIONFAILED);

                if (!objectHeader->hashcodeCached){

                        PKIX_CHECK(pkix_LockObject(object, plContext),
                                    PKIX_ERRORLOCKINGOBJECT);
                        objectLocked = PKIX_TRUE;

                        if (!objectHeader->hashcodeCached){
                                /* save cached copy in case we need it again */
                                objectHeader->hashcode = objectHash;
                                objectHeader->hashcodeCached = PKIX_TRUE;
                        }

                        PKIX_CHECK(pkix_UnlockObject(object, plContext),
                                    PKIX_ERRORUNLOCKINGOBJECT);
                        objectLocked = PKIX_FALSE;
                }
        }

        *pValue = objectHeader->hashcode;

cleanup:

        if (objectLocked){
                pkixTempResult = pkix_UnlockObject(object, plContext);
                if (pkixTempResult) return pkixTempResult;
        }

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_ToString (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        PKIX_PL_Object *objectHeader = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_PL_ToStringCallback func = NULL;
        pkix_ClassTable_Entry entry;
        PKIX_Boolean objectLocked = PKIX_FALSE;
        PKIX_PL_String *objectString;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        /* Shift pointer from user data to object header */
        PKIX_CHECK(pkix_pl_Object_GetHeader(object, &objectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* if we don't have a cached copy from before, we create one */
        if (!objectHeader->stringRep){

                /* first, special handling for system types */
                if (objectHeader->type < PKIX_NUMTYPES){
                        entry = systemClasses[objectHeader->type];
                        func = entry.toStringFunction;
                        if (func == NULL){
                                func = pkix_pl_Object_ToString_Default;
                        }
                } else {
                        PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                        PR_Lock(classTableLock);
                        pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                                (classTable,
                                (void *)&objectHeader->type,
                                objectHeader->type,
                                NULL,
                                (void **)&ctEntry,
                                plContext);
                        PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                        PR_Unlock(classTableLock);
                        if (pkixErrorResult){
                                PKIX_ERROR_FATAL
                                        (PKIX_ERRORGETTINGCLASSTABLEENTRY);
                        }

                        if ((ctEntry == NULL) ||
                            (ctEntry->toStringFunction == NULL)) {
                                PKIX_ERROR_FATAL(PKIX_UNDEFINEDCALLBACK);
                        }

                        func = ctEntry->toStringFunction;
                }

                PKIX_CHECK(func(object, &objectString, plContext),
                            PKIX_OBJECTSPECIFICFUNCTIONFAILED);

                if (!objectHeader->stringRep){

                        PKIX_CHECK(pkix_LockObject(object, plContext),
                                    PKIX_ERRORLOCKINGOBJECT);
                        objectLocked = PKIX_TRUE;

                        if (!objectHeader->stringRep){
                                /* save a cached copy */
                                objectHeader->stringRep = objectString;
                        }

                        PKIX_CHECK(pkix_UnlockObject(object, plContext),
                                    PKIX_ERRORUNLOCKINGOBJECT);
                        objectLocked = PKIX_FALSE;
                }
        }

        PKIX_INCREF(objectHeader->stringRep);
        *pString = objectHeader->stringRep;

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_InvalidateCache (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_InvalidateCache(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_Object *objectHeader = NULL;
        PKIX_Boolean objectLocked = PKIX_FALSE;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_InvalidateCache");
        PKIX_NULLCHECK_ONE(object);

        /* Shift pointer from user data to object header */
        PKIX_CHECK(pkix_pl_Object_GetHeader(object, &objectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        PKIX_CHECK(pkix_LockObject(object, plContext),
                    PKIX_ERRORLOCKINGOBJECT);
        objectLocked = PKIX_TRUE;

        /* invalidate hashcode */
        objectHeader->hashcode = 0;
        objectHeader->hashcodeCached = PKIX_FALSE;

        PKIX_DECREF(objectHeader->stringRep);

        PKIX_CHECK(pkix_UnlockObject(object, plContext),
                    PKIX_ERRORUNLOCKINGOBJECT);
        objectLocked = PKIX_FALSE;


cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_Compare (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_Compare(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Int32 *pResult,
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        PKIX_PL_Object *firstObjectHeader = NULL;
        PKIX_PL_Object *secondObjectHeader = NULL;
        pkix_ClassTable_Entry *ctEntry = NULL;
        PKIX_PL_ComparatorCallback func = NULL;
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_Compare");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* Shift pointer from user data to object header */
        PKIX_CHECK(pkix_pl_Object_GetHeader
                    (firstObject, &firstObjectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* Shift pointer from user data to object header */
        PKIX_CHECK(pkix_pl_Object_GetHeader
                    (secondObject, &secondObjectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        /* first, special handling for system types */
        if (firstObjectHeader->type < PKIX_NUMTYPES){
                entry = systemClasses[firstObjectHeader->type];
                func = entry.comparator;
                if (!func){
                        PKIX_ERROR(PKIX_UNDEFINEDCOMPARATOR);
                }
        } else {
                PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
                PR_Lock(classTableLock);
                pkixErrorResult = pkix_pl_PrimHashTable_Lookup
                        (classTable,
                        (void *)&firstObjectHeader->type,
                        firstObjectHeader->type,
                        NULL,
                        (void **)&ctEntry,
                        plContext);
                PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
                PR_Unlock(classTableLock);
                if (pkixErrorResult){
                        PKIX_ERROR_FATAL(PKIX_ERRORGETTINGCLASSTABLEENTRY);
                }

                if ((ctEntry == NULL) || (ctEntry->comparator == NULL)) {
                        PKIX_ERROR_FATAL(PKIX_UNDEFINEDCOMPARATOR);
                }

                func = ctEntry->comparator;
        }

        PKIX_CHECK(func(firstObject, secondObject, pResult, plContext),
                    PKIX_OBJECTSPECIFICFUNCTIONFAILED);

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_Lock (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_Lock(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ENTER(OBJECT, "PKIX_PL_Object_Lock");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_LockObject(object, plContext),
                    PKIX_LOCKOBJECTFAILED);

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: PKIX_PL_Object_Unlock (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_Unlock(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ENTER(OBJECT, "PKIX_PL_Object_Unlock");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_UnlockObject(object, plContext),
                    PKIX_UNLOCKOBJECTFAILED);

cleanup:

        PKIX_RETURN(OBJECT);
}


/*
 * FUNCTION: PKIX_PL_Object_GetType (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Object_GetType(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pType,
        void *plContext)
{
        PKIX_PL_Object *objectHeader = NULL;

        PKIX_ENTER(OBJECT, "PKIX_PL_Object_GetType");
        PKIX_NULLCHECK_TWO(object, pType);

        /* Shift pointer from user data to object header */
        PKIX_CHECK(pkix_pl_Object_GetHeader(object, &objectHeader, plContext),
                    PKIX_RECEIVEDCORRUPTEDOBJECTARGUMENT);

        *pType = objectHeader->type;

cleanup:

        PKIX_RETURN(OBJECT);
}
