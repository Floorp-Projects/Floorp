/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_bytearray.c
 *
 * ByteArray Object Functions
 *
 */

#include "pkix_pl_bytearray.h"

/* --Private-ByteArray-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_ByteArray_ToHexString
 * DESCRIPTION:
 *
 *  Creates a hex-String representation of the ByteArray pointed to by "array"
 *  and stores the result at "pString". The hex-String consists of hex-digit
 *  pairs separated by spaces, and the entire string enclosed within square
 *  brackets, e.g. [43 61 6E 20 79 6F 75 20 72 65 61 64 20 74 68 69 73 3F].
 *  A zero-length ByteArray is represented as [].
 * PARAMETERS
 *  "array"
 *      ByteArray to be represented by the hex-String; must be non-NULL
 *  "pString"
 *      Address where String will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_ByteArray_ToHexString(
        PKIX_PL_ByteArray *array,
        PKIX_PL_String **pString,
        void *plContext)
{
        char *tempText = NULL;
        char *stringText = NULL; /* "[XX XX XX ...]" */
        PKIX_UInt32 i, outputLen, bufferSize;

        PKIX_ENTER(BYTEARRAY, "pkix_pl_ByteArray_ToHexString");
        PKIX_NULLCHECK_TWO(array, pString);

        if ((array->length) == 0) {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII, "[]", 0, pString, plContext),
                        PKIX_COULDNOTCREATESTRING);
        } else {
                /*
                 * Allocate space for format string
                 * '[' + "XX" + (n-1)*" XX" + ']' + '\0'
                 */
                bufferSize = 2 + (3*(array->length));

                PKIX_CHECK(PKIX_PL_Malloc
                        (bufferSize, (void **)&stringText, plContext),
                        PKIX_COULDNOTALLOCATEMEMORY);

                stringText[0] = 0;
                outputLen = 0;

                PKIX_BYTEARRAY_DEBUG("\tCalling PR_smprintf).\n");
                tempText = PR_smprintf
                        ("[%02X", (0x0FF&((char *)(array->array))[0]));
                PKIX_BYTEARRAY_DEBUG("\tCalling PL_strlen).\n");
                outputLen += PL_strlen(tempText);

                PKIX_BYTEARRAY_DEBUG("\tCalling PL_strcat).\n");
                stringText = PL_strcat(stringText, tempText);

                PKIX_BYTEARRAY_DEBUG("\tCalling PR_smprintf_free).\n");
                PR_smprintf_free(tempText);

                for (i = 1; i < array->length; i++) {
                        PKIX_BYTEARRAY_DEBUG("\tCalling PR_smprintf).\n");
                        tempText = PR_smprintf
                                (" %02X", (0x0FF&((char *)(array->array))[i]));

                        if (tempText == NULL){
                                PKIX_ERROR(PKIX_PRSMPRINTFFAILED);
                        }

                        PKIX_BYTEARRAY_DEBUG("\tCalling PL_strlen).\n");
                        outputLen += PL_strlen(tempText);

                        PKIX_BYTEARRAY_DEBUG("\tCalling PL_strcat).\n");
                        stringText = PL_strcat(stringText, tempText);

                        PKIX_BYTEARRAY_DEBUG("\tCalling PR_smprintf_free).\n");
                        PR_smprintf_free(tempText);
                        tempText = NULL;
                }

                stringText[outputLen++] = ']';
                stringText[outputLen] = 0;

                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII,
                        stringText,
                        0,
                        pString,
                        plContext),
                        PKIX_COULDNOTCREATESTRING);
        }

cleanup:

        PKIX_FREE(stringText);
        PKIX_RETURN(BYTEARRAY);
}

/*
 * FUNCTION: pkix_pl_ByteArray_Comparator
 * (see comments for PKIX_PL_ComparatorCallback in pkix_pl_system.h)
 *
 *  NOTE:
 *  It is not clear that this definition of comparing byte arrays makes
 *  sense. It does allow you to tell whether two blocks of memory are
 *  identical, so we only use it for the Equals function (i.e. we don't
 *  register it as a Compare function for ByteArray).
 */
static PKIX_Error *
pkix_pl_ByteArray_Comparator(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Int32 *pResult,
        void *plContext)
{
        PKIX_PL_ByteArray *firstByteArray = NULL;
        PKIX_PL_ByteArray *secondByteArray = NULL;
        unsigned char *firstData = NULL;
        unsigned char *secondData = NULL;
        PKIX_UInt32 i;

        PKIX_ENTER(BYTEARRAY, "pkix_pl_ByteArray_Comparator");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        PKIX_CHECK(pkix_CheckTypes
                (firstObject, secondObject, PKIX_BYTEARRAY_TYPE, plContext),
                PKIX_ARGUMENTSNOTBYTEARRAYS);

        /* It's safe to cast */
        firstByteArray = (PKIX_PL_ByteArray *)firstObject;
        secondByteArray = (PKIX_PL_ByteArray *)secondObject;

        *pResult = 0;
        firstData = (unsigned char *)firstByteArray->array;
        secondData = (unsigned char *)secondByteArray->array;

        if (firstByteArray->length < secondByteArray->length) {
                *pResult = -1;
        } else if (firstByteArray->length > secondByteArray->length) {
                *pResult = 1;
        } else if (firstByteArray->length == secondByteArray->length) {
                /* Check if both array contents are identical */
                for (i = 0;
                    (i < firstByteArray->length) && (*pResult == 0);
                    i++) {
                        if (firstData[i] < secondData[i]) {
                                *pResult = -1;
                        } else if (firstData[i] > secondData[i]) {
                                *pResult = 1;
                        }
                }
        }

cleanup:

        PKIX_RETURN(BYTEARRAY);
}

/*
 * FUNCTION: pkix_pl_ByteArray_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_ByteArray_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_ByteArray *array = NULL;
        char *tempText = NULL;
        char *stringText = NULL; /* "[OOO, OOO, ... OOO]" */
        PKIX_UInt32 i, outputLen, bufferSize;

        PKIX_ENTER(BYTEARRAY, "pkix_pl_ByteArray_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BYTEARRAY_TYPE, plContext),
                    PKIX_OBJECTNOTBYTEARRAY);

        array = (PKIX_PL_ByteArray *)object;

        if ((array->length) == 0) {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII, "[]", 0, pString, plContext),
                        PKIX_COULDNOTCREATESTRING);
        } else {
                /* Allocate space for "XXX, ". */
                bufferSize = 2+5*array->length;

                /* Allocate space for format string */
                PKIX_CHECK(PKIX_PL_Malloc
                        (bufferSize, (void **)&stringText, plContext),
                        PKIX_MALLOCFAILED);

                stringText[0] = 0;
                outputLen = 0;

                PKIX_BYTEARRAY_DEBUG("\tCalling PR_smprintf).\n");
                tempText =
                        PR_smprintf
                            ("[%03u", (0x0FF&((char *)(array->array))[0]));
                PKIX_BYTEARRAY_DEBUG("\tCalling PL_strlen).\n");
                outputLen += PL_strlen(tempText);

                PKIX_BYTEARRAY_DEBUG("\tCalling PL_strcat).\n");
                stringText = PL_strcat(stringText, tempText);

                PKIX_BYTEARRAY_DEBUG("\tCalling PR_smprintf_free).\n");
                PR_smprintf_free(tempText);

                for (i = 1; i < array->length; i++) {
                        PKIX_BYTEARRAY_DEBUG("\tCalling PR_smprintf).\n");
                        tempText = PR_smprintf
                                (", %03u",
                                (0x0FF&((char *)(array->array))[i]));

                        if (tempText == NULL){
                                PKIX_ERROR(PKIX_PRSMPRINTFFAILED);
                        }

                        PKIX_BYTEARRAY_DEBUG("\tCalling PL_strlen).\n");
                        outputLen += PL_strlen(tempText);

                        PKIX_BYTEARRAY_DEBUG("\tCalling PL_strcat).\n");
                        stringText = PL_strcat(stringText, tempText);

                        PKIX_BYTEARRAY_DEBUG("\tCalling PR_smprintf_free).\n");
                        PR_smprintf_free(tempText);
                        tempText = NULL;
                }

                stringText[outputLen++] = ']';
                stringText[outputLen] = 0;

                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII, stringText, 0, pString, plContext),
                        PKIX_STRINGCREATEFAILED);

        }

cleanup:

        PKIX_FREE(stringText);
        PKIX_RETURN(BYTEARRAY);
}

/*
 * FUNCTION: pkix_pl_ByteArray_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_ByteArray_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Int32 cmpResult = 0;

        PKIX_ENTER(BYTEARRAY, "pkix_pl_ByteArray_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        /* Sanity check: Test that "first" is a ByteArray */
        PKIX_CHECK(pkix_CheckType(first, PKIX_BYTEARRAY_TYPE, plContext),
                    PKIX_FIRSTARGUMENTNOTBYTEARRAY);

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        /* If types differ, then we will return false */
        *pResult = PKIX_FALSE;

        /* Second type may not be a BA */
        if (secondType != PKIX_BYTEARRAY_TYPE) goto cleanup;

        /* It's safe to cast here */
        PKIX_CHECK(pkix_pl_ByteArray_Comparator
                (first, second, &cmpResult, plContext),
                PKIX_BYTEARRAYCOMPARATORFAILED);

        /* ByteArrays are equal iff Comparator Result is 0 */
        *pResult = (cmpResult == 0);

cleanup:

        PKIX_RETURN(BYTEARRAY);
}

/*
 * FUNCTION: pkix_pl_ByteArray_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_ByteArray_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_ByteArray *array = NULL;

        PKIX_ENTER(BYTEARRAY, "pkix_pl_ByteArray_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BYTEARRAY_TYPE, plContext),
                    PKIX_OBJECTNOTBYTEARRAY);

        array = (PKIX_PL_ByteArray*)object;

        PKIX_FREE(array->array);
        array->array = NULL;
        array->length = 0;

cleanup:

        PKIX_RETURN(BYTEARRAY);
}

/*
 * FUNCTION: pkix_pl_ByteArray_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_ByteArray_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_ByteArray *array = NULL;

        PKIX_ENTER(BYTEARRAY, "pkix_pl_ByteArray_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BYTEARRAY_TYPE, plContext),
                    PKIX_OBJECTNOTBYTEARRAY);

        array = (PKIX_PL_ByteArray*)object;

        PKIX_CHECK(pkix_hash
                ((const unsigned char *)array->array,
                array->length,
                pHashcode,
                plContext),
                PKIX_HASHFAILED);

cleanup:

        PKIX_RETURN(BYTEARRAY);
}

/*
 * FUNCTION: pkix_pl_ByteArray_RegisterSelf
 * DESCRIPTION:
 * Registers PKIX_BYTEARRAY_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_ByteArray_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(BYTEARRAY, "pkix_pl_ByteArray_RegisterSelf");

        entry.description = "ByteArray";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_ByteArray);
        entry.destructor = pkix_pl_ByteArray_Destroy;
        entry.equalsFunction = pkix_pl_ByteArray_Equals;
        entry.hashcodeFunction = pkix_pl_ByteArray_Hashcode;
        entry.toStringFunction = pkix_pl_ByteArray_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_BYTEARRAY_TYPE] = entry;

        PKIX_RETURN(BYTEARRAY);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_ByteArray_Create (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_ByteArray_Create(
        void *array,
        PKIX_UInt32 length,
        PKIX_PL_ByteArray **pByteArray,
        void *plContext)
{
        PKIX_PL_ByteArray *byteArray = NULL;

        PKIX_ENTER(BYTEARRAY, "PKIX_PL_ByteArray_Create");
        PKIX_NULLCHECK_ONE(pByteArray);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_BYTEARRAY_TYPE,
                sizeof (PKIX_PL_ByteArray),
                (PKIX_PL_Object **)&byteArray,
                plContext),
                PKIX_COULDNOTCREATEOBJECTSTORAGE);

        byteArray->length = length;
        byteArray->array = NULL;

        if (length != 0){
                /* Alloc space for array */
                PKIX_NULLCHECK_ONE(array);

                PKIX_CHECK(PKIX_PL_Malloc
                            (length, (void**)&(byteArray->array), plContext),
                            PKIX_MALLOCFAILED);

                PKIX_BYTEARRAY_DEBUG("\tCalling PORT_Memcpy).\n");
                (void) PORT_Memcpy(byteArray->array, array, length);
        }

        *pByteArray = byteArray;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(byteArray);
        }

        PKIX_RETURN(BYTEARRAY);
}

/*
 * FUNCTION: PKIX_PL_ByteArray_GetPointer (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_ByteArray_GetPointer(
        PKIX_PL_ByteArray *byteArray,
        void **pArray,
        void *plContext)
{
        void *bytes = NULL;
        PKIX_ENTER(BYTEARRAY, "PKIX_PL_ByteArray_GetPointer");
        PKIX_NULLCHECK_TWO(byteArray, pArray);

        if (byteArray->length != 0){
                PKIX_CHECK(PKIX_PL_Malloc
                            (byteArray->length, &bytes, plContext),
                            PKIX_MALLOCFAILED);

                PKIX_BYTEARRAY_DEBUG("\tCalling PORT_Memcpy).\n");
                (void) PORT_Memcpy
                        (bytes, byteArray->array, byteArray->length);
        }

        *pArray = bytes;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_FREE(bytes);
        }

        PKIX_RETURN(BYTEARRAY);
}

/*
 * FUNCTION: PKIX_PL_ByteArray_GetLength (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_ByteArray_GetLength(
        PKIX_PL_ByteArray *byteArray,
        PKIX_UInt32 *pLength,
        void *plContext)
{
        PKIX_ENTER(BYTEARRAY, "PKIX_PL_ByteArray_GetLength");
        PKIX_NULLCHECK_TWO(byteArray, pLength);

        *pLength = byteArray->length;

        PKIX_RETURN(BYTEARRAY);
}
