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
* File TIMEZONE.H
*
* Modification History:
*
*   Date        Name        Description
*   04/21/97    aliu        Overhauled header.
*   07/09/97    helena      Changed createInstance to createDefault.
*   08/06/97    aliu        Removed dependency on internal header for Hashtable.
********************************************************************************
*/

#ifndef _NLSTIMEZONE
#define _NLSTIMEZONE

#include "unistring.h"
class SimpleTimeZone;
class Hashtable;
   
/**
 * TimeZone is an abstract class representing a time zone.  A TimeZone is needed for
 * Calendar to produce local time for a particular time zone.  A TimeZone comprises
 * three basic pieces of information:<ul>
 *    <li>A time zone offset; that, is the number of milliseconds to add or subtract
 *      from a time expressed in terms of GMT to convert it to the same time in that
 *      time zone (without taking daylight savings time into account).
 *    <li>Logic necessary to take daylight savings time into account if daylight savings
 *      time is observed in that time zone (e.g., the days and hours on which daylight
 *      savings time begins and ends).
 *    <li>An ID.  This is a text string that uniquely identifies the time zone.</ul>
 *
 * (Only the ID is actually implemented in TimeZone; subclasses of TimeZone may handle
 * daylight savings time and GMT offset in different ways.  Currently we only have one
 * TimeZone subclass: SimpleTimeZone.)
 * <P>
 * The TimeZone class contains a static list containing a TimeZone object for every
 * combination of GMT offset and daylight-savings time rules currently in use in the
 * world, each with a unique ID.  Each ID consists of a region (usually a continent or
 * ocean) and a city in that region, separated by a slash, (for example, Pacific
 * Standard Time is "America/Los_Angeles.")  Because older versions of this class used
 * three- or four-letter abbreviations instead, there is also a table that maps the older
 * abbreviations to the newer ones (for example, "PST" maps to "America/LosAngeles").
 * Anywhere the API requires an ID, you can use either form.
 * <P>
 * To create a new TimeZone, you call the factory function TimeZone::createTimeZone()
 * and pass it a time zone ID.  You can use the createAvailableIDs() function to
 * obtain a list of all the time zone IDs recognized by createTimeZone().
 * <P>
 * You can also use TimeZone::createDefault() to create a TimeZone.  This function uses
 * platform-specific APIs to produce a TimeZone for the time zone corresponding to 
 * the client's computer's physical location.  For example, if you're in Japan (assuming
 * your machine is set up correctly), TimeZone::createDefault() will return a TimeZone
 * for Japanese Standard Time ("Asia/Tokyo").
 */
#ifdef NLS_MAC
#pragma export on
#endif
class T_FORMAT_API TimeZone {
public:
    virtual ~TimeZone();

    /**
     * Creates a new TimeZone for the given tmie zone ID.  If the given ID is not
     * supported by TimeZone class, returns null.  The result is a new TimeZone object
     * that the client owns and is responsible for deleting.
     *
     * @param ID  A time zone ID.
     * @return    A new TimeZone corresponding to the time zone with the given ID,
     *  or NULL if there is no time zone with the given ID.
     */
    static TimeZone* createTimeZone(const UnicodeString& ID);

    /**
     * Returns a list of time zone IDs, one for each time zone with a given GMT offset.
     * The return value is a list because there may be several times zones with the same
     * GMT offset that differ in the way they handle daylight savings time.  For example,
     * the state of Arizona doesn't observe Daylight Savings time.  So if you ask for
     * the time zone IDs corresponding to GMT-7:00, you'll get back two time zone IDs:
     * "America/Denver," which corresponds to Mountain Standard Time in the winter and
     * Mountain Daylight Time in the summer, and "America/Phoenix", which corresponds to
     * Mountain Standard Time year-round, even in the summer.
     * <P>
     * The caller owns the list that is returned, but does not own the strings contained
     * in that list.  Delete the array, but DON'T delete the elements in the array.
     *
     * @param rawOffset  An offset from GMT in milliseconds.
     * @param numIDs     Receives the number of items in the array that is returned.
     * @return           An array of UnicodeString pointers, where each UnicodeString is
     *                   a time zone ID for a time zone with the given GMT offset.  If
     *                   there is no timezone that matches the GMT offset
     *                   specified, NULL is returned.
     */
    static const UnicodeString** const createAvailableIDs(t_int32 rawOffset, long& numIDs);

    /**
     * Returns a list of all time zone IDs supported by the TimeZone class (i.e., all
     * IDs that it's legal to pass to createTimeZone()).  The caller owns the list that
     * is returned, but does not own the strings contained in that list.  Delete the array,
     * but DON'T delete the elements in the array.
     *
     * @param numIDs  Receives the number of zone IDs returned.
     * @return        An array of UnicodeString pointers, where each is a time zone ID
     *                supported by the TimeZone class.
     */
    static const UnicodeString** const createAvailableIDs(long& numIDs);

    /**
     * Creates a new copy of the default TimeZone for this host. Unless the default time
     * zone has already been set using adoptDefault() or serDefault(), the default is
     * determined by querying the system using methods in TPlatformUtilities. If the
     * system routines fail, or if they specify a TimeZone or TimeZone offset which is not
     * recognized, the TimeZone indicated by the ID kLastResortID is instantiated
     * and made the default.
     *
     * @return   A default TimeZone. Clients are responsible for deleting the time zone
     *           object returned.
     */
    static TimeZone* createDefault();

    /**
     * Sets the default time zone (i.e., what's returned by getDefault()) to be the
     * specified time zone.  If NULL is specified for the time zone, the default time
     * zone is set to the default host time zone.  This call adopts the TimeZone object
     * passed in; the clent is no longer responsible for deleting it.
     *
     * @param zone  A pointer to the new TimeZone object to use as the default.
     */
    static void adoptDefault(TimeZone* zone);

    /**
     * Same as adoptDefault(), except that the TimeZone object passed in is NOT adopted;
     * the caller remains responsible for deleting it.
     *
     * @param zone  The given timezone.
     */
    static void setDefault(const TimeZone& zone);

    /**
     * Returns true if the two TimeZones are equal.  (The TimeZone version only compares
     * IDs, but subclasses are expected to also compare the fields they add.)
     *
     * @param that  The TimeZone object to be compared with.
     * @return      True if the given TimeZone is equal to this TimeZone; false
     *              otherwise.
     */
    virtual t_bool operator==(const TimeZone& that) const;

    /**
     * Returns true if the two TimeZones are NOT equal; that is, if operator==() returns
     * false.
     *
     * @param that  The TimeZone object to be compared with.
     * @return      True if the given TimeZone is not equal to this TimeZone; false
     *              otherwise.
     */
    t_bool operator!=(const TimeZone& that) const {return !operator==(that);}

    /**
     * Returns the TimeZone's adjusted GMT offset (i.e., the number of milliseconds to add
     * to GMT to get local time in this time zone, taking daylight savings time into
     * account) as of a particular reference date.  The reference date is used to determine
     * whether daylight savings time is in effect and needs to be figured into the offset
     * that is returned (in other words, what is the adjusted GMT offset in this time zone
     * at this particular date and time?).  For the time zones produced by createTimeZone(),
     * the reference data is specified according to the Gregorian calendar, and the date
     * and time fields are in GMT, NOT local time.
     *
     * @param era        The reference date's era
     * @param year       The reference date's year
     * @param month      The reference date's month (0-based; 0 is January)
     * @param day        The reference date's day-in-month (1-based)
     * @param dayOfWeek  The reference date's day-of-week (1-based; 1 is Sunday)
     * @param millis     The reference date's milliseconds in day, UTT (NOT local time).
     * @return           The offset in milliseconds to add to GMT to get local time.
     */
    virtual t_int32 getOffset(t_uint8 era, t_int32 year, t_int32 month, t_int32 day,
                              t_uint8 dayOfWeek, t_int32 millis) const = 0;

    /**
     * Sets the TimeZone's raw GMT offset (i.e., the number of milliseconds to add
     * to GMT to get local time, before taking daylight savings time into account).
     *
     * @param offsetMillis  The new raw GMT offset for this time zone.
     */
    virtual void setRawOffset(t_int32 offsetMillis) = 0;

    /**
     * Returns the TimeZone's raw GMT offset (i.e., the number of milliseconds to add
     * to GMT to get local time, before taking daylight savings time into account).
     *
     * @return   The TimeZone's raw GMT offset.
     */
    virtual t_int32 getRawOffset() const = 0;

    /**
     * Fills in "ID" with the TimeZone's ID.
     *
     * @param ID  Receives this TimeZone's ID.
     * @return    "ID"
     */
    UnicodeString& getID(UnicodeString& ID) const;

    /**
     * Sets the TimeZone's ID to the specified value.  This doesn't affect any other
     * fields (for example, if you say<
     * blockquote><pre>
     * .     TimeZone* foo = TimeZone::createTimeZone("America/New_York");
     * .     foo.setID("America/Los_Angeles");
     * </pre></blockquote>
     * the time zone's GMT offset and daylight-savings rules don't change to those for
     * Los Angeles.  They're still those for New York.  Only the ID has changed.)
     *
     * @param ID  The new timezone ID.
     */
    void setID(const UnicodeString& ID);

    /**
     * Queries if this TimeZone uses Daylight Savings Time.
     *
     * @return   True if this TimeZone uses Daylight Savings Time; false otherwise.
     */
    virtual t_bool useDaylightTime() const = 0;

    /**
     * Returns true if the given date is within the period when daylight savings time
     * is in effect; false otherwise.  If the TimeZone doesn't observe daylight savings
     * time, this functions always returns false.
     * @param date The date to test.
     * @return true if the given date is in Daylight Savings Time;
     * false otherwise.
     */
    virtual t_bool inDaylightTime(Date date, ErrorCode& status) const = 0;

    /**
     * Clones TimeZone objects polymorphically. Clients are responsible for deleting
     * the TimeZone object cloned.
     *
     * @return   A new copy of this TimeZone object.
     */
    virtual TimeZone* clone() const = 0;

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual method. This method is to
     * implement a simple version of RTTI, since not all C++ compilers support genuine
     * RTTI. Polymorphic operator==() and clone() methods call this method.
     * <P>
     * Concrete subclasses of TimeZone must implement getDynamicClassID() and also a
     * static method and data member:
     * <pre>
     * .     static ClassID getStaticClassID() { return (ClassID)&fgClassID; }
     * .     static char fgClassID;
     * </pre>
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     */
    virtual ClassID getDynamicClassID() const = 0;

	/**
	 * Netscape Extension for library termination
	 */
	static void terminateLibrary(void);

protected:

    /**
     * Default constructor.  ID is initialized to the empty string.
     */
    TimeZone();

    /**
     * Copy constructor.
     */
    TimeZone(const TimeZone& source);

    /**
     * Default assignment operator.
     */
    TimeZone& operator=(const TimeZone& right);

private:
    /**
     * Convert a non-localized string to an integer using a system function. Return a
     * failing ErrorCode status if all characters are not parsed.
     */
//  static t_int32          stringToInteger(const UnicodeString& string, ErrorCode& status);

    /**
     * Delete function for fgHashtable.
     */
    static void             deleteTimeZone(void*);

    static Hashtable*       fgHashtable; // hash table of objects in kSystemTimeZones,
                                         // maps zone ID to TimeZone object (lazy evaluated)
    static TimeZone*        fgDefaultZone; // default time zone (lazy evaluated)
    static UnicodeString*   fgAvailableIDs; // array containing all the IDs in kSystemTimeZones
    static t_int32          fgAvailableIDsCount; // number of IDs in fgAvailableIDs
    static UnicodeString    kLastResortID; // ID of time zone to use as default if we can't
                                           // get a default from the system

    /**
     * Return a reference to the static Hashtable of registered TimeZone
     * objects.  Performs initialization if necessary.
     * <P>
     * This method is also responsible for initializing the array
     * fgAvailableIDs and fgAvailableIDsCount.
     */
    static const Hashtable& getHashtable();

    /**
     * Responsible for setting up fgDefaultZone.  Uses routines in TPlatformUtilities
     * (i.e., platform-specific calls) to get the current system time zone.  Failing
     * that, uses the platform-specific default time zone.  Failing that, uses the time
     * zone specified by kLastResortID.
     */
    static void             initDefault();

    static SimpleTimeZone   kSystemTimeZones[]; // an array of TimeZone objects for
                                                // all possible time zones in
                                                // use around the world as of 1997.
    static const int        kSystemTimeZonesCount; // number of TimeZones in kSystemTimeZones.
    static const t_int32    millisPerHour; // number of milliseconds in an hour
    static const char*      compatibilityMap[]; // maps old-style 3-letter time zone IDs
                                                // to the new region/city form (e.g., maps
                                                // "PST" to "America/Los_Angeles")
    static const int        compatibilityMapCount; // number of entries in compatibilityMap.

    UnicodeString           fID;    // this time zone's ID
};
#ifdef NLS_MAC
#pragma export off
#endif


// -------------------------------------

inline UnicodeString&
TimeZone::getID(UnicodeString& ID) const
{
    ID = fID;
    return ID;
}

// -------------------------------------

inline void
TimeZone::setID(const UnicodeString& ID)
{
    fID = ID;
}

#endif //_TIMEZONE
//eof
