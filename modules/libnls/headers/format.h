/*
********************************************************************************
*																			   *
* COPYRIGHT:																   *
*	(C) Copyright Taligent, Inc.,  1997										   *
*	(C) Copyright International Business Machines Corporation,	1997		   *
*	Licensed Material - Program-Property of IBM - All Rights Reserved.		   *
*	US Government Users Restricted Rights - Use, duplication, or disclosure	   *
*	restricted by GSA ADP Schedule Contract with IBM Corp.					   *
*																			   *
********************************************************************************
*
* File FORMAT.H
*
* Modification History:
*
*	Date		Name		Description
*	02/19/97	aliu		Converted from java.
*	03/17/97	clhuang		Updated per C++ implementation.
*	03/27/97	helena		Updated to pass the simple test after code review.
********************************************************************************
*/
// *****************************************************************************
// This file was generated from the java source file Format.java
// *****************************************************************************
 
#ifndef _FORMAT
#define _FORMAT
 
#include "ptypes.h"
#include "unistring.h"
#include "fmtable.h"

#include "fieldpos.h"
#include "parsepos.h"

/** 
 * Base class for all formats.  This is an abstract base class which
 * specifies the protocol for classes which convert other objects or
 * values, such as numeric values and dates, and their string
 * representations.  In some cases these representations may be
 * localized or contain localized characters or strings.  For example,
 * a numeric formatter such as DecimalFormat may convert a numeric
 * value such as 12345 to the string "$12,345".  It may also parse
 * the string back into a numeric value.  A date and time formatter
 * like SimpleDateFormat may represent a specific date, encoded
 * numerically, as a string such as "Wednesday, February 26, 1997 AD".
 * <P>
 * Many of the concrete subclasses of Format employ the notion of
 * a pattern.  A pattern is a string representation of the rules which
 * govern the interconversion between values and strings.  For example,
 * a DecimalFormat object may be associated with the pattern
 * "$#,##0.00;($#,##0.00)", which is a common US English format for
 * currency values, yielding strings such as "$1,234.45" for 1234.45,
 * and "($987.65)" for 987.6543.  The specific syntax of a pattern
 * is defined by each subclass.
 * <P>
 * Even though many subclasses use patterns, the notion of a pattern
 * is not inherent to Format classes in general, and is not part of
 * the explicit base class protocol.
 * <P>
 * Two complex formatting classes bear mentioning.  These are
 * MessageFormat and ChoiceFormat.  ChoiceFormat is a subclass of
 * NumberFormat which allows the user to format different number ranges
 * as strings.  For instance, 0 may be represented as "no files", 1 as
 * "one file", and any number greater than 1 as "many files".
 * MessageFormat is a formatter which utilizes other Format objects to
 * format a string containing with multiple values.  For instance,
 * A MessageFormat object might produce the string "There are no files
 * on the disk MyDisk on February 27, 1997." given the arguments 0,
 * "MyDisk", and the date value of 2/27/97.  See the ChoiceFormat
 * and MessageFormat headers for further information.
 * <P>
 * If formatting is unsuccessful, a failing ErrorCode is returned when
 * the Format cannot format the type of object, otherwise if there is
 * something illformed about the the Unicode replacement character
 * 0xFFFD is returned.
 * <P>
 * If there is no match when parsing, a parse failure ErrorCode is
 * retured for methods which take no ParsePosition.  For the method
 * that takes a ParsePosition, the index parameter is left unchanged.
 * <P>
 * [Subclassing.] All base classes that provide static functions that
 * create objects for Locales must implement the following static:
 * <pre>
 * .      public static const Locale* getAvailableLocales(long&)
 * </pre>
 */
#ifdef NLS_MAC
#pragma export on
#endif

class T_FORMAT_API Format {
public:

    virtual ~Format();

    /**
     * Return true if the given Format objects are semantically equal.
     * Objects of different subclasses are considered unequal.
     */
    virtual t_bool operator==(const Format& other) const = 0;

    /**
     * Return true if the given Format objects are not semantically
     * equal.
     */
    t_bool operator!=(const Format& other) const { return !operator==(other); }

    /**
     * Clone this object polymorphically.  The caller is responsible
     * for deleting the result when done.
     */
    virtual Format* clone() const = 0;

    /**
     * Formats an object to produce a string.
     *
     * @param obj       The object to format.
     * @param result    Output parameter which will be filled in with the
     *                  formatted string.
     * @param status    Output parameter filled in with success or failure status.
     * @return          Reference to 'result' parameter.
     */
    UnicodeString& format(const Formattable& obj,
                          UnicodeString& result,
                          ErrorCode& status) const;

    /**
     * Format an object to produce a string.  This is a pure virtual method which
     * subclasses must implement. This method allows polymorphic formatting
     * of Formattable objects. If a subclass of Format receives a Formattable
     * object type it doesn't handle (e.g., if a numeric Formattable is passed
     * to a DateFormat object) then it returns a failing ErrorCode.
     *
     * @param obj           The object to format.
     * @param toAppendTo    Where the text is to be appended.
     * @param pos           On input: an alignment field, if desired.
     *                      On output: the offsets of the alignment field.
     * @param status        Output param filled with success/failure status.
     * @return              The value passed in as toAppendTo (this allows chaining,
     *                      as with UnicodeString::append())
     */
    virtual UnicodeString& format(const Formattable& obj,
                                  UnicodeString& toAppendTo,
                                  FieldPosition& pos,
                                  ErrorCode& status) const = 0;

    /**
     * Parse a string to produce an object.  This is a pure virtual
     * method which subclasses must implement.  This method allows
     * polymorphic parsing of strings into Formattable objects.
     * <P>
     * Before calling, set parse_pos.index to the offset you want to
     * start parsing at in the source.  After calling, parse_pos.index
     * is the end of the text you parsed.  If error occurs, index is
     * unchanged.
     * <P>
     * When parsing, leading whitespace is discarded (with successful
     * parse), while trailing whitespace is left as is.
     * <P>
     * Example:
     * <P>
     * Parsing "_12_xy" (where _ represents a space) for a number,
     * with index == 0 will result in the number 12, with
     * parse_pos.index updated to 3 (just before the second space).
     * Parsing a second time will result in a failing ErrorCode since
     * "xy" is not a number, and leave index at 3.
     * <P>
     * Subclasses will typically supply specific parse methods that
     * return different types of values. Since methods can't overload
     * on return types, these will typically be named "parse", while
     * this polymorphic method will always be called parseObject.  Any
     * parse method that does not take a parse_pos should set status
     * to an error value when no text in the required format is at the
     * start position.
     *
     * @param source    The string to be parsed into an object.
     * @param result    Formattable to be set to the parse result.
     *                  If parse fails, return contents are undefined.
     * @param parse_pos The position to start parsing at. Upon return
     *                  this param is set to the position after the
     *                  last character successfully parsed. If the
     *                  source is not parsed successfully, this param
     *                  will remain unchanged.
     */
    virtual void parseObject(const UnicodeString& source,
                             Formattable& result,
                             ParsePosition& parse_pos) const = 0;

    /**
     * Parses a string to produce an object. This is a convenience method
     * which calls the pure virtual parseObject() method, and returns a
     * failure ErrorCode if the ParsePosition indicates failure.
     *
     * @param source    The string to be parsed into an object.
     * @param result    Formattable to be set to the parse result.
     *                  If parse fails, return contents are undefined.
     * @param status    Output param to be filled with success/failure
     *                  result code.
     */
    void parseObject(const UnicodeString& source,
                     Formattable& result,
                     ErrorCode& status) const;

    /**
     * Returns a unique class ID POLYMORPHICALLY.  Pure virtual method.
     * This method is to implement a simple version of RTTI, since not all
     * C++ compilers support genuine RTTI.  Polymorphic operator==() and
     * clone() methods call this method.
     * <P>
     * Concrete subclasses of Format must implement getDynamicClassID()
     * and also a static method and data member:
     *
     *      static ClassID getStaticClassID() { return (ClassID)&fgClassID; }
     *      static char fgClassID;
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     */
    virtual ClassID getDynamicClassID() const = 0;

protected:
	/**
	 * Default constructor for subclass use only.  Does nothing.
	 */
	Format();

	Format(const Format&); // Does nothing; for subclasses only

	Format& operator=(const Format&); // Does nothing; for subclasses
};

#ifdef NLS_MAC
#pragma export off
#endif


#endif // _FORMAT
//eof

