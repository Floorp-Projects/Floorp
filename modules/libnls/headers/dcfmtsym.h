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
* File DCFMTSYM.H
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/18/97    clhuang     Updated per C++ implementation.
*   03/27/97    helena      Updated to pass the simple test after code review.
*   08/26/97    aliu        Added currency/intl currency symbol support.
********************************************************************************
*/

#ifndef _DCFMTSYM
#define _DCFMTSYM
 
#include "ptypes.h"
#include "locid.h"

/**
 * This class represents the set of symbols needed by DecimalFormat
 * to format numbers. DecimalFormat creates for itself an instance of
 * DecimalFormatSymbols from its locale data.  If you need to change any
 * of these symbols, you can get the DecimalFormatSymbols object from
 * your DecimalFormat and modify it.
 * <P>
 * Here are the special characters used in the parts of the
 * subpattern, with notes on their usage.
 * <pre>
 * .       Symbol   Meaning
 * .         0      a digit
 * .         #      a digit, zero shows as absent
 * .         .      placeholder for decimal separator
 * .         ,      placeholder for grouping separator.
 * .         ;      separates formats.
 * .         -      default negative prefix.
 * .         %      divide by 100 and show as percentage
 * .         X      any other characters can be used in the prefix or suffix
 * .         '      used to quote special characters in a prefix or suffix.
 *  </pre>
 * [Notes] 
 * <P>
 * If there is no explicit negative subpattern, - is prefixed to the
 * positive form. That is, "0.00" alone is equivalent to "0.00;-0.00".
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
 
class T_FORMAT_API DecimalFormatSymbols {
public:
    /**
     * Create a DecimalFormatSymbols object for the given locale.
	 *
	 * @param locale	The locale to get symbols for.
	 * @param status	Input/output parameter, set to success or
	 *					failure code upon return.
     */
    DecimalFormatSymbols(const Locale& locale, ErrorCode& status);

    /**
     * Create a DecimalFormatSymbols object for the default locale.
	 * This constructor will not fail.  If the resource file data is
	 * not available, it will use hard-coded last-resort data and
	 * set status to USING_FALLBACK_ERROR.
	 *
	 * @param status	Input/output parameter, set to success or
	 *					failure code upon return.
     */
    DecimalFormatSymbols( ErrorCode& status);

	/**
	 * Copy constructor.
	 */
	DecimalFormatSymbols(const DecimalFormatSymbols&);

	/**
	 * Assignment operator.
	 */
	DecimalFormatSymbols& operator=(const DecimalFormatSymbols&);

	/**
	 * Destructor.
	 */
	~DecimalFormatSymbols();

    /**
     * Return true if another object is semantically equal to this one.
     */
    t_bool operator==(const DecimalFormatSymbols& other) const;

	/**
	 * Return true if another object is semantically unequal to this one.
	 */
	t_bool operator!=(const DecimalFormatSymbols& other) const { return !operator==(other); }

    /**
     * character used for zero. Different for Arabic, etc.
     */
    UniChar getZeroDigit() const;
    void setZeroDigit(UniChar zeroDigit);

    /**
     * character used for thousands separator. Different for French, etc.
     */
    UniChar getGroupingSeparator() const;
    void setGroupingSeparator(UniChar groupingSeparator);

    /**
     * character used for decimal sign. Different for French, etc.
     */
    UniChar getDecimalSeparator() const;
    void setDecimalSeparator(UniChar decimalSeparator);

    /**
     * character used for per mill sign. Different for Arabic, etc.
     */
    UniChar getPerMill() const;
    void setPerMill(UniChar perMill);

    /**
     * character used for percent sign. Different for Arabic, etc.
     */
    UniChar getPercent() const;
    void setPercent(UniChar percent);

    /**
     * character used for a digit in a pattern.
     */
    UniChar getDigit() const;
    void setDigit(UniChar digit);

    /**
     * character used to separate positive and negative subpatterns
     * in a pattern.
     */
    UniChar getPatternSeparator() const;
    void setPatternSeparator(UniChar patternSeparator);

    /**
     * character used to represent infinity. Almost always left
     * unchanged.
     */
    UnicodeString& getInfinity(UnicodeString& result) const;
    void setInfinity(const UnicodeString& infinity);

    /**
     * character used to represent NaN. Almost always left
     * unchanged.
     */
    UnicodeString& getNaN(UnicodeString& result) const;
    void setNaN(const UnicodeString& NaN);

    /**
     * character used to represent minus sign. If no explicit
     * negative format is specified, one is formed by prefixing
     * minusSign to the positive format.
     */
    UniChar getMinusSign() const;
    void setMinusSign(UniChar minusSign);
 
    /**
     * character used to represent exponential. Almost always left
     * unchanged.
     */
    UniChar getExponentialSymbol() const;
    void setExponentialSymbol(UniChar exponential);

    /**
     * Return the string denoting the local currency.
     */
    UnicodeString& getCurrencySymbol(UnicodeString& result) const;
    void setCurrencySymbol(const UnicodeString& currency);

    /**
     * Return the international string denoting the local currency.
     */
    UnicodeString& getInternationalCurrencySymbol(UnicodeString& result) const;
    void setInternationalCurrencySymbol(const UnicodeString& currency);

    /**
     * Return the monetary decimal separator.
     */
    UniChar getMonetaryDecimalSeparator() const;
    void setMonetaryDecimalSeparator(UniChar sep);

private:
    /**
     * Initializes the symbols from the LocaleElements resource bundle.
     * Note: The organization of LocaleElements badly needs to be
     * cleaned up.
     */
    void initialize(const Locale& locale, ErrorCode& success, t_bool useLastResortData = FALSE);


    /**
     * Initialize the symbols from the given array of UnicodeStrings.
     * The array must be of the correct size.
     */
    void initialize(const UnicodeString* numberElements, const UnicodeString* currencyElements);
    
    /**
     * The resource tags we use to retrieve decimal format data from
     * locale resource bundles.
     */
    static const UnicodeString kNumberElements;
    static const UnicodeString kCurrencyElements;

    static const int NUMBER_ELEMENTS_LENGTH;
    static const int CURRENCY_ELEMENTS_LENGTH;
    static const UnicodeString kLastResortNumberElements[];
    static const UnicodeString kLastResortCurrencyElements[];
    static const UniChar kLastResortPerMill[];
    static const UniChar kLastResortInfinity[];
    static const UniChar kLastResortNaN[];
    static const UniChar kLastResortCurrency[];
    static const UniChar kLastResortIntlCurrency[];

    UniChar         fDecimalSeparator;
    UniChar         fGroupingSeparator;
    UniChar         fPatternSeparator;
    UniChar         fPercent;
    UniChar         fZeroDigit;
    UniChar         fDigit;
    UniChar         fMinusSign;
    UnicodeString   currencySymbol;
    UnicodeString   intlCurrencySymbol;
    UniChar         monetarySeparator;
    UniChar         fExponential;

    UniChar         fPerMill;
    UnicodeString   fInfinity;
    UnicodeString   fNaN;
};
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getZeroDigit() const
{
    return fZeroDigit;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setZeroDigit(UniChar zeroDigit)
{
    fZeroDigit = zeroDigit;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getGroupingSeparator() const
{
    return fGroupingSeparator;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setGroupingSeparator(UniChar groupingSeparator)
{
    fGroupingSeparator = groupingSeparator;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getDecimalSeparator() const
{
    return fDecimalSeparator;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setDecimalSeparator(UniChar decimalSeparator)
{
    fDecimalSeparator = decimalSeparator;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getPerMill() const
{
    return fPerMill;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setPerMill(UniChar perMill)
{
    fPerMill = perMill;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getPercent() const
{
    return fPercent;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setPercent(UniChar percent)
{
    fPercent = percent;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getDigit() const
{
    return fDigit;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setDigit(UniChar digit)
{
    fDigit = digit;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getPatternSeparator() const
{
    return fPatternSeparator;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setPatternSeparator(UniChar patternSeparator)
{
    fPatternSeparator = patternSeparator;
}
 
// -------------------------------------
 
inline UnicodeString&
DecimalFormatSymbols::getInfinity(UnicodeString& result) const
{
    result = fInfinity;
	return result;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setInfinity(const UnicodeString& infinity)
{
    fInfinity = infinity;
}
 
// -------------------------------------
 
inline UnicodeString&
DecimalFormatSymbols::getNaN(UnicodeString& result) const
{
    result = fNaN;
	return result;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setNaN(const UnicodeString& NaN)
{
    fNaN = NaN;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getMinusSign() const
{
    return fMinusSign;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setMinusSign(UniChar minusSign)
{
    fMinusSign = minusSign;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getExponentialSymbol() const
{
    return fExponential;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setExponentialSymbol(UniChar exponential)
{
    fExponential = exponential;
}

// -------------------------------------
 
inline UnicodeString&
DecimalFormatSymbols::getCurrencySymbol(UnicodeString& result) const
{
    result = currencySymbol;
    return result;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setCurrencySymbol(const UnicodeString& str)
{
    currencySymbol = str;
}
 
// -------------------------------------
 
inline UnicodeString&
DecimalFormatSymbols::getInternationalCurrencySymbol(UnicodeString& result) const
{
    result = intlCurrencySymbol;
    return result;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setInternationalCurrencySymbol(const UnicodeString& str)
{
    intlCurrencySymbol = str;
}
 
// -------------------------------------
 
inline UniChar
DecimalFormatSymbols::getMonetaryDecimalSeparator() const
{
    return monetarySeparator;
}
 
// -------------------------------------
 
inline void
DecimalFormatSymbols::setMonetaryDecimalSeparator(UniChar sep)
{
    monetarySeparator = sep;
}
  
#endif // _DCFMTSYM
//eof
