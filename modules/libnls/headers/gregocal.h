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
* File GREGOCAL.H
*
* Modification History:
*
*	Date		Name		Description
*	04/22/97	aliu		Overhauled header.
********************************************************************************
*/

#ifndef _GREGOCAL
#define _GREGOCAL

#ifndef _CALENDAR
#include "calendar.h"
#endif

/**
 * Concrete class which provides the standard calendar used by most of the world.
 * <P>
 * The standard (Gregorian) calendar has 2 eras, BC and AD.
 * <P>
 * This implementation handles a single discontinuity, which corresponds by default to
 * the date the Gregorian calendar was originally instituted (October 15, 1582). Not all
 * countries adopted the Gregorian calendar then, so this cutover date may be changed by
 * the caller.
 * <P>
 * Prior to the institution of the Gregorian Calendar, New Year's Day was March 25. To
 * avoid confusion, this Calendar always uses January 1. A manual adjustment may be made
 * if desired for dates that are prior to the Gregorian changeover and which fall
 * between January 1 and March 24.
 */
#ifdef NLS_MAC
#pragma export on
#endif
 
class T_FORMAT_API GregorianCalendar: public Calendar {
public:

    /**
     * Useful constants for GregorianCalendar and TimeZone.
     */
    enum EEras {
        BC,
        AD
    };

    /**
     * Constructs a default GregorianCalendar using the current time in the default time
     * zone with the default locale.
     *
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(ErrorCode& success);

    /**
     * Constructs a GregorianCalendar based on the current time in the given time zone
     * with the default locale. Clients are no longer responsible for deleting the given
     * time zone object after it's adopted.
     *
     * @param zoneToAdopt     The given timezone.
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(TimeZone* zoneToAdopt, ErrorCode& success);

    /**
     * Constructs a GregorianCalendar based on the current time in the given time zone
     * with the default locale.
     *
     * @param zone     The given timezone.
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(const TimeZone& zone, ErrorCode& success);

    /**
     * Constructs a GregorianCalendar based on the current time in the default time zone
     * with the given locale.
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(const Locale& aLocale, ErrorCode& success);

    /**
     * Constructs a GregorianCalendar based on the current time in the given time zone
     * with the given locale. Clients are no longer responsible for deleting the given
     * time zone object after it's adopted.
     *
     * @param zoneToAdopt     The given timezone.
     * @param aLocale  The given locale.
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(TimeZone* zoneToAdopt, const Locale& aLocale, ErrorCode& success);

    /**
     * Constructs a GregorianCalendar based on the current time in the given time zone
     * with the given locale.
     *
     * @param zone     The given timezone.
     * @param aLocale  The given locale.
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(const TimeZone& zone, const Locale& aLocale, ErrorCode& success);

    /**
     * Constructs a GregorianCalendar with the given AD date set in the default time
     * zone with the default locale.
     *
     * @param year     The value used to set the YEAR time field in the calendar.
     * @param month    The value used to set the MONTH time field in the calendar. Month
     *                 value is 0-based. e.g., 0 for January.
     * @param date     The value used to set the DATE time field in the calendar.
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(t_int32 year, t_int32 month, t_int32 date, ErrorCode& success);

    /**
     * Constructs a GregorianCalendar with the given AD date and time set for the
     * default time zone with the default locale.
     *
     * @param year     The value used to set the YEAR time field in the calendar.
     * @param month    The value used to set the MONTH time field in the calendar. Month
     *                 value is 0-based. e.g., 0 for January.
     * @param date     The value used to set the DATE time field in the calendar.
     * @param hour     The value used to set the HOUR_OF_DAY time field in the calendar.
     * @param minute   The value used to set the MINUTE time field in the calendar.
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(t_int32 year, t_int32 month, t_int32 date, t_int32 hour, t_int32 minute, ErrorCode& success);

    /**
     * Constructs a GregorianCalendar with the given AD date and time set for the
     * default time zone with the default locale.
     *
     * @param year     The value used to set the YEAR time field in the calendar.
     * @param month    The value used to set the MONTH time field in the calendar. Month
     *                 value is 0-based. e.g., 0 for January.
     * @param date     The value used to set the DATE time field in the calendar.
     * @param hour     The value used to set the HOUR_OF_DAY time field in the calendar.
     * @param minute   The value used to set the MINUTE time field in the calendar.
     * @param second   The value used to set the SECOND time field in the calendar.
     * @param success  Indicates the status of GregorianCalendar object construction.
     *                 Returns ZERO_ERROR if constructed successfully.
     */
    GregorianCalendar(t_int32 year, t_int32 month, t_int32 date, t_int32 hour, t_int32 minute, t_int32 second, ErrorCode& success);

    /**
     * Destructor
     */
    virtual ~GregorianCalendar();

    /**
     * Copy constructor
     */
    GregorianCalendar(const GregorianCalendar& source);

    /**
     * Default assignment operator
     */
    GregorianCalendar& operator=(const GregorianCalendar& right);

    /**
     * Create and return a polymorphic copy of this calendar.
     */
    virtual Calendar* clone() const;

    /**
     * Sets the GregorianCalendar change date. This is the point when the switch from
     * Julian dates to Gregorian dates occurred. Default is 00:00:00 local time, October
     * 15, 1582. Previous to this time and date will be Julian dates.
     *
     * @param date     The given Gregorian cutover date.
     * @param success  Output param set to success/failure code on exit.
     */
    void setGregorianChange(Date date, ErrorCode& success);

    /**
     * Gets the Gregorian Calendar change date. This is the point when the switch from
     * Julian dates to Gregorian dates occurred. Default is 00:00:00 local time, October
     * 15, 1582. Previous to this time and date will be Julian dates.
     *
     * @return   The Gregorian cutover time for this calendar.
     */
    Date getGregorianChange() const;

    /**
     * Return true if the given year is a leap year. Determination of whether a year is
     * a leap year is actually very complicated. We do something crude and mostly
     * correct here, but for a real determination you need a lot of contextual
     * information. For example, in Sweden, the change from Julian to Gregorian happened
     * in a complex way resulting in missed leap years and double leap years between
     * 1700 and 1753. Another example is that after the start of the Julian calendar in
     * 45 B.C., the leap years did not regularize until 8 A.D. This method ignores these
     * quirks, and pays attention only to the Julian onset date and the Gregorian
     * cutover (which can be changed).
     *
     * @param year  The given year.
     * @return      True if the given year is a leap year; false otherwise.
     */
    t_bool isLeapYear(t_int32 year) const;

    /**
     * Compares the equality of two GregorianCalendar objects. Objects of different
     * subclasses are considered unequal.  This is a strict equality test; see the
     * documentation for Calendar::operator==().
     *
     * @param that  The GregorianCalendar object to be compared with.
     * @return      True if the given GregorianCalendar is the same as this
     *              GregorianCalendar; false otherwise.
     */
    virtual t_bool operator==(const Calendar& that) const;

    /**
     * Calendar override.
     * Return true if another Calendar object is equivalent to this one.  An equivalent
     * Calendar will behave exactly as this one does, but may be set to a different time.
     */
    virtual t_bool equivalentTo(const Calendar& other) const;

    /**
     * (Overrides Calendar) Date Arithmetic function. Adds the specified (signed) amount
     * of time to the given time field, based on the calendar's rules.  For more
     * information, see the documentation for Calendar::add().
     *
     * @param field   The time field.
     * @param amount  The amount of date or time to be added to the field.
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid, this will be set to
     *                an error status.
     */
    virtual void add(EDateFields field, t_int32 amount, ErrorCode& status);

    /**
     * (Overrides Calendar) Rolls up or down by the given amount in the specified field.
     * For more information, see the documentation for Calendar::roll().
     *
     * @param field   The time field.
     * @param amount  Indicates amount to roll.
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid, this will be set to
     *                an error status.
     */
    virtual void roll(EDateFields field, int amount, ErrorCode& status);

    /**
     * (Overrides Calendar) Returns minimum value for the given field. e.g. for
     * Gregorian DAY_OF_MONTH, 1.
     */
    virtual t_int32 getMinimum(EDateFields field) const;

    /**
     * (Overrides Calendar) Returns maximum value for the given field. e.g. for
     * Gregorian DAY_OF_MONTH, 31.
     */
    virtual t_int32 getMaximum(EDateFields field) const;

    /**
     * (Overrides Calendar) Returns highest minimum value for the given field if varies.
     * Otherwise same as getMinimum(). For Gregorian, no difference.
     */
    virtual t_int32 getGreatestMinimum(EDateFields field) const;

    /**
     * (Overrides Calendar) Returns lowest maximum value for the given field if varies.
     * Otherwise same as getMaximum(). For Gregorian DAY_OF_MONTH, 28.
     */
    virtual t_int32 getLeastMaximum(EDateFields field) const;

    /**
     * (Overrides Calendar) Return true if the current date for this Calendar is in
     * Daylight Savings Time. Recognizes DST_OFFSET, if it is set.
     *
     * @param status Fill-in parameter which receives the status of this operation.
     * @return   True if the current date for this Calendar is in Daylight Savings Time,
     *           false, otherwise.
     */
    virtual t_bool inDaylightTime(ErrorCode& status) const;

public:

    /**
     * Override Calendar Returns a unique class ID POLYMORPHICALLY. Pure virtual
     * override. This method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone() methods call
     * this method.
     *
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     */
    virtual ClassID getDynamicClassID() const { return (ClassID)&fgClassID; }

    /**
     * Return the class ID for this class. This is useful only for comparing to a return
     * value from getDynamicClassID(). For example:
     *
     *      Base* polymorphic_pointer = createPolymorphicObject();
     *      if (polymorphic_pointer->getDynamicClassID() ==
     *          Derived::getStaticClassID()) ...
     *
     * @return   The class ID for all objects of this class.
     */
    static ClassID getStaticClassID() { return (ClassID)&fgClassID; }

protected:

    /**
     * (Overrides Calendar) Converts GMT as milliseconds to time field values.
     */
    virtual void computeFields(ErrorCode& status);

    /**
     * (Overrides Calendar) Converts Calendar's time field values to GMT as
     * milliseconds.
     *
     * @param status  Output param set to success/failure code on exit. If any value
     *                previously set in the time field is invalid, this will be set to
     *                an error status.
     */
    virtual void computeTime(ErrorCode& status);

private:

    /**
     * Return the length of the given month in the given year, accounting for Gregorian
     * leap years.
     */
    int monthLength(int month, int year) const;

    /**
     * Converts time as milliseconds to Julian date. The Julian date used here is not a
     * true Julian date, since it is measured from midnight, not noon.
     *
     * @param millis  The given milliseconds.
     * @return        The Julian date number.
     */
    static long millisToJulianDay(Date millis);

    /**
     * Converts Julian date to time as milliseconds. The Julian date used here is not a
     * true Julian date, since it is measured from midnight, not noon.
     *
     * @param julian  The given Julian date number.
     * @return        Time as milliseconds.
     */
    static Date julianDayToMillis(long julian);

    /**
     * Convert a quasi Julian date to the day of the week. The Julian date used here is
     * not a true Julian date, since it is measured from midnight, not noon. Return
     * value is one-based.
     *
     * @return   Day number from 1..7 (SUN..SAT).
     */
    static int julianDayToDayOfWeek(long julian);

    /**
     * Compute the date-based fields given the milliseconds since the epoch start. Do
     * not compute the time-based fields (HOUR, MINUTE, etc.).
     *
     * @param theTime  The given time as LOCAL milliseconds, not UTC.
     */
    void timeToFields(Date theTime, ErrorCode& status);

    /**
     * Return the week number of a day, within a period. This may be the week number in
     * a year, or the week number in a month. Usually this will be a value >= 1, but if
     * some initial days of the period are excluded from week 1, because
     * minimalDaysInFirstWeek is > 1, then the week number will be zero for those
     * initial days. Requires the day of week for the given date in order to determine
     * the day of week of the first day of the period.
     *
     * @param date  Day-of-year or day-of-month. Should be 1 for first day of period.
     * @param day   Day-of-week for given dayOfPeriod. 1-based with 1=Sunday.
     * @return      Week number, one-based, or zero if the day falls in part of the
     *              month before the first week, when there are days before the first
     *              week because the minimum days in the first week is more than one.
     */
    int weekNumber(int date, int day);

    /**
     * Validates the values of the set time fields.  True if they're all valid.
     */
    t_bool validateFields() const;

    /**
     * Validates the value of the given time field.  True if it's valid.
     */
    t_bool boundsCheck(t_int32 value, EDateFields field) const;

    // This is measured from the standard epoch, not in Julian Days.
    Date                fGregorianCutover;

    static char fgClassID;

    static const Date   kPapalCutover; // Cutover decreed by Pope Gregory
    static const Date   kJulianOnset; // Onset of Julian calendar, 45 B.C.
    static const long   kEpochStartAsJulianDay; // 1970-01-01 as Julian Day
    static const int    kMonthLength[]; // lengths of months in non-leap year
    static const int    kLeapMonthLength[]; // lengths of months in leap year
    static const int    NUM_DAYS[]; // day-in-year numbers for the first of each month in non-leap years
    static const int    LEAP_NUM_DAYS[]; // day-in-year numbers for the firstof each month in leap years
    static const Date   EARLIEST_USABLE_MILLIS; // earliest Data value for which we can compute fields
    static const int    EARLIEST_USABLE_YEAR; // earliest year for which we can compute dates and fields
    static const Date   EARLIEST_INVALID_MILLIS; // earliest Date value for which we can't compute fields
                                                 // (i.e., high bound of valid range)
    static const Date   ONE_WEEK; // number of milliseconds in a week
};

#ifdef NLS_MAC
#pragma export off
#endif


inline int GregorianCalendar::julianDayToDayOfWeek(long julian)
{
	// If julian is negative, then julian%7 will be negative, so we adjust
	// accordingly.  We add 1 because Julian day 0 is Monday.
	int dayOfWeek = (julian + 1) % 7;
	return dayOfWeek + ((dayOfWeek < 0) ? (7 + SUNDAY) : SUNDAY);
}

#endif // _GREGOCAL
//eof
