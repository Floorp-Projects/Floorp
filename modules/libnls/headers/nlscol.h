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


#ifndef _NLSCOL_H
#define _NLSCOL_H


#include "nlsxp.h"
#include "nlsuni.h"
#include "nlsloc.h"

#ifdef NLS_CPLUSPLUS
#include "coll.h"
#include "sortkey.h"
#endif

NLS_BEGIN_PROTOS

/**************************** Types *********************************/


/**
* NO_DECOMPOSITION : Accented characters will not be decomposed for sorting.  
* Please see class description for more details.
* CANONICAL_DECOMPOSITION : Characters that are canonical variants according 
* to Unicode 2.0 will be decomposed for sorting.  This is the default setting. 
* FULL_DECOMPOSITION : Both canonical variants and compatibility variants be
* decomposed for sorting.
*/

typedef int NLS_DecompositionMode;
enum _NLS_DecompositionMode {
        NLS_NO_DECOMPOSITION,
        NLS_CANONICAL_DECOMPOSITION,
        NLS_FULL_DECOMPOSITION
    };
/**
 * Base letter represents a primary difference.  Set comparison
 * level to PRIMARY to ignore secondary and tertiary differences.
 * Use this to set the strength of a collation object.
 * Example of primary difference, "abc" < "abd"
 * 
 * Diacritical differences on the same base letter represent a secondary
 * difference.  Set comparison level to SECONDARY to ignore tertiary
 * differences. Use this to set the strength of a collation object.
 * Example of secondary difference, "ä" >> "a".
 *
 * Uppercase and lowercase versions of the same character represents a
 * tertiary difference.  Set comparison level to TERTIARY to include
 * all comparison differences. Use this to set the strength of a collation
 * object.
 * Example of tertiary difference, "abc" <<< "ABC".
 *
 * Two characters are considered "identical" when they are equivalent
 * unicode spellings.
 * For example, "ä" == "a¨".
 *
 * ECollationStrength is also used to determine the strength of sort keys 
 * generated from Collation objects.
 */

typedef int NLS_CollationStrength;
enum _NLS_CollationStrength {
        NLS_PRIMARY,
        NLS_SECONDARY,
        NLS_TERTIARY,
        NLS_IDENTICAL
    };
/* UnicodeString is the basic type which replaces the char* datatype.
    - A UnicodeString is an opaque data type, the Unicode String Accessors must be 
      used to access the data.
    */
#ifndef NLS_CPLUSPLUS
typedef struct _Collator Collator;          /* deprecated names */
typedef struct _CollationKey CollationKey;
typedef struct _Collation Collation;
typedef struct _SortKey SortKey;
#endif

/************************ Library Termination  ******************************/
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_ColTerminate(void);

/************************ Collation Constructor/Destructor ******************************/

NLSCOLAPI_PUBLIC(const Collator*)
NLS_GetDefaultCollation();

NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_NewCollation(Collator ** collation, const Locale * locale);

NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteCollation(Collator * collation);

/************************ Collation Version ******************************/

/*
	Get the major and minor version number from the collation engine. 
	Major version numbers are changed when the collation key and/or sorting for a 
	given locale has changed. The version number is of format 01.00 where 
	the high order number is the Major version number, and the low order 
	number is the minor version number. 
 */
NLSCOLAPI_PUBLIC(const char*)
NLS_GetCollationVersionNumber(Collator *collation);

/************************ Collation Methods ******************************/

NLSCOLAPI_PUBLIC(NLS_ComparisonResult)
NLS_CollationCompare(const Collator * collation, const UnicodeString *    source, 
            const UnicodeString * target);

NLSCOLAPI_PUBLIC(NLS_ComparisonResult)
NLS_CollationCompareRange(const Collator * collation, const UnicodeString * source, 
            TextOffset sourceStart, TextOffset sourceEnd, 
            const UnicodeString * target,
            TextOffset targetStart, TextOffset targetEnd);

/** Get a sort key.
 * key, must be created by the caller using NLS_NewSortKey prior to calling the routine. 
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_CollationGetSortKey(const Collator * collation, const UnicodeString * source,
            SortKey * key);

/** Get a sort key.
 * key, must be created by the caller using NLS_NewSortKey prior to calling the routine. 
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_CollationGetSortKeyUniChar(const Collator * collation, const UniChar * source, size_t length,
            CollationKey * key);


/** Get a sort key of a specified range.
* key, must be created by the caller using NLS_NewSortKey, prior to calling the routine. 
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_CollationGetSortKeyForRange(const Collator * collation, const UnicodeString * source,
            TextOffset start, TextOffset end, CollationKey * key);

/** Get a sort key for a non-Unicode encoding.
 *  Key and Collator must be created by caller prior to calling the routine.
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_CollationGetSortKeyChar(const Collator *collation, const byte* source, size_t length,
	    const char* encoding, CollationKey *key);

/**
 * Convenience method for comparing the equality of two strings based on
 * the collation rules.
 */
NLSCOLAPI_PUBLIC(nlsBool)
NLS_CollationGreater(const Collator * collation, const UnicodeString * source, 
            const UnicodeString * target);
/**
 * Convenience method for comparing two strings based on the collation
 * rules.
 */
NLSCOLAPI_PUBLIC(nlsBool)
NLS_CollationGreaterOrEqual(const Collator * collation, const UnicodeString * source, 
            const UnicodeString * target);
/**
 * Convenience method for comparing two strings based on the collation
 * rules.
 */
NLSCOLAPI_PUBLIC(nlsBool)
NLS_CollationEquals(const Collator * collation, const UnicodeString * source, 
            const UnicodeString * target);
    
/**
 * Get the decomposition mode of the collation object.
 */
NLSCOLAPI_PUBLIC(NLS_DecompositionMode)
NLS_CollationGetDecomposition(const Collator * collation);
/**
 * Set the decomposition mode of the collation object.
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_CollationSetDecomposition(Collator * collation, NLS_DecompositionMode mode);
/**
 * Determines the minimum strength that will be use in comparison or
 * transformation.
 */
NLSCOLAPI_PUBLIC(NLS_CollationStrength)
NLS_CollationGetStrength(const Collator * collation);
/**
 * Sets the minimum strength to be used in comparison or transformation.
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_CollationSetStrength(Collator * collation,    NLS_CollationStrength newStrength);
/**
 * Get name of the object for the desired Locale, in the desired langauge
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_CollationGetDisplayName(const Collator * collation,
					const Locale * objectLocale,
                    const Locale * displayLocale,
                    UnicodeString * name);
/**
 * Get name of the object for the desired Locale, in the langauge of the
 * default locale.
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_CollationGetDefaultDisplayName(const Collator * collation,
					const Locale * objectLocale,
                    UnicodeString * name);


/************************ SortKey Methods ******************************/

/**
 * This creates an empty sort key based on the null string.  An empty 
 * sort key contains no sorting information.  When comparing two empty
 * sort keys, the result is EQUAL.
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_NewSortKey(CollationKey ** key);

/**
 * Creates a sort key based on the sort key values.  
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_NewSortKeyFromValues(CollationKey ** key,
            const byte * values,
            size_t count);

/** 
 * Sort key destructor.
 */
NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteSortKey(CollationKey *key);

/**
 * Compare if two sort keys are the same.
 * @param source the sort key to compare to.
 * @return Returns TRUE if two sort keys are equal, FALSE otherwise.
 */
NLSCOLAPI_PUBLIC(nlsBool)
NLS_SortKeyEqual(const CollationKey * source, const CollationKey * target);

/** 
 * Extracts the sort key values, the byte array must be pre-allocated 
 * by the caller, and count must specify the number of elements.  
 */
NLSCOLAPI_PUBLIC(size_t)
NLS_GetSortKeyValueLength(const CollationKey * source);

NLSCOLAPI_PUBLIC(NLS_ErrorCode)
NLS_SortKeyExtractValues(const CollationKey * source, byte * array, size_t count);

/**
 * Convenience method which does a string(bit-wise) comparison of the
 * two sort keys.
 */
NLSCOLAPI_PUBLIC(NLS_ComparisonResult)
NLS_SortKeyCompare(const CollationKey * source, const CollationKey * target);


/************************ End ******************************/
NLS_END_PROTOS
#endif /* _NLSCOL_H */

