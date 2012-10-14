/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TIME2_H
#define TIME2_H

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

// System specific includes

// End system specific

#define EPOCH_YEAR                  (1900)
#define EPOCH_SECONDS               (0x80000000)
#define DAYS_PER_LEAP_YEAR          (366)
#define DAYS_PER_YEAR               (365)
#define DAYS_PER_WEEK               (7)
#define LEAP_FOUR_CENTURY           (400)
#define LEAP_CENTURY                (100)
#define LEAP_YEAR                   (4)
// Calculated with 365.2425 days/year for the Gregorian calendar
#define SECONDS_PER_GREGORIAN_YEAR  (31556952)
#define SECONDS_PER_YEAR            (31536000)
#define SECONDS_PER_LEAP_YEAR       (31622400)
#define SECONDS_PER_DAY             (86400)
#define SECONDS_PER_HOUR            (3600)
#define SECONDS_PER_MINUTE          (60)
#define MINUTES_PER_HOUR            (60)
#define HOURS_PER_DAY               (24)
#define MAX_WEEKS_PER_MONTH         (6)
#define MONTHS_PER_YEAR             (12)

#define STARTING_TIME (101 * SECONDS_PER_GREGORIAN_YEAR)

//If 30 minutes have elapsed, clear time display
#define SNTP_MAX_TIME_SINCE_UPDATE 30*60

typedef enum {
    SUNDAY = 0, MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY
} DAYS_NAMES_TO_NUMBER;

typedef enum {
    JANUARY = 0,
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
    DECEMBER
} MONTH_NAMES_TO_NUMBER;

typedef struct {
    time_t         time;
    unsigned short millitm;
    short          timezone;
    short          dstflag;
} timeb;

typedef struct {
    // Standard Gregorian calendar date
    unsigned long year;         // Range >= EPOCH_YEAR
    unsigned long month;        // Range 0-11, use MONTH_NAME_x for text
    unsigned long day;          // Range 1-31

    // Standard 24 hour time format
    unsigned long hour;         // Range 0-23
    unsigned long minute;       // Range 0-59
    unsigned long seconds;      // Range 0-59

    // Specifics about a year and the current date
    unsigned long day_of_year;  // Range 0-365
    unsigned long day_of_january_first; // Range 0-6, use DAY_NAME_x for text
    unsigned long day_of_week;  // Range 0-6, use DAY_NAME_x for text
    unsigned long week_of_year; // Range 1-53

    // Variables that are associated with leap years and calculations
    unsigned long leap_years;   // Non negative integer
    unsigned long leap_flag;    // Range 0-1, boolean

    // Time zone specifics set by user
    long time_zone;                                   // Timezone offset in seconds
    unsigned long daylight_saving_offset;             // Number of seconds to add (adjust) when DST starts
                                                      //      e.g.  daylight_saving_offset = 3600; // 1 hour
                                                      //      when DST starts the code will add 1 hour to the GMT time and
                                                      //      when DST stops it will stop adding 1 hour
    unsigned long daylight_saving_start_month;        // Range 0-11, use MONTH_NAME_x for text
    unsigned long daylight_saving_start_day;          // Range 1-31, if set to 0, only then will week of month is used
    unsigned long daylight_saving_start_day_of_week;  // Range 0-6, use DAY_NAME_x for text
    unsigned long daylight_saving_start_week_of_month;// Range 0-6, First, second, etc. DAY_NAME of the month, zero denotes last week in month, e.g. day_of_week = 0, week_of_month = 1 => First Sunday of month
    unsigned long daylight_saving_start_time;         // Range 0-86399, number of seconds past midnight
    unsigned long daylight_saving_stop_month;         // Range 0-11, use MONTH_NAME_x for text
    unsigned long daylight_saving_stop_day;           // Range 1-31, if set to 0, only then will week of month is used
    unsigned long daylight_saving_stop_day_of_week;   // Range 0-6, use DAY_NAME_x for text
    unsigned long daylight_saving_stop_week_of_month; // Range 0-6, First, second, etc. DAY_NAME of the month, zero denotes last week in month, e.g. day_of_week = 0, week_of_month = 1 => First Sunday of month
    unsigned long daylight_saving_stop_time;          // Range 0-86399, number of seconds past midnight
    unsigned long daylight_saving_year_calc;          // Non negative integer, non 0 means daylight saving start and stop second is calculated for given year
    unsigned long daylight_saving_auto_adjust;        // Range 0-1, boolean

    // Time zone specifics set by make_date
    unsigned long is_daylight_saving;           // Range 0-1, boolean
    unsigned long daylight_saving_start_second; // Second in the year that daylight saving starts
    unsigned long daylight_saving_stop_second;  // Second in the year that daylight saving stops
} time2_s;

typedef struct {
    const char *TZ_NAME;
    const long offset;
} TZ_STRUCT;

// Time date externs
extern const char *const   MONTH_NAMES_LONG[];
extern const char *const   MONTH_NAMES_SHORT[];
extern const unsigned long DAYS_IN_MONTH[];
extern const char *const   DAY_NAMES_LONG[];
extern const char *const   DAY_NAMES_SHORT[];
extern const char *const   DAY_NAMES_ABBREV[];
extern const TZ_STRUCT     TZ_TABLE[];

long make_date(unsigned long secondsPastEpoch, time2_s *timeStruct);
const char *get_month_name_long(unsigned long month);
const char *get_month_name_short(unsigned long month);
const char *get_day_name_long(unsigned long day);
const char *get_day_name_short(unsigned long day);
const char *get_day_name_abrrev(unsigned long day);
long get_leap_years(unsigned long year);
unsigned long get_seconds_past_epoch(unsigned long nth, unsigned long day_name, unsigned long month, unsigned long year);
long is_leap_year(unsigned long year);
long range_check_time_struct(time2_s *ts);
unsigned long date_to_seconds(const time2_s *ts);
unsigned long time_to_seconds(const time2_s *ts);
unsigned long date_time_to_seconds(const time2_s *ts);
unsigned long get_local_time(void);
void get_precise_local_time(unsigned long *local_time);
long get_local_timezone(void);
long get_local_dst_active(void);
unsigned long time_to_short_string(const time2_s *ts, char *time_string);
unsigned long gmt_string_to_seconds(char *gmt_string, unsigned long *seconds);
unsigned long seconds_to_gmt_string(unsigned long seconds, char *gmt_string);
unsigned long diff_time(unsigned long t1, unsigned long t2);

// This function is similar to the strcmp function in the string.h library
// It includes support for handling the NTP most significant bit rollover
// If t1 < t2 cmp_time returns -1
// If t1 == t2 cmp_time returns 0
// If t1 > t2 cmp_time returns 1
long cmp_time(unsigned long t1, unsigned long t2);

long diff_current_time(unsigned long t1, unsigned long *difference);
long parse_month(char *pString);
long parse_day_of_week(char *pString);
long parse_timezone(char *pString);
long parse_time(char *pString);

/*
 * Formats ulMilliseconds into elasped time in the form of
 * HRS:MIN:SEC for less than 24 hours and then
 *  <n> DAY(S) HRS:MIN:SEC for greater than 24 hours
*/
void ascTimeDuration(unsigned long ulMilliseconds, char *ptimStr, int length);
void ascFormatDate(time2_s *ts, char * pDateStr, int length);
void ascFormatHrMinTime(time2_s *ts, char * pTimeStr, int length, char bFlashColon);
void FormatDateTime(char * ptimeStr, int length);
void FormatDebugTime(char *timeStr, int length,long ts);
char *GetDateTimeString(void);

// Called once a second to update the time. Returns true indicating
// a roll-over indicating the display needs updated.
// Once a minute we need to update the time/date display and
// update the minute/hour/day etc.
//
int TimeOfDayUpdate(void);

// Set the time of day clock
void SetTimeOfDay(time2_s *time);

// Blank "turn off" the display of time
// called when there is a change to
// the time keeping systems on the phone
void ClearTimeDisplay(void);

#endif
