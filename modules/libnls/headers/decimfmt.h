/*
********************************************************************************
*                                                                              *
* COPYRIGHT:                                                                   *
*   (C) Copyright Taligent, Inc.,  1997                                        *
*   (C) Copyright International Business Machines Corporation,  1997           *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.         *
*   US Government Users Restricted Rights - Use, duplication, or disclosure    *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                     *
*                                                                              *
********************************************************************************
*
* File DECIMFMT.H
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/20/97    clhuang     Updated per C++ implementation.
*   04/03/97    aliu        Rewrote parsing and formatting completely, and
*                           cleaned up and debugged.  Actually works now.
*   04/17/97    aliu        Changed DigitCount to int per code review.
*   07/10/97    helena      Made ParsePosition a class and get rid of the function
*                           hiding problems.
*   09/09/97    aliu        Ported over support for exponential formats.
********************************************************************************
*/
 
#ifndef _DECIMFMT
#define _DECIMFMT
 
#include "ptypes.h"
#include "numfmt.h"
#include "locid.h"
class DecimalFormatSymbols;
class DigitList;

/**
 * Concrete class for formatting decimal numbers, allowing a variety
 * of parameters, and localization to Western, Arabic, or Indic numbers.
 * <P>
 * Normally, you get the proper NumberFormat for a specific locale
 * (including the default locale) using the NumberFormat factory methods,
 * rather than constructing a DecimalNumberFormat directly.
 * <P>
 * Either the prefixes or the suffixes must be different for the parse
 * to distinguish positive from negative.  Parsing will be unreliable
 * if the digits, thousands or decimal separators are the same, or if
 * any of them occur in the prefixes or suffixes.
 * <P>
 * [Special cases:] 
 * <P>
 * NaN is formatted as a single character, typically \\uFFFD.
 * <P>
 * +/-Infinity is formatted as a single character, typically \\u221E,
 * plus the positive and negative pre/suffixes.
 * <P>
 * Note: this class is designed for common users; for very large or small
 * numbers, use a format that can express exponential values.
 * <P>
 * [Example:] 
 * <pre>
 * .    // normally we would have a GUI with a menu for this
 * .    long count;
 * .    Locale* locales = NumberFormat::getAvailableLocales(count);
 *
 * .    double myNumber = -1234.56;
 * .    NumberFormat* form;
 *
 * .    // just for fun, we print out a number with the locale number, currency
 * .    // and percent format for each locale we can.
 * .    for (int j = 0; j &lt; 3; ++j) {
 * .        cout &lt;&lt; "FORMAT" &lt;&lt; endl;
 * .        for (int i = 0; i &lt; count; ++i) {
 * .            if (locales[i]->getCountry().length() == 0) {
 * .               // skip language-only
 * .               continue;
 * .            }
 * .            cout &lt;&lt; locales[i]->getDisplayName();
 * .            switch (j) {
 * .            default:
 * .                form = NumberFormat::getInstance(*locales[i]); break;
 * .            case 1:
 * .                form = NumberFormat::getDefaultCurrency(*locales[i]); break;
 * .            case 0:
 * .                form = NumberFormat::getDefaultPercent(*locales[i]); break;
 * .            }
 * .            UnicodeString str;
 * .            ErrorCode status;
 * .            UnicodeString pattern;
 * .            if (form->getDynamicClassID() == DecimalFormat::getStaticClassID())
 * .                 ((DecimalFormat*)form)->toPattern(pattern);
 * .            cout &lt;&lt; ": " &lt;&lt; pattern
 * .                 &lt;&lt; " -> " &lt;&lt; form->format(myNumber, str);
 * .            cout &lt;&lt; " -> " &lt;&lt; form->parse(form->format(myNumber, str), status)
 * .                 &lt;&lt; endl;
 * .        }
 * .    }
 * </pre>
 * [The following shows the structure of the pattern.] 
 * <pre>
 * .    pattern    := subpattern{;subpattern}
 * .    subpattern := {prefix}integer{.fraction}{suffix}
 * .    
 * .    prefix     := '\\u0000'..'\\uFFFD' - specialCharacters
 * .    suffix     := '\\u0000'..'\\uFFFD' - specialCharacters
 * .    integer    := '#'* '0'* '0'
 * .    fraction   := '0'* '#'*
 *    
 *  Notation:
 * .    X*       0 or more instances of X
 * .    (X | Y)  either X or Y.
 * .    X..Y     any character from X up to Y, inclusive.
 * .    S - T    characters in S, except those in T
 * </pre>
 * The first subpattern is for positive numbers. The second (optional)
 * subpattern is used for negative numbers. (In both cases, ',' can
 * occur inside the integer portion--it is just too messy to indicate
 * in BNF.)  For the second subpattern, only the PREFIX and SUFFIX are
 * noted; other attributes are taken only from the first subpattern.
 * <P>
 * Here are the special characters used in the parts of the
 * subpattern, with notes on their usage.
 * <pre>
 * .    Symbol   Meaning
 * .      0      a digit, showing up a zero if it is zero
 * .      #      a digit, supressed if zero
 * .      .      placeholder for decimal separator
 * .      ,      placeholder for grouping separator
 * .      ;      separates postive from negative formats
 * .      -      default negative prefix
 * .      %      divide by 100 and show as percentage
 * .      X      any other characters can be used in the prefix or suffix
 * .      '      used to quote special characters in a prefix or suffix
 * </pre>
 * [Notes] 
 * <P>
 * If there is no explicit negative subpattern, - is prefixed to the
 * positive form. That is, "0.00" alone is equivalent to "0.00;-0.00".
 * <P>
 * Illegal formats, such as "#.#.#" in the same format, will cause a
 * failing ErrorCode to be returned. 
 * <P>
 * The grouping separator is commonly used for thousands, but in some
 * countries for ten-thousands. The interval is a constant number of
 * digits between the grouping characters, such as 100,000,000 or 1,0000,0000.
 * If you supply a pattern with multiple grouping characters, the interval
 * between the last one and the end of the integer is the one that is
 * used. So "#,##,###,####" == "######,####" == "##,####,####".
 * <P>
 * This class only handles localized digits where the 10 digits are
 * contiguous in Unicode, from 0 to 9. Other digits sets (such as
 * superscripts) would need a different subclass.
 */

#ifdef NLS_MAC
#pragma export on
#endif
 
class T_FORMAT_API DecimalFormat: public NumberFormat {
public:
    /**
     * Create a DecimalFormat using the default pattern and symbols
     * for the default locale. This is a convenient way to obtain a
     * DecimalFormat when internationalization is not the main concern.
     * <P>
     * To obtain standard formats for a given locale, use the factory methods
     * on NumberFormat such as getNumberInstance. These factories will
     * return the most appropriate sub-class of NumberFormat for a given
     * locale.
     * @param status    Output param set to success/failure code. If the
     *                  pattern is invalid this will be set to a failure code.
     */
    DecimalFormat(ErrorCode& status);

    /**
     * Create a DecimalFormat from the given pattern and the symbols
     * for the default locale. This is a convenient way to obtain a
     * DecimalFormat when internationalization is not the main concern.
     * <P>
     * To obtain standard formats for a given locale, use the factory methods
     * on NumberFormat such as getNumberInstance. These factories will
     * return the most appropriate sub-class of NumberFormat for a given
     * locale.
     * @param pattern   A non-localized pattern string.
     * @param status    Output param set to success/failure code. If the
     *                  pattern is invalid this will be set to a failure code.
     */
	DecimalFormat(const UnicodeString& pattern,
				  ErrorCode& status);

    /**
     * Create a DecimalFormat from the given pattern and symbols.
     * Use this constructor when you need to completely customize the
     * behavior of the format.
     * <P>
     * To obtain standard formats for a given
     * locale, use the factory methods on NumberFormat such as
     * getInstance or getCurrencyInstance. If you need only minor adjustments
     * to a standard format, you can modify the format returned by
     * a NumberFormat factory method.
     *
     * @param pattern           a non-localized pattern string
     * @param symbolsToAdopt    the set of symbols to be used.  The caller should not
     *                          delete this object after making this call.
     * @param status            Output param set to success/failure code. If the
     *                          pattern is invalid this will be set to a failure code.
     */
    DecimalFormat(	const UnicodeString& pattern,
					DecimalFormatSymbols* symbolsToAdopt,
					ErrorCode& status);

    /**
     * Create a DecimalFormat from the given pattern and symbols.
     * Use this constructor when you need to completely customize the
     * behavior of the format.
     * <P>
     * To obtain standard formats for a given
     * locale, use the factory methods on NumberFormat such as
     * getInstance or getCurrencyInstance. If you need only minor adjustments
     * to a standard format, you can modify the format returned by
     * a NumberFormat factory method.
     *
     * @param pattern           a non-localized pattern string
     * @param symbols   the set of symbols to be used
     * @param status            Output param set to success/failure code. If the
     *                          pattern is invalid this will be set to a failure code.
     */
    DecimalFormat(	const UnicodeString& pattern,
					const DecimalFormatSymbols& symbols,
					ErrorCode& status);

	/**
	 * Copy constructor.
	 */
	DecimalFormat(const DecimalFormat& source);

	/**
	 * Assignment operator.
	 */
	DecimalFormat& operator=(const DecimalFormat& rhs);

	/**
	 * Destructor.
	 */
	virtual ~DecimalFormat();

    /**
     * Clone this Format object polymorphically. The caller owns the
	 * result and should delete it when done.
     */
    virtual Format* clone() const;

    /**
	 * Return true if the given Format objects are semantically equal.
	 * Objects of different subclasses are considered unequal.
     */
    virtual t_bool operator==(const Format& other) const;

   /**
	* Format a double or long number using base-10 representation.
	*
	* @param number		The value to be formatted.
	* @param toAppendTo	The string to append the formatted string to.
	*					This is an output parameter.
	* @param pos		On input: an alignment field, if desired.
	*					On output: the offsets of the alignment field.
	* @return			A reference to 'toAppendTo'.
	*/
    virtual UnicodeString& format(double number,
								  UnicodeString& toAppendTo,
								  FieldPosition& pos) const;
    virtual UnicodeString& format(long number,
								  UnicodeString& toAppendTo,
								  FieldPosition& pos) const;
   virtual UnicodeString& format(const Formattable& obj,
                                  UnicodeString& toAppendTo,
                                  FieldPosition& pos,
                                  ErrorCode& status) const;

   /**
	* Parse the given string using this object's choices. The method
	* does string comparisons to try to find an optimal match.
	* If no object can be parsed, index is unchanged, and NULL is
	* returned.
	*
	* @param text			The text to be parsed.
 	* @param result			Formattable to be set to the parse result.
	*						If parse fails, return contents are undefined.
	* @param parsePosition	The position to start parsing at on input.
	*						On output, moved to after the last successfully
	*						parse character. On parse failure, does not change.
	*/
    virtual void parse(const UnicodeString& text,
					   Formattable& result,
					   ParsePosition& parsePosition) const;

	// Declare here again to get rid of function hiding problems.
	virtual void parse(const UnicodeString& text, 
                       Formattable& result, 
                       ErrorCode& error) const;

    /**
     * Returns the decimal format symbols, which is generally not changed
     * by the programmer or user.
     * @return desired DecimalFormatSymbols
     * @see DecimalFormatSymbols
     */
    virtual const DecimalFormatSymbols* getDecimalFormatSymbols() const;

    /**
     * Sets the decimal format symbols, which is generally not changed
     * by the programmer or user.
     * @param symbolsToAdopt DecimalFormatSymbols to be adopted.
     */
    virtual void adoptDecimalFormatSymbols(DecimalFormatSymbols* symbolsToAdopt);

    /**
     * Sets the decimal format symbols, which is generally not changed
     * by the programmer or user.
     * @param symbols DecimalFormatSymbols.
     */
    virtual void setDecimalFormatSymbols(const DecimalFormatSymbols& symbols);


    /**
     * Get the positive prefix.
     *
     * Examples: +123, $123, sFr123
     */
    UnicodeString& getPositivePrefix(UnicodeString& result) const;

    /**
     * Set the positive prefix.
     *
     * Examples: +123, $123, sFr123
     */
    virtual void setPositivePrefix(const UnicodeString& newValue);

    /**
     * Get the negative prefix.
     *
     * Examples: -123, ($123) (with negative suffix), sFr-123
     */
    UnicodeString& getNegativePrefix(UnicodeString& result) const;
    /**
     * Set the negative prefix.
     *
     * Examples: -123, ($123) (with negative suffix), sFr-123
     */
    virtual void setNegativePrefix(const UnicodeString& newValue);

    /**
     * Get the positive suffix.
     *
     * Example: 123%
     */
    UnicodeString& getPositiveSuffix(UnicodeString& result) const;

    /**
     * Set the positive suffix.
     *
     * Example: 123%
     */
    virtual void setPositiveSuffix(const UnicodeString& newValue);

    /**
     * Get the negative suffix.
     *
     * Examples: -123%, ($123) (with positive suffixes)
     */
    UnicodeString& getNegativeSuffix(UnicodeString& result) const;

    /**
     * Set the positive suffix.
     *
     * Examples: 123%
     */
    virtual void setNegativeSuffix(const UnicodeString& newValue);

    /**
     * Get the multiplier for use in percent, permill, etc.
     * For a percentage, set the suffixes to have "%" and the multiplier to be 100.
     * (For Arabic, use arabic percent symbol).
     * For a permill, set the suffixes to have "\u2031" and the multiplier to be 1000.
     *
     * Examples: with 100, 1.23 -> "123", and "123" -> 1.23
     */
    t_int32 getMultiplier() const;

    /**
     * Set the multiplier for use in percent, permill, etc.
     * For a percentage, set the suffixes to have "%" and the multiplier to be 100.
     * (For Arabic, use arabic percent symbol).
     * For a permill, set the suffixes to have "\u2031" and the multiplier to be 1000.
     *
     * Examples: with 100, 1.23 -> "123", and "123" -> 1.23
     */
    virtual void setMultiplier(t_int32 newValue);

    /**
     * Return the grouping size. Grouping size is the number of digits between
     * grouping separators in the integer portion of a number.  For example,
     * in the number "123,456.78", the grouping size is 3.
     * @see setGroupingSize
     * @see NumberFormat::isGroupingUsed
     * @see DecimalFormatSymbols::getGroupingSeparator
     */
    int getGroupingSize() const;

    /**
     * Set the grouping size. Grouping size is the number of digits between
     * grouping separators in the integer portion of a number.  For example,
     * in the number "123,456.78", the grouping size is 3.
     * @see getGroupingSize
     * @see NumberFormat::setGroupingUsed
     * @see DecimalFormatSymbols::setGroupingSeparator
     */
    virtual void setGroupingSize(int newValue);

    /**
     * Allows you to get the behavior of the decimal separator with integers.
     * (The decimal separator will always appear with decimals.)
     *
     * Example: Decimal ON: 12345 -> 12345.; OFF: 12345 -> 12345
     */
    t_bool isDecimalSeparatorAlwaysShown() const;

    /**
     * Allows you to set the behavior of the decimal separator with integers.
     * (The decimal separator will always appear with decimals.)
     *
     * Example: Decimal ON: 12345 -> 12345.; OFF: 12345 -> 12345
     */
    virtual void setDecimalSeparatorAlwaysShown(t_bool newValue);

    /**
     * Synthesizes a pattern string that represents the current state
     * of this Format object.
     * @see applyPattern
     */
    virtual UnicodeString& toPattern(UnicodeString& result) const;

    /**
     * Synthesizes a localized pattern string that represents the current
     * state of this Format object.
     *
     * @see applyPattern
     */
    virtual UnicodeString& toLocalizedPattern(UnicodeString& result) const;
 
    /**
     * Apply the given pattern to this Format object.  A pattern is a
     * short-hand specification for the various formatting properties.
     * These properties can also be changed individually through the
     * various setter methods.
     * <P>
     * There is no limit to integer digits are set
     * by this routine, since that is the typical end-user desire;
     * use setMaximumInteger if you want to set a real value.
     * For negative numbers, use a second pattern, separated by a semicolon
     * <pre>
     * .      Example "#,#00.0#" -> 1,234.56
     * </pre>
     * This means a minimum of 2 integer digits, 1 fraction digit, and
     * a maximum of 2 fraction digits.
     * <pre>
     * .      Example: "#,#00.0#;(#,#00.0#)" for negatives in parantheses.
     * </pre>
     * In negative patterns, the minimum and maximum counts are ignored;
     * these are presumed to be set in the positive pattern.
     *
     * @param pattern   The pattern to be applied.
     * @param status    Output param set to success/failure code on
     *                  exit. If the pattern is invalid, this will be
     *                  set to a failure result.
     */
    virtual void applyPattern(const UnicodeString& pattern,
							  ErrorCode& status);

    /**
     * Apply the given pattern to this Format object.  The pattern
     * is assumed to be in a localized notation. A pattern is a
     * short-hand specification for the various formatting properties.
     * These properties can also be changed individually through the
     * various setter methods.
     * <P>
     * There is no limit to integer digits are set
     * by this routine, since that is the typical end-user desire;
     * use setMaximumInteger if you want to set a real value.
     * For negative numbers, use a second pattern, separated by a semicolon
     * <pre>
     * .      Example "#,#00.0#" -> 1,234.56
     * </pre>
     * This means a minimum of 2 integer digits, 1 fraction digit, and
     * a maximum of 2 fraction digits.
     *
     * Example: "#,#00.0#;(#,#00.0#)" for negatives in parantheses.
     *
     * In negative patterns, the minimum and maximum counts are ignored;
     * these are presumed to be set in the positive pattern.
     *
     * @param pattern   The localized pattern to be applied.
     * @param status    Output param set to success/failure code on
     *                  exit. If the pattern is invalid, this will be
     *                  set to a failure result.
     */
     virtual void applyLocalizedPattern(const UnicodeString& pattern,
									   ErrorCode& status);

	/**
	 * The resource tags we use to retrieve decimal format data from
	 * locale resource bundles.
	 */
	static const UnicodeString kNumberPatterns;

public:

    /**
     * Return the class ID for this class.  This is useful only for
     * comparing to a return value from getDynamicClassID().  For example:
     * <pre>
     * .      Base* polymorphic_pointer = createPolymorphicObject();
     * .      if (polymorphic_pointer->getDynamicClassID() ==
     * .          Derived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     */
	static ClassID getStaticClassID() { return (ClassID)&fgClassID; }

	/**
	 * Returns a unique class ID POLYMORPHICALLY.  Pure virtual override.
	 * This method is to implement a simple version of RTTI, since not all
	 * C++ compilers support genuine RTTI.  Polymorphic operator==() and
	 * clone() methods call this method.
	 *
	 * @return			The class ID for this object. All objects of a
	 *					given class have the same class ID.  Objects of
	 *					other classes have different class IDs.
	 */
	virtual ClassID getDynamicClassID() const { return getStaticClassID(); }

private:
	static char fgClassID;

	/**
	 * Do real work of constructing a new DecimalFormat.
	 */
	void construct(ErrorCode&				status,
				   const UnicodeString*		pattern = 0,
				   DecimalFormatSymbols*	symbolsToAdopt = 0,
				   const Locale&			locale = Locale::getDefault());

    /**
     * Does the real work of generating a pattern.
     */
    UnicodeString& toPattern(UnicodeString& result, t_bool localized) const;

    /**
     * Does the real work of applying a pattern.
	 * @param pattern	The pattern to be applied.
	 * @param localized	If true, the pattern is localized; else false.
	 * @param status	Output param set to success/failure code on
	 *					exit. If the pattern is invalid, this will be
	 *					set to a failure result.
     */
    void applyPattern(const UnicodeString& pattern,
							t_bool localized,
							ErrorCode& status);

    /**
     * Do the work of formatting a number, either a double or a long.
     */
    UnicodeString& subformat(UnicodeString& result,
                             FieldPosition& fieldPosition,
                             t_bool         isNegative,
                             t_bool         isInteger) const;

    static const int STATUS_INFINITE;
    static const int STATUS_POSITIVE;
    static const int STATUS_LENGTH;

    /**
     * Parse the given text into a number.  The text is parsed beginning at
     * parsePosition, until an unparseable character is seen.
     * @param text The string to parse.
     * @param parsePosition The position at which to being parsing.  Upon
     * return, the first unparseable character.
     * @param digits The DigitList to set to the parsed value.
     * @param isExponent If true, parse an exponent.  This means no
     * infinite values and integer only.
     * @param status Upon return contains boolean status flags indicating
     * whether the value was infinite and whether it was positive.
     */
    t_bool subparse(const UnicodeString& text, ParsePosition& parsePosition,
                    DigitList& digits, t_bool isExponent,
                    t_bool* status) const;

    /**
     * Constants.
     */
    static const t_int8 kMaxDigit; // The largest digit, in this case 9

    /*transient*/ DigitList* digitList;

    UnicodeString           fPositivePrefix;
    UnicodeString           fPositiveSuffix;
    UnicodeString           fNegativePrefix;
    UnicodeString           fNegativeSuffix;
    t_int32                 fMultiplier;
    int                     fGroupingSize;
    t_bool                  fDecimalSeparatorAlwaysShown;
    /*transient*/ t_bool    isCurrencyFormat;
    DecimalFormatSymbols*   fSymbols;

    t_bool                  useExponentialNotation;
    t_int8                  minExponentDigits;

    // Constants for characters used in programmatic (unlocalized) patterns.
    static const UniChar    PATTERN_ZERO_DIGIT;
    static const UniChar    PATTERN_GROUPING_SEPARATOR;
    static const UniChar    PATTERN_DECIMAL_SEPARATOR;
    static const UniChar    PATTERN_PER_MILLE;
    static const UniChar    PATTERN_PERCENT;
    static const UniChar    PATTERN_DIGIT;
    static const UniChar    PATTERN_SEPARATOR;
    static const UniChar    PATTERN_EXPONENT;

    /**
     * The CURRENCY_SIGN is the standard Unicode symbol for currency.  It
     * is used in patterns and substitued with either the currency symbol,
     * or if it is doubled, with the international currency symbol.  If the
     * CURRENCY_SIGN is seen in a pattern, then the decimal separator is
     * replaced with the monetary decimal separator.
     */
    static const UniChar    CURRENCY_SIGN;
    
    static const UniChar    QUOTE;

};


#ifdef NLS_MAC
#pragma export off
#endif
 
#endif // _DECIMFMT
//eof
