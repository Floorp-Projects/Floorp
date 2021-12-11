/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_common.c
 *
 * Common utility functions used by various PKIX_PL functions
 *
 */

#include "pkix_pl_common.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_LockObject
 * DESCRIPTION:
 *
 *  Locks the object pointed to by "object".
 *
 * PARAMETERS:
 *  "object"
 *      Address of object. Must be non-NULL
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_LockObject(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_Object *objectHeader;

        PKIX_ENTER(OBJECT, "pkix_LockObject");
        PKIX_NULLCHECK_ONE(object);

        if (object == (PKIX_PL_Object *)PKIX_ALLOC_ERROR()) {
                goto cleanup;
        }

        PKIX_OBJECT_DEBUG("\tShifting object pointer).\n");
        /* The header is sizeof(PKIX_PL_Object) before the object pointer */

        objectHeader = object-1;

        PKIX_OBJECT_DEBUG("\tCalling PR_Lock).\n");
        PR_Lock(objectHeader->lock);

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: pkix_UnlockObject
 * DESCRIPTION:
 *
 *  Unlocks the object pointed to by "object".
 *
 * PARAMETERS:
 *  "object"
 *      Address of Object. Must be non-NULL
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_UnlockObject(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_Object *objectHeader;
        PRStatus result;

        PKIX_ENTER(OBJECT, "pkix_UnlockObject");
        PKIX_NULLCHECK_ONE(object);

        if (object == (PKIX_PL_Object *)PKIX_ALLOC_ERROR()) {
                goto cleanup;
        }

        PKIX_OBJECT_DEBUG("\tShifting object pointer).\n");
        /* The header is sizeof(PKIX_PL_Object) before the object pointer */

        objectHeader = object-1;

        PKIX_OBJECT_DEBUG("\tCalling PR_Unlock).\n");
        result = PR_Unlock(objectHeader->lock);

        if (result == PR_FAILURE) {
                PKIX_OBJECT_DEBUG("\tPR_Unlock failed.).\n");
                PKIX_ERROR_FATAL(PKIX_ERRORUNLOCKINGOBJECT);
        }

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: pkix_pl_UInt32_Overflows
 * DESCRIPTION:
 *
 *  Returns a PKIX_Boolean indicating whether the unsigned integer
 *  represented by "string" is too large to fit in 32-bits (i.e.
 *  whether it overflows). With the exception of the string "0",
 *  all other strings are stripped of any leading zeros. It is assumed
 *  that every character in "string" is from the set {'0' - '9'}.
 *
 * PARAMETERS
 *  "string"
 *      Address of array of bytes representing PKIX_UInt32 that's being tested
 *      for 32-bit overflow
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  PKIX_TRUE if PKIX_UInt32 represented by "string" overflows;
 *  PKIX_FALSE otherwise
 */
PKIX_Boolean
pkix_pl_UInt32_Overflows(char *string){
        char *firstNonZero = NULL;
        PKIX_UInt32 length, i;
        char *MAX_UINT32_STRING = "4294967295";

        PKIX_DEBUG_ENTER(OID);

        PKIX_OID_DEBUG("\tCalling PL_strlen).\n");
        length = PL_strlen(string);

        if (length < MAX_DIGITS_32){
                return (PKIX_FALSE);
        }

        firstNonZero = string;
        for (i = 0; i < length; i++){
                if (*string == '0'){
                        firstNonZero++;
                }
        }

        PKIX_OID_DEBUG("\tCalling PL_strlen).\n");
        length = PL_strlen(firstNonZero);

        if (length > MAX_DIGITS_32){
                return (PKIX_TRUE);
        }

        PKIX_OID_DEBUG("\tCalling PL_strlen).\n");
        if (length == MAX_DIGITS_32){
                PKIX_OID_DEBUG("\tCalling PORT_Strcmp).\n");
                if (PORT_Strcmp(firstNonZero, MAX_UINT32_STRING) > 0){
                        return (PKIX_TRUE);
                }
        }

        return (PKIX_FALSE);
}

/*
 * FUNCTION: pkix_pl_getOIDToken
 * DESCRIPTION:
 *
 *  Takes the array of DER-encoded bytes pointed to by "derBytes"
 *  (representing an OID) and the value of "index" representing the index into
 *  the array, and decodes the bytes until an integer token is retrieved. If
 *  successful, this function stores the integer component at "pToken" and
 *  stores the index representing the next byte in the array at "pIndex"
 *  (following the last byte that was used in the decoding). This new output
 *  index can be used in subsequent calls as an input index, allowing each
 *  token of the OID to be retrieved consecutively. Note that there is a
 *  special case for the first byte, in that it encodes two separate integer
 *  tokens. For example, the byte {2a} represents the integer tokens {1,2}.
 *  This special case is not handled here and must be handled by the caller.
 *
 * PARAMETERS
 *  "derBytes"
 *      Address of array of bytes representing a DER-encoded OID.
 *      Must be non-NULL.
 *  "index"
 *      Index into the array that this function will begin decoding at.
 *  "pToken"
 *      Destination for decoded OID token. Must be non-NULL.
 *  "pIndex"
 *      Destination for index of next byte following last byte used.
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
static PKIX_Error *
pkix_pl_getOIDToken(
        char *derBytes,
        PKIX_UInt32 index,
        PKIX_UInt32 *pToken,
        PKIX_UInt32 *pIndex,
        void *plContext)
{
        PKIX_UInt32 retval, i, tmp;

        PKIX_ENTER(OID, "pkix_pl_getOIDToken");
        PKIX_NULLCHECK_THREE(derBytes, pToken, pIndex);

        /*
         * We should only need to parse a maximum of four bytes, because
         * RFC 3280 "mandates support for OIDs which have arc elements
         * with values that are less than 2^28, that is, they MUST be between
         * 0 and 268,435,455, inclusive.  This allows each arc element to be
         * represented within a single 32 bit word."
         */

        for (i = 0, retval = 0; i < 4; i++) {
            retval <<= 7;
            tmp = derBytes[index];
            index++;
            retval |= (tmp & 0x07f);
            if ((tmp & 0x080) == 0){
                    *pToken = retval;
                    *pIndex = index;
                    goto cleanup;
            }
        }

        PKIX_ERROR(PKIX_INVALIDENCODINGOIDTOKENVALUETOOBIG);

cleanup:

        PKIX_RETURN(OID);

}

/*
 * FUNCTION: pkix_pl_helperBytes2Ascii
 * DESCRIPTION:
 *
 *  Converts an array of integers pointed to by "tokens" with a length of
 *  "numTokens", to an ASCII string consisting of those integers with dots in
 *  between them and stores the result at "pAscii". The ASCII representation is
 *  guaranteed to end with a NUL character. This is particularly useful for
 *  OID's and IP Addresses.
 *
 *  The return value "pAscii" is not reference-counted and will need to
 *  be freed with PKIX_PL_Free.
 *
 * PARAMETERS
 *  "tokens"
 *      Address of array of integers. Must be non-NULL.
 *  "numTokens"
 *      Length of array of integers. Must be non-zero.
 *  "pAscii"
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
PKIX_Error *
pkix_pl_helperBytes2Ascii(
        PKIX_UInt32 *tokens,
        PKIX_UInt32 numTokens,
        char **pAscii,
        void *plContext)
{
        char *tempString = NULL;
        char *outputString = NULL;
        char *format = "%d";
        PKIX_UInt32 i = 0;
        PKIX_UInt32 outputLen = 0;
        PKIX_Int32 error;

        PKIX_ENTER(OBJECT, "pkix_pl_helperBytes2Ascii");
        PKIX_NULLCHECK_TWO(tokens, pAscii);

        if (numTokens == 0) {
                PKIX_ERROR_FATAL(PKIX_HELPERBYTES2ASCIINUMTOKENSZERO);
        }

        /*
         * tempString will hold the string representation of a PKIX_UInt32 type
         * The maximum value that can be held by an unsigned 32-bit integer
         * is (2^32 - 1) = 4294967295 (which is ten digits long)
         * Since tempString will hold the string representation of a
         * PKIX_UInt32, we allocate 11 bytes for it (1 byte for '\0')
         */

        PKIX_CHECK(PKIX_PL_Malloc
                    (MAX_DIGITS_32 + 1, (void **)&tempString, plContext),
                    PKIX_MALLOCFAILED);

        for (i = 0; i < numTokens; i++){
                PKIX_OBJECT_DEBUG("\tCalling PR_snprintf).\n");
                error = PR_snprintf(tempString,
                                    MAX_DIGITS_32 + 1,
                                    format,
                                    tokens[i]);
                if (error == -1){
                        PKIX_ERROR(PKIX_PRSNPRINTFFAILED);
                }

                PKIX_OBJECT_DEBUG("\tCalling PL_strlen).\n");
                outputLen += PL_strlen(tempString);

                /* Include a dot to separate each number */
                outputLen++;
        }

        /* Allocate space for the destination string */
        PKIX_CHECK(PKIX_PL_Malloc
                    (outputLen, (void **)&outputString, plContext),
                    PKIX_MALLOCFAILED);

        *outputString = '\0';

        /* Concatenate all strings together */
        for (i = 0; i < numTokens; i++){

                PKIX_OBJECT_DEBUG("\tCalling PR_snprintf).\n");
                error = PR_snprintf(tempString,
                                    MAX_DIGITS_32 + 1,
                                    format,
                                    tokens[i]);
                if (error == -1){
                        PKIX_ERROR(PKIX_PRSNPRINTFFAILED);
                }

                PKIX_OBJECT_DEBUG("\tCalling PL_strcat).\n");
                (void) PL_strcat(outputString, tempString);

                /* we don't want to put a "." at the very end */
                if (i < (numTokens - 1)){
                        PKIX_OBJECT_DEBUG("\tCalling PL_strcat).\n");
                        (void) PL_strcat(outputString, ".");
                }
        }

        /* Ensure output string ends with terminating null */
        outputString[outputLen-1] = '\0';

        *pAscii = outputString;
        outputString = NULL;

cleanup:
        
        PKIX_FREE(outputString);
        PKIX_FREE(tempString);

        PKIX_RETURN(OBJECT);

}

/*
 * FUNCTION: pkix_pl_ipAddrBytes2Ascii
 * DESCRIPTION:
 *
 *  Converts the DER encoding of an IPAddress pointed to by "secItem" to an
 *  ASCII representation and stores the result at "pAscii". The ASCII
 *  representation is guaranteed to end with a NUL character. The input
 *  SECItem must contain non-NULL data and must have a positive length.
 *
 *  The return value "pAscii" is not reference-counted and will need to
 *  be freed with PKIX_PL_Free.
 *  XXX this function assumes that IPv4 addresses are being used
 *  XXX what about IPv6? can NSS tell the difference
 *
 * PARAMETERS
 *  "secItem"
 *      Address of SECItem which contains bytes and length of DER encoding.
 *      Must be non-NULL.
 *  "pAscii"
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
PKIX_Error *
pkix_pl_ipAddrBytes2Ascii(
        SECItem *secItem,
        char **pAscii,
        void *plContext)
{
        char *data = NULL;
        PKIX_UInt32 *tokens = NULL;
        PKIX_UInt32 numTokens = 0;
        PKIX_UInt32 i = 0;
        char *asciiString = NULL;

        PKIX_ENTER(OBJECT, "pkix_pl_ipAddrBytes2Ascii");
        PKIX_NULLCHECK_THREE(secItem, pAscii, secItem->data);

        if (secItem->len == 0) {
                PKIX_ERROR_FATAL(PKIX_IPADDRBYTES2ASCIIDATALENGTHZERO);
        }

        data = (char *)(secItem->data);
        numTokens = secItem->len;

        /* allocate space for array of integers */
        PKIX_CHECK(PKIX_PL_Malloc
                    (numTokens * sizeof (PKIX_UInt32),
                    (void **)&tokens,
                    plContext),
                    PKIX_MALLOCFAILED);

        /* populate array of integers */
        for (i = 0; i < numTokens; i++){
                tokens[i] = data[i];
        }

        /* convert array of integers to ASCII */
        PKIX_CHECK(pkix_pl_helperBytes2Ascii
                    (tokens, numTokens, &asciiString, plContext),
                    PKIX_HELPERBYTES2ASCIIFAILED);

        *pAscii = asciiString;

cleanup:

        PKIX_FREE(tokens);

        PKIX_RETURN(OBJECT);
}


/*
 * FUNCTION: pkix_pl_oidBytes2Ascii
 * DESCRIPTION:
 *
 *  Converts the DER encoding of an OID pointed to by "secItem" to an ASCII
 *  representation and stores it at "pAscii". The ASCII representation is
 *  guaranteed to end with a NUL character. The input SECItem must contain
 *  non-NULL data and must have a positive length.
 *
 *  Example: the six bytes {2a 86 48 86 f7 0d} represent the
 *  four integer tokens {1, 2, 840, 113549}, which we will convert
 *  into ASCII yielding "1.2.840.113549"
 *
 *  The return value "pAscii" is not reference-counted and will need to
 *  be freed with PKIX_PL_Free.
 *
 * PARAMETERS
 *  "secItem"
 *      Address of SECItem which contains bytes and length of DER encoding.
 *      Must be non-NULL.
 *  "pAscii"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OID Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_oidBytes2Ascii(
        SECItem *secItem,
        char **pAscii,
        void *plContext)
{
        char *data = NULL;
        PKIX_UInt32 *tokens = NULL;
        PKIX_UInt32 token = 0;
        PKIX_UInt32 numBytes = 0;
        PKIX_UInt32 numTokens = 0;
        PKIX_UInt32 i = 0, x = 0, y = 0;
        PKIX_UInt32 index = 0;
        char *asciiString = NULL;

        PKIX_ENTER(OID, "pkix_pl_oidBytes2Ascii");
        PKIX_NULLCHECK_THREE(secItem, pAscii, secItem->data);

        if (secItem->len == 0) {
                PKIX_ERROR_FATAL(PKIX_OIDBYTES2ASCIIDATALENGTHZERO);
        }

        data = (char *)(secItem->data);
        numBytes = secItem->len;
        numTokens = 0;

        /* calculate how many integer tokens are represented by the bytes. */
        for (i = 0; i < numBytes; i++){
                if ((data[i] & 0x080) == 0){
                        numTokens++;
                }
        }

        /* if we are unable to retrieve any tokens at all, we throw an error */
        if (numTokens == 0){
                PKIX_ERROR(PKIX_INVALIDDERENCODINGFOROID);
        }

        /* add one more token b/c the first byte always contains two tokens */
        numTokens++;

        /* allocate space for array of integers */
        PKIX_CHECK(PKIX_PL_Malloc
                    (numTokens * sizeof (PKIX_UInt32),
                    (void **)&tokens,
                    plContext),
                    PKIX_MALLOCFAILED);

        /* populate array of integers */
        for (i = 0; i < numTokens; i++){

                /* retrieve integer token */
                PKIX_CHECK(pkix_pl_getOIDToken
                            (data, index, &token, &index, plContext),
                            PKIX_GETOIDTOKENFAILED);

                if (i == 0){

                        /*
                         * special case: the first DER-encoded byte represents
                         * two tokens. We take advantage of fact that first
                         * token must be 0, 1, or 2; and second token must be
                         * between {0, 39} inclusive if first token is 0 or 1.
                         */

                        if (token < 40)
                                x = 0;
                        else if (token < 80)
                                x = 1;
                        else
                                x = 2;
                        y = token - (x * 40);

                        tokens[0] = x;
                        tokens[1] = y;
                        i++;
                } else {
                        tokens[i] = token;
                }
        }

        /* convert array of integers to ASCII */
        PKIX_CHECK(pkix_pl_helperBytes2Ascii
                    (tokens, numTokens, &asciiString, plContext),
                    PKIX_HELPERBYTES2ASCIIFAILED);

        *pAscii = asciiString;

cleanup:

        PKIX_FREE(tokens);
        PKIX_RETURN(OID);

}

/*
 * FUNCTION: pkix_UTF16_to_EscASCII
 * DESCRIPTION:
 *
 *  Converts array of bytes pointed to by "utf16String" with length of
 *  "utf16Length" (which must be even) into a freshly allocated Escaped ASCII
 *  string and stores a pointer to that string at "pDest" and stores the
 *  string's length at "pLength". The Escaped ASCII string's length does not
 *  include the final NUL character. The caller is responsible for freeing
 *  "pDest" using PKIX_PL_Free. If "debug" is set, uses EscASCII_Debug
 *  encoding.
 *
 * PARAMETERS:
 *  "utf16String"
 *      Address of array of bytes representing data source. Must be non-NULL.
 *  "utf16Length"
 *      Length of data source. Must be even.
 *  "debug"
 *      Boolean value indicating whether debug mode is desired.
 *  "pDest"
 *      Address where data will be stored. Must be non-NULL.
 *  "pLength"
 *      Address where data length will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a String Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_UTF16_to_EscASCII(
        const void *utf16String,
        PKIX_UInt32 utf16Length,
        PKIX_Boolean debug,
        char **pDest,
        PKIX_UInt32 *pLength,
        void *plContext)
{
        char *destPtr = NULL;
        PKIX_UInt32 i, charLen;
        PKIX_UInt32 x = 0, y = 0, z = 0;
        unsigned char *utf16Char = (unsigned char *)utf16String;

        PKIX_ENTER(STRING, "pkix_UTF16_to_EscASCII");
        PKIX_NULLCHECK_THREE(utf16String, pDest, pLength);

        /* Assume every pair of bytes becomes &#xNNNN; */
        charLen = 4*utf16Length;

        /* utf16Lenght must be even */
        if ((utf16Length % 2) != 0){
                PKIX_ERROR(PKIX_UTF16ALIGNMENTERROR);
        }

        /* Count how many bytes we need */
        for (i = 0; i < utf16Length; i += 2) {
                if ((utf16Char[i] == 0x00)&&
                        pkix_isPlaintext(utf16Char[i+1], debug)) {
                        if (utf16Char[i+1] == '&') {
                                /* Need to convert this to &amp; */
                                charLen -= 3;
                        } else {
                                /* We can fit this into one char */
                                charLen -= 7;
                        }
                } else if ((utf16Char[i] >= 0xD8) && (utf16Char[i] <= 0xDB)) {
                        if ((i+3) >= utf16Length) {
                                PKIX_ERROR(PKIX_UTF16HIGHZONEALIGNMENTERROR);
                        } else if ((utf16Char[i+2] >= 0xDC)&&
                                (utf16Char[i+2] <= 0xDF)) {
                                /* Quartet of bytes will become &#xNNNNNNNN; */
                                charLen -= 4;
                                /* Quartet of bytes will produce 12 chars */
                                i += 2;
                        } else {
                                /* Second pair should be DC00-DFFF */
                                PKIX_ERROR(PKIX_UTF16LOWZONEERROR);
                        }
                }
        }

        *pLength = charLen;

        /* Ensure this string is null terminated */
        charLen++;

        /* Allocate space for character array */
        PKIX_CHECK(PKIX_PL_Malloc(charLen, (void **)pDest, plContext),
                    PKIX_MALLOCFAILED);

        destPtr = *pDest;
        for (i = 0; i < utf16Length; i += 2) {
                if ((utf16Char[i] == 0x00)&&
                    pkix_isPlaintext(utf16Char[i+1], debug)) {
                        /* Write a single character */
                        *destPtr++ = utf16Char[i+1];
                } else if ((utf16Char[i+1] == '&') && (utf16Char[i] == 0x00)){
                        *destPtr++ = '&';
                        *destPtr++ = 'a';
                        *destPtr++ = 'm';
                        *destPtr++ = 'p';
                        *destPtr++ = ';';
                } else if ((utf16Char[i] >= 0xD8)&&
                            (utf16Char[i] <= 0xDB)&&
                            (utf16Char[i+2] >= 0xDC)&&
                            (utf16Char[i+2] <= 0xDF)) {
                        /*
                         * Special UTF pairs are of the form:
                         * x = D800..DBFF; y = DC00..DFFF;
                         * The result is of the form:
                         * ((x - D800) * 400 + (y - DC00)) + 0001 0000
                         */
                        x = 0x0FFFF & ((utf16Char[i]<<8) | utf16Char[i+1]);
                        y = 0x0FFFF & ((utf16Char[i+2]<<8) | utf16Char[i+3]);
                        z = ((x - 0xD800) * 0x400 + (y - 0xDC00)) + 0x00010000;

                        /* Sprintf &#xNNNNNNNN; */
                        PKIX_STRING_DEBUG("\tCalling PR_snprintf).\n");
                        if (PR_snprintf(destPtr, 13, "&#x%08X;", z) ==
                            (PKIX_UInt32)(-1)) {
                                PKIX_ERROR(PKIX_PRSNPRINTFFAILED);
                        }
                        i += 2;
                        destPtr += 12;
                } else {
                        /* Sprintf &#xNNNN; */
                        PKIX_STRING_DEBUG("\tCalling PR_snprintf).\n");
                        if (PR_snprintf
                            (destPtr,
                            9,
                            "&#x%02X%02X;",
                            utf16Char[i],
                            utf16Char[i+1]) ==
                            (PKIX_UInt32)(-1)) {
                                PKIX_ERROR(PKIX_PRSNPRINTFFAILED);
                        }
                        destPtr += 8;
                }
        }
        *destPtr = '\0';

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_FREE(*pDest);
        }

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: pkix_EscASCII_to_UTF16
 * DESCRIPTION:
 *
 *  Converts array of bytes pointed to by "escAsciiString" with length of
 *  "escAsciiLength" into a freshly allocated UTF-16 string and stores a
 *  pointer to that string at "pDest" and stores the string's length at
 *  "pLength". The caller is responsible for freeing "pDest" using
 *  PKIX_PL_Free. If "debug" is set, uses EscASCII_Debug encoding.
 *
 * PARAMETERS:
 *  "escAsciiString"
 *      Address of array of bytes representing data source. Must be non-NULL.
 *  "escAsciiLength"
 *      Length of data source. Must be even.
 *  "debug"
 *      Boolean value indicating whether debug mode is desired.
 *  "pDest"
 *      Address where data will be stored. Must be non-NULL.
 *  "pLength"
 *      Address where data length will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a String Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_EscASCII_to_UTF16(
        const char *escAsciiString,
        PKIX_UInt32 escAsciiLen,
        PKIX_Boolean debug,
        void **pDest,
        PKIX_UInt32 *pLength,
        void *plContext)
{
        PKIX_UInt32 newLen, i, j, charSize;
        PKIX_UInt32 x = 0, y = 0, z = 0;
        unsigned char *destPtr = NULL;
        unsigned char testChar, testChar2;
        unsigned char *stringData = (unsigned char *)escAsciiString;

        PKIX_ENTER(STRING, "pkix_EscASCII_to_UTF16");
        PKIX_NULLCHECK_THREE(escAsciiString, pDest, pLength);

        if (escAsciiLen == 0) {
                PKIX_CHECK(PKIX_PL_Malloc(escAsciiLen, pDest, plContext),
                            PKIX_MALLOCFAILED);
                goto cleanup;
        }

        /* Assume each unicode character takes two bytes */
        newLen = escAsciiLen*2;

        /* Count up number of unicode encoded  characters */
        for (i = 0; i < escAsciiLen; i++) {
                if (!pkix_isPlaintext(stringData[i], debug)&&
                    (stringData[i] != '&')) {
                        PKIX_ERROR(PKIX_ILLEGALCHARACTERINESCAPEDASCII);
                } else if (PL_strstr(escAsciiString+i, "&amp;") ==
                            escAsciiString+i) {
                        /* Convert EscAscii "&amp;" to two bytes */
                        newLen -= 8;
                        i += 4;
                } else if ((PL_strstr(escAsciiString+i, "&#x") ==
                            escAsciiString+i)||
                            (PL_strstr(escAsciiString+i, "&#X") ==
                            escAsciiString+i)) {
                        if (((i+7) <= escAsciiLen)&&
                            (escAsciiString[i+7] == ';')) {
                                /* Convert &#xNNNN; to two bytes */
                                newLen -= 14;
                                i += 7;
                        } else if (((i+11) <= escAsciiLen)&&
                                (escAsciiString[i+11] == ';')) {
                                /* Convert &#xNNNNNNNN; to four bytes */
                                newLen -= 20;
                                i += 11;
                        } else {
                                PKIX_ERROR(PKIX_ILLEGALUSEOFAMP);
                        }
                }
        }

        PKIX_CHECK(PKIX_PL_Malloc(newLen, pDest, plContext),
                    PKIX_MALLOCFAILED);

        /* Copy into newly allocated space */
        destPtr = (unsigned char *)*pDest;

        i = 0;
        while (i < escAsciiLen) {
                /* Copy each byte until you hit a &amp; */
                if (pkix_isPlaintext(escAsciiString[i], debug)) {
                        *destPtr++ = 0x00;
                        *destPtr++ = escAsciiString[i++];
                } else if (PL_strstr(escAsciiString+i, "&amp;") ==
                            escAsciiString+i) {
                        /* Convert EscAscii "&amp;" to two bytes */
                        *destPtr++ = 0x00;
                        *destPtr++ = '&';
                        i += 5;
                } else if (((PL_strstr(escAsciiString+i, "&#x") ==
                            escAsciiString+i)||
                            (PL_strstr(escAsciiString+i, "&#X") ==
                            escAsciiString+i))&&
                            ((i+7) <= escAsciiLen)) {

                        /* We're either looking at &#xNNNN; or &#xNNNNNNNN; */
                        charSize = (escAsciiString[i+7] == ';')?4:8;

                        /* Skip past the &#x */
                        i += 3;

                        /* Make sure there is a terminating semi-colon */
                        if (((i+charSize) > escAsciiLen)||
                            (escAsciiString[i+charSize] != ';')) {
                                PKIX_ERROR(PKIX_TRUNCATEDUNICODEINESCAPEDASCII);
                        }

                        for (j = 0; j < charSize; j++) {
                                if (!PKIX_ISXDIGIT
                                    (escAsciiString[i+j])) {
                                        PKIX_ERROR(PKIX_ILLEGALUNICODECHARACTER);
                                } else if (charSize == 8) {
                                        x |= (pkix_hex2i
                                                        (escAsciiString[i+j]))
                                                        <<(4*(7-j));
                                }
                        }

                        testChar =
                                (pkix_hex2i(escAsciiString[i])<<4)|
                                pkix_hex2i(escAsciiString[i+1]);
                        testChar2 =
                                (pkix_hex2i(escAsciiString[i+2])<<4)|
                                pkix_hex2i(escAsciiString[i+3]);

                        if (charSize == 4) {
                                if ((testChar >= 0xD8)&&
                                    (testChar <= 0xDF)) {
                                        PKIX_ERROR(PKIX_ILLEGALSURROGATEPAIR);
                                } else if ((testChar == 0x00)&&
                                  pkix_isPlaintext(testChar2, debug)) {
                                      PKIX_ERROR(
                                          PKIX_ILLEGALCHARACTERINESCAPEDASCII);
                                }
                                *destPtr++ = testChar;
                                *destPtr++ = testChar2;
                        } else if (charSize == 8) {
                                /* First two chars must be 0001-0010 */
                                if (!((testChar == 0x00)&&
                                    ((testChar2 >= 0x01)&&
                                    (testChar2 <= 0x10)))) {
                                      PKIX_ERROR(
                                          PKIX_ILLEGALCHARACTERINESCAPEDASCII);
                                }
                                /*
                                 * Unicode Strings of the form:
                                 * x =  0001 0000..0010 FFFF
                                 * Encoded as pairs of UTF-16 where
                                 * y = ((x - 0001 0000) / 400) + D800
                                 * z = ((x - 0001 0000) % 400) + DC00
                                 */
                                x -= 0x00010000;
                                y = (x/0x400)+ 0xD800;
                                z = (x%0x400)+ 0xDC00;

                                /* Copy four bytes */
                                *destPtr++ = (y&0xFF00)>>8;
                                *destPtr++ = (y&0x00FF);
                                *destPtr++ = (z&0xFF00)>>8;
                                *destPtr++ = (z&0x00FF);
                        }
                        /* Move past the Hex digits and the semi-colon */
                        i += charSize+1;
                } else {
                        /* Do not allow any other non-plaintext character */
                        PKIX_ERROR(PKIX_ILLEGALCHARACTERINESCAPEDASCII);
                }
        }

        *pLength = newLen;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_FREE(*pDest);
        }

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: pkix_UTF16_to_UTF8
 * DESCRIPTION:
 *
 *  Converts array of bytes pointed to by "utf16String" with length of
 *  "utf16Length" into a freshly allocated UTF-8 string and stores a pointer
 *  to that string at "pDest" and stores the string's length at "pLength" (not
 *  counting the null terminator, if requested. The caller is responsible for
 *  freeing "pDest" using PKIX_PL_Free.
 *
 * PARAMETERS:
 *  "utf16String"
 *      Address of array of bytes representing data source. Must be non-NULL.
 *  "utf16Length"
 *      Length of data source. Must be even.
 *  "null-term"
 *      Boolean value indicating whether output should be null-terminated.
 *  "pDest"
 *      Address where data will be stored. Must be non-NULL.
 *  "pLength"
 *      Address where data length will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a String Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_UTF16_to_UTF8(
        const void *utf16String,
        PKIX_UInt32 utf16Length,
        PKIX_Boolean null_term,
        void **pDest,
        PKIX_UInt32 *pLength,
        void *plContext)
{
        PKIX_Boolean result;
        PKIX_UInt32 reallocLen;
        char *endPtr = NULL;

        PKIX_ENTER(STRING, "pkix_UTF16_to_UTF8");
        PKIX_NULLCHECK_THREE(utf16String, pDest, pLength);

        /* XXX How big can a UTF8 string be compared to a UTF16? */
        PKIX_CHECK(PKIX_PL_Calloc(1, utf16Length*2, pDest, plContext),
                    PKIX_CALLOCFAILED);

        PKIX_STRING_DEBUG("\tCalling PORT_UCS2_UTF8Conversion).\n");
        result = PORT_UCS2_UTF8Conversion
                (PKIX_FALSE, /* False = From UCS2 */
                (unsigned char *)utf16String,
                utf16Length,
                (unsigned char *)*pDest,
                utf16Length*2, /* Max Size */
                pLength);
        if (result == PR_FALSE){
                PKIX_ERROR(PKIX_PORTUCS2UTF8CONVERSIONFAILED);
        }

        reallocLen = *pLength;

        if (null_term){
                reallocLen++;
        }

        PKIX_CHECK(PKIX_PL_Realloc(*pDest, reallocLen, pDest, plContext),
                    PKIX_REALLOCFAILED);

        if (null_term){
                endPtr = (char*)*pDest + reallocLen - 1;
                *endPtr = '\0';
        }

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_FREE(*pDest);
        }

        PKIX_RETURN(STRING);
}

/*
 * FUNCTION: pkix_UTF8_to_UTF16
 * DESCRIPTION:
 *
 *  Converts array of bytes pointed to by "utf8String" with length of
 *  "utf8Length" into a freshly allocated UTF-16 string and stores a pointer
 *  to that string at "pDest" and stores the string's length at "pLength". The
 *  caller is responsible for freeing "pDest" using PKIX_PL_Free.
 *
 * PARAMETERS:
 *  "utf8String"
 *      Address of array of bytes representing data source. Must be non-NULL.
 *  "utf8Length"
 *      Length of data source. Must be even.
 *  "pDest"
 *      Address where data will be stored. Must be non-NULL.
 *  "pLength"
 *      Address where data length will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a String Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_UTF8_to_UTF16(
        const void *utf8String,
        PKIX_UInt32 utf8Length,
        void **pDest,
        PKIX_UInt32 *pLength,
        void *plContext)
{
        PKIX_Boolean result;

        PKIX_ENTER(STRING, "pkix_UTF8_to_UTF16");
        PKIX_NULLCHECK_THREE(utf8String, pDest, pLength);

        /* XXX How big can a UTF8 string be compared to a UTF16? */
        PKIX_CHECK(PKIX_PL_Calloc(1, utf8Length*2, pDest, plContext),
                    PKIX_MALLOCFAILED);

        PKIX_STRING_DEBUG("\tCalling PORT_UCS2_UTF8Conversion).\n");
        result = PORT_UCS2_UTF8Conversion
                (PKIX_TRUE, /* True = From UTF8 */
                (unsigned char *)utf8String,
                utf8Length,
                (unsigned char *)*pDest,
                utf8Length*2, /* Max Size */
                pLength);
        if (result == PR_FALSE){
                PKIX_ERROR(PKIX_PORTUCS2UTF8CONVERSIONFAILED);
        }

        PKIX_CHECK(PKIX_PL_Realloc(*pDest, *pLength, pDest, plContext),
                    PKIX_REALLOCFAILED);

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_FREE(*pDest);
        }

        PKIX_RETURN(STRING);
}
