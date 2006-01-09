/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

/*
 * prtime.c --
 *
 *     NSPR date and time functions
 *
 */

#include "prinit.h"
#include "prtime.h"
#include "prlock.h"
#include "prprf.h"
#include "prlog.h"

#include <string.h>
#include <ctype.h>

#ifdef XP_MAC
#include <time.h>
#endif

/* 
 * The COUNT_LEAPS macro counts the number of leap years passed by
 * till the start of the given year Y.  At the start of the year 4
 * A.D. the number of leap years passed by is 0, while at the start of
 * the year 5 A.D. this count is 1. The number of years divisible by
 * 100 but not divisible by 400 (the non-leap years) is deducted from
 * the count to get the correct number of leap years.
 *
 * The COUNT_DAYS macro counts the number of days since 01/01/01 till the
 * start of the given year Y. The number of days at the start of the year
 * 1 is 0 while the number of days at the start of the year 2 is 365
 * (which is ((2)-1) * 365) and so on. The reference point is 01/01/01
 * midnight 00:00:00.
 */

#define COUNT_LEAPS(Y)   ( ((Y)-1)/4 - ((Y)-1)/100 + ((Y)-1)/400 )
#define COUNT_DAYS(Y)  ( ((Y)-1)*365 + COUNT_LEAPS(Y) )
#define DAYS_BETWEEN_YEARS(A, B)  (COUNT_DAYS(B) - COUNT_DAYS(A))




/*
 * Static variables used by functions in this file
 */

/*
 * The following array contains the day of year for the last day of
 * each month, where index 1 is January, and day 0 is January 1.
 */

static const int lastDayOfMonth[2][13] = {
    {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364},
    {-1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365}
};

/*
 * The number of days in a month
 */

static const PRInt8 nDays[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

/*
 * Declarations for internal functions defined later in this file.
 */

static void        ComputeGMT(PRTime time, PRExplodedTime *gmt);
static int        IsLeapYear(PRInt16 year);
static void        ApplySecOffset(PRExplodedTime *time, PRInt32 secOffset);

/*
 *------------------------------------------------------------------------
 *
 * ComputeGMT --
 *
 *     Caveats:
 *     - we ignore leap seconds
 *     - our leap-year calculation is only correct for years 1901-2099
 *
 *------------------------------------------------------------------------
 */

static void
ComputeGMT(PRTime time, PRExplodedTime *gmt)
{
    PRInt32 tmp, rem;
    PRInt32 numDays;
    PRInt64 numDays64, rem64;
    int isLeap;
    PRInt64 sec;
    PRInt64 usec;
    PRInt64 usecPerSec;
    PRInt64 secPerDay;

    /*
     * We first do the usec, sec, min, hour thing so that we do not
     * have to do LL arithmetic.
     */

    LL_I2L(usecPerSec, 1000000L);
    LL_DIV(sec, time, usecPerSec);
    LL_MOD(usec, time, usecPerSec);
    LL_L2I(gmt->tm_usec, usec);
    /* Correct for weird mod semantics so the remainder is always positive */
    if (gmt->tm_usec < 0) {
        PRInt64 one;

        LL_I2L(one, 1L);
        LL_SUB(sec, sec, one);
        gmt->tm_usec += 1000000L;
    }

    LL_I2L(secPerDay, 86400L);
    LL_DIV(numDays64, sec, secPerDay);
    LL_MOD(rem64, sec, secPerDay);
    /* We are sure both of these numbers can fit into PRInt32 */
    LL_L2I(numDays, numDays64);
    LL_L2I(rem, rem64);
    if (rem < 0) {
        numDays--;
        rem += 86400L;
    }

    /* Compute day of week.  Epoch started on a Thursday. */

    gmt->tm_wday = (numDays + 4) % 7;
    if (gmt->tm_wday < 0) {
        gmt->tm_wday += 7;
    }

    /* Compute the time of day. */
    
    gmt->tm_hour = rem / 3600;
    rem %= 3600;
    gmt->tm_min = rem / 60;
    gmt->tm_sec = rem % 60;

    /* Compute the four-year span containing the specified time */

    tmp = numDays / (4 * 365 + 1);
    rem = numDays % (4 * 365 + 1);

    if (rem < 0) {
        tmp--;
        rem += (4 * 365 + 1);
    }

    /*
     * Compute the year after 1900 by taking the four-year span and
     * adjusting for the remainder.  This works because 2000 is a 
     * leap year, and 1900 and 2100 are out of the range.
     */
    
    tmp = (tmp * 4) + 1970;
    isLeap = 0;

    /*
     * 1970 has 365 days
     * 1971 has 365 days
     * 1972 has 366 days (leap year)
     * 1973 has 365 days
     */

    if (rem >= 365) {                                /* 1971, etc. */
        tmp++;
        rem -= 365;
        if (rem >= 365) {                        /* 1972, etc. */
            tmp++;
            rem -= 365;
            if (rem >= 366) {                        /* 1973, etc. */
                tmp++;
                rem -= 366;
            } else {
                isLeap = 1;
            }
        }
    }

    gmt->tm_year = tmp;
    gmt->tm_yday = rem;

    /* Compute the month and day of month. */

    for (tmp = 1; lastDayOfMonth[isLeap][tmp] < gmt->tm_yday; tmp++) {
    }
    gmt->tm_month = --tmp;
    gmt->tm_mday = gmt->tm_yday - lastDayOfMonth[isLeap][tmp];

    gmt->tm_params.tp_gmt_offset = 0;
    gmt->tm_params.tp_dst_offset = 0;
}


/*
 *------------------------------------------------------------------------
 *
 * PR_ExplodeTime --
 *
 *     Cf. struct tm *gmtime(const time_t *tp) and
 *         struct tm *localtime(const time_t *tp)
 *
 *------------------------------------------------------------------------
 */

PR_IMPLEMENT(void)
PR_ExplodeTime(
        PRTime usecs,
        PRTimeParamFn params,
        PRExplodedTime *exploded)
{
    ComputeGMT(usecs, exploded);
    exploded->tm_params = params(exploded);
    ApplySecOffset(exploded, exploded->tm_params.tp_gmt_offset
            + exploded->tm_params.tp_dst_offset);
}


/*
 *------------------------------------------------------------------------
 *
 * PR_ImplodeTime --
 *
 *     Cf. time_t mktime(struct tm *tp)
 *     Note that 1 year has < 2^25 seconds.  So an PRInt32 is large enough.
 *
 *------------------------------------------------------------------------
 */
#if defined(HAVE_WATCOM_BUG_2)
PRTime __pascal __export __loadds
#else
PR_IMPLEMENT(PRTime)
#endif
PR_ImplodeTime(const PRExplodedTime *exploded)
{
    PRExplodedTime copy;
    PRTime retVal;
    PRInt64 secPerDay, usecPerSec;
    PRInt64 temp;
    PRInt64 numSecs64;
    PRInt32 numDays;
    PRInt32 numSecs;

    /* Normalize first.  Do this on our copy */
    copy = *exploded;
    PR_NormalizeTime(&copy, PR_GMTParameters);

    numDays = DAYS_BETWEEN_YEARS(1970, copy.tm_year);
    
    numSecs = copy.tm_yday * 86400 + copy.tm_hour * 3600
            + copy.tm_min * 60 + copy.tm_sec;

    LL_I2L(temp, numDays);
    LL_I2L(secPerDay, 86400);
    LL_MUL(temp, temp, secPerDay);
    LL_I2L(numSecs64, numSecs);
    LL_ADD(numSecs64, numSecs64, temp);

    /* apply the GMT and DST offsets */
    LL_I2L(temp,  copy.tm_params.tp_gmt_offset);
    LL_SUB(numSecs64, numSecs64, temp);
    LL_I2L(temp,  copy.tm_params.tp_dst_offset);
    LL_SUB(numSecs64, numSecs64, temp);

    LL_I2L(usecPerSec, 1000000L);
    LL_MUL(temp, numSecs64, usecPerSec);
    LL_I2L(retVal, copy.tm_usec);
    LL_ADD(retVal, retVal, temp);

    return retVal;
}

/*
 *-------------------------------------------------------------------------
 *
 * IsLeapYear --
 *
 *     Returns 1 if the year is a leap year, 0 otherwise.
 *
 *-------------------------------------------------------------------------
 */

static int IsLeapYear(PRInt16 year)
{
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
        return 1;
    else
        return 0;
}

/*
 * 'secOffset' should be less than 86400 (i.e., a day).
 * 'time' should point to a normalized PRExplodedTime.
 */

static void
ApplySecOffset(PRExplodedTime *time, PRInt32 secOffset)
{
    time->tm_sec += secOffset;

    /* Note that in this implementation we do not count leap seconds */
    if (time->tm_sec < 0 || time->tm_sec >= 60) {
        time->tm_min += time->tm_sec / 60;
        time->tm_sec %= 60;
        if (time->tm_sec < 0) {
            time->tm_sec += 60;
            time->tm_min--;
        }
    }

    if (time->tm_min < 0 || time->tm_min >= 60) {
        time->tm_hour += time->tm_min / 60;
        time->tm_min %= 60;
        if (time->tm_min < 0) {
            time->tm_min += 60;
            time->tm_hour--;
        }
    }

    if (time->tm_hour < 0) {
        /* Decrement mday, yday, and wday */
        time->tm_hour += 24;
        time->tm_mday--;
        time->tm_yday--;
        if (time->tm_mday < 1) {
            time->tm_month--;
            if (time->tm_month < 0) {
                time->tm_month = 11;
                time->tm_year--;
                if (IsLeapYear(time->tm_year))
                    time->tm_yday = 365;
                else
                    time->tm_yday = 364;
            }
            time->tm_mday = nDays[IsLeapYear(time->tm_year)][time->tm_month];
        }
        time->tm_wday--;
        if (time->tm_wday < 0)
            time->tm_wday = 6;
    } else if (time->tm_hour > 23) {
        /* Increment mday, yday, and wday */
        time->tm_hour -= 24;
        time->tm_mday++;
        time->tm_yday++;
        if (time->tm_mday >
                nDays[IsLeapYear(time->tm_year)][time->tm_month]) {
            time->tm_mday = 1;
            time->tm_month++;
            if (time->tm_month > 11) {
                time->tm_month = 0;
                time->tm_year++;
                time->tm_yday = 0;
            }
        }
        time->tm_wday++;
        if (time->tm_wday > 6)
            time->tm_wday = 0;
    }
}

PR_IMPLEMENT(void)
PR_NormalizeTime(PRExplodedTime *time, PRTimeParamFn params)
{
    int daysInMonth;
    PRInt32 numDays;

    /* Get back to GMT */
    time->tm_sec -= time->tm_params.tp_gmt_offset
            + time->tm_params.tp_dst_offset;
    time->tm_params.tp_gmt_offset = 0;
    time->tm_params.tp_dst_offset = 0;

    /* Now normalize GMT */

    if (time->tm_usec < 0 || time->tm_usec >= 1000000) {
        time->tm_sec +=  time->tm_usec / 1000000;
        time->tm_usec %= 1000000;
        if (time->tm_usec < 0) {
            time->tm_usec += 1000000;
            time->tm_sec--;
        }
    }

    /* Note that we do not count leap seconds in this implementation */
    if (time->tm_sec < 0 || time->tm_sec >= 60) {
        time->tm_min += time->tm_sec / 60;
        time->tm_sec %= 60;
        if (time->tm_sec < 0) {
            time->tm_sec += 60;
            time->tm_min--;
        }
    }

    if (time->tm_min < 0 || time->tm_min >= 60) {
        time->tm_hour += time->tm_min / 60;
        time->tm_min %= 60;
        if (time->tm_min < 0) {
            time->tm_min += 60;
            time->tm_hour--;
        }
    }

    if (time->tm_hour < 0 || time->tm_hour >= 24) {
        time->tm_mday += time->tm_hour / 24;
        time->tm_hour %= 24;
        if (time->tm_hour < 0) {
            time->tm_hour += 24;
            time->tm_mday--;
        }
    }

    /* Normalize month and year before mday */
    if (time->tm_month < 0 || time->tm_month >= 12) {
        time->tm_year += time->tm_month / 12;
        time->tm_month %= 12;
        if (time->tm_month < 0) {
            time->tm_month += 12;
            time->tm_year--;
        }
    }

    /* Now that month and year are in proper range, normalize mday */

    if (time->tm_mday < 1) {
        /* mday too small */
        do {
            /* the previous month */
            time->tm_month--;
            if (time->tm_month < 0) {
                time->tm_month = 11;
                time->tm_year--;
            }
            time->tm_mday += nDays[IsLeapYear(time->tm_year)][time->tm_month];
        } while (time->tm_mday < 1);
    } else {
        daysInMonth = nDays[IsLeapYear(time->tm_year)][time->tm_month];
        while (time->tm_mday > daysInMonth) {
            /* mday too large */
            time->tm_mday -= daysInMonth;
            time->tm_month++;
            if (time->tm_month > 11) {
                time->tm_month = 0;
                time->tm_year++;
            }
            daysInMonth = nDays[IsLeapYear(time->tm_year)][time->tm_month];
        }
    }

    /* Recompute yday and wday */
    time->tm_yday = time->tm_mday +
            lastDayOfMonth[IsLeapYear(time->tm_year)][time->tm_month];
	    
    numDays = DAYS_BETWEEN_YEARS(1970, time->tm_year) + time->tm_yday;
    time->tm_wday = (numDays + 4) % 7;
    if (time->tm_wday < 0) {
        time->tm_wday += 7;
    }

    /* Recompute time parameters */

    time->tm_params = params(time);

    ApplySecOffset(time, time->tm_params.tp_gmt_offset
            + time->tm_params.tp_dst_offset);
}


/*
 *-------------------------------------------------------------------------
 *
 * PR_LocalTimeParameters --
 * 
 *     returns the time parameters for the local time zone
 *
 *     The following uses localtime() from the standard C library.
 *     (time.h)  This is our fallback implementation.  Unix and PC
 *     use this version.  Mac has its own machine-dependent
 *     implementation of this function.
 *
 *-------------------------------------------------------------------------
 */

#include <time.h>

#if defined(HAVE_INT_LOCALTIME_R)

/*
 * In this case we could define the macro as
 *     #define MT_safe_localtime(timer, result) \
 *             (localtime_r(timer, result) == 0 ? result : NULL)
 * I chose to compare the return value of localtime_r with -1 so 
 * that I can catch the cases where localtime_r returns a pointer
 * to struct tm.  The macro definition above would not be able to
 * detect such mistakes because it is legal to compare a pointer
 * with 0.
 */

#define MT_safe_localtime(timer, result) \
        (localtime_r(timer, result) == -1 ? NULL: result)

#elif defined(HAVE_POINTER_LOCALTIME_R)

#define MT_safe_localtime localtime_r

#else

#if defined(XP_MAC)
extern struct tm *Maclocaltime(const time_t * t);
#endif

static PRLock *monitor = NULL;

static struct tm *MT_safe_localtime(const time_t *clock, struct tm *result)
{
    struct tm *tmPtr;
    int needLock = PR_Initialized();  /* We need to use a lock to protect
                                       * against NSPR threads only when the
                                       * NSPR thread system is activated. */

    if (needLock) {
        if (monitor == NULL) {
            monitor = PR_NewLock();
        }
        PR_Lock(monitor);
    }

    /*
     * Microsoft (all flavors) localtime() returns a NULL pointer if 'clock'
     * represents a time before midnight January 1, 1970.  In
     * that case, we also return a NULL pointer and the struct tm
     * object pointed to by 'result' is not modified.
     *
     * Watcom C/C++ 11.0 localtime() treats time_t as unsigned long
     * hence, does not recognize negative values of clock as pre-1/1/70.
     * We have to manually check (WIN16 only) for negative value of
     * clock and return NULL.
     *
     * With negative values of clock, emx returns the struct tm for
     * clock plus ULONG_MAX. So we also have to check for the invalid
     * structs returned for timezones west of Greenwich when clock == 0.
     */
    
#if defined(XP_MAC)
    tmPtr = Maclocaltime(clock);
#else
    tmPtr = localtime(clock);
#endif

#if defined(WIN16) || defined(XP_OS2_EMX)
    if ( (PRInt32) *clock < 0 ||
         ( (PRInt32) *clock == 0 && tmPtr->tm_year != 70))
        result = NULL;
    else
        *result = *tmPtr;
#else
    if (tmPtr) {
        *result = *tmPtr;
    } else {
        result = NULL;
    }
#endif /* WIN16 */

    if (needLock) PR_Unlock(monitor);

    return result;
}

#endif  /* definition of MT_safe_localtime() */

#if defined(XP_UNIX) || defined(XP_PC) || defined(XP_BEOS)

PR_IMPLEMENT(PRTimeParameters)
PR_LocalTimeParameters(const PRExplodedTime *gmt)
{

    PRTimeParameters retVal;
    struct tm localTime;
    time_t secs;
    PRTime secs64;
    PRInt64 usecPerSec;
    PRInt64 usecPerSec_1;
    PRInt64 maxInt32;
    PRInt64 minInt32;
    PRInt32 dayOffset;
    PRInt32 offset2Jan1970;
    PRInt32 offsetNew;
    int isdst2Jan1970;

    /*
     * Calculate the GMT offset.  First, figure out what is
     * 00:00:00 Jan. 2, 1970 GMT (which is exactly a day, or 86400
     * seconds, since the epoch) in local time.  Then we calculate
     * the difference between local time and GMT in seconds:
     *     gmt_offset = local_time - GMT
     *
     * Caveat: the validity of this calculation depends on two
     * assumptions:
     * 1. Daylight saving time was not in effect on Jan. 2, 1970.
     * 2. The time zone of the geographic location has not changed
     *    since Jan. 2, 1970.
     */

    secs = 86400L;
    (void) MT_safe_localtime(&secs, &localTime);

    /* GMT is 00:00:00, 2nd of Jan. */

    offset2Jan1970 = (PRInt32)localTime.tm_sec 
            + 60L * (PRInt32)localTime.tm_min
            + 3600L * (PRInt32)localTime.tm_hour
            + 86400L * (PRInt32)((PRInt32)localTime.tm_mday - 2L);

    isdst2Jan1970 = localTime.tm_isdst;

    /*
     * Now compute DST offset.  We calculate the overall offset
     * of local time from GMT, similar to above.  The overall
     * offset has two components: gmt offset and dst offset.
     * We subtract gmt offset from the overall offset to get
     * the dst offset.
     *     overall_offset = local_time - GMT
     *     overall_offset = gmt_offset + dst_offset
     * ==> dst_offset = local_time - GMT - gmt_offset
     */

    secs64 = PR_ImplodeTime(gmt);    /* This is still in microseconds */
    LL_I2L(usecPerSec, PR_USEC_PER_SEC);
    LL_I2L(usecPerSec_1, PR_USEC_PER_SEC - 1);
    /* Convert to seconds, truncating down (3.1 -> 3 and -3.1 -> -4) */
    if (LL_GE_ZERO(secs64)) {
        LL_DIV(secs64, secs64, usecPerSec);
    } else {
        LL_NEG(secs64, secs64);
        LL_ADD(secs64, secs64, usecPerSec_1);
        LL_DIV(secs64, secs64, usecPerSec);
        LL_NEG(secs64, secs64);
    }
    LL_I2L(maxInt32, PR_INT32_MAX);
    LL_I2L(minInt32, PR_INT32_MIN);
    if (LL_CMP(secs64, >, maxInt32) || LL_CMP(secs64, <, minInt32)) {
        /* secs64 is too large or too small for time_t (32-bit integer) */
        retVal.tp_gmt_offset = offset2Jan1970;
        retVal.tp_dst_offset = 0;
        return retVal;
    }
    LL_L2I(secs, secs64);

    /*
     * On Windows, localtime() (and our MT_safe_localtime() too)
     * returns a NULL pointer for time before midnight January 1,
     * 1970 GMT.  In that case, we just use the GMT offset for
     * Jan 2, 1970 and assume that DST was not in effect.
     */

    if (MT_safe_localtime(&secs, &localTime) == NULL) {
        retVal.tp_gmt_offset = offset2Jan1970;
        retVal.tp_dst_offset = 0;
        return retVal;
    }

    /*
     * dayOffset is the offset between local time and GMT in 
     * the day component, which can only be -1, 0, or 1.  We
     * use the day of the week to compute dayOffset.
     */

    dayOffset = (PRInt32) localTime.tm_wday - gmt->tm_wday;

    /*
     * Need to adjust for wrapping around of day of the week from
     * 6 back to 0.
     */

    if (dayOffset == -6) {
        /* Local time is Sunday (0) and GMT is Saturday (6) */
        dayOffset = 1;
    } else if (dayOffset == 6) {
        /* Local time is Saturday (6) and GMT is Sunday (0) */
        dayOffset = -1;
    }

    offsetNew = (PRInt32)localTime.tm_sec - gmt->tm_sec
            + 60L * ((PRInt32)localTime.tm_min - gmt->tm_min)
            + 3600L * ((PRInt32)localTime.tm_hour - gmt->tm_hour)
            + 86400L * (PRInt32)dayOffset;

    if (localTime.tm_isdst <= 0) {
        /* DST is not in effect */
        retVal.tp_gmt_offset = offsetNew;
        retVal.tp_dst_offset = 0;
    } else {
        /* DST is in effect */
        if (isdst2Jan1970 <=0) {
            /*
             * DST was not in effect back in 2 Jan. 1970.
             * Use the offset back then as the GMT offset,
             * assuming the time zone has not changed since then.
             */
            retVal.tp_gmt_offset = offset2Jan1970;
            retVal.tp_dst_offset = offsetNew - offset2Jan1970;
        } else {
            /*
             * DST was also in effect back in 2 Jan. 1970.
             * Then our clever trick (or rather, ugly hack) fails.
             * We will just assume DST offset is an hour.
             */
            retVal.tp_gmt_offset = offsetNew - 3600;
            retVal.tp_dst_offset = 3600;
        }
    }
    
    return retVal;
}

#endif    /* defined(XP_UNIX) !! defined(XP_PC) */

/*
 *------------------------------------------------------------------------
 *
 * PR_USPacificTimeParameters --
 *
 *     The time parameters function for the US Pacific Time Zone.
 *
 *------------------------------------------------------------------------
 */

PR_IMPLEMENT(PRTimeParameters)
PR_USPacificTimeParameters(const PRExplodedTime *gmt)
{
    PRTimeParameters retVal;
    PRExplodedTime st;

    /*
     * Based on geographic location and GMT, figure out offset of
     * standard time from GMT.  In this example implementation, we
     * assume the local time zone is US Pacific Time.
     */

    retVal.tp_gmt_offset = -8L * 3600L;

    /*
     * Make a copy of GMT.  Note that the tm_params field of this copy
     * is ignored.
     */

    st.tm_usec = gmt->tm_usec;
    st.tm_sec = gmt->tm_sec;
    st.tm_min = gmt->tm_min;
    st.tm_hour = gmt->tm_hour;
    st.tm_mday = gmt->tm_mday;
    st.tm_month = gmt->tm_month;
    st.tm_year = gmt->tm_year;
    st.tm_wday = gmt->tm_wday;
    st.tm_yday = gmt->tm_yday;

    /* Apply the offset to GMT to obtain the local standard time */
    ApplySecOffset(&st, retVal.tp_gmt_offset);

    /*
     * Apply the rules on standard time or GMT to obtain daylight saving
     * time offset.  In this implementation, we use the US DST rule.
     */
    if (st.tm_month < 3) {
        retVal.tp_dst_offset = 0L;
    } else if (st.tm_month == 3) {
        if (st.tm_wday == 0) {
            /* A Sunday */
            if (st.tm_mday <= 7) {
                /* First Sunday */
                /* 01:59:59 PST -> 03:00:00 PDT */
                if (st.tm_hour < 2) {
                    retVal.tp_dst_offset = 0L;
                } else {
                    retVal.tp_dst_offset = 3600L;
                }
            } else {
                /* Not first Sunday */
                retVal.tp_dst_offset = 3600L;
            }
        } else {
            /* Not a Sunday.  See if before first Sunday or after */
            if (st.tm_wday + 1 <= st.tm_mday) {
                /* After first Sunday */
                retVal.tp_dst_offset = 3600L;
            } else {
                /* Before first Sunday */
                retVal.tp_dst_offset = 0L;
            } 
        }
    } else if (st.tm_month < 9) {
        retVal.tp_dst_offset = 3600L;
    } else if (st.tm_month == 9) {
        if (st.tm_wday == 0) {
            if (31 - st.tm_mday < 7) {
                /* Last Sunday */
                /* 01:59:59 PDT -> 01:00:00 PST */
                if (st.tm_hour < 1) {
                    retVal.tp_dst_offset = 3600L;
                } else {
                    retVal.tp_dst_offset = 0L;
                }
            } else {
                /* Not last Sunday */
                retVal.tp_dst_offset = 3600L;
            }
        } else {
            /* See if before or after last Sunday */
            if (7 - st.tm_wday <= 31 - st.tm_mday) {
                /* before last Sunday */
                retVal.tp_dst_offset = 3600L;
            } else {
                retVal.tp_dst_offset = 0L;
            }
        }
    } else {
        retVal.tp_dst_offset = 0L;
    }
    return retVal;
}

/*
 *------------------------------------------------------------------------
 *
 * PR_GMTParameters --
 *
 *     Returns the PRTimeParameters for Greenwich Mean Time.
 *     Trivially, both the tp_gmt_offset and tp_dst_offset fields are 0.
 *
 *------------------------------------------------------------------------
 */

PR_IMPLEMENT(PRTimeParameters)
PR_GMTParameters(const PRExplodedTime *gmt)
{
#if defined(XP_MAC)
#pragma unused (gmt)
#endif

    PRTimeParameters retVal = { 0, 0 };
    return retVal;
}

/*
 * The following code implements PR_ParseTimeString().  It is based on
 * ns/lib/xp/xp_time.c, revision 1.25, by Jamie Zawinski <jwz@netscape.com>.
 */

/*
 * We only recognize the abbreviations of a small subset of time zones
 * in North America, Europe, and Japan.
 *
 * PST/PDT: Pacific Standard/Daylight Time
 * MST/MDT: Mountain Standard/Daylight Time
 * CST/CDT: Central Standard/Daylight Time
 * EST/EDT: Eastern Standard/Daylight Time
 * AST: Atlantic Standard Time
 * NST: Newfoundland Standard Time
 * GMT: Greenwich Mean Time
 * BST: British Summer Time
 * MET: Middle Europe Time
 * EET: Eastern Europe Time
 * JST: Japan Standard Time
 */

typedef enum
{
  TT_UNKNOWN,

  TT_SUN, TT_MON, TT_TUE, TT_WED, TT_THU, TT_FRI, TT_SAT,

  TT_JAN, TT_FEB, TT_MAR, TT_APR, TT_MAY, TT_JUN,
  TT_JUL, TT_AUG, TT_SEP, TT_OCT, TT_NOV, TT_DEC,

  TT_PST, TT_PDT, TT_MST, TT_MDT, TT_CST, TT_CDT, TT_EST, TT_EDT,
  TT_AST, TT_NST, TT_GMT, TT_BST, TT_MET, TT_EET, TT_JST
} TIME_TOKEN;

/*
 * This parses a time/date string into a PRTime
 * (microseconds after "1-Jan-1970 00:00:00 GMT").
 * It returns PR_SUCCESS on success, and PR_FAILURE
 * if the time/date string can't be parsed.
 *
 * Many formats are handled, including:
 *
 *   14 Apr 89 03:20:12
 *   14 Apr 89 03:20 GMT
 *   Fri, 17 Mar 89 4:01:33
 *   Fri, 17 Mar 89 4:01 GMT
 *   Mon Jan 16 16:12 PDT 1989
 *   Mon Jan 16 16:12 +0130 1989
 *   6 May 1992 16:41-JST (Wednesday)
 *   22-AUG-1993 10:59:12.82
 *   22-AUG-1993 10:59pm
 *   22-AUG-1993 12:59am
 *   22-AUG-1993 12:59 PM
 *   Friday, August 04, 1995 3:54 PM
 *   06/21/95 04:24:34 PM
 *   20/06/95 21:07
 *   95-06-08 19:32:48 EDT
 *
 * If the input string doesn't contain a description of the timezone,
 * we consult the `default_to_gmt' to decide whether the string should
 * be interpreted relative to the local time zone (PR_FALSE) or GMT (PR_TRUE).
 * The correct value for this argument depends on what standard specified
 * the time string which you are parsing.
 */

PR_IMPLEMENT(PRStatus)
PR_ParseTimeString(
        const char *string,
        PRBool default_to_gmt,
        PRTime *result)
{
  PRExplodedTime tm;
  TIME_TOKEN dotw = TT_UNKNOWN;
  TIME_TOKEN month = TT_UNKNOWN;
  TIME_TOKEN zone = TT_UNKNOWN;
  int zone_offset = -1;
  int date = -1;
  PRInt32 year = -1;
  int hour = -1;
  int min = -1;
  int sec = -1;

  const char *rest = string;

#ifdef DEBUG
  int iterations = 0;
#endif

  PR_ASSERT(string && result);
  if (!string || !result) return PR_FAILURE;

  while (*rest)
        {

#ifdef DEBUG
          if (iterations++ > 1000)
                {
                  PR_ASSERT(0);
                  return PR_FAILURE;
                }
#endif

          switch (*rest)
                {
                case 'a': case 'A':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'p' || rest[1] == 'P') &&
                          (rest[2] == 'r' || rest[2] == 'R'))
                        month = TT_APR;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 's') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_AST;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'g' || rest[2] == 'G'))
                        month = TT_AUG;
                  break;
                case 'b': case 'B':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 's' || rest[1] == 'S') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_BST;
                  break;
                case 'c': case 'C':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 'd' || rest[1] == 'D') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_CDT;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_CST;
                  break;
                case 'd': case 'D':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'e' || rest[1] == 'E') &&
                          (rest[2] == 'c' || rest[2] == 'C'))
                        month = TT_DEC;
                  break;
                case 'e': case 'E':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 'd' || rest[1] == 'D') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_EDT;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 'e' || rest[1] == 'E') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_EET;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_EST;
                  break;
                case 'f': case 'F':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'e' || rest[1] == 'E') &&
                          (rest[2] == 'b' || rest[2] == 'B'))
                        month = TT_FEB;
                  else if (dotw == TT_UNKNOWN &&
                                   (rest[1] == 'r' || rest[1] == 'R') &&
                                   (rest[2] == 'i' || rest[2] == 'I'))
                        dotw = TT_FRI;
                  break;
                case 'g': case 'G':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 'm' || rest[1] == 'M') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_GMT;
                  break;
                case 'j': case 'J':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'a' || rest[1] == 'A') &&
                          (rest[2] == 'n' || rest[2] == 'N'))
                        month = TT_JAN;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_JST;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'l' || rest[2] == 'L'))
                        month = TT_JUL;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'n' || rest[2] == 'N'))
                        month = TT_JUN;
                  break;
                case 'm': case 'M':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'a' || rest[1] == 'A') &&
                          (rest[2] == 'r' || rest[2] == 'R'))
                        month = TT_MAR;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'a' || rest[1] == 'A') &&
                                   (rest[2] == 'y' || rest[2] == 'Y'))
                        month = TT_MAY;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 'd' || rest[1] == 'D') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_MDT;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 'e' || rest[1] == 'E') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_MET;
                  else if (dotw == TT_UNKNOWN &&
                                   (rest[1] == 'o' || rest[1] == 'O') &&
                                   (rest[2] == 'n' || rest[2] == 'N'))
                        dotw = TT_MON;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_MST;
                  break;
                case 'n': case 'N':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'o' || rest[1] == 'O') &&
                          (rest[2] == 'v' || rest[2] == 'V'))
                        month = TT_NOV;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_NST;
                  break;
                case 'o': case 'O':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'c' || rest[1] == 'C') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        month = TT_OCT;
                  break;
                case 'p': case 'P':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 'd' || rest[1] == 'D') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_PDT;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_PST;
                  break;
                case 's': case 'S':
                  if (dotw == TT_UNKNOWN &&
                          (rest[1] == 'a' || rest[1] == 'A') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        dotw = TT_SAT;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'e' || rest[1] == 'E') &&
                                   (rest[2] == 'p' || rest[2] == 'P'))
                        month = TT_SEP;
                  else if (dotw == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'n' || rest[2] == 'N'))
                        dotw = TT_SUN;
                  break;
                case 't': case 'T':
                  if (dotw == TT_UNKNOWN &&
                          (rest[1] == 'h' || rest[1] == 'H') &&
                          (rest[2] == 'u' || rest[2] == 'U'))
                        dotw = TT_THU;
                  else if (dotw == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'e' || rest[2] == 'E'))
                        dotw = TT_TUE;
                  break;
                case 'u': case 'U':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 't' || rest[1] == 'T') &&
                          !(rest[2] >= 'A' && rest[2] <= 'Z') &&
                          !(rest[2] >= 'a' && rest[2] <= 'z'))
                        /* UT is the same as GMT but UTx is not. */
                        zone = TT_GMT;
                  break;
                case 'w': case 'W':
                  if (dotw == TT_UNKNOWN &&
                          (rest[1] == 'e' || rest[1] == 'E') &&
                          (rest[2] == 'd' || rest[2] == 'D'))
                        dotw = TT_WED;
                  break;

                case '+': case '-':
                  {
                        const char *end;
                        int sign;
                        if (zone_offset != -1)
                          {
                                /* already got one... */
                                rest++;
                                break;
                          }
                        if (zone != TT_UNKNOWN && zone != TT_GMT)
                          {
                                /* GMT+0300 is legal, but PST+0300 is not. */
                                rest++;
                                break;
                          }

                        sign = ((*rest == '+') ? 1 : -1);
                        rest++; /* move over sign */
                        end = rest;
                        while (*end >= '0' && *end <= '9')
                          end++;
                        if (rest == end) /* no digits here */
                          break;

                        if ((end - rest) == 4)
                          /* offset in HHMM */
                          zone_offset = (((((rest[0]-'0')*10) + (rest[1]-'0')) * 60) +
                                                         (((rest[2]-'0')*10) + (rest[3]-'0')));
                        else if ((end - rest) == 2)
                          /* offset in hours */
                          zone_offset = (((rest[0]-'0')*10) + (rest[1]-'0')) * 60;
                        else if ((end - rest) == 1)
                          /* offset in hours */
                          zone_offset = (rest[0]-'0') * 60;
                        else
                          /* 3 or >4 */
                          break;

                        zone_offset *= sign;
                        zone = TT_GMT;
                        break;
                  }

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                  {
                        int tmp_hour = -1;
                        int tmp_min = -1;
                        int tmp_sec = -1;
                        const char *end = rest + 1;
                        while (*end >= '0' && *end <= '9')
                          end++;

                        /* end is now the first character after a range of digits. */

                        if (*end == ':')
                          {
                                if (hour >= 0 && min >= 0) /* already got it */
                                  break;

                                /* We have seen "[0-9]+:", so this is probably HH:MM[:SS] */
                                if ((end - rest) > 2)
                                  /* it is [0-9][0-9][0-9]+: */
                                  break;
                                else if ((end - rest) == 2)
                                  tmp_hour = ((rest[0]-'0')*10 +
                                                          (rest[1]-'0'));
                                else
                                  tmp_hour = (rest[0]-'0');

                                while (*rest && *rest != ':')
                                  rest++;
                                rest++;

                                /* move over the colon, and parse minutes */

                                end = rest + 1;
                                while (*end >= '0' && *end <= '9')
                                  end++;

                                if (end == rest)
                                  /* no digits after first colon? */
                                  break;
                                else if ((end - rest) > 2)
                                  /* it is [0-9][0-9][0-9]+: */
                                  break;
                                else if ((end - rest) == 2)
                                  tmp_min = ((rest[0]-'0')*10 +
                                                         (rest[1]-'0'));
                                else
                                  tmp_min = (rest[0]-'0');

                                /* now go for seconds */
                                rest = end;
                                if (*rest == ':')
                                  rest++;
                                end = rest;
                                while (*end >= '0' && *end <= '9')
                                  end++;

                                if (end == rest)
                                  /* no digits after second colon - that's ok. */
                                  ;
                                else if ((end - rest) > 2)
                                  /* it is [0-9][0-9][0-9]+: */
                                  break;
                                else if ((end - rest) == 2)
                                  tmp_sec = ((rest[0]-'0')*10 +
                                                         (rest[1]-'0'));
                                else
                                  tmp_sec = (rest[0]-'0');

                                /* If we made it here, we've parsed hour and min,
                                   and possibly sec, so it worked as a unit. */

                                /* skip over whitespace and see if there's an AM or PM
                                   directly following the time.
                                 */
                                if (tmp_hour <= 12)
                                  {
                                        const char *s = end;
                                        while (*s && (*s == ' ' || *s == '\t'))
                                          s++;
                                        if ((s[0] == 'p' || s[0] == 'P') &&
                                                (s[1] == 'm' || s[1] == 'M'))
                                          /* 10:05pm == 22:05, and 12:05pm == 12:05 */
                                          tmp_hour = (tmp_hour == 12 ? 12 : tmp_hour + 12);
                                        else if (tmp_hour == 12 &&
                                                         (s[0] == 'a' || s[0] == 'A') &&
                                                         (s[1] == 'm' || s[1] == 'M'))
                                          /* 12:05am == 00:05 */
                                          tmp_hour = 0;
                                  }

                                hour = tmp_hour;
                                min = tmp_min;
                                sec = tmp_sec;
                                rest = end;
                                break;
                          }
                        else if ((*end == '/' || *end == '-') &&
                                         end[1] >= '0' && end[1] <= '9')
                          {
                                /* Perhaps this is 6/16/95, 16/6/95, 6-16-95, or 16-6-95
                                   or even 95-06-05...
                                   #### But it doesn't handle 1995-06-22.
                                 */
                                int n1, n2, n3;
                                const char *s;

                                if (month != TT_UNKNOWN)
                                  /* if we saw a month name, this can't be. */
                                  break;

                                s = rest;

                                n1 = (*s++ - '0');                                /* first 1 or 2 digits */
                                if (*s >= '0' && *s <= '9')
                                  n1 = n1*10 + (*s++ - '0');

                                if (*s != '/' && *s != '-')                /* slash */
                                  break;
                                s++;

                                if (*s < '0' || *s > '9')                /* second 1 or 2 digits */
                                  break;
                                n2 = (*s++ - '0');
                                if (*s >= '0' && *s <= '9')
                                  n2 = n2*10 + (*s++ - '0');

                                if (*s != '/' && *s != '-')                /* slash */
                                  break;
                                s++;

                                if (*s < '0' || *s > '9')                /* third 1, 2, or 4 digits */
                                  break;
                                n3 = (*s++ - '0');
                                if (*s >= '0' && *s <= '9')
                                  n3 = n3*10 + (*s++ - '0');

                                if (*s >= '0' && *s <= '9')                /* optional digits 3 and 4 */
                                  {
                                        n3 = n3*10 + (*s++ - '0');
                                        if (*s < '0' || *s > '9')
                                          break;
                                        n3 = n3*10 + (*s++ - '0');
                                  }

                                if ((*s >= '0' && *s <= '9') ||        /* followed by non-alphanum */
                                        (*s >= 'A' && *s <= 'Z') ||
                                        (*s >= 'a' && *s <= 'z'))
                                  break;

                                /* Ok, we parsed three 1-2 digit numbers, with / or -
                                   between them.  Now decide what the hell they are
                                   (DD/MM/YY or MM/DD/YY or YY/MM/DD.)
                                 */

                                if (n1 > 31 || n1 == 0)  /* must be YY/MM/DD */
                                  {
                                        if (n2 > 12) break;
                                        if (n3 > 31) break;
                                        year = n1;
                                        if (year < 70)
                                            year += 2000;
                                        else if (year < 100)
                                            year += 1900;
                                        month = (TIME_TOKEN)(n2 + ((int)TT_JAN) - 1);
                                        date = n3;
                                        rest = s;
                                        break;
                                  }

                                if (n1 > 12 && n2 > 12)  /* illegal */
                                  {
                                        rest = s;
                                        break;
                                  }

                                if (n3 < 70)
                                    n3 += 2000;
                                else if (n3 < 100)
                                    n3 += 1900;

                                if (n1 > 12)  /* must be DD/MM/YY */
                                  {
                                        date = n1;
                                        month = (TIME_TOKEN)(n2 + ((int)TT_JAN) - 1);
                                        year = n3;
                                  }
                                else                  /* assume MM/DD/YY */
                                  {
                                        /* #### In the ambiguous case, should we consult the
                                           locale to find out the local default? */
                                        month = (TIME_TOKEN)(n1 + ((int)TT_JAN) - 1);
                                        date = n2;
                                        year = n3;
                                  }
                                rest = s;
                          }
                        else if ((*end >= 'A' && *end <= 'Z') ||
                                         (*end >= 'a' && *end <= 'z'))
                          /* Digits followed by non-punctuation - what's that? */
                          ;
                        else if ((end - rest) == 4)                /* four digits is a year */
                          year = (year < 0
                                          ? ((rest[0]-'0')*1000L +
                                                 (rest[1]-'0')*100L +
                                                 (rest[2]-'0')*10L +
                                                 (rest[3]-'0'))
                                          : year);
                        else if ((end - rest) == 2)                /* two digits - date or year */
                          {
                                int n = ((rest[0]-'0')*10 +
                                                 (rest[1]-'0'));
                                /* If we don't have a date (day of the month) and we see a number
                                     less than 32, then assume that is the date.

                                         Otherwise, if we have a date and not a year, assume this is the
                                         year.  If it is less than 70, then assume it refers to the 21st
                                         century.  If it is two digits (>= 70), assume it refers to this
                                         century.  Otherwise, assume it refers to an unambiguous year.

                                         The world will surely end soon.
                                   */
                                if (date < 0 && n < 32)
                                  date = n;
                                else if (year < 0)
                                  {
                                        if (n < 70)
                                          year = 2000 + n;
                                        else if (n < 100)
                                          year = 1900 + n;
                                        else
                                          year = n;
                                  }
                                /* else what the hell is this. */
                          }
                        else if ((end - rest) == 1)                /* one digit - date */
                          date = (date < 0 ? (rest[0]-'0') : date);
                        /* else, three or more than four digits - what's that? */

                        break;
                  }
                }

          /* Skip to the end of this token, whether we parsed it or not.
                 Tokens are delimited by whitespace, or ,;-/
                 But explicitly not :+-.
           */
          while (*rest &&
                         *rest != ' ' && *rest != '\t' &&
                         *rest != ',' && *rest != ';' &&
                         *rest != '-' && *rest != '+' &&
                         *rest != '/' &&
                         *rest != '(' && *rest != ')' && *rest != '[' && *rest != ']')
                rest++;
          /* skip over uninteresting chars. */
        SKIP_MORE:
          while (*rest &&
                         (*rest == ' ' || *rest == '\t' ||
                          *rest == ',' || *rest == ';' || *rest == '/' ||
                          *rest == '(' || *rest == ')' || *rest == '[' || *rest == ']'))
                rest++;

          /* "-" is ignored at the beginning of a token if we have not yet
                 parsed a year (e.g., the second "-" in "30-AUG-1966"), or if
                 the character after the dash is not a digit. */         
          if (*rest == '-' && ((rest > string && isalpha(rest[-1]) && year < 0)
              || rest[1] < '0' || rest[1] > '9'))
                {
                  rest++;
                  goto SKIP_MORE;
                }

        }

  if (zone != TT_UNKNOWN && zone_offset == -1)
        {
          switch (zone)
                {
                case TT_PST: zone_offset = -8 * 60; break;
                case TT_PDT: zone_offset = -7 * 60; break;
                case TT_MST: zone_offset = -7 * 60; break;
                case TT_MDT: zone_offset = -6 * 60; break;
                case TT_CST: zone_offset = -6 * 60; break;
                case TT_CDT: zone_offset = -5 * 60; break;
                case TT_EST: zone_offset = -5 * 60; break;
                case TT_EDT: zone_offset = -4 * 60; break;
                case TT_AST: zone_offset = -4 * 60; break;
                case TT_NST: zone_offset = -3 * 60 - 30; break;
                case TT_GMT: zone_offset =  0 * 60; break;
                case TT_BST: zone_offset =  1 * 60; break;
                case TT_MET: zone_offset =  1 * 60; break;
                case TT_EET: zone_offset =  2 * 60; break;
                case TT_JST: zone_offset =  9 * 60; break;
                default:
                  PR_ASSERT (0);
                  break;
                }
        }

  /* If we didn't find a year, month, or day-of-the-month, we can't
         possibly parse this, and in fact, mktime() will do something random
         (I'm seeing it return "Tue Feb  5 06:28:16 2036", which is no doubt
         a numerologically significant date... */
  if (month == TT_UNKNOWN || date == -1 || year == -1)
      return PR_FAILURE;

  memset(&tm, 0, sizeof(tm));
  if (sec != -1)
        tm.tm_sec = sec;
  if (min != -1)
  tm.tm_min = min;
  if (hour != -1)
        tm.tm_hour = hour;
  if (date != -1)
        tm.tm_mday = date;
  if (month != TT_UNKNOWN)
        tm.tm_month = (((int)month) - ((int)TT_JAN));
  if (year != -1)
        tm.tm_year = year;
  if (dotw != TT_UNKNOWN)
        tm.tm_wday = (((int)dotw) - ((int)TT_SUN));

  if (zone == TT_UNKNOWN && default_to_gmt)
        {
          /* No zone was specified, so pretend the zone was GMT. */
          zone = TT_GMT;
          zone_offset = 0;
        }

  if (zone_offset == -1)
         {
           /* no zone was specified, and we're to assume that everything
             is local. */
          struct tm localTime;
          time_t secs;

          PR_ASSERT(tm.tm_month > -1 
                                   && tm.tm_mday > 0 
                                   && tm.tm_hour > -1
                                   && tm.tm_min > -1
                                   && tm.tm_sec > -1);

            /*
             * To obtain time_t from a tm structure representing the local
             * time, we call mktime().  However, we need to see if we are
             * on 1-Jan-1970 or before.  If we are, we can't call mktime()
             * because mktime() will crash on win16. In that case, we
             * calculate zone_offset based on the zone offset at 
             * 00:00:00, 2 Jan 1970 GMT, and subtract zone_offset from the
             * date we are parsing to transform the date to GMT.  We also
             * do so if mktime() returns (time_t) -1 (time out of range).
           */

          /* month, day, hours, mins and secs are always non-negative
             so we dont need to worry about them. */  
            if(tm.tm_year >= 1970)
                {
                  PRInt64 usec_per_sec;

                  localTime.tm_sec = tm.tm_sec;
                  localTime.tm_min = tm.tm_min;
                  localTime.tm_hour = tm.tm_hour;
                  localTime.tm_mday = tm.tm_mday;
                  localTime.tm_mon = tm.tm_month;
                  localTime.tm_year = tm.tm_year - 1900;
                  /* Set this to -1 to tell mktime "I don't care".  If you set
                     it to 0 or 1, you are making assertions about whether the
                     date you are handing it is in daylight savings mode or not;
                     and if you're wrong, it will "fix" it for you. */
                  localTime.tm_isdst = -1;
                  secs = mktime(&localTime);
                  if (secs != (time_t) -1)
                    {
#if defined(XP_MAC) && (__MSL__ < 0x6000)
                      /*
                       * The mktime() routine in MetroWerks MSL C
                       * Runtime library returns seconds since midnight,
                       * 1 Jan. 1900, not 1970 - in versions of MSL (Metrowerks Standard
                       * Library) prior to version 6.  Only for older versions of
                       * MSL do we adjust the value of secs to the NSPR epoch
                       */
                      secs -= ((365 * 70UL) + 17) * 24 * 60 * 60;
#endif
                      LL_I2L(*result, secs);
                      LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
                      LL_MUL(*result, *result, usec_per_sec);
                      return PR_SUCCESS;
                    }
                }
                
                /* So mktime() can't handle this case.  We assume the
                   zone_offset for the date we are parsing is the same as
                   the zone offset on 00:00:00 2 Jan 1970 GMT. */
                secs = 86400;
                (void) MT_safe_localtime(&secs, &localTime);
                zone_offset = localTime.tm_min
                              + 60 * localTime.tm_hour
                              + 1440 * (localTime.tm_mday - 2);
        }

        /* Adjust the hours and minutes before handing them to
           PR_ImplodeTime(). Note that it's ok for them to be <0 or >24/60 

           We adjust the time to GMT before going into PR_ImplodeTime().
           The zone_offset represents the difference between the time
           zone parsed and GMT
         */
        tm.tm_hour -= (zone_offset / 60);
        tm.tm_min  -= (zone_offset % 60);

  *result = PR_ImplodeTime(&tm);

  return PR_SUCCESS;
}

/*
 *******************************************************************
 *******************************************************************
 **
 **    OLD COMPATIBILITY FUNCTIONS
 **
 *******************************************************************
 *******************************************************************
 */


/*
 *-----------------------------------------------------------------------
 *
 * PR_FormatTime --
 *
 *     Format a time value into a buffer. Same semantics as strftime().
 *
 *-----------------------------------------------------------------------
 */

PR_IMPLEMENT(PRUint32)
PR_FormatTime(char *buf, int buflen, const char *fmt, const PRExplodedTime *tm)
{
    struct tm a;
    a.tm_sec = tm->tm_sec;
    a.tm_min = tm->tm_min;
    a.tm_hour = tm->tm_hour;
    a.tm_mday = tm->tm_mday;
    a.tm_mon = tm->tm_month;
    a.tm_wday = tm->tm_wday;
    a.tm_year = tm->tm_year - 1900;
    a.tm_yday = tm->tm_yday;
    a.tm_isdst = tm->tm_params.tp_dst_offset ? 1 : 0;

/*
 * On some platforms, for example SunOS 4, struct tm has two additional
 * fields: tm_zone and tm_gmtoff.
 */

#if defined(SUNOS4) || (__GLIBC__ >= 2) || defined(XP_BEOS) \
        || defined(NETBSD) || defined(OPENBSD) || defined(FREEBSD) \
        || defined(DARWIN)
    a.tm_zone = NULL;
    a.tm_gmtoff = tm->tm_params.tp_gmt_offset + tm->tm_params.tp_dst_offset;
#endif

    return strftime(buf, buflen, fmt, &a);
}


/*
 * The following string arrays and macros are used by PR_FormatTimeUSEnglish().
 */

static const char* abbrevDays[] =
{
   "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

static const char* days[] =
{
   "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

static const char* abbrevMonths[] =
{
   "Jan", "Feb", "Mar", "Apr", "May", "Jun",
   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char* months[] =
{ 
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};


/*
 * Add a single character to the given buffer, incrementing the buffer pointer
 * and decrementing the buffer size. Return 0 on error.
 */
#define ADDCHAR( buf, bufSize, ch )             \
do                                              \
{                                               \
   if( bufSize < 1 )                            \
   {                                            \
      *(--buf) = '\0';                          \
      return 0;                                 \
   }                                            \
   *buf++ = ch;                                 \
   bufSize--;                                   \
}                                               \
while(0)


/*
 * Add a string to the given buffer, incrementing the buffer pointer
 * and decrementing the buffer size appropriately.  Return 0 on error.
 */
#define ADDSTR( buf, bufSize, str )             \
do                                              \
{                                               \
   PRUint32 strSize = strlen( str );              \
   if( strSize > bufSize )                      \
   {                                            \
      if( bufSize==0 )                          \
         *(--buf) = '\0';                       \
      else                                      \
         *buf = '\0';                           \
      return 0;                                 \
   }                                            \
   memcpy(buf, str, strSize);                   \
   buf += strSize;                              \
   bufSize -= strSize;                          \
}                                               \
while(0)

/* Needed by PR_FormatTimeUSEnglish() */
static unsigned int  pr_WeekOfYear(const PRExplodedTime* time,
        unsigned int firstDayOfWeek);


/***********************************************************************************
 *
 * Description:
 *  This is a dumbed down version of strftime that will format the date in US
 *  English regardless of the setting of the global locale.  This functionality is
 *  needed to write things like MIME headers which must always be in US English.
 *
 **********************************************************************************/
             
PR_IMPLEMENT(PRUint32)
PR_FormatTimeUSEnglish( char* buf, PRUint32 bufSize,
                        const char* format, const PRExplodedTime* time )
{
   char*         bufPtr = buf;
   const char*   fmtPtr;
   char          tmpBuf[ 40 ];        
   const int     tmpBufSize = sizeof( tmpBuf );

   
   for( fmtPtr=format; *fmtPtr != '\0'; fmtPtr++ )
   {
      if( *fmtPtr != '%' )
      {
         ADDCHAR( bufPtr, bufSize, *fmtPtr );
      }
      else
      {
         switch( *(++fmtPtr) )
         {
         case '%':
            /* escaped '%' character */
            ADDCHAR( bufPtr, bufSize, '%' );
            break;
            
         case 'a':
            /* abbreviated weekday name */
            ADDSTR( bufPtr, bufSize, abbrevDays[ time->tm_wday ] );
            break;
               
         case 'A':
            /* full weekday name */
            ADDSTR( bufPtr, bufSize, days[ time->tm_wday ] );
            break;
        
         case 'b':
            /* abbreviated month name */
            ADDSTR( bufPtr, bufSize, abbrevMonths[ time->tm_month ] );
            break;
        
         case 'B':
            /* full month name */
            ADDSTR(bufPtr, bufSize,  months[ time->tm_month ] );
            break;
        
         case 'c':
            /* Date and time. */
            PR_FormatTimeUSEnglish( tmpBuf, tmpBufSize, "%a %b %d %H:%M:%S %Y", time );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;
        
         case 'd':
            /* day of month ( 01 - 31 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2ld",time->tm_mday );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;

         case 'H':
            /* hour ( 00 - 23 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2ld",time->tm_hour );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
        
         case 'I':
            /* hour ( 01 - 12 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2ld",
                        (time->tm_hour%12) ? time->tm_hour%12 : (PRInt32) 12 );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
        
         case 'j':
            /* day number of year ( 001 - 366 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.3d",time->tm_yday + 1);
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
        
         case 'm':
            /* month number ( 01 - 12 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2ld",time->tm_month+1);
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
        
         case 'M':
            /* minute ( 00 - 59 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2ld",time->tm_min );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
       
         case 'p':
            /* locale's equivalent of either AM or PM */
            ADDSTR( bufPtr, bufSize, (time->tm_hour<12)?"AM":"PM" ); 
            break;
        
         case 'S':
            /* seconds ( 00 - 61 ), allows for leap seconds */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2ld",time->tm_sec );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
     
         case 'U':
            /* week number of year ( 00 - 53  ),  Sunday  is  the first day of week 1 */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d", pr_WeekOfYear( time, 0 ) );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;
        
         case 'w':
            /* weekday number ( 0 - 6 ), Sunday = 0 */
            PR_snprintf(tmpBuf,tmpBufSize,"%d",time->tm_wday );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
        
         case 'W':
            /* Week number of year ( 00 - 53  ),  Monday  is  the first day of week 1 */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d", pr_WeekOfYear( time, 1 ) );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;
        
         case 'x':
            /* Date representation */
            PR_FormatTimeUSEnglish( tmpBuf, tmpBufSize, "%m/%d/%y", time );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;
        
         case 'X':
            /* Time representation. */
            PR_FormatTimeUSEnglish( tmpBuf, tmpBufSize, "%H:%M:%S", time );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;
        
         case 'y':
            /* year within century ( 00 - 99 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d",time->tm_year % 100 );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
        
         case 'Y':
            /* year as ccyy ( for example 1986 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.4d",time->tm_year );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;
        
         case 'Z':
            /* Time zone name or no characters if  no  time  zone exists.
             * Since time zone name is supposed to be independant of locale, we
             * defer to PR_FormatTime() for this option.
             */
            PR_FormatTime( tmpBuf, tmpBufSize, "%Z", time );
            ADDSTR( bufPtr, bufSize, tmpBuf ); 
            break;

         default:
            /* Unknown format.  Simply copy format into output buffer. */
            ADDCHAR( bufPtr, bufSize, '%' );
            ADDCHAR( bufPtr, bufSize, *fmtPtr );
            break;
            
         }
      }
   }

   ADDCHAR( bufPtr, bufSize, '\0' );
   return (PRUint32)(bufPtr - buf - 1);
}



/***********************************************************************************
 *
 * Description:
 *  Returns the week number of the year (0-53) for the given time.  firstDayOfWeek
 *  is the day on which the week is considered to start (0=Sun, 1=Mon, ...).
 *  Week 1 starts the first time firstDayOfWeek occurs in the year.  In other words,
 *  a partial week at the start of the year is considered week 0.  
 *
 **********************************************************************************/

static unsigned int
pr_WeekOfYear(const PRExplodedTime* time, unsigned int firstDayOfWeek)
{
   int dayOfWeek;
   int dayOfYear;

  /* Get the day of the year for the given time then adjust it to represent the
   * first day of the week containing the given time.
   */
  dayOfWeek = time->tm_wday - firstDayOfWeek;
  if (dayOfWeek < 0)
    dayOfWeek += 7;
  
  dayOfYear = time->tm_yday - dayOfWeek;


  if( dayOfYear <= 0 )
  {
     /* If dayOfYear is <= 0, it is in the first partial week of the year. */
     return 0;
  }
  else
  {
     /* Count the number of full weeks ( dayOfYear / 7 ) then add a week if there
      * are any days left over ( dayOfYear % 7 ).  Because we are only counting to
      * the first day of the week containing the given time, rather than to the
      * actual day representing the given time, any days in week 0 will be "absorbed"
      * as extra days in the given week.
      */
     return (dayOfYear / 7) + ( (dayOfYear % 7) == 0 ? 0 : 1 );
  }
}

