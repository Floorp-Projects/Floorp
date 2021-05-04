/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_string.c
 *
 * String Object Functions
 *
 */

#include "pkix_pl_string.h"

/* --Private-String-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_String_Comparator
 * (see comments for PKIX_PL_ComparatorCallback in pkix_pl_system.h)
 *
 * NOTE:
 *  This function is a utility function called by pkix_pl_String_Equals().
 *  It is not officially registered as a comparator.
 */
static PKIX_Error *
pkix_pl_String_Comparator(
        PKIX_PL_String *firstString,
        PKIX_PL_String *secondString,
        PKIX_Int32 *pResult,
        void *plContext)
{
        PKIX_UInt32 i;
        PKIX_Int32 result;
        unsigned char *p1 = NULL;
        unsigned char *p2 = NULL;

        PKIX_ENTER(STRING, "pkix_pl_String_Comparator");
        PKIX_NULLCHECK_THREE(firstString, secondString, pResult);

        result = 0;

        p1 = (unsigned char*) firstString->utf16String;
        p2 = (unsigned char*) secondString->utf16String;

        /* Compare characters until you find a difference */
        for (i = 0; ((i < firstString->utf16Length) &&
                    (i < secondString->utf16Length) &&
                    result == 0); i++, p1++, p2++) {
                if (*p1 < *p2){
                        result = -1;
                } else if (*p1 > *p2){
                        result = 1;
                }
        }

        /* If two arrays are identical so far, the longer one is greater */
        if (result == 0) {
                if (firstString->utf16Length < secondString->utf16Length) {
                        result = -1;
                } else if (firstString->utf16Length >
                            secondString->utf16Length) {
                        result = 1;
                }
        }

        *pResult = result;

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: pkix_pl_String_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_String_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_String *string = NULL;

        PKIX_ENTER(STRING, "pkix_pl_String_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_STRING_TYPE, plContext),
                    PKIX_ARGUMENTNOTSTRING);

        string = (PKIX_PL_String*)object;

        /* XXX For debugging Destroy EscASCII String  */
        if (string->escAsciiString != NULL) {
                PKIX_FREE(string->escAsciiString);
                string->escAsciiString = NULL;
                string->escAsciiLength = 0;
        }

        /* Destroy UTF16 String */
        if (string->utf16String != NULL) {
                PKIX_FREE(string->utf16String);
                string->utf16String = NULL;
                string->utf16Length = 0;
        }

cleanup:

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: pkix_pl_String_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_String_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *string = NULL;
        char *ascii = NULL;
        PKIX_UInt32 length;

        PKIX_ENTER(STRING, "pkix_pl_String_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_STRING_TYPE, plContext),
                    PKIX_ARGUMENTNOTSTRING);

        string = (PKIX_PL_String*)object;

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                (string, PKIX_ESCASCII, (void **)&ascii, &length, plContext),
                PKIX_STRINGGETENCODEDFAILED);

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII, ascii, 0, pString, plContext),
                    PKIX_STRINGCREATEFAILED);

        goto cleanup;

cleanup:

        PKIX_FREE(ascii);

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: pkix_pl_String_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_String_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Int32 cmpResult = 0;

        PKIX_ENTER(STRING, "pkix_pl_String_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* Sanity check: Test that "firstObject" is a Strings */
        PKIX_CHECK(pkix_CheckType(firstObject, PKIX_STRING_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTSTRING);

        /* "SecondObject" doesn't have to be a string */
        PKIX_CHECK(PKIX_PL_Object_GetType
                (secondObject, &secondType, plContext),
                PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        /* If types differ, then we will return false */
        *pResult = PKIX_FALSE;

        if (secondType != PKIX_STRING_TYPE) goto cleanup;

        /* It's safe to cast here */
        PKIX_CHECK(pkix_pl_String_Comparator
                ((PKIX_PL_String*)firstObject,
                (PKIX_PL_String*)secondObject,
                &cmpResult,
                plContext),
                PKIX_STRINGCOMPARATORFAILED);

        /* Strings are equal iff Comparator Result is 0 */
        *pResult = (cmpResult == 0);

cleanup:

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: pkix_pl_String_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_String_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_String *string = NULL;

        PKIX_ENTER(STRING, "pkix_pl_String_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_STRING_TYPE, plContext),
                PKIX_OBJECTNOTSTRING);

        string = (PKIX_PL_String*)object;

        PKIX_CHECK(pkix_hash
                ((const unsigned char *)string->utf16String,
                string->utf16Length,
                pHashcode,
                plContext),
                PKIX_HASHFAILED);

cleanup:

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: pkix_pl_String_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_STRING_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_String_RegisterSelf(
        void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(STRING, "pkix_pl_String_RegisterSelf");

        entry.description = "String";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_String);
        entry.destructor = pkix_pl_String_Destroy;
        entry.equalsFunction = pkix_pl_String_Equals;
        entry.hashcodeFunction = pkix_pl_String_Hashcode;
        entry.toStringFunction = pkix_pl_String_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_STRING_TYPE] = entry;

        PKIX_RETURN(STRING);
}


/* --Public-String-Functions----------------------------------------- */

/*
 * FUNCTION: PKIX_PL_String_Create (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_String_Create(
        PKIX_UInt32 fmtIndicator,
        const void *stringRep,
        PKIX_UInt32 stringLen,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *string = NULL;
        unsigned char *utf16Char = NULL;
        PKIX_UInt32 i;

        PKIX_ENTER(STRING, "PKIX_PL_String_Create");
        PKIX_NULLCHECK_TWO(pString, stringRep);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_STRING_TYPE,
                    sizeof (PKIX_PL_String),
                    (PKIX_PL_Object **)&string,
                    plContext),
                    PKIX_COULDNOTALLOCATENEWSTRINGOBJECT);

        string->utf16String = NULL;
        string->utf16Length = 0;

        /* XXX For Debugging */
        string->escAsciiString = NULL;
        string->escAsciiLength = 0;

        switch (fmtIndicator) {
        case PKIX_ESCASCII: case PKIX_ESCASCII_DEBUG:
                PKIX_STRING_DEBUG("\tCalling PL_strlen).\n");
                string->escAsciiLength = PL_strlen(stringRep);

                /* XXX Cache for Debugging */
                PKIX_CHECK(PKIX_PL_Malloc
                            ((string->escAsciiLength)+1,
                            (void **)&string->escAsciiString,
                            plContext),
                            PKIX_MALLOCFAILED);

                (void) PORT_Memcpy
                        (string->escAsciiString,
                        (void *)((char *)stringRep),
                        (string->escAsciiLength)+1);

                /* Convert the EscASCII string to UTF16 */
                PKIX_CHECK(pkix_EscASCII_to_UTF16
                            (string->escAsciiString,
                            string->escAsciiLength,
                            (fmtIndicator == PKIX_ESCASCII_DEBUG),
                            &string->utf16String,
                            &string->utf16Length,
                            plContext),
                            PKIX_ESCASCIITOUTF16FAILED);
                break;
        case PKIX_UTF8:
                /* Convert the UTF8 string to UTF16 */
                PKIX_CHECK(pkix_UTF8_to_UTF16
                            (stringRep,
                            stringLen,
                            &string->utf16String,
                            &string->utf16Length,
                            plContext),
                            PKIX_UTF8TOUTF16FAILED);
                break;
        case PKIX_UTF16:
                /* UTF16 Strings must be even in length */
                if (stringLen%2 == 1) {
                        PKIX_DECREF(string);
                        PKIX_ERROR(PKIX_UTF16ALIGNMENTERROR);
                }

                utf16Char = (unsigned char *)stringRep;

                /* Make sure this is a valid UTF-16 String */
                for (i = 0; \
                    (i < stringLen) && (pkixErrorResult == NULL); \
                    i += 2) {
                        /* Check that surrogate pairs are valid */
                        if ((utf16Char[i] >= 0xD8)&&
                            (utf16Char[i] <= 0xDB)) {
                                if ((i+2) >= stringLen) {
                                  PKIX_ERROR(PKIX_UTF16HIGHZONEALIGNMENTERROR);
                                  /* Second pair should be DC00-DFFF */
                                } else if (!((utf16Char[i+2] >= 0xDC)&&
                                      (utf16Char[i+2] <= 0xDF))) {
                                  PKIX_ERROR(PKIX_UTF16LOWZONEERROR);
                                } else {
                                  /*  Surrogate quartet is valid. */
                                  i += 2;
                                }
                        }
                }

                /* Create UTF16 String */
                string->utf16Length = stringLen;

                /* Alloc space for string */
                PKIX_CHECK(PKIX_PL_Malloc
                            (stringLen, &string->utf16String, plContext),
                            PKIX_MALLOCFAILED);

                PKIX_STRING_DEBUG("\tCalling PORT_Memcpy).\n");
                (void) PORT_Memcpy
                        (string->utf16String, stringRep, stringLen);
                break;

        default:
                PKIX_ERROR(PKIX_UNKNOWNFORMAT);
        }

        *pString = string;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(string);
        }

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: PKIX_PL_Sprintf (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Sprintf(
        PKIX_PL_String **pOut,
        void *plContext,
        const PKIX_PL_String *fmt,
        ...)
{
        PKIX_PL_String *tempString = NULL;
        PKIX_UInt32 tempUInt = 0;
        void *pArg = NULL;
        char *asciiText =  NULL;
        char *asciiFormat = NULL;
        char *convertedAsciiFormat = NULL;
        char *convertedAsciiFormatBase = NULL;
        va_list args;
        PKIX_UInt32 length, i, j, dummyLen;

        PKIX_ENTER(STRING, "PKIX_PL_Sprintf");
        PKIX_NULLCHECK_TWO(pOut, fmt);

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                    ((PKIX_PL_String *)fmt,
                    PKIX_ESCASCII,
                    (void **)&asciiFormat,
                    &length,
                    plContext),
                    PKIX_STRINGGETENCODEDFAILED);

        PKIX_STRING_DEBUG("\tCalling PR_Malloc).\n");
        convertedAsciiFormat = PR_Malloc(length + 1);
        if (convertedAsciiFormat == NULL)
                PKIX_ERROR_ALLOC_ERROR();

        convertedAsciiFormatBase = convertedAsciiFormat;

        PKIX_STRING_DEBUG("\tCalling va_start).\n");
        va_start(args, fmt);

        i = 0;
        j = 0;
        while (i < length) {
                if ((asciiFormat[i] == '%')&&((i+1) < length)) {
                        switch (asciiFormat[i+1]) {
                        case 's':
                                convertedAsciiFormat[j++] = asciiFormat[i++];
                                convertedAsciiFormat[j++] = asciiFormat[i++];
                                convertedAsciiFormat[j] = '\0';

                                tempString = va_arg(args, PKIX_PL_String *);
                                if (tempString != NULL) {
                                        PKIX_CHECK_NO_GOTO(
                                                PKIX_PL_String_GetEncoded
                                                ((PKIX_PL_String*)
                                                tempString,
                                                PKIX_ESCASCII,
                                                &pArg,
                                                &dummyLen,
                                                plContext),
                                                PKIX_STRINGGETENCODEDFAILED);
                                        /* need to cleanup var args before
                                         * we ditch out to cleanup. */
                                        if (pkixErrorResult) {
                                            va_end(args);
                                            goto cleanup;
                                        }
                                } else {
                                        /* there may be a NULL in var_args */
                                        pArg = NULL;
                                }
                                if (asciiText != NULL) {
                                    asciiText = PR_sprintf_append(asciiText,
                                          (const char *)convertedAsciiFormat,
                                          pArg);
                                } else {
                                    asciiText = PR_smprintf
                                        ((const char *)convertedAsciiFormat,
                                        pArg);
                                }
                                if (pArg != NULL) {
                                        PKIX_PL_Free(pArg, plContext);
                                        pArg = NULL;
                                }
                                convertedAsciiFormat += j;
                                j = 0;
                                break;
                         case 'd':
                         case 'i':
                         case 'o':
                         case 'u':
                         case 'x':
                         case 'X':
                                convertedAsciiFormat[j++] = asciiFormat[i++];
                                convertedAsciiFormat[j++] = asciiFormat[i++];
                                convertedAsciiFormat[j] = '\0';

                                tempUInt = va_arg(args, PKIX_UInt32);
                                if (asciiText != NULL) {
                                    asciiText = PR_sprintf_append(asciiText,
                                          (const char *)convertedAsciiFormat,
                                          tempUInt);
                                } else {
                                    asciiText = PR_smprintf
                                        ((const char *)convertedAsciiFormat,
                                        tempUInt);
                                }     
                                convertedAsciiFormat += j;
                                j = 0;
                                break;
                        default:
                                convertedAsciiFormat[j++] = asciiFormat[i++];
                                convertedAsciiFormat[j++] = asciiFormat[i++];
                                break;
                        }
                } else {
                        convertedAsciiFormat[j++] = asciiFormat[i++];
                }
        }

        /* for constant string value at end of fmt */
        if (j > 0) {
                convertedAsciiFormat[j] = '\0';
                if (asciiText != NULL) {
                    asciiText = PR_sprintf_append(asciiText,
                                    (const char *)convertedAsciiFormat);
                } else {
                    asciiText = PR_smprintf((const char *)convertedAsciiFormat);
                }
        }

        va_end(args);

        /* Copy temporary char * into a string object */
        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, (void *)asciiText, 0, pOut, plContext),
                PKIX_STRINGCREATEFAILED);

cleanup:

        PKIX_FREE(asciiFormat);

        if (convertedAsciiFormatBase){
                PR_Free(convertedAsciiFormatBase);
        }

        if (asciiText){
                PKIX_STRING_DEBUG("\tCalling PR_smprintf_free).\n");
                PR_smprintf_free(asciiText);
        }

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: PKIX_PL_GetString (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_GetString(
        /* ARGSUSED */ PKIX_UInt32 stringID,
        char *defaultString,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_ENTER(STRING, "PKIX_PL_GetString");
        PKIX_NULLCHECK_TWO(pString, defaultString);

        /* XXX Optimization - use stringID for caching */
        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII, defaultString, 0, pString, plContext),
                    PKIX_STRINGCREATEFAILED);

cleanup:

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: PKIX_PL_String_GetEncoded (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_String_GetEncoded(
        PKIX_PL_String *string,
        PKIX_UInt32 fmtIndicator,
        void **pStringRep,
        PKIX_UInt32 *pLength,
        void *plContext)
{
        PKIX_ENTER(STRING, "PKIX_PL_String_GetEncoded");
        PKIX_NULLCHECK_THREE(string, pStringRep, pLength);

        switch (fmtIndicator) {
        case PKIX_ESCASCII: case PKIX_ESCASCII_DEBUG:
                PKIX_CHECK(pkix_UTF16_to_EscASCII
                            (string->utf16String,
                            string->utf16Length,
                            (fmtIndicator == PKIX_ESCASCII_DEBUG),
                            (char **)pStringRep,
                            pLength,
                            plContext),
                            PKIX_UTF16TOESCASCIIFAILED);
                break;
        case PKIX_UTF8:
                PKIX_CHECK(pkix_UTF16_to_UTF8
                            (string->utf16String,
                            string->utf16Length,
                            PKIX_FALSE,
                            pStringRep,
                            pLength,
                            plContext),
                            PKIX_UTF16TOUTF8FAILED);
                break;
        case PKIX_UTF8_NULL_TERM:
                PKIX_CHECK(pkix_UTF16_to_UTF8
                            (string->utf16String,
                            string->utf16Length,
                            PKIX_TRUE,
                            pStringRep,
                            pLength,
                            plContext),
                            PKIX_UTF16TOUTF8FAILED);
                break;
        case PKIX_UTF16:
                *pLength = string->utf16Length;

                PKIX_CHECK(PKIX_PL_Malloc(*pLength, pStringRep, plContext),
                            PKIX_MALLOCFAILED);

                PKIX_STRING_DEBUG("\tCalling PORT_Memcpy).\n");
                (void) PORT_Memcpy(*pStringRep, string->utf16String, *pLength);
                break;
        default:
                PKIX_ERROR(PKIX_UNKNOWNFORMAT);
        }

cleanup:

        PKIX_RETURN(STRING);
}
