/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef _NLSUNI_H
#define _NLSUNI_H


#include "nlsxp.h"

#ifdef NLS_CPLUSPLUS
#include "unistring.h"
#include "unicode.h"
#endif

NLS_BEGIN_PROTOS

/**************************** Types *********************************/

/**
 * LESS is returned if source string is compared to be less than target
 * string in the compare() method.
 * EQUAL is returned if source string is compared to be equal to target
 * string in the compare() method.
 * GREATER is returned if source string is compared to be greater than
 * target string in the compare() method.
 */
typedef int NLS_ComparisonResult;
enum _NLS_ComparisonResult {
        NLS_LESS	= -1,
        NLS_EQUAL	= 0,
        NLS_GREATER = 1
    };


/**
 * Public data for enumerated Unicode general category types
 */
typedef int NLS_UniCharType; 
enum _NLS_UniCharType {
		NLS_UNASSIGNED				= 0,
		NLS_UPPERCASE_LETTER		= 1,
		NLS_LOWERCASE_LETTER		= 2,
		NLS_TITLECASE_LETTER		= 3,
		NLS_MODIFIER_LETTER			= 4,
		NLS_OTHER_LETTER			= 5,
		NLS_NON_SPACING_MARK		= 6,
		NLS_ENCLOSING_MARK			= 7,
		NLS_COMBINING_SPACING_MARK	= 8,
		NLS_DECIMAL_DIGIT_NUMBER	= 9,
		NLS_LETTER_NUMBER			= 10,
		NLS_OTHER_NUMBER			= 11,
		NLS_SPACE_SEPARATOR			= 12,
		NLS_LINE_SEPARATOR			= 13,
		NLS_PARAGRAPH_SEPARATOR		= 14,
		NLS_CONTROL					= 15,
		NLS_FORMAT					= 16,
		NLS_PRIVATE_USE				= 17,
		NLS_SURROGATE				= 18,
		NLS_DASH_PUNCTUATION		= 19,
		NLS_START_PUNCTUATION		= 20,
		NLS_END_PUNCTUATION			= 21,
		NLS_CONNECTOR_PUNCTUATION	= 22,
		NLS_OTHER_PUNCTUATION		= 23,
		NLS_MATH_SYMBOL				= 24,
		NLS_CURRENCY_SYMBOL			= 25,
		NLS_MODIFIER_SYMBOL			= 26,
		NLS_OTHER_SYMBOL			= 27,
		NLS_GENERAL_TYPES_COUNT		= 28
};


/* UnicodeString is the basic type which replaces the char* datatype.
    - A UnicodeString is an opaque data type, the Unicode String Accessors must be 
      used to access the data.
    */
#ifndef NLS_CPLUSPLUS
typedef struct _UnicodeString UnicodeString;
#endif


/********************* UnicodeString Constructor functions *******************************/
/* Constructors for UnicodeStrings, an Empty Unicode String */
NLSUNIAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewUnicodeString(UnicodeString ** result);

/* Constructors for UnicodeStrings, a copy of an Unicode String */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_NewUnicodeStringFromUnicodeString(UnicodeString ** result, 
                                         const UnicodeString * source);

/* Constructors for UnicodeStrings, a copy of an UniChar[] */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_NewUnicodeStringFromUniChar(UnicodeString ** result,
                                   const UniChar * that, size_t thatLength);

/* Constructors for UnicodeStrings, a copy of a Char[] */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_NewUnicodeStringFromChar(UnicodeString ** result,
                                const char * that, const char * from_charset);

/********************* UnicodeString Destructor functions *******************************/
/* Destructor for an UnicodeStrings */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteUnicodeString(UnicodeString * that);

/********************* UnicodeString easy conversion functions *******************************/

NLSUNIAPI_PUBLIC(size_t)
NLS_CharSizeForUnicodeString(const UnicodeString * source, const char * to_charset);


NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_CharFromUnicodeString(const UnicodeString * source,
								char * buffer, size_t bufferLength, 
								const char * to_charset);



/********************* UnicodeString Comparison functions *******************************/

/** Compares a UnicodeString to something else
 *    All versions of compare() do bitwise comparison; internationally-
 *    sensitive comparison requires the NLS_Collation API's. */


/* Comparison for two UnicodeStrings */
NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UnicodeStringCompare(const UnicodeString * source, const UnicodeString * target);

/* Comparison offsets for two UnicodeStrings */
NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UnicodeStringCompareOffset(const UnicodeString * source, TextOffset sourceOffset, 
                         size_t sourcelength, const UnicodeString * target,
                         TextOffset targetOffset, size_t targetLength);

/* Comparison offsets for two UnicodeStrings */
NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UnicodeStringCompareRange(const UnicodeString * source, TextOffset sourceStartOffset, 
                         TextOffset sourceEndOffset, const UnicodeString * target,
                         TextOffset targetStartOffset, TextOffset targetEndOffset);


/* Comparison to a UniChar[] */
NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UnicodeStringCompareToUniChar(const UnicodeString * source, 
                                  const UniChar * target, size_t targetLength);

/* Case Insensitive comparison */

/* Comparison UnicodeString */
NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UnicodeStringCompareIgnoreCase(const UnicodeString * source, const UnicodeString * target);

/* Comparison to a UniChar[] */
NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UnicodeStringCompareIgnoreCaseToUniChar(const UnicodeString * source, 
                                  const UniChar * target, size_t targetLength);

/********************* UnicodeString Binary Operator functions *******************************/
/* Equal for two UnicodeStrings */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UnicodeStringEqual(const UnicodeString * source, const UnicodeString * target);

/* Not Equal for two UnicodeStrings */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UnicodeStringNotEqual(const UnicodeString * source, const UnicodeString * target);

/* Greater Than for two UnicodeStrings */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UnicodeStringGreaterThan(const UnicodeString * source, const UnicodeString * target);

/* Less Than for two UnicodeStrings */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UnicodeStringLessThan(const UnicodeString * source, const UnicodeString * target);

/* Empty UnicodeStrings */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UnicodeStringIsEmpty(const UnicodeString * source);

/********************* UnicodeString Accessor functions *******************************/
/** Allows UnicodeString to be used with interfaces that use UniChar*.
 *    Returns a pointer to the UnicodeString's internal storage.  This
 *    storage is still owned by the UnicodeString, and the caller is not
 *    allowed to change it.  The string returned by this function is
 *    correctly null-terminated. */

NLSUNIAPI_PUBLIC(const UniChar *)
NLS_UnicodeStringAccessUniCharArray(const UnicodeString * source);

/** Return the UniChar at the given index
 */
NLSUNIAPI_PUBLIC(UniChar)
NLS_UnicodeStringIndexUniChar(const UnicodeString * source, TextOffset offset);


/** Extract a substring
 *    Extracts the specified substring of the UnicodeString into the
 *    storage referred to by extractInto. */

NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringSubstring(const UnicodeString * source,    
                           TextOffset thisOffset, size_t thisLength, 
                           UnicodeString * result);

/** Extract a substring
 *    The UniChar[] extractInto must be at least thislength+1
 */

NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringSubstringUniChar(const UnicodeString * source,
                                  TextOffset thisOffset, size_t thisLength,
                                  UniChar * extractInto);


/** Extract a substring
 *    Same as extract(), but the substring is specified as starting and
 *    ending offsets (the range is [thisStart, thisEnd) ), rather than
 *    as a starting offset and a length. */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringSubstringBetween(const UnicodeString * source,
                                  TextOffset thisStart, TextOffset thisEnd, 
                                  UnicodeString * result);


/** Concatenate UnicodeStrings
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringConcatenateUnicodeString(UnicodeString * target,
                                  const UnicodeString * source);

/** Concatenate a UniChar[]
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringConcatenateUniChar(UnicodeString * target,
                                  const UniChar * source);

/** Insert a UnicodeString
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringInsertUnicodeString(UnicodeString * target,
                                     TextOffset thisOffset, 
                                     const UnicodeString* source);

/** Insert a UniChar[]
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringInsertUniChar(UnicodeString * target,
                                     TextOffset thisOffset, 
                                     const UniChar * source);

/** delete characters,
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringRemove(UnicodeString * target,
                        TextOffset    offset, size_t length);

/** delete characters
 *    Same as remove(), but the range of characters to delete is specified
 *    as a pair of starting and ending offsets [start, end), rather than
 *    a starting offset and a character count. */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringRemoveBetween(UnicodeString * target,
                               TextOffset    start, TextOffset end);

/** Replace Range, for UnicodeString, UniChar[], and Char[]
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringReplace(UnicodeString * target,
                         TextOffset targetOffset, size_t targetLength,
                         const UnicodeString *    source,
                         TextOffset sourceOffset, size_t sourceLength);
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringReplaceUniChar(UnicodeString * target,
                         TextOffset targetOffset, size_t targetLength,
                         const UniChar *    source, size_t sourceLength);

/** replace characters
 *    Same as replace(), but the affected subranges are specified as
 *    pairs of starting and ending offsets ( [start, end) ) rather than
 *    starting offsets and lengths. */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringReplaceBetween(UnicodeString * target,
                         TextOffset targetStart, TextOffset targetEnd,
                         const UnicodeString *    source,
                         TextOffset sourceStart, TextOffset sourceEnd);

/** 
 * Scanning, and indexing, returns TextOffset within target, or -1 
 * if the substring does not exist.
 *
 */

NLSUNIAPI_PUBLIC(TextOffset)
NLS_UnicodeStringIndexOfString(const UnicodeString * target,
						TextOffset startOffset, 
						const UnicodeString * subString);

NLSUNIAPI_PUBLIC(TextOffset)
NLS_UnicodeStringLastIndexOfString(const UnicodeString * target,
						TextOffset startOffset, 
						const UnicodeString *    subString);

NLSUNIAPI_PUBLIC(TextOffset)
NLS_UnicodeStringIndexOfUniChar(const UnicodeString * target,
						TextOffset startOffset, 
						const UniChar * subString, 
						size_t subStringLength);

NLSUNIAPI_PUBLIC(TextOffset)
NLS_UnicodeStringLastIndexOfUniChar(const UnicodeString * target,
						TextOffset startOffset, 
						const UniChar * subString, 
						size_t subStringLength);


/** Locale InSensitive toUpper & toLower 
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringtoUpper(UnicodeString * target);
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringtoLower(UnicodeString * target);

/** Return the Size of the UnicodeString
 */
NLSUNIAPI_PUBLIC(size_t)
NLS_UnicodeStringSize(const UnicodeString * source);

/************************ UniChar Type Methods ******************************/

/*
 * Creates a UniChar* from a char*.
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_NewUniCharFromChar(UniChar** uniChar, const char *from_charset, const byte *inputBuffer, size_t inputBufferLength);

/*
 * Create a new UniChar* from a UniChar*
 * May return NULL in the case of a memory allocation failure
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_NewUniCharFromUniChar(UniChar** uniChar, const UniChar *inputBuffer, size_t uniCharCount);

/*
 * deletes the memory associated with a UniChar*
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteUniChar(UniChar* uniChar);

/** normally, UniChar string is not null terminated,
 ** and there's no simple way to get the length from the array.
 */
/** Return the Length of the UniChar string
 */
NLSUNIAPI_PUBLIC(size_t)
NLS_UniCharLength(const UniChar *src);

/** Locale InSensitive toUpper & toLower 
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UniChartoUpper(UniChar * target, size_t length);
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UniChartoLower(UniChar * target, size_t length);

/* Comparison for two UniChar */
NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UniCharCompare(const UniChar * source, size_t sourceLength, const UniChar * target, size_t targetLength);

/* Comparison UniChar */
NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UniCharCompareIgnoreCase(const UniChar * source, size_t sourceLength, const UniChar * target, size_t targetLength);

/* Equal for two UniChar */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UniCharEqual(const UniChar * source, size_t sourceLength, const UniChar * target, size_t targetLength);

/* Not Equal for two UniChar */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UniCharNotEqual(const UniChar * source, size_t sourceLength, const UniChar * target, size_t targetLength);

/* Greater Than for two UniChar */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UniCharGreaterThan(const UniChar * source, size_t sourceLength, const UniChar * target, size_t targetLength);

/* Less Than for two UniChar */
NLSUNIAPI_PUBLIC(nlsBool) 
NLS_UniCharLessThan(const UniChar * source, size_t sourceLength, const UniChar * target, size_t targetLength);

/** 
 * Scanning, and indexing, returns TextOffset within target, or -1 
 * if the substring does not exist.
 *
 */

NLSUNIAPI_PUBLIC(TextOffset)
NLS_UniCharIndexOfString(const UniChar * target, size_t targetLength,
						TextOffset startOffset, 
						const UniChar * subString, size_t subStringLength);

NLSUNIAPI_PUBLIC(TextOffset)
NLS_UniCharLastIndexOfString(const UniChar * target, size_t targetLength,
						TextOffset startOffset, 
						const UniChar *    subString, size_t subStringLength);

/*
 * UniChar character functions
 */
NLSUNIAPI_PUBLIC(nlsBool)
NLS_IsLowerCase (UniChar ch);

NLSUNIAPI_PUBLIC(nlsBool)
NLS_IsUpperCase (UniChar ch);

NLSUNIAPI_PUBLIC(nlsBool)
NLS_IsTitleCase (UniChar ch);

NLSUNIAPI_PUBLIC(nlsBool)
NLS_IsDigit (UniChar ch);

NLSUNIAPI_PUBLIC(nlsBool)
NLS_IsDefined (UniChar ch);

NLSUNIAPI_PUBLIC(nlsBool)
NLS_IsSpaceChar (UniChar ch);

NLSUNIAPI_PUBLIC(nlsBool)
NLS_IsLetter (UniChar ch);

NLSUNIAPI_PUBLIC(UniChar)
NLS_ToLowerCase (UniChar ch);

NLSUNIAPI_PUBLIC(UniChar)
NLS_ToUpperCase (UniChar ch);

NLSUNIAPI_PUBLIC(UniChar)
NLS_ToTitleCase (UniChar ch);

NLSUNIAPI_PUBLIC(NLS_UniCharType)
NLS_GetType(UniChar ch);

NLSUNIAPI_PUBLIC(const UniChar*)
NLS_UniCharStrchr(const UniChar* target, size_t targetLength, UniChar ch);

NLSUNIAPI_PUBLIC(const UniChar*)
NLS_UniCharStrrchr(const UniChar* target, size_t targetLength, UniChar ch);

NLSUNIAPI_PUBLIC(NLS_ComparisonResult)
NLS_UniCharFileNameCompare(const UniChar* source, size_t sourceLength, const UniChar* target, size_t targetLength); 

NLSUNIAPI_PUBLIC(UniChar*)
NLS_UniCharAppendString(const UniChar* source, size_t sourceLength, const UniChar* target, size_t targetLength);

NLSUNIAPI_PUBLIC(nlsBool)
NLS_UniCharRegularExpressionSearch(const UniChar* source, size_t sourceLength, const char *expression);

NLSUNIAPI_PUBLIC(nlsBool)
NLS_UniCharRegularUniCharExpressionSearch(const UniChar* source, size_t sourceLength, const UniChar* uExpression, size_t uExpressionLength);

/************************ Formatting ******************************/

NLSUNIAPI_PUBLIC(size_t)
NLS_SPrintF (UnicodeString *result, const UnicodeString *pattern, ...);


/************************ End ******************************/
NLS_END_PROTOS
#endif /* _NLSUNI_H */

