/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1996, 1997                                           *
*   (C) Copyright International Business Machines Corporation,  1996, 1997              *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*/
//	Copyright (C) 1996-1997 Taligent, Inc. All rights reserved.
//
//  FILE NAME : unicode.h
//
//	CREATED
//		Wednesday, December 11, 1996
//
//	CREATED BY
//		Helena Shih
//
//
//********************************************************************************************



#ifndef _UNICODE
#define _UNICODE

#include "ptypes.h"
class CompactByteArray;
class CompactShortArray;

#ifdef NLS_MAC
#pragma export on
#endif

/**
 * The Unicode class allows you to query the properties associated with individual 
 * Unicode character values.  
 * <p>
 * The Unicode character information, provided implicitly by the 
 * Unicode character encoding standard, includes information about the sript 
 * (for example, symbols or control characters) to which the character belongs,
 * as well as semantic information such as whether a character is a digit or 
 * uppercase, lowercase, or uncased.
 * <P>
 * @subclassing Do not subclass.
 */
class T_UTILITY_API Unicode
{
public:
    /**
     * The minimum value a UniChar can have.  The lowest value a
     * UniChar can have is 0x0000.
     */
    static const UniChar   MIN_VALUE;

    /**
     * The maximum value a UniChar can have.  The greatest value a
     * UniChar can have is 0xffff.
     */
    static const UniChar   MAX_VALUE;

    /**
     * Public data for enumerated Unicode general category types
     */

    enum EUnicodeGeneralTypes
	{
		UNASSIGNED				= 0,
		UPPERCASE_LETTER		= 1,
		LOWERCASE_LETTER		= 2,
		TITLECASE_LETTER		= 3,
		MODIFIER_LETTER			= 4,
		OTHER_LETTER			= 5,
		NON_SPACING_MARK		= 6,
		ENCLOSING_MARK			= 7,
		COMBINING_SPACING_MARK	= 8,
		DECIMAL_DIGIT_NUMBER	= 9,
		LETTER_NUMBER			= 10,
		OTHER_NUMBER			= 11,
		SPACE_SEPARATOR			= 12,
		LINE_SEPARATOR			= 13,
		PARAGRAPH_SEPARATOR		= 14,
		CONTROL					= 15,
		FORMAT					= 16,
		PRIVATE_USE				= 17,
		SURROGATE				= 18,
		DASH_PUNCTUATION		= 19,
		START_PUNCTUATION		= 20,
		END_PUNCTUATION			= 21,
		CONNECTOR_PUNCTUATION	= 22,
		OTHER_PUNCTUATION		= 23,
		MATH_SYMBOL				= 24,
		CURRENCY_SYMBOL			= 25,
		MODIFIER_SYMBOL			= 26,
		OTHER_SYMBOL			= 27,
		GENERAL_TYPES_COUNT		= 28
	};

    /**
     * This specifies the language directional property of a character set.
     */
    enum EDirectionProperty { 
        LEFT_TO_RIGHT               = 0, 
        RIGHT_TO_LEFT               = 1, 
        EUROPEAN_NUMBER             = 2,
        EUROPEAN_NUMBER_SEPARATOR   = 3,
        EUROPEAN_NUMBER_TERMINATOR  = 4,
        ARABIC_NUMBER               = 5,
        COMMON_NUMBER_SEPARATOR     = 6,
        BLOCK_SEPARATOR             = 7,
        SEGMENT_SEPARATOR           = 8,
        WHITE_SPACE_NEUTRAL         = 9, 
        OTHER_NEUTRAL               = 10 
    };

	/**
	 * Values returned by the getCellWidth() function.
	 * @see Unicode#getCellWidth
	 */
	enum ECellWidths
	{
		ZERO_WIDTH				= 0,
		HALF_WIDTH				= 1,
		FULL_WIDTH				= 2,
		NEUTRAL					= 3
	};

   /**
     * Determines whether the specified UniChar is a lowercase character
	 * according to Unicode 2.0.14.
     *
     * @param ch	the character to be tested
     * @return 	true if the character is lowercase; false otherwise.
     *
     * @see Unicode#isUpperCase
     * @see Unicode#isTitleCase
     * @see Unicode#toLowerCase
     */
	static	t_bool			isLowerCase(			UniChar		ch);
    /**
     * Determines whether the specified character is an uppercase character
	 * according to Unicode 2.0.14.
     *
     * @param ch	the character to be tested
     * @return 	true if the character is uppercase; false otherwise.
     * @see Unicode#isLowerCase
     * @see Unicode#isTitleCase
     * @see Unicode#toUpperCase
     */

    static	t_bool			isUpperCase(			UniChar ch);

    /**
     * Determines whether the specified character is a titlecase character
	 * according to Unicode 2.0.14.
     *
     * @param ch	the character to be tested
     * @return 	true if the character is titlecase; false otherwise.
     * @see Unicode#isUpperCase
     * @see Unicode#isLowerCase
     * @see Unicode#toTitleCase
     */

    static	t_bool			isTitleCase(			UniChar ch);

    /**
     * Determines whether the specified character is a digit according to Unicode
     * 2.0.14.
     *
     * @param ch	the character to be tested
     * @return 	true if the character is a digit; false otherwise.
     */

    static	t_bool			isDigit(			UniChar ch) ;

	/**
     * Determines whether the specified numeric value is actually a defined character
	 * according to Unicode 2.0.14.
     *
     * @param ch	the character to be tested
     * @return 	true if the character has a defined Unicode meaning; false otherwise.
     *
     * @see Unicode#isDigit
     * @see Unicode#isLetter
     * @see Unicode#isLetterOrDigit
     * @see Unicode#isUpperCase
     * @see Unicode#isLowerCase
     * @see Unicode#isTitleCase
     */

     static  t_bool          isDefined(UniChar       ch);

    /**
     * Determines whether the specified character is a control character according 
     * to Unicode 2.0.14.
     *
     * @param ch    the character to be tested
     * @return  true if the Unicode character is a control character; false otherwise.
     *
     * @see Unicode#isPrintable
     */
    static  t_bool          isControl(UniChar ch);

    /**
     * Determines whether the specified character is a printable character according 
     * to Unicode 2.0.14.
     *
     * @param ch    the character to be tested
     * @return  true if the Unicode character is a printable character; false otherwise.
     *
     * @see Unicode#isControl
     */
    static  t_bool          isPrintable(UniChar ch);

    /**
     * Determines whether the specified character is of the base form according 
     * to Unicode 2.0.14.
     *
     * @param ch    the character to be tested
     * @return  true if the Unicode character is of the base form; false otherwise.
     *
     * @see Unicode#isLetter
     * @see Unicode#isDigit
     */

     static  t_bool          isBaseForm(UniChar ch);
    /**
     * Determines whether the specified character is a letter
     * according to Unicode 2.0.14.
     *
     * @param ch    the character to be tested
     * @return  true if the character is a letter; false otherwise.
     *
     *
     * @see Unicode#isDigit
     * @see Unicode#isLetterOrDigit
     * @see Unicode#isUpperCase
     * @see Unicode#isLowerCase
     * @see Unicode#isTitleCase
     */
    static  t_bool          isLetter(UniChar        ch);

    /**
     * A convenience method for determining if a Unicode character
     * is allowed as the first character in a Java identifier.
     * <P>
     * A character may start a Java identifier if and only if
     * it is one of the following:
     * <ul>
     * <li>  a letter
     * <li>  a currency symbol (such as "$")
     * <li>  a connecting punctuation symbol (such as "_").
     * </ul>
     *
     * @param   ch  the Unicode character.
     * @return  TRUE if the character may start a Java identifier;
     *          FALSE otherwise.
     * @see     isJavaIdentifierPart
     * @see     isLetter
     * @see     isUnicodeIdentifierStart
     */
    static t_bool isJavaIdentifierStart(UniChar ch);

    /**
     * A convenience method for determining if a Unicode character 
     * may be part of a Java identifier other than the starting
     * character.
     * <P>
     * A character may be part of a Java identifier if and only if
     * it is one of the following:
     * <ul>
     * <li>  a letter
     * <li>  a currency symbol (such as "$")
     * <li>  a connecting punctuation character (such as "_").
     * <li>  a digit
     * <li>  a numeric letter (such as a Roman numeral character)
     * <li>  a combining mark
     * <li>  a non-spacing mark
     * <li>  an ignorable control character
     * </ul>
     * 
     * @param   ch  the Unicode character.
     * @return  TRUE if the character may be part of a Unicode identifier; 
     *          FALSE otherwise.
     * @see     isIdentifierIgnorable
     * @see     isJavaIdentifierStart
     * @see     isLetter
     * @see     isDigit
     * @see     isUnicodeIdentifierPart
     */
    static t_bool isJavaIdentifierPart(UniChar ch);

    /**
     * A convenience method for determining if a Unicode character 
     * is allowed to start in a Unicode identifier.
     * A character may start a Unicode identifier if and only if
     * it is a letter.
     *
     * @param   ch  the Unicode character.
     * @return  TRUE if the character may start a Unicode identifier;
     *          FALSE otherwise.
     * @see     isJavaIdentifierStart
     * @see     isLetter
     * @see     isUnicodeIdentifierPart
     */
    static t_bool isUnicodeIdentifierStart(UniChar ch);

    /**
     * A convenience method for determining if a Unicode character
     * may be part of a Unicode identifier other than the starting
     * character.
     * <P>
     * A character may be part of a Unicode identifier if and only if
     * it is one of the following:
     * <ul>
     * <li>  a letter
     * <li>  a connecting punctuation character (such as "_").
     * <li>  a digit
     * <li>  a numeric letter (such as a Roman numeral character)
     * <li>  a combining mark
     * <li>  a non-spacing mark
     * <li>  an ignorable control character
     * </ul>
     * 
     * @param   ch  the Unicode character.
     * @return  TRUE if the character may be part of a Unicode identifier;
     *          FALSE otherwise.
     * @see     isIdentifierIgnorable
     * @see     isJavaIdentifierPart
     * @see     isLetterOrDigit
     * @see     isUnicodeIdentifierStart
     */
    static t_bool isUnicodeIdentifierPart(UniChar ch);

    /**
     * A convenience method for determining if a Unicode character 
     * should be regarded as an ignorable character in a Java 
     * identifier or a Unicode identifier.
     * <P>
     * The following Unicode characters are ignorable in a Java identifier
     * or a Unicode identifier:
     * <table>
     * <tr><td>0x0000 through 0x0008,</td>
     *                                 <td>ISO control characters that</td></tr>
     * <tr><td>0x000E through 0x001B,</td> <td>are not whitespace</td></tr>
     * <tr><td>and 0x007F through 0x009F</td></tr>
     * <tr><td>0x200C through 0x200F</td>  <td>join controls</td></tr>
     * <tr><td>0x200A through 0x200E</td>  <td>bidirectional controls</td></tr>
     * <tr><td>0x206A through 0x206F</td>  <td>format controls</td></tr>
     * <tr><td>0xFEFF</td>               <td>zero-width no-break space</td></tr>
     * </table>
     * 
     * @param   ch  the Unicode character.
     * @return  TRUE if the character may be part of a Unicode identifier;
     *          FALSE otherwise.
     * @see     isJavaIdentifierPart
     * @see     isUnicodeIdentifierPart
     */
    static t_bool isIdentifierIgnorable(UniChar ch);

    /**
     * The given character is mapped to its lowercase equivalent according to
     * Unicode 2.0.14; if the character has no lowercase equivalent, the character 
     * itself is returned.
     * <P>
     * A character has a lowercase equivalent if and only if a lowercase mapping
     * is specified for the character in the Unicode 2.0 attribute table.
     * <P>
     * Unicode::toLowerCase() only deals with the general letter case conversion.
     * For language specific case conversion behavior, use UnicodeString::toLower().
     * For example, the case conversion for dot-less i and dotted I in Turkish,
     * or for final sigma in Greek.
     *
     * @param ch    the character to be converted
     * @return  the lowercase equivalent of the character, if any;
     *      otherwise the character itself.
     *
     * @see UnicodeString#toLower
     * @see Unicode#isLowerCase
     * @see Unicode#isUpperCase
     * @see Unicode#toUpperCase
     * @see Unicode#toTitleCase
     */
   static   UniChar         toLowerCase(UniChar     ch); 

    /**
     * The given character is mapped to its uppercase equivalent according to Unicode
     * 2.0.14; if the character has no uppercase equivalent, the character itself is 
     * returned.
     * <P>
     * Unicode::toUpperCase() only deals with the general letter case conversion.
     * For language specific case conversion behavior, use UnicodeString::toUpper().
     * For example, the case conversion for dot-less i and dotted I in Turkish,
     * or ess-zed (i.e., "sharp S") in German.
     *
     * @param ch    the character to be converted
     * @return  the uppercase equivalent of the character, if any;
     *      otherwise the character itself.
     *
     * @see UnicodeString#toUpper
     * @see Unicode#isUpperCase
     * @see Unicode#isLowerCase
     * @see Unicode#toLowerCase
     * @see Unicode#toTitleCase
     */
    static  UniChar         toUpperCase(UniChar     ch);

    /**
     * The given character is mapped to its titlecase equivalent according to Unicode
     * 2.0.14.  There are only four Unicode characters that are truly titlecase forms
     * that are distinct from uppercase forms.  As a rule, if a character has no
     * true titlecase equivalent, its uppercase equivalent is returned.
     * <P>
     * A character has a titlecase equivalent if and only if a titlecase mapping
     * is specified for the character in the Unicode 2.0.14 data.
     *
     * @param ch    the character to be converted
     * @return  the titlecase equivalent of the character, if any;
     *      otherwise the character itself.
     * @see Unicode#isTitleCase
     * @see Unicode#toUpperCase
     * @see Unicode#toLowerCase
     */
    static  UniChar             toTitleCase(UniChar     ch);

    /**
     * Determines if the specified character is a Unicode space character
     * according to Unicode 2.0.14.
     *
     * @param ch    the character to be tested
     * @return  true if the character is a space character; false otherwise.
     */
    static  t_bool              isSpaceChar(UniChar     ch);

   /**
     * Returns a value indicating a character category according to Unicode
     * 2.0.14.
     * @param ch            the character to be tested
     * @return a value of type int, the character category.
     * @see Unicode#UNASSIGNED
     * @see Unicode#UPPERCASE_LETTER
     * @see Unicode#LOWERCASE_LETTER
     * @see Unicode#TITLECASE_LETTER
     * @see Unicode#MODIFIER_LETTER
     * @see Unicode#OTHER_LETTER
     * @see Unicode#NON_SPACING_MARK
     * @see Unicode#ENCLOSING_MARK
     * @see Unicode#COMBINING_SPACING_MARK
     * @see Unicode#DECIMAL_DIGIT_NUMBER
     * @see Unicode#OTHER_NUMBER
     * @see Unicode#SPACE_SEPARATOR
     * @see Unicode#LINE_SEPARATOR
     * @see Unicode#PARAGRAPH_SEPARATOR
     * @see Unicode#CONTROL
     * @see Unicode#PRIVATE_USE
     * @see Unicode#SURROGATE
     * @see Unicode#DASH_PUNCTUATION
     * @see Unicode#OPEN_PUNCTUATION
     * @see Unicode#CLOSE_PUNCTUATION
     * @see Unicode#CONNECTOR_PUNCTUATION
     * @see Unicode#OTHER_PUNCTUATION
     * @see Unicode#LETTER_NUMBER
     * @see Unicode#MATH_SYMBOL
     * @see Unicode#CURRENCY_SYMBOL
     * @see Unicode#MODIFIER_SYMBOL
     * @see Unicode#OTHER_SYMBOL
     */
    static  t_int8          getType(UniChar     ch);

    /**
     * Returns the linguistic direction property of a character.
     * <P>
     * Returns the linguistic direction property of a character.
     * For example, 0x0041 (letter A) has the LEFT_TO_RIGHT directional 
     * property.
     * @see #EDirectionProperty
     */
    static EDirectionProperty characterDirection(UniChar ch);

    /**
     * Returns a value indicating the display-cell width of the character
     * when used in Asian text, according to the Unicode standard (see p. 6-130
     * of The Unicode Standard, Version 2.0).  The results for various characters
     * are as follows:
     * <P>
     *      ZERO_WIDTH: Characters which are considered to take up no display-cell space:
     *          control characters
     *          format characters
     *          line and paragraph separators
     *          non-spacing marks
     *          combining Hangul jungseong
     *          combining Hangul jongseong
     *          unassigned Unicode values
     * <P>
     *      HALF_WIDTH: Characters which take up half a cell in standard Asian text:
     *          all characters in the General Scripts Area except combining Hangul choseong
     *              and the characters called out specifically above as ZERO_WIDTH
     *          alphabetic and Arabic presentation forms
     *          halfwidth CJK punctuation
     *          halfwidth Katakana
     *          halfwidth Hangul Jamo
     *          halfwidth forms, arrows, and shapes
     * <P>
     *      FULL_WIDTH:  Characters which take up a full cell in standard Asian text:
     *          combining Hangul choseong
     *          all characters in the CJK Phonetics and Symbols Area
     *          all characters in the CJK Ideographs Area
     *          all characters in the Hangul Syllables Area
     *          CJK compatibility ideographs
     *          CJK compatibility forms
     *          small form variants
     *          fullwidth ASCII
     *          fullwidth punctuation and currency signs
     * <P>
     *      NEUTRAL:  Characters whose cell width is context-dependent:
     *          all characters in the Symbols Area, except those specifically called out above
     *          all characters in the Surrogates Area
     *          all charcaters in the Private Use Area
     * <P>
     * For Korean text, this algorithm should work properly with properly normalized Korean
     * text.  Precomposed Hangul syllables and non-combining jamo are all considered full-
     * width characters.  For combining jamo, we treat we treat choseong (initial consonants)
     * as double-width characters and junseong (vowels) and jongseong (final consonants)
     * as non-spacing marks.  This will work right in text that uses the precomposed
     * choseong characters instead of teo choseong characters in a row, and which uses the
     * choseong filler character at the beginning of syllables that don't have an initial
     * consonant.  The results may be slightly off with Korean text following different
     * conventions.
     */
    static t_uint16         getCellWidth(UniChar    ch);

    static	UniChar*			toLowerCase(UniChar* ch, t_int32 size);
    static	UniChar*			toUpperCase(UniChar* ch, t_int32 size);
    static	int				    compare(const UniChar* ch, t_int32 size, const UniChar* ch1, t_int32 size1);
    static	int				    compareIgnoreCase(const UniChar* ch, t_int32 size, const UniChar* ch1, t_int32 size1);
    static  TextOffset			indexOf(const UniChar* ch, t_int32 size, const UniChar* character, t_int32 length, TextOffset fromOffset = 0);
    static  TextOffset			lastIndexOf(const UniChar* ch, t_int32 size, const UniChar* character, t_int32 length, TextOffset fromOffset = 0);
    static  size_t              length(const UniChar* str);
    static  UniChar*            copy(const UniChar* str);


	//
	// Netscape Added Methods
	//

	/**
     * Releases a UniChar array allocated by one of the Unicode methods.
	 */
	static	void				release(UniChar* buffer);

	/**
     * Returns a pointer to the first instance of ch in the
	 * Unicode string.
     * @param str	            string to search
     * @param strLength			length of str in UniChars
	 * @param ch				UniChar to search for
	 */
	static	const UniChar*		strchr(const UniChar* str, size_t strLength, UniChar ch);

	/**
     * Returns a pointer to the last instance of ch in the
	 * Unicode string.
     * @param str	            string to search
     * @param strLength			length of str in UniChars
	 * @param ch				UniChar to search for
	 */
	static	const UniChar*		strrchr(const UniChar* str, size_t strLength, UniChar ch);

	/**
     * Compares two UniChar arrays based on the underlying file system
	 * mode of comparison.
     * @param source            array to be compared
     * @param sourceLength		length of source in UniChars
	 * @param target			array to be compared
	 * @param targetLength		length of target in UniChars
	 */
	static	int					fileNameCompare(const UniChar* source, size_t sourceLength, const UniChar* target, size_t targetLength);

	/**
     * Concatenates two UniChar arrays and returns the result in a new buffer
     * @param str1	            first string
     * @param sourceLength		length of str1 in UniChars
	 * @param str2				second string
	 * @param str2Length		length of second string
	 */
	static	UniChar*			appendString(const UniChar* str1,size_t str1Length, const UniChar* str2,size_t str2Length);

	/**
	 * Regular Expression-like search through a UniChar string.
	 * @param string			string to search
	 * @param stringLength		length of string in UniChars
	 * @param expression		regular expression (as a C-string)
	 */
	static	TextOffset			regularExpressionSearch(const UniChar* string, size_t stringLength, const char* expression);

	/**
	 * Regular Expression-like search through a UniChar string.
	 * @param source			string to search
	 * @param sourceLength		length of source in UniChars
	 * @param expression		regular expression (as a UniChar)
	 * @param expressionLength	length of regular expression in UniChars
	 */
	static	TextOffset			regularExpressionSearch(const UniChar* string, size_t stringLength, const UniChar* expression, size_t expressionLength);

protected:
	// These constructors, destructor, and assignment operator must
	// be protected (not private, as they semantically are) to make
	// various UNIX compilers happy. [LIU]
							Unicode();
							Unicode(	const	Unicode&	other);
							~Unicode();
	const	Unicode&		operator=(	const	Unicode&	other);

private:
    static  const t_int8    isLetterMask;
    static  const t_uint16  indicies[];
    static  const t_int8    values[];
    static  const t_int32   offsetCount;
    static  const t_uint16  caseIndex[];
    static  const t_int16   caseValues[];
    static  const t_int32   caseCount;
    static  const t_uint16  fCharDirIndices[];
    static  const t_int8    fCharDirValues[];
    static  const t_int32   fCharDirCount;

    static  CompactByteArray *tables;
    static  CompactShortArray *ulTables;
    static  CompactByteArray *dirTables;

    static  t_bool  tablesCreated;
    static  t_bool  ulTablesCreated;
    static  t_bool  dirTablesCreated;
    static  void    createTables();
    static  void    createUlTables();
    static  void    createDirTables();

};

#ifdef NLS_MAC
#pragma export off
#endif


#endif
