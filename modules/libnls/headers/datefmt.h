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
* File DATEFMT.H
*
* Modification History:
*
*	Date		Name		Description
*	02/19/97	aliu		Converted from java.
*	04/01/97	aliu		Added support for centuries.
********************************************************************************
*/

#ifndef _DATEFMT
#define _DATEFMT
 
#include "ptypes.h"
#include "calendar.h"
#include "numfmt.h"
#include "format.h"
#include "locid.h"
class TimeZone;

/**
 * DateFormat is an abstract class for a family of classes that convert dates and
 * times from their internal representations to textual form and back again in a
 * language-independent manner. Converting from the internal representation (milliseconds
 * since midnight, January 1, 1970) to text is known as "formatting," and converting
 * from text to millis is known as "parsing."  We currently define only one concrete
 * subclass of DateFormat: SimpleDateFormat, which can handle pretty much all normal
 * date formatting and parsing actions.
 * <P>
 * DateFormat helps you to format and parse dates for any locale. Your code can
 * be completely independent of the locale conventions for months, days of the
 * week, or even the calendar format: lunar vs. solar.
 * <P>
 * To format a date for the current Locale, use one of the static factory
 * methods:
 * <pre>
 * .       UnicodeString myString;
 * .       DateFormat* df(DateFormat::createDateInstance());
 * .       df->format(myDate, myString);
 * .       delete df;
 * </pre>
 * If you are formatting multiple numbers, it is more efficient to get the
 * format and use it multiple times so that the system doesn't have to fetch the
 * information about the local language and country conventions multiple times.
 * <pre>
 * .       Date a[a_length];
 * .       UnicodeString myString;
 * .       DateFormat* df(DateFormat::createDateInstance());
 * .       for (int i = 0; i &lt; a_length; ++i) {
 * .           cout &lt;&lt; df->format(myDate[i], myString) &lt;&lt; "; ";
 * .       }
 * .       delete df;
 * </pre>
 * To format a date for a different Locale, specify it in the call to
 * getDateInstance().
 * <pre>
 * .       DateFormat* df(DateFormat::getDateInstance(Locale.FRANCE));
 * </pre>
 * You can use a DateFormat to parse also.
 * <pre>
 * .       ErrorCode status = ZERO_ERROR;
 * .       Formattable* myDate = df.parse(myString, status);
 * </pre>
 * Use createDateInstance() to produce the normal date format for that country.
 * There are other static factory methods available. Use createTimeInstance()
 * to produce the normal time format for that country. Use createDateTimeInstance()
 * to produce a DateFormat that formats both date and time. You can pass in
 * different options to these factory methods to control the length of the
 * result; from SHORT to MEDIUM to LONG to FULL. The exact result depends on the
 * locale, but generally:
 * <ul type=round>
 *   <li>   SHORT is completely numeric, such as 12/13/52 or 3:30pm
 *   <li>   MEDIUM is longer, such as Jan 12, 1952
 *   <li>   LONG is longer, such as January 12, 1952 or 3:30:32pm
 *   <li>   FULL is pretty completely specified, such as
 *          Tuesday, April 12, 1952 AD or 3:30:42pm PST.
 * </ul>
 * You can also set the time zone on the format if you wish. If you want even
 * more control over the format or parsing, (or want to give your users more
 * control), you can try casting the DateFormat you get from the factory methods
 * to a SimpleDateFormat. This will work for the majority of countries; just
 * remember to chck getDynamicClassID() before carrying out the cast.
 * <P>
 * You can also use forms of the parse and format methods with ParsePosition and
 * FieldPosition to allow you to
 * <ul type=round>
 *   <li>   Progressively parse through pieces of a string.
 *   <li>   Align any particular field, or find out where it is for selection
 *          on the screen.
 * </ul>
 */
 
#ifdef NLS_MAC
#pragma export on
#endif
 
class T_FORMAT_API DateFormat : public Format {
public:
    /**
     * The following enum values are used in FieldPosition with date/time formatting.
     * They are also used to index into DateFormatSymbols::fgPatternChars, which
     * is the list of standard internal-representation pattern characters, and
     * the resource bundle localPatternChars data. For this reason, this enum
     * should be treated with care; don't change the order or contents of it
     * unless you really know what you are doing. You'll probably have to change
     * the code in DateFormatSymbols, SimpleDateFormat, and all the locale
     * resource bundle data files.
     */
    enum EField
    {
		ERA_FIELD,			// ERA field alignment.
		YEAR_FIELD,			// YEAR field alignment.
		MONTH_FIELD,		// MONTH field alignment.
		DATE_FIELD,			// DATE field alignment.
		HOUR_OF_DAY1_FIELD,	// One-based HOUR_OF_DAY field alignment.
							// HOUR_OF_DAY1_FIELD is used for the one-based 24-hour clock.
							// For example, 23:59 + 01:00 results in 24:59.
		HOUR_OF_DAY0_FIELD,	// Zero-based HOUR_OF_DAY field alignment.
							// HOUR_OF_DAY0_FIELD is used for the zero-based 24-hour clock.
							// For example, 23:59 + 01:00 results in 00:59.
		MINUTE_FIELD,		// MINUTE field alignment.
		SECOND_FIELD,		// SECOND field alignment.
		MILLISECOND_FIELD,	// MILLISECOND field alignment.
		DAY_OF_WEEK_FIELD,	// DAY_OF_WEEK field alignment.
		DAY_OF_YEAR_FIELD,	// DAY_OF_YEAR field alignment.
		DAY_OF_WEEK_IN_MONTH_FIELD,// DAY_OF_WEEK_IN_MONTH field alignment.
		WEEK_OF_YEAR_FIELD,	// WEEK_OF_YEAR field alignment.
		WEEK_OF_MONTH_FIELD,// WEEK_OF_MONTH field alignment.
		AM_PM_FIELD,		// AM_PM field alignment.
		HOUR1_FIELD,		// One-based HOUR field alignment.
							// HOUR1_FIELD is used for the one-based 12-hour clock.
							// For example, 11:30 PM + 1 hour results in 12:30 AM.
		HOUR0_FIELD,		// Zero-based HOUR field alignment.
							// HOUR0_FIELD is used for the zero-based 12-hour clock.
							// For example, 11:30 PM + 1 hour results in 00:30 AM.
		TIMEZONE_FIELD		// TIMEZONE field alignment.
	};

    /**
     * Constants for various style patterns. These reflect the order of items in
     * the DateTimePatterns resource. There are 4 time patterns, 4 date patterns,
     * and then the date-time pattern. Each block of 4 values in the resource occurs
     * in the order full, long, medium, short.
     */

	enum EStyle
	{
		FULL,
		LONGG,
		MEDIUM,
		SHORT,

		DEFAULT = MEDIUM,
		DATE_OFFSET = 4,
		NONE = -1,
		DATE_TIME = 8
	};
	
	typedef enum EStyle EStyle;
	/**
     * Destructor.
	 */
	virtual ~DateFormat();
 
	/**
     * Equality operator.  Returns true if the two formats have the same behavior.
     */
	virtual t_bool operator==(const Format&) const;

    /**
     * Format an object to produce a string. This method handles Formattable
     * objects with a Date type. If a the Formattable object type is not a Date,
     * then it returns a failing ErrorCode.
     *
     * @param obj           The object to format. Must be a Date.
     * @param toAppendTo    The result of the formatting operation is appended to
     *                      this string.
     * @param pos           On input: an alignment field, if desired.
     *                      On output: the offsets of the alignment field.
     * @param status        Output param filled with success/failure status.
     * @return              The value passed in as toAppendTo (this allows chaining,
     *                      as with UnicodeString::append())
     */
	virtual UnicodeString& format(const Formattable& obj,
								  UnicodeString& toAppendTo,
								  FieldPosition& pos,
								  ErrorCode& status) const;

    /**
     * Formats a Date into a date/time string. This is an abstract method which
     * concrete subclasses must implement.
     * <P>
     * On input, the FieldPosition parameter may have its "field" member filled with
     * an enum value specifying a field.  On output, the FieldPosition will be filled
     * in with the text offsets for that field.  
     * <P> For example, given a time text
     * "1996.07.10 AD at 15:08:56 PDT", if the given fieldPosition.field is
     * DateFormat::YEAR_FIELD, the offsets fieldPosition.beginIndex and
     * statfieldPositionus.getEndIndex will be set to 0 and 4, respectively. 
     * <P> Notice
     * that if the same time field appears more than once in a pattern, the status will
     * be set for the first occurence of that time field. For instance,
     * formatting a Date to the time string "1 PM PDT (Pacific Daylight Time)"
     * using the pattern "h a z (zzzz)" and the alignment field
     * DateFormat::TIMEZONE_FIELD, the offsets fieldPosition.beginIndex and
     * fieldPosition.getEndIndex will be set to 5 and 8, respectively, for the first
     * occurence of the timezone pattern character 'z'.
     *
     * @param date          a Date to be formatted into a date/time string.
     * @param toAppendTo    the result of the formatting operation is appended to
     *                      the end of this string.
     * @param fieldPosition On input: an alignment field, if desired (see examples above)
     *                      On output: the offsets of the alignment field (see examples above)
     * @return              A reference to 'toAppendTo'.
     */
	virtual UnicodeString& format(	Date date,
									UnicodeString& toAppendTo,
									FieldPosition& fieldPosition) const = 0;

	/**
     * Formats a Date into a date/time string. If there is a problem, you won't
     * know, using this method. Use the overloaded format() method which takes a
     * FieldPosition& to detect formatting problems.
	 *
	 * @param date		The Date value to be formatted into a string.
	 * @param result	Output param which will receive the formatted date.
	 * @return			A reference to 'result'.
	 */
	UnicodeString& format(Date date, UnicodeString& result) const;

	/**
     * Parse a date/time string.
	 *
	 * @param text		The string to be parsed into a Date value.
	 * @param status	Output param to be set to success/failure code. If
	 *					'text' cannot be parsed, it will be set to a failure
	 *					code.
	 * @result			The parsed Date value, if successful.
	 */
	virtual Date parse(	const UnicodeString& text,
						ErrorCode& status) const;

    /**
     * Parse a date/time string beginning at the given parse position. For
     * example, a time text "07/10/96 4:5 PM, PDT" will be parsed into a Date
     * that is equivalent to Date(837039928046).
     * <P>
     * By default, parsing is lenient: If the input is not in the form used by
     * this object's format method but can still be parsed as a date, then the
     * parse succeeds. Clients may insist on strict adherence to the format by
     * calling setLenient(false).
     *
     * @see DateFormat::setLenient(boolean)
     *
     * @param text  The date/time string to be parsed
     * @param pos   On input, the position at which to start parsing; on
     *              output, the position at which parsing terminated, or the
     *              start position if the parse failed.
     * @return      A valid Date if the input could be parsed.
     */
	virtual Date parse(	const UnicodeString& text,
						ParsePosition& pos) const = 0;

    /**
     * Parse a string to produce an object. This methods handles parsing of
     * date/time strings into Formattable objects with Date types.
     * <P>
     * Before calling, set parse_pos.index to the offset you want to start
     * parsing at in the source. After calling, parse_pos.index is the end of
     * the text you parsed. If error occurs, index is unchanged.
     * <P>
     * When parsing, leading whitespace is discarded (with a successful parse),
     * while trailing whitespace is left as is.
     * <P>
     * See Format::parseObject() for more.
     *
     * @param source    The string to be parsed into an object.
     * @param result    Formattable to be set to the parse result.
     *                  If parse fails, return contents are undefined.
     * @param parse_pos The position to start parsing at. Upon return
     *                  this param is set to the position after the
     *                  last character successfully parsed. If the
     *                  source is not parsed successfully, this param
     *                  will remain unchanged.
     * @return          A newly created Formattable* object, or NULL
     *                  on failure.  The caller owns this and should
     *                  delete it when done.
     */
	virtual void parseObject(const UnicodeString& source,
							 Formattable& result,
							 ParsePosition& parse_pos) const;

	/**
     * Create a default date/time formatter that uses the SHORT style for both
     * the date and the time.
	 *
	 * @return A date/time formatter which the caller owns.
	 */
	static DateFormat* createInstance();

	/**
     * Creates a time formatter with the given formatting style for the given
     * locale.
	 * 
	 * @param style		The given formatting style. For example,
	 *					SHORT for "h:mm a" in the US locale.
	 * @param aLocale	The given locale.
	 * @return			A time formatter which the caller owns.
	 */
	static DateFormat* createTimeInstance(EStyle style = DEFAULT,
										  const Locale& aLocale = Locale::getDefault());

	/**
     * Creates a date formatter with the given formatting style for the given
     * const locale.
	 * 
	 * @param style		The given formatting style. For example,
	 *					SHORT for "M/d/yy" in the US locale.
	 * @param aLocale	The given locale.
	 * @return			A date formatter which the caller owns.
	 */
	static DateFormat* createDateInstance(EStyle style = DEFAULT,
										  const Locale& aLocale = Locale::getDefault());

    /**
     * Creates a date/time formatter with the given formatting styles for the
     * given locale.
     * 
     * @param dateStyle The given formatting style for the date portion of the result.
     *                  For example, SHORT for "M/d/yy" in the US locale.
     * @param timeStyle The given formatting style for the time portion of the result.
     *                  For example, SHORT for "h:mm a" in the US locale.
     * @param aLocale   The given locale.
     * @return          A date/time formatter which the caller owns.
     */
	static DateFormat* createDateTimeInstance(EStyle dateStyle = DEFAULT,
											  EStyle timeStyle = DEFAULT,
											  const Locale& aLocale = Locale::getDefault());

    /**
     * Gets the set of locales for which DateFormats are installed.
     * @param count Filled in with the number of locales in the list that is returned.
     * @return the set of locales for which DateFormats are installed.  The caller
     *  does NOT own this list and must not delete it.
     */
	static const Locale* getAvailableLocales(t_int32& count);
  
    /**
     * Returns true if the formatter is set for lenient parsing.
     */
    virtual t_bool isLenient() const;

    /**
     * Specify whether or not date/time parsing is to be lenient. With lenient
     * parsing, the parser may use heuristics to interpret inputs that do not
     * precisely match this object's format. With strict parsing, inputs must
     * match this object's format.
     * @see Calendar::setLenient
     */
	virtual void setLenient(t_bool lenient);
	
	/**
     * Gets the calendar associated with this date/time formatter.
	 * @return the calendar associated with this date/time formatter.
	 */
	virtual const Calendar* getCalendar() const;
	
	/**
     * Set the calendar to be used by this date format. Initially, the default
     * calendar for the specified or default locale is used.  The caller should
	 * not delete the Calendar object after it is adopted by this call.
	 */
	virtual void adoptCalendar(Calendar* calendarToAdopt);

	/**
     * Set the calendar to be used by this date format. Initially, the default
     * calendar for the specified or default locale is used.
	 */
    virtual void setCalendar(const Calendar& newCalendar);

   
    /**
     * Gets the number formatter which this date/time formatter uses to format
     * and parse the numeric portions of the pattern.
     * @return the number formatter which this date/time formatter uses.
     */
	virtual const NumberFormat* getNumberFormat() const;
	
    /**
     * Allows you to set the number formatter.  The caller should
     * not delete the NumberFormat object after it is adopted by this call.
     * @param formatToAdopt     NumberFormat object to be adopted.
     */
	virtual void adoptNumberFormat(NumberFormat* formatToAdopt);

	/**
     * Allows you to set the number formatter.
	 * @param formatToAdopt		NumberFormat object to be adopted.
	 */
	virtual void setNumberFormat(const NumberFormat& newNumberFormat);

    /**
     * Returns a reference to the TimeZone used by this DateFormat's calendar.
     * @return the time zone associated with the calendar of DateFormat.
     */
    virtual const TimeZone& getTimeZone() const;
    
    /**
     * Sets the time zone for the calendar of this DateFormat object. The caller
     * no longer owns the TimeZone object and should not delete it after this call.
     * @param zone the new time zone.
     */
    virtual void adoptTimeZone(TimeZone* zoneToAdopt);

    /**
     * Sets the time zone for the calendar of this DateFormat object.
     * @param zone the new time zone.
     */
    virtual void setTimeZone(const TimeZone& zone);

    
protected:
    /**
     * Default constructor.  Creates a DateFormat with no Calendar or NumberFormat
     * associated with it.  This constructor depends on the subclasses to fill in
     * the calendar and numberFormat fields.
     */
    DateFormat();

    /**
     * Copy constructor.
     */
    DateFormat(const DateFormat&);

    /**
     * Default assignment operator.
     */
    DateFormat& operator=(const DateFormat&);

    /**
     * The calendar that DateFormat uses to produce the time field values needed
     * to implement date/time formatting. Subclasses should generally initialize
     * this to the default calendar for the locale associated with this DateFormat.
     */
    Calendar* fCalendar;

    /**
     * The number formatter that DateFormat uses to format numbers in dates and
     * times. Subclasses should generally initialize this to the default number
     * format for the locale associated with this DateFormat.
     */
    NumberFormat* fNumberFormat;

private:
    /**
     * Gets the date/time formatter with the given formatting styles for the
     * given locale.
     * @param dateStyle the given date formatting style.
     * @param timeStyle the given time formatting style.
     * @param inLocale the given locale.
     * @return a date/time formatter, or 0 on failure.
     */
    static DateFormat* create(EStyle timeStyle, EStyle dateStyle, const Locale&);

    /**
     * fgLocales and fgLocalesCount cache the available locales array that is returned
     * by getAvailableLocales().
     */
    static t_int32          fgLocalesCount;

    /**
     * fgLocales and fgLocalesCount cache the available locales array that is returned
     * by getAvailableLocales().
     */
    static const Locale*    fgLocales;
};

#ifdef NLS_MAC
#pragma export off
#endif


#endif // _DATEFMT
//eof
