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
* File CHOICFMT.H
*
* Modification History:
*
*	Date		Name		Description
*	02/19/97	aliu		Converted from java.
*	03/20/97	helena		Finished first cut of implementation and got rid 
*							of nextDouble/previousDouble and replaced with
*							boolean array.
*	4/10/97		aliu		Clean up.  Modified to work on AIX.
*	8/6/97		nos			Removed overloaded constructor, member var 'buffer'.
********************************************************************************
*/ 
#ifndef _CHOICFMT
#define _CHOICFMT
 
#ifndef _PTYPE
#include "ptypes.h"
#endif

#ifndef _UNISTRING
#include "unistring.h"
#endif

#ifndef _NUMFMT
#include "numfmt.h"
#endif

#ifndef _FIELDPOS
#include "fieldpos.h"
#endif

#ifndef _FORMAT
#include "format.h"
#endif

/**
 * A ChoiceFormat allows you to attach a format to a range of numbers.
 * It is generally used in a MessageFormat for doing things like plurals.
 * The choice is specified with an ascending list of doubles, where each item
 * specifies a half-open interval up to the next item:
 * <pre>
 * .    X matches j if and only if limit[j] &lt;= X &lt; limit[j+1]
 * </pre>
 * If there is no match, then either the first or last index is used, depending
 * on whether the number is too low or too high.  The length of the array of
 * formats must be the same as the length of the array of limits.
 * For example,
 * <pre>
 * .     {1,2,3,4,5,6,7},
 * .          {"Sun","Mon","Tue","Wed","Thur","Fri","Sat"}
 * .     {0, 1, ChoiceFormat.nextDouble(1)},
 * .          {"no files", "one file", "many files"}
 * </pre>
 * (nextDouble can be used to get the next higher double, to make the half-open
 * interval.)
 * <P>
 * Here is a simple example that shows formatting and parsing:
 * <pre>
 * .       double limits[] = {1,2,3,4,5,6,7};
 * .       UnicodeString monthNames[] = {"Sun","Mon","Tue","Wed","Thur","Fri","Sat"};
 * .       ChoiceFormat* form = new ChoiceFormat(limits, monthNames, 7);
 * .       ParsePosition* parse_pos = new ParsePosition();
 * .       // TODO Fix this ParsePosition stuff...
 * .       UnicodeString str;
 * .       for (double i = 0.0; i &lt;= 8.0; ++i) {
 * .           status.index = 0;
 * .           cout &lt;&lt; i &lt;&lt; " -> " &lt;&lt; form->format(i, str) &lt;&lt; " -> "
 * .                &lt;&lt; form->parse(form->format(i, str),parse_pos));
 * .       }
 * </pre>
 * Here is a more complex example, with a pattern format.
 * <pre>
 * .       double filelimits[] = {0,1,2};
 * .       String filepart[] = {"are no files","is one file","are {2} files"};
 * .       ChoiceFormat* fileform = new ChoiceFormat(filelimits, filepart, 3);
 * .       Format testFormats[] = {fileform, null, &NumberFormat::getInstance()};
 * .       MessageFormat* pattform
 * .           = new MessageFormat("There {0} on {1}",testFormats);
 * .       Formattable testArgs[] = {0L, "ADisk", 0L};
 * .       UnicodeString str;
 * .       for (int i = 0; i &lt; 4; ++i) {
 * .           testArgs[0] = Formattable((long)i);
 * .           testArgs[2] = testArgs[0];
 * .           cout &lt;&lt; pattform->toPattern() &lt;&lt; " -> ";
 * .           cout &lt;&lt; pattform->format(testArgs, str);
 * .       }
 * </pre>
 * ChoiceFormat objects may be converted to and from patterns.  The
 * syntax of these patterns is [TODO fill in this section with detail].
 * Here is an example of a ChoiceFormat pattern:
 * <P>
 * You can either do this programmatically, as in the above example,
 * or by using a pattern (see ChoiceFormat for more information) as in:
 * <pre>
 * .       "0#are no files|1#is one file|1&lt;are many files"
 * </pre>
 * Here the notation is:
 * <pre>
 * .       &lt;number> "#"  Specifies a limit value.
 * .       &lt;number> "<"  Specifies a limit of nextDouble(&lt;number>).
 * .       &lt;number> ">"  Specifies a limit of previousDouble(&lt;number>).
 * </pre>
 * Each limit value is followed by a string, which is terminated by
 * a vertical bar character ("|"), except for the last string, which
 * is terminated by the end of the string.
 */
#ifdef NLS_MAC
#pragma export on
#endif

class T_FORMAT_API ChoiceFormat: public NumberFormat {
public:
	/**
	 * Construct a new ChoiceFormat with the limits and the corresponding formats
	 * based on the pattern.
	 *
	 * @param pattern	Pattern used to construct object.
	 * @param status	Output param to receive success code.  If the
	 *					pattern cannot be parsed, set to failure code.
	 */
	ChoiceFormat(const UnicodeString& newPattern,
				 ErrorCode& status);

	/**
	 * Construct a new ChoiceFormat with the given limits and formats.	Copy
	 * the limits and formats instead of adopting them.
	 *
	 * @param limits		Array of limit values.
	 * @param formats	Array of formats.
	 * @param count				Size of 'limits' and 'formats' arrays.
	 */
	ChoiceFormat(const double* limits,
				 const UnicodeString* formats,
				 t_int32 count);
	/**
	 * Copy constructor.
	 */
	ChoiceFormat(const ChoiceFormat&);

	/**
	 * Assignment operator.
	 */
	const ChoiceFormat& operator=(const ChoiceFormat&);

	/**
	 * Destructor.
	 */
	virtual ~ChoiceFormat();

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
	 * Return true if the given Format objects are not semantically equal.
	 * Objects of different subclasses are considered unequal.
	 */
	virtual t_bool operator!=(const Format& other) const;

	/**
	 * Sets the pattern.
	 * @param pattern	The pattern to be applied.
	 * @param status	Output param set to success/failure code on
	 *					exit. If the pattern is invalid, this will be
	 *					set to a failure result.
	 */
	virtual void applyPattern(const UnicodeString& pattern,
							  ErrorCode& status);

	/**
	 * Gets the pattern.
	 */
	virtual UnicodeString& toPattern(UnicodeString &pattern) const;

    /**
     * Set the choices to be used in formatting.  The arrays are adopted and
     * should not be deleted by the caller.
     *
     * @param limitsToAdopt     Contains the top value that you want
     *                          parsed with that format,and should be in
     *                          ascending sorted order. When formatting X,
     *                          the choice will be the i, where limit[i]
     *                          &lt;= X &lt; limit[i+1].
     * @param formatsToAdopt    The format strings you want to use for each limit.
     * @param count             The size of the above arrays.
     */
	virtual void adoptChoices(double* limitsToAdopt,
							  UnicodeString* formatsToAdopt,
							  t_int32 count);	

    /**
     * Set the choices to be used in formatting.
     *
     * @param limitsToCopy      Contains the top value that you want
     *                          parsed with that format,and should be in
     *                          ascending sorted order. When formatting X,
     *                          the choice will be the i, where limit[i]
     *                          &lt;= X &lt; limit[i+1].
     * @param formatsToCopy     The format strings you want to use for each limit.
     * @param count             The size of the above arrays.
     */
	virtual void setChoices(const double* limitsToCopy,
							const UnicodeString* formatsToCopy,
							t_int32 count);	
	/**
	 * Get the limits passed in the constructor.
	 * @return the limits.
	 */
	virtual const double* getLimits(t_int32& count) const;

	/**
	 * Get the formats passed in the constructor.
	 * @return the formats.
	 */
	virtual const UnicodeString* getFormats(t_int32& count) const;

   /**
	* Format a double or long number using this object's choices.
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
	virtual	UnicodeString& format(const Formattable* objs,
								  t_int32 cnt,
								  UnicodeString& toAppendTo,
								  FieldPosition& pos,
								  ErrorCode& success) const;
	virtual UnicodeString& format(const Formattable& obj,
								  UnicodeString& toAppendTo,
								  FieldPosition& pos, 
                                  ErrorCode& status) const;

   /**
    * Return a long if possible (e.g. within range LONG_MAX,
    * LONG_MAX], and with no decimals), otherwise a double.  If
    * IntegerOnly is set, will stop at a decimal point (or equivalent;
    * e.g. for rational numbers "1 2/3", will stop after the 1).
    * <P>
    * If no object can be parsed, parsePosition is unchanged, and NULL is
    * returned.
    *
    * @param text           The text to be parsed.
    * @param result         Formattable to be set to the parse result.
    *                       If parse fails, return contents are undefined.
    * @param parsePosition  The position to start parsing at on input.
    *                       On output, moved to after the last successfully
    *                       parse character. On parse failure, does not change.
    * @return               A Formattable object of numeric type.  The caller
    *                       owns this an must delete it.  NULL on failure.
    * @see                  NumberFormat::isParseIntegerOnly
    */
	virtual void parse(const UnicodeString& text,
					   Formattable& result,
					   ParsePosition& parsePosition) const;
	virtual void parse(const UnicodeString& text,
					   Formattable& result,
					   ErrorCode& status) const;
	
public:
	/**
	 * Returns a unique class ID POLYMORPHICALLY.  Pure virtual override.
	 * This method is to implement a simple version of RTTI, since not all
	 * C++ compilers support genuine RTTI.	Polymorphic operator==() and
	 * clone() methods call this method.
	 *
	 * @return			The class ID for this object. All objects of a
	 *					given class have the same class ID.	 Objects of
	 *					other classes have different class IDs.
	 */
	virtual ClassID getDynamicClassID() const;

    /**
     * Return the class ID for this class.  This is useful only for
     * comparing to a return value from getDynamicClassID().  For example:
     * <pre>
     * .       Base* polymorphic_pointer = createPolymorphicObject();
     * .       if (polymorphic_pointer->getDynamicClassID() ==
     * .           Derived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     */
	static ClassID getStaticClassID() { return (ClassID)&fgClassID; }

 
    /*
     * Finds the least double greater than d (if positive == true),
     * or the greatest double less than d (if positive == false).
     * If NaN, returns same value.
     * <P>
     * Does not affect floating-point flags,
     */
	static double nextDouble(double d, t_bool positive);

    /**
     * Finds the least double greater than d.
     * If NaN, returns same value.
     * Used to make half-open intervals.
     * @see ChoiceFormat::previousDouble
     */
	static double nextDouble(double d );

    /**
     * Finds the greatest double less than d.
     * If NaN, returns same value.
     * @see ChoiceFormat::nextDouble
     */
	static double previousDouble(double d );

private:
    /**
     * Converts a string to a double value using a default NumberFormat object
     * which is static (shared by all ChoiceFormat instances).
     * @param string the string to be converted with.
     * @param status error code.
     * @return the converted double number.
     */
    static double stod(const UnicodeString& string, ErrorCode& status);
    /**
     * Converts a double value to a string using a default NumberFormat object
     * which is static (shared by all ChoiceFormat instances).
     * @param value the double number to be converted with.
     * @param string the result string.
     * @param status error code.
     * @return the converted string.
     */
    static UnicodeString& dtos(double value, UnicodeString& string, ErrorCode& status);

	static NumberFormat* fgNumberFormat;
	static char fgClassID;

	double*			choiceLimits;
	UnicodeString*	choiceFormats;
	t_bool*			doubleFlags; // this array will soon disappear
	t_int32			count;
};
#ifdef NLS_MAC
#pragma export off
#endif

 
inline ClassID 
ChoiceFormat::getDynamicClassID() const
{ 
	return ChoiceFormat::getStaticClassID(); 
}

inline t_bool
ChoiceFormat::operator!= (const Format& that) const
{
	return !(*this == that);
}

inline double ChoiceFormat::nextDouble( double d )
{
	return ChoiceFormat::nextDouble( d, TRUE );
}
	
inline double ChoiceFormat::previousDouble( double d )
{
	return ChoiceFormat::nextDouble( d, FALSE );
}

#endif // _CHOICFMT
//eof
