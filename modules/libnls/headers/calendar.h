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
* File CALENDAR.H
*
* Modification History:
*
*	Date		Name		Description
*	04/22/97	aliu		Expanded and corrected comments and other header
*							contents.
*	05/01/97	aliu		Made equals(), before(), after() arguments const.
*	05/20/97	aliu		Replaced fAreFieldsSet with fAreFieldsInSync and
*							fAreAllFieldsSet.
********************************************************************************
*/

#ifndef _CALENDAR
#define _CALENDAR

#include "timezone.h"
#include "locid.h"


/**
 * Calendar is an abstract base class for converting between a Date and a set of integer
 * fields such as YEAR, MONTH, DATE, HOUR and so on. A Date is a signed numeric value
 * representing the number of milliseconds since midnight, January 1, 1970, GMT.
 * <P>
 * Calendar contains two kinds of time representations: current "time" in milliseconds
 * (an instance of Date), and a set of time "fields" representing the current time.
 * Calling setTime() method only sets the current time without recomputing the time fields
 * until the next get() method is called. Similarly, calling one of the set() methods only
 * sets time fields without recomputing the current time value until the next getTime()
 * method is called.
 * <P>
 * Subclasses of Calendar interpret a Date according to the rules of a specific calendar
 * system. The main subclass of Calendar is GregorianCalendar. Future subclasses could
 * represent the various types of lunar calendars in use in many parts of the world.
 * (Subclasses of Calendar aren't free to change the meaning of Date, however; a particular
 * Date value should represent the same instant in time for all calendars, no matter what
 * they call it.  For example, a Date value of 0 is midnight, January 1, 1970 according
 * to the Gregorian calendar, but midnight, the 23rd of Tevet, 5730, according to the
 * Hebrew calendar and midnight, the 23rd of Shawwal, 1389, according to the Islamic
 * calendar.  All of these are different names for the same instant in time.)
 * <P>
 * A Calendar object can produce all the time field values needed to implement the
 * date/time formatting for a particular language and calendar style (e.g.
 * Japanese-Gregorian, Japanese-Traditional).
 * <P>
 * When computing a Date from time fields, two special circumstances may arise: there
 * might be insufficient information to compute the Date (such as only year and month
 * but no day in the month), or there may be inconsistent information (such as "Tuesday,
 * July 15, 1996" -- July 15, 1996 is actually a Monday).
 * <P>
 * Inconsistent information. The calendar will give preference to the combinations of
 * fields in the following order (and disregard other inconsistent information).
 * <pre>
 * .   month + day-of-month
 * .   month + week-of-month + day-of-week
 * .   month + day-of-week-of-month + day-of-week
 * .   day-of-year
 * .   day-of-week + week-of-year
 * .   hour-of-day
 * .   ampm + hour-of-ampm
 * </pre>
 * Note: for some non-Gregorian calendars, different fields may be necessary for
 * complete disambiguation. For example, a full specification of the historial Arabic
 * astronomical calendar requires year, month, day-of-month AND day-of-week in some
 * cases.
 * <P>
 * Note: There are certain possible ambiguities in interpretation of certain singular
 * times, which are resolved in the following ways:
 * <P>
 * a. 24:00:00 "belongs" to the following day. That is, 23:59 on Dec 31, 1969 &lt; 24:00 on
 * Jan 1, 1970 &lt; 24:01:00 on Jan 1, 1970
 * <P>
 * b. Although historically not precise, midnight also belongs to "am", and noon belongs
 * to "pm", so on the same day, 12:00 am (midnight) &lt; 12:01 am, and 12:00 pm (noon) &lt;
 * 12:01 pm
 * <P>
 * The date or time format strings are not part of the definition of a calendar, as
 * those must be modifiable or overridable by the user at runtime.
 * <P>
 * Calendar provides an API for field "rolling", where fields can be incremented or
 * decremented, but wrap around. For example, rolling the month up in the date September
 * 12, 1996 results in October 12, 1996. After rolling a field, other fields may change
 * to maintain a legal date value. This change will be done so as to minimize the time
 * adjustment. For example, rolling the month up on January 31, 1996 results in February
 * 29, 1996.
 * <P>
 * Calendar also provides a date arithmetic function for adding the specified (signed)
 * amount of time to a particular time field. For example, subtracting 5 days from the
 * date "September 12, 1996" results in "September 7, 1996".
 * <P>
 * Providers who subclass Calendar should override both computeFields() and computeTime(),
 * not getTime() and setTime().
 */

#ifdef NLS_MAC
#pragma export on
#endif

class T_FORMAT_API Calendar {
public:

    /**
     * Field IDs for date and time. Used to specify date/time fields. ERA is calendar
     * specific. Example ranges given are for illustration only; see specific Calendar
     * subclasses for actual ranges.
     */
    enum EDateFields {
        ERA,                  // Example: 0..1
        YEAR,                 // Example: 1..big number
        MONTH,                // Example: 0..11
        WEEK_OF_YEAR,         // Example: 1..53
        WEEK_OF_MONTH,        // Example: 1..4
        DATE,                 // Example: 1..31
        DAY_OF_YEAR,          // Example: 1..365
        DAY_OF_WEEK,          // Example: 1..7
        DAY_OF_WEEK_IN_MONTH, // Example: 1..4, may be specified as -1
        AM_PM,                // Example: 0..1
        HOUR,                 // Example: 0..11
        HOUR_OF_DAY,          // Example: 0..23
        MINUTE,               // Example: 0..59
        SECOND,               // Example: 0..59
        MILLISECOND,          // Example: 0..999
        ZONE_OFFSET,          // Example: -12*kMillisPerHour..12*kMillisPerHour
        DST_OFFSET,           // Example: 0 or kMillisPerHour
        FIELD_COUNT,

        DAY_OF_MONTH = DATE   // Synonyms
    };

    /**
     * Useful constant for days of week. Note: Calendar day-of-week is 1-based. Clients
     * who create locale resources for the field of first-day-of-week should be aware of
     * this. For instance, in US locale, first-day-of-week is set to 1, i.e., SUNDAY.
     */
    enum EDaysOfWeek {
        SUNDAY = 1,
        MONDAY,
        TUESDAY,
        WEDNESDAY,
        THURSDAY,
        FRIDAY,
        SATURDAY
    };

    /**
     * Useful constants for month. Note: Calendar month is 0-based.
     */
    enum EMonths {
        JANUARY,
        FEBRUARY,
        MARCH,
        APRIL,
        MAY,
        JUNE,
        JULY,
        AUGUST,
        SEPTEMBER,
        OCTOBER,
        NOVEMBER,
        DECEMBER,
        UNDECIMBER
    };

    /**
     * Useful constants for hour in 12-hour clock. Used in GregorianCalendar.
     */
    enum EAmpm {
        AM,
        PM
    };

    /**
     * destructor
     */
    virtual ~Calendar();

    /**
     * Create and return a polymorphic copy of this calendar.
     */
    virtual Calendar* clone() const = 0;

    /**
     * Creates a Calendar using the default timezone and locale. Clients are responsible
     * for deleting the object returned.
     *
     * @param success  Indicates the success/failure of Calendar creation. Filled in
     *                 with ZERO_ERROR if created successfully, set to a failure result
     *                 otherwise.
     * @return         A Calendar if created successfully. NULL otherwise.
     */
    static Calendar* createInstance(ErrorCode& success);

    /**
     * Creates a Calendar using the given timezone and the default locale. If creation of
     * a new Calendar is successful, the Calendar takes ownership of zoneToAdopt; the
     * client should not delete it.
     *
     * @param zoneToAdopt  The given timezone to be adopted.
     * @param success      Indicates the success/failure of Calendar creation. Filled in
     *                     with ZERO_ERROR if created successfully, set to a failure result
     *                     otherwise.
     * @return             A Calendar if created successfully. NULL otherwise.
     */
    static Calendar* createInstance(TimeZone* zoneToAdopt, ErrorCode& success);

    /**
     * Creates a Calendar using the given timezone and the default locale.  The TimeZone
     * is _not_ adopted; the client is still responsible for deleting it.
     *
     * @param zone  The timezone.
     * @param success      Indicates the success/failure of Calendar creation. Filled in
     *                     with ZERO_ERROR if created successfully, set to a failure result
     *                     otherwise.
     * @return             A Calendar if created successfully. NULL otherwise.
     */
    static Calendar* createInstance(const TimeZone& zone, ErrorCode& success);

    /**
     * Creates a Calendar using the default timezone and the given locale.
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the success/failure of Calendar creation. Filled in
     *                 with ZERO_ERROR if created successfully, set to a failure result
     *                 otherwise.
     * @return         A Calendar if created successfully. NULL otherwise.
     */
    static Calendar* createInstance(const Locale& aLocale, ErrorCode& success);

    /**
     * Creates a Calendar using the given timezone and given locale. If creation of
     * a new Calendar is successful, the Calendar takes ownership of zoneToAdopt; the
     * client should not delete it.
     *
     * @param zoneToAdopt  The given timezone to be adopted.
     * @param aLocale      The given locale.
     * @param success      Indicates the success/failure of Calendar creation. Filled in
     *                     with ZERO_ERROR if created successfully, set to a failure result
     *                     otherwise.
     * @return             A Calendar if created successfully. NULL otherwise.
     */
    static Calendar* createInstance(TimeZone* zoneToAdopt, const Locale& aLocale, ErrorCode& success);

    /**
     * Gets a Calendar using the given timezone and given locale.  The TimeZone
     * is _not_ adopted; the client is still responsible for deleting it.
     *
     * @param zone  The timezone.
     * @param aLocale      The given locale.
     * @param success      Indicates the success/failure of Calendar creation. Filled in
     *                     with ZERO_ERROR if created successfully, set to a failure result
     *                     otherwise.
     * @return             A Calendar if created successfully. NULL otherwise.
     */
    static Calendar* createInstance(const TimeZone& zoneToAdopt, const Locale& aLocale, ErrorCode& success);

    /**
     * Returns a list of the locales for which Calendars are installed.
     *
     * @param count  Number of locales returned.
     * @return       An array of Locale objects representing the set of locales for which
     *               Calendars are installed.  The system retains ownership of this list;
     *               the caller must NOT delete it.
     */
    static const Locale* getAvailableLocales(t_int32& count);

    /**
     * Returns the current system time in milliseconds since 0:00:00 GMT 1/1/70.
     *
     * @return   The current system time in milliseconds.
     */
    static Date getNow();

    /**
     * Gets this Calendar's time as milliseconds. May involve recalculation of time due
     * to previous calls to set time field values. The time specified is non-local UCT
     * (GMT) time. Although this method is const, this object may actually be changed
     * (semantically const).
     *
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid or restricted by
     *                leniency, this will be set to an error status.
     * @return        The current time in UCT (GMT) time, or zero if the operation
     *                failed.
     */
    Date getTime(ErrorCode& status) const;

    /**
     * Sets this Calendar's current time with the given Date. The time specified should
     * be in non-local UCT (GMT) time.
     *
     * @param date  The given Date in UCT (GMT) time.
     */
    void setTime(Date date, ErrorCode& status);

    /**
     * Compares the equality of two Calendar objects. Objects of different subclasses
     * are considered unequal. This comparison is very exacting; two Calendar objects
     * must be in exactly the same state to be considered equal. To compare based on the
     * represented time, use equals() instead.
     *
     * @param that  The Calendar object to be compared with.
     * @return      True if the given Calendar is the same as this Calendar; false
     *              otherwise.
     */
    virtual t_bool operator==(const Calendar& that) const;

    /**
     * Compares the inequality of two Calendar objects.
     *
     * @param that  The Calendar object to be compared with.
     * @return      True if the given Calendar is not the same as this Calendar; false
     *              otherwise.
     */
    t_bool operator!=(const Calendar& that) const {return !operator==(that);}

    /**
     * Compares the Calendar time, whereas Calendar::operator== compares the equality of
     * Calendar objects.
     *
     * @param when    The Calendar to be compared with this Calendar. Although this is a
     *                const parameter, the object may be modified physically
     *                (semantically const).
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid or restricted by
     *                leniency, this will be set to an error status.
     * @return        True if the current time of this Calendar is equal to the time of
     *                Calendar when; false otherwise.
     */
    t_bool equals(const Calendar& when, ErrorCode& status) const;

    /**
     * Returns true if this Calendar's current time is before "when"'s current time.
     *
     * @param when    The Calendar to be compared with this Calendar. Although this is a
     *                const parameter, the object may be modified physically
     *                (semantically const).
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid or restricted by
     *                leniency, this will be set to an error status.
     * @return        True if the current time of this Calendar is before the time of
     *                Calendar when; false otherwise.
     */
    t_bool before(const Calendar& when, ErrorCode& status) const;

    /**
     * Returns true if this Calendar's current time is after "when"'s current time.
     *
     * @param when    The Calendar to be compared with this Calendar. Although this is a
     *                const parameter, the object may be modified physically
     *                (semantically const).
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid or restricted by
     *                leniency, this will be set to an error status.
     * @return        True if the current time of this Calendar is after the time of
     *                Calendar when; false otherwise.
     */
    t_bool after(const Calendar& when, ErrorCode& status) const;

    /**
     * Return true if another Calendar object is equivalent to this one.  An equivalent
     * Calendar will behave exactly as this one does, but may be set to a different time.
     */
    virtual t_bool equivalentTo(const Calendar& other) const;

    /**
     * Date Arithmetic function. Adds the specified (signed) amount of time to the given
     * time field, based on the calendar's rules. For example, to subtract 5 days from
     * the current time of the calendar, call add(Calendar::DATE, -5). When adding on
     * the month or Calendar::MONTH field, other fields like date might conflict and
     * need to be changed. For instance, adding 1 month on the date 01/31/96 will result
     * in 02/29/96.
     *
     * @param field   Specifies which date field to modify.
     * @param amount  The amount of time to be added to the field, in the natural unit
     *                for that field (e.g., days for the day fields, hours for the hour
     *                field.)
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid or restricted by
     *                leniency, this will be set to an error status.
     */
    virtual void add(EDateFields field, t_int32 amount, ErrorCode& status) = 0;

    /**
     * Time Field Rolling function. Rolls (up/down) a single unit of time on the given
     * time field. For example, to roll the current date up by one day, call
     * roll(Calendar::DATE, true). When rolling on the year or Calendar::YEAR field, it
     * will roll the year value in the range between getMinimum(Calendar::YEAR) and the
     * value returned by getMaximum(Calendar::YEAR). When rolling on the month or
     * Calendar::MONTH field, other fields like date might conflict and, need to be
     * changed. For instance, rolling the month up on the date 01/31/96 will result in
     * 02/29/96. Rolling up always means rolling forward in time; e.g., rolling the year
     * up on "100 BC" will result in "99 BC", for Gregorian calendar. When rolling on the
     * hour-in-day or Calendar::HOUR_OF_DAY field, it will roll the hour value in the range
     * between 0 and 23, which is zero-based.
     * <P>
     * NOTE: Do not use this method -- use roll(EDateFields, int, ErrorCode&) instead.
     *
     * @param field   The time field.
     * @param up      Indicates if the value of the specified time field is to be rolled
     *                up or rolled down. Use true if rolling up, false otherwise.
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid or restricted by
     *                leniency, this will be set to an error status.
     */
    void roll(EDateFields field, t_bool up, ErrorCode& status);

    /**
     * Time Field Rolling function. Rolls by the given amount on the given
     * time field. For example, to roll the current date up by one day, call
     * roll(Calendar::DATE, +1, status). When rolling on the month or
     * Calendar::MONTH field, other fields like date might conflict and, need to be
     * changed. For instance, rolling the month up on the date 01/31/96 will result in
     * 02/29/96.  Rolling by a positive value always means rolling forward in time;
     * e.g., rolling the year by +1 on "100 BC" will result in "99 BC", for Gregorian
     * calendar. When rolling on the hour-in-day or Calendar::HOUR_OF_DAY field, it will
     * roll the hour value in the range between 0 and 23, which is zero-based.
     * <P>
     * The only difference between roll() and add() is that roll() does not change
     * the value of more significant fields when it reaches the minimum or maximum
     * of its range, whereas add() does.
     *
     * @param field   The time field.
     * @param amount  Indicates amount to roll.
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid, this will be set to
     *                an error status.
     */
    virtual void roll(EDateFields field, int amount, ErrorCode& status) = 0;

    /**
     * Sets the calendar's time zone to be the one passed in. The Calendar takes ownership
     * of the TimeZone; the caller is no longer responsible for deleting it.  If the
     * given time zone is NULL, this function has no effect.
     *
     * @param value  The given time zone.
     */
    void adoptTimeZone(TimeZone* value);

    /**
     * Sets the calendar's time zone to be the same as the one passed in. The TimeZone
     * passed in is _not_ adopted; the client is still responsible for deleting it.
     *
     * @param value  The given time zone.
     */
    void setTimeZone(const TimeZone& zone);

    /**
     * Returns a reference to the time zone owned by this calendar. The returned reference
     * is only valid until clients make another call to adoptTimeZone or setTimeZone,
     * or this Calendar is destroyed.
     *
     * @return   The time zone object associated with this calendar.
     */
    const TimeZone& getTimeZone() const;

    /**
     * Returns the time zone owned by this calendar. The caller owns the returned object
     * and must delete it when done.  After this call, the new time zone associated
     * with this Calendar is the default TimeZone as returned by TimeZone::createDefault().
     *
     * @return   The time zone object which was associated with this calendar.
     */
    TimeZone* orphanTimeZone();

    /**
     * Queries if the current date for this Calendar is in Daylight Savings Time.
     *
     * @param status Fill-in parameter which receives the status of this operation.
     * @return   True if the current date for this Calendar is in Daylight Savings Time,
     *           false, otherwise.
     */
    virtual t_bool inDaylightTime(ErrorCode& status) const = 0;

    /**
     * Specifies whether or not date/time interpretation is to be lenient. With lenient
     * interpretation, a date such as "February 942, 1996" will be treated as being
     * equivalent to the 941st day after February 1, 1996. With strict interpretation,
     * such dates will cause an error when computing time from the time field values
     * representing the dates.
     *
     * @param lenient  True specifies date/time interpretation to be lenient.
     *
     * @see            DateFormat#setLenient
     */
    void setLenient(t_bool lenient);

    /**
     * Tells whether date/time interpretation is to be lenient.
     *
     * @return   True tells that date/time interpretation is to be lenient.
     */
    t_bool isLenient() const;

    /**
     * Sets what the first day of the week is; e.g., Sunday in US, Monday in France.
     *
     * @param value  The given first day of the week.
     */
    void setFirstDayOfWeek(EDaysOfWeek value);

    /**
     * Gets what the first day of the week is; e.g., Sunday in US, Monday in France.
     *
     * @return   The first day of the week.
     */
    EDaysOfWeek getFirstDayOfWeek() const;

    /**
     * Sets what the minimal days required in the first week of the year are; For
     * example, if the first week is defined as one that contains the first day of the
     * first month of a year, call the method with value 1. If it must be a full week,
     * use value 7.
     *
     * @param value  The given minimal days required in the first week of the year.
     */
    void setMinimalDaysInFirstWeek(t_uint8 value);

    /**
     * Gets what the minimal days required in the first week of the year are; e.g., if
     * the first week is defined as one that contains the first day of the first month
     * of a year, getMinimalDaysInFirstWeek returns 1. If the minimal days required must
     * be a full week, getMinimalDaysInFirstWeek returns 7.
     *
     * @return   The minimal days required in the first week of the year.
     */
    t_uint8 getMinimalDaysInFirstWeek() const;

    /**
     * Gets the minimum value for the given time field. e.g., for Gregorian
     * DAY_OF_MONTH, 1.
     *
     * @param field  The given time field.
     * @return       The minimum value for the given time field.
     */
    virtual t_int32 getMinimum(EDateFields field) const = 0;

    /**
     * Gets the maximum value for the given time field. e.g. for Gregorian DAY_OF_MONTH,
     * 31.
     *
     * @param field  The given time field.
     * @return       The maximum value for the given time field.
     */
    virtual t_int32 getMaximum(EDateFields field) const = 0;

    /**
     * Gets the highest minimum value for the given field if varies. Otherwise same as
     * getMinimum(). For Gregorian, no difference.
     *
     * @param field  The given time field.
     * @return       The highest minimum value for the given time field.
     */
    virtual t_int32 getGreatestMinimum(EDateFields field) const = 0;

    /**
     * Gets the lowest maximum value for the given field if varies. Otherwise same as
     * getMaximum(). e.g., for Gregorian DAY_OF_MONTH, 28.
     *
     * @param field  The given time field.
     * @return       The lowest maximum value for the given time field.
     */
    virtual t_int32 getLeastMaximum(EDateFields field) const = 0;

    /**
     * Gets the value for a given time field. Recalculate the current time field values
     * if the time value has been changed by a call to setTime(). Return zero for unset
     * fields if any fields have been explicitly set by a call to set(). To force a
     * recomputation of all fields regardless of the previous state, call complete().
     * This method is semantically const, but may alter the object in memory.
     *
     * @param field  The given time field.
     * @param status Fill-in parameter which receives the status of the operation.
     * @return       The value for the given time field, or zero if the field is unset,
     *               and set() has been called for any other field.
     */
    t_int32 get(EDateFields field, ErrorCode& status) const;

    /**
     * Determines if the given time field has a value set. This can affect in the
     * resolving of time in Calendar. Unset fields have a value of zero, by definition.
     *
     * @return   True if the given time field has a value set; false otherwise.
     */
    t_bool isSet(EDateFields field) const;

    /**
     * Sets the given time field with the given value.
     *
     * @param field  The given time field.
     * @param value  The value to be set for the given time field.
     */
    void set(EDateFields field, t_int32 value);

    /**
     * Sets the values for the fields YEAR, MONTH, and DATE. Other field values are
     * retained; call clear() first if this is not desired.
     *
     * @param year   The value used to set the YEAR time field.
     * @param month  The value used to set the MONTH time field. Month value is 0-based.
     *               e.g., 0 for January.
     * @param date   The value used to set the DATE time field.
     */
    void set(t_int32 year, t_int32 month, t_int32 date);

    /**
     * Sets the values for the fields YEAR, MONTH, DATE, HOUR_OF_DAY, and MINUTE. Other
     * field values are retained; call clear() first if this is not desired.
     *
     * @param year    The value used to set the YEAR time field.
     * @param month   The value used to set the MONTH time field. Month value is
     *                0-based. E.g., 0 for January.
     * @param date    The value used to set the DATE time field.
     * @param hour    The value used to set the HOUR_OF_DAY time field.
     * @param minute  The value used to set the MINUTE time field.
     */
    void set(t_int32 year, t_int32 month, t_int32 date, t_int32 hour, t_int32 minute);

    /**
     * Sets the values for the fields YEAR, MONTH, DATE, HOUR_OF_DAY, MINUTE, and SECOND.
     * Other field values are retained; call clear() first if this is not desired.
     *
     * @param year    The value used to set the YEAR time field.
     * @param month   The value used to set the MONTH time field. Month value is
     *                0-based. E.g., 0 for January.
     * @param date    The value used to set the DATE time field.
     * @param hour    The value used to set the HOUR_OF_DAY time field.
     * @param minute  The value used to set the MINUTE time field.
     * @param second  The value used to set the SECOND time field.
     */
    void set(t_int32 year, t_int32 month, t_int32 date, t_int32 hour, t_int32 minute, t_int32 second);

    /**
     * Clears the values of all the time fields, making them both unset and assigning
     * them a value of zero. The field values will be determined during the next
     * resolving of time into time fields.
     */
    void clear();

    /**
     * Clears the value in the given time field, both making it unset and assigning it a
     * value of zero. This field value will be determined during the next resolving of
     * time into time fields.
     *
     * @param field  The time field to be cleared.
     */
    void clear(EDateFields field);

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual method. This method is to
     * implement a simple version of RTTI, since not all C++ compilers support genuine
     * RTTI. Polymorphic operator==() and clone() methods call this method.
     * <P>
     * Concrete subclasses of Calendar must implement getDynamicClassID() and also a
     * static method and data member:
     *
     *      static ClassID getStaticClassID() { return (ClassID)&fgClassID; }
     *      static char fgClassID;
     *
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     */
    virtual ClassID getDynamicClassID() const = 0;

protected:

     /**
      * Constructs a Calendar with the default time zone as returned by
      * TimeZone::createInstance(), and the default locale.
      *
      * @param success  Indicates the status of Calendar object construction. Returns
      *                 ZERO_ERROR if constructed successfully.
      */
    Calendar(ErrorCode& success);

    /**
     * Copy constructor
     */
    Calendar(const Calendar& source);

    /**
     * Default assignment operator
     */
    Calendar& operator=(const Calendar& right);

    /**
     * Constructs a Calendar with the given time zone and locale. Clients are no longer
     * responsible for deleting the given time zone object after it's adopted.
     *
     * @param zoneToAdopt     The given time zone.
     * @param aLocale  The given locale.
     * @param success  Indicates the status of Calendar object construction. Returns
     *                 ZERO_ERROR if constructed successfully.
     */
    Calendar(TimeZone* zone, const Locale& aLocale, ErrorCode& success);

    /**
     * Constructs a Calendar with the given time zone and locale.
     *
     * @param zone     The given time zone.
     * @param aLocale  The given locale.
     * @param success  Indicates the status of Calendar object construction. Returns
     *                 ZERO_ERROR if constructed successfully.
     */
    Calendar(const TimeZone& zone, const Locale& aLocale, ErrorCode& success);

    /**
     * Converts Calendar's time field values to GMT as milliseconds.
     *
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid or restricted by
     *                leniency, this will be set to an error status.
     */
    virtual void computeTime(ErrorCode& status) = 0;

    /**
     * Converts GMT as milliseconds to time field values. This allows you to sync up the
     * time field values with a new time that is set for the calendar.  This method
     * does NOT recompute the time first; to recompute the time, then the fields, use
     * the method complete().
     */
    virtual void computeFields(ErrorCode& status) = 0;

    /**
     * Recomputes the current time from currently set fields, and then fills in any
     * unset fields in the time field list.
     *
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid or restricted by
     *                leniency, this will be set to an error status.
     */
    void complete(ErrorCode& status);

    /**
     * Gets the value for a given time field. Subclasses can use this function to get
     * field values without forcing recomputation of time.
     *
     * @param field  The given time field.
     * @return       The value for the given time field.
     */
    t_int32 internalGet(EDateFields field) const {return fFields[field];}

    /**
     * Sets the value for a given time field.  This is a fast internal method for
     * subclasses.  It does not affect the areFieldsInSync, isTimeSet, or areAllFieldsSet
     * flags.
     */
    void internalSet(EDateFields field, t_int32 value);

protected:
    /**
     * The flag which indicates if the current time is set in the calendar.
     */
    t_bool      fIsTimeSet;

    /**
     * True if the fields are in sync with the currently set time of this Calendar.
     * If false, then the next attempt to get the value of a field will
     * force a recomputation of all fields from the current value of the time
     * field.
     * <P>
     * This should really be named areFieldsInSync, but the old name is retained
     * for backward compatibility.
     */
    t_bool      fAreFieldsSet;

    /**
     * True if all of the fields have been set.  This is initially false, and set to
     * true by computeFields().
     */
    t_bool      fAreAllFieldsSet;

    /**
     * Get the current time without recomputing.
     */
    Date        internalGetTime() const { return fTime; }

    /**
     * Set the current time without affecting flags or fields.
     */
    void        internalSetTime(Date time) { fTime = time; }

    /**
     * The time fields containing values into which the millis is computed.
     */
    t_int32     fFields[FIELD_COUNT];

    /**
     * The flags which tell if a specified time field for the calendar is set.
     */
    t_bool      fIsSet[FIELD_COUNT];

    /**
     * True if the ZONE_OFFSET field was set by the user.  Meaningless if fIsSet[ZONE_OFFSET]
     * is false.
     */
    t_bool      fUserSetZoneOffset;

    /**
     * True if the DST_OFFSET field was set by the user.  Meaningless if fIsSet[ZONE_OFFSET]
     * is false.
     */
    t_bool      fUserSetDSTOffset;

private:
    /**
     * The current time set for the calendar.
     */
    Date        fTime;

    /**
     * @see   #setLenient
     */
    t_bool      fLenient;

    /**
     * Time zone affects the time calculation done by Calendar. Calendar subclasses use
     * the time zone data to produce the local time.
     */
    TimeZone*   fZone;

    /**
     * Both firstDayOfWeek and minimalDaysInFirstWeek are locale-dependent. They are
     * used to figure out the week count for a specific date for a given locale. These
     * must be set when a Calendar is constructed. For example, in US locale,
     * firstDayOfWeek is SUNDAY; minimalDaysInFirstWeek is 1. They are used to figure
     * out the week count for a specific date for a given locale. These must be set when
     * a Calendar is constructed.
     */
    EDaysOfWeek fFirstDayOfWeek;
    t_uint8     fMinimalDaysInFirstWeek;

    /**
     * Sets firstDayOfWeek and minimalDaysInFirstWeek. Called at Calendar construction
     * time.
     *
     * @param desiredLocale  The given locale.
     * @param success        Indicates the status of setting the week count data from
     *                       the resource for the given locale. Returns ZERO_ERROR if
     *                       constructed successfully.
     */
    void        setWeekCountData(const Locale& desiredLocale, ErrorCode& success);

    /**
     * Convert a UnicodeString to a long integer, using the standard C library. Return
     * both the value obtained, and a ErrorCode indicating success or failure. We fail
     * if the string is zero length, of if strtol() does not parse all of the characters
     * in the string, or if the value is not in the range 1..7.
     */
    static int  stringToDayNumber(const UnicodeString& string, ErrorCode& status);

    /**
     * The resource tag for the resource where the week-count data is stored.
     */
    static const char* kDateTimeElements;
};

#ifdef NLS_MAC
#pragma export off
#endif

// -------------------------------------

inline void
Calendar::roll(EDateFields field, t_bool up, ErrorCode& status)
{
	roll(field, (up ? +1 : -1), status);
}

// -------------------------------------

/**
 * Fast method for subclasses.  The caller must maintain fUserSetDSTOffset and
 * fUserSetZoneOffset, as well as the isSet[] array.
 */
inline void
Calendar::internalSet(EDateFields field, t_int32 value)
{
	fFields[field] = value;
}


#endif // _CALENDAR
