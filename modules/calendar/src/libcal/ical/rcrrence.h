/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*- */
/* 
 * rcrrence.h
 * John Sun
 * 2/2/98 10:45:13 AM
 */

#ifndef __RECURRENCE_H_
#define __RECURRENCE_H_

#include "datetime.h"
#include "duration.h"
#include "rrday.h"
#include "ptrarray.h"
#include "jutility.h"
#include "dategntr.h"
#include "bymgntr.h"
#include "byhgntr.h"
#include "bywngntr.h"
#include "byydgntr.h"
#include "bymogntr.h"
#include "bymdgntr.h"
#include "bydygntr.h"
#include "bydwgntr.h"
#include "bydmgntr.h"
#include "deftgntr.h"
#include "jlog.h"

class Recurrence
{
    
public:
   
    /* moved the Recurrence Type constants to JulianUtility. */
    
    /**
    * Creates a Recurrence object specified by the given string.
    *
    * @param startDate	       the start time of the first recurrence
    * @param stopDate	       the end time of the first recurrence
    * @param duration	       the time between start and stopdate
    * @param ruleString        the recurrence rule string to parse
    */
    Recurrence(DateTime startDate, DateTime stopDate, Duration * duration, 
        UnicodeString & ruleString);
    
    /**
     * Creates a Recurrence object specified by the given string.
     *
     * @param startDate	       the start time of the first recurrence
     * @param duration	       the time between start and stopdate
     * @param ruleString       the recurrence rule string to parse
     */
    Recurrence(DateTime startDate, Duration * duration, UnicodeString & ruleString);

    /**
     * Creates a Recurrence object specified by the given string.
     *
     * @param startDate	       the start time of the first recurrence
     * @param ruleString       the recurrence rule string to parse
     */
    Recurrence(DateTime startDate, UnicodeString & ruleString);

    /**
     * Destructor
     */
    ~Recurrence();

    /**
     * Checks if recurrence rule is valid.  Validity is checked during parsing.
     *
     * @return          TRUE if valid rule, FALSE otherwise
     */
    t_bool isValid() const;


    /*static void toPatternOut(JulianPtrArray * srr, JulianPtrArray * ser,
        JulianPtrArray * srd, JulianPtrArray * sed);*/

    /**
     * The same functionality as enforce except it works with a vector of strings.
     * @param           startDate   start time of recurrence
     * @param           srr         vector of RRULE strings
     * @param           ser         vector of EXRULE strings
     * @param           srd         vector of RDATE strings
     * @param           sed         vector of EXDATE strings
     * @param           bound       number of dates to be produced
     * @param           out         vector containing generated dates
     *
     */
    static void stringEnforce(DateTime startDate, JulianPtrArray * srr, 
        JulianPtrArray * ser, JulianPtrArray * srd, JulianPtrArray * sed, 
        t_int32 bound, JulianPtrArray * out, JLog * log);

    
    /**
     * The enforce method handles two lists of recurrences which specifies rules for defining
     * occurrence dates and rules for defining exclusion dates. The method appropriately
     * generates all the dates and returns a list with all excluded dates removed.
     *
     * A fearful function that forcefully fathoms the fullness of a few recurrences.
     *
     * @param rr a vector of recurrence rules (Recurrence objects)
     * @param er a vector of exclusion rules  (Recurrence objects)
     * @param rd a vector of explicit recurrence dates (DateTime objects)
     * @param ed a vector of explicit exclusion dates (DateTime objects)
     * @param bound number of dates to be produced
     */
    static void enforce(JulianPtrArray * rr, JulianPtrArray * er, JulianPtrArray * rd, 
        JulianPtrArray * ed, t_int32 bound, JulianPtrArray * out, JLog * log);
         
      
    /**
     * Unzips a recurrence rule (i.e., produces a set of dates which the recurrence represents) to a certain
     * number of dates inside of a given timezone.
     *
     * @param   bound   the number of dates to produce
     * @param   out     vector containing generated dates
     */
    void unzip(t_int32 bound, JulianPtrArray * out, JLog * log);
   

    /**
     * Converts iCal Recurrence Day string to Calendar::EDaysOfWeek value
     * @param           s       recurrence day string
     * @param           error   output if bad day value
     *
     * @return          the Calendar::EDaysOfWeek value of s
     */
    static t_int32 stringToDay(UnicodeString & s, t_bool & error);

    /* index into m_GntrVctr of appropriate generator */
    static const t_int32 ms_iByMinuteGntrIndex;
    static const t_int32 ms_iByHourGntrIndex;
    static const t_int32 ms_iByWeekNoGntrIndex;
    static const t_int32 ms_iByYearDayGntrIndex;
    static const t_int32 ms_iByMonthGntrIndex;
    static const t_int32 ms_iByMonthDayGntrIndex;
    static const t_int32 ms_iByDayYearlyGntrIndex;
    static const t_int32 ms_iByDayMonthlyGntrIndex;
    static const t_int32 ms_iByDayWeeklyGntrIndex;
    static const t_int32 ms_iDefaultGntrIndex;
    static const t_int32 ms_iGntrSize;

private:

    /* Hide default constructor */
    Recurrence(); 

    /* The ordering of this array represents the recursive structure through which dates
     * will be determined. the first array contains tags which operate over a year, the
     * second those which operate over a month, the third a week, etc.
     */
    static const t_int32 * ms_aaiGenOrder[];

    /* Generator order of each span.  There are six spans. */
    static const t_int32 ms_aiGenOrderZero[];
    static const t_int32 ms_aiGenOrderOne[];
    static const t_int32 ms_aiGenOrderTwo[];
    static const t_int32 ms_aiGenOrderThree[];
    static const t_int32 ms_aiGenOrderFour[];
    static const t_int32 ms_aiGenOrderFive[];
    
    /* length of each generator order array */ 
    static const t_int32 ms_GenOrderLen;
    static const t_int32 ms_GenOrderZeroLen;
    static const t_int32 ms_GenOrderOneLen;
    static const t_int32 ms_GenOrderTwoLen;
    static const t_int32 ms_GenOrderThreeLen;
    static const t_int32 ms_GenOrderFourLen;
    static const t_int32 ms_GenOrderFiveLen;

    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    
    /** variable to specify unset value */
    static const t_int32 ms_iUNSET;

     /* Variables that correspond directly to properties defined in the spec */
    t_int32 m_iType;        /* init to JulianUtility::RT_NONE */
    Duration * m_Interval;  /* init to NULL */
    t_int32 m_iCount;       /* init to ms_iUNSET */
    t_int32 m_iWkSt;        /* init to Calendar::MONDAY */
    DateTime m_Until;       /* init to Invalid DateTime */

    /* ByDay contains an array of day, integer pairs. days are specified from the standard Java 
     calendar object, IE, Calendar.MONDAY. the integer specifies the nth instance of that day 
     of the week. if all instances are specified (ie, BYDAY=FR), then the integer value is    
     left to zero. */
    RRDay *  m_aaiByDay;
    
    /* ptr to parameters for each generator */
    t_int32 * m_aiByMinute;
    t_int32 * m_aiByHour;
    t_int32 * m_aiByMonthDay;
    t_int32 * m_aiByYearDay;
    t_int32 * m_aiBySetPos;
    t_int32 * m_aiByWeekNo;
    t_int32 * m_aiByMonth;

    /* the bysetpos length */
    t_int32 m_iBySetPosLen; 
   
    /* vector of generators, order is extremely important! */
    JulianPtrArray * m_GntrVctr; 

    /* Individual GENERATORS */
    ByMinuteGenerator * ms_ByMinuteGntr;
    ByHourGenerator * ms_ByHourGntr;
    ByWeekNoGenerator * ms_ByWeekNoGntr;
    ByYearDayGenerator * ms_ByYearDayGntr;
    ByMonthGenerator * ms_ByMonthGntr;
    ByMonthDayGenerator * ms_ByMonthDayGntr;
    ByDayYearlyGenerator * ms_ByDayYearlyGntr;
    ByDayMonthlyGenerator * ms_ByDayMonthlyGntr;
    ByDayWeeklyGenerator * ms_ByDayWeeklyGntr;
    DefaultGenerator * ms_DefaultGntr;

    /* Number of active generators */
    t_int32 m_iActiveGenerators;  /* init to 0 */

    /* Variables for storing DTSTART, DTEND and DURATION */
    DateTime m_StartDate;   
    DateTime m_StopDate;    
    Duration * m_Duration;    /* init to NULL */

    /* cache validity */
    t_bool m_bParseValid; 

    /*-----------------------------
    ** METHODS
    **---------------------------*/
    
    /* initializes data */
    void init();

    /**
     * Helper method.  Breaks recurrence rule strings into parameters 
     * and fills parameters vector with parameters.
     * @param           strLine     recurrence rule string
     * @param           parameters  vector to fill in with recurrence parameters
     *
     */
    void parsePropertyLine(UnicodeString & strLine, JulianPtrArray * parameters);
    
    /**
     * Parses Recurrence rule string, populates data members, generators'
     * parameters.  Returns TRUE if recurrence parse went OK, FALSE
     * if there was a failure.  A failure sets m_bParseValid to FALSE.
     * @param           s           recurrence rule string
     *
     * @return          TRUE if rule is valid, FALSE otherwise
     */
    t_bool parse(UnicodeString & s);

    /**
     * Cleanup method takes vector of dates and make sure output number
     * in less than length and start value is included.  
     * @param           vDatesIn    input vector of dates
     * @param           length      number of dates to be produced
     * @param           out         output vector of dates
     *
     */
    void cleanup(JulianPtrArray * vDatesIn, t_int32 length,
        JulianPtrArray * out);

    /**
     * Sets interval to i, depending on m_iType.  For example,
     * if m_iType is set to JulianUtility::RT_YEARLY and i is 2,
     * then the interval is every 2 years.
     * @param           i   value of interval w/respect to m_iType
     */
    void internalSetInterval(t_int32 i);
    

    /**
     * Used for resetting a date value that is floating somewhere in an
     * interval, to the beginning of that interval. Depends upon the
     * initial date, the recurrence interval type, the week start date, and
     * the state of other active generators.
     * 
     * Example: type = MONTHLY, s = Jul 15, 1997 10:15 PM PDT
     *          resets to Jul 1, 1997 10:15 PM PDT
     *          if the BYHOUR and BYMINUTE tags are active it
     *          will further be clocked back to
     *          Jul 1, 1997 12:00 AM PDT
     * @param           t       date value to reset
     * @param           type    current interval type
     * @param           wkSt    week start value
     *
     */
    void reset(DateTime & t, t_int32 type, t_int32 wkSt);
    
    /**
     * Removes dates in vDates that do not follow BYSETPOS rule.
     * Usually, this involves deleting almost every date except 1
     * from vDates.  
     * @param           vDates  vector of dates.
     *
     */
    void handleSetPos(JulianPtrArray * vDates);

    /*-----------------------------
    ** STATIC METHODS
    **---------------------------*/

    /**
     * Tokenizes string with delimeters and puts tokens in vectorToFillIn
     * @param           s               string to tokenize
     * @param           strDelim        string delimeters
     * @param           vectorToFillIn  output vector of tokens to fill in
     *
     */
    static void parseDelimited(UnicodeString & s, UnicodeString & strDelim, 
        JulianPtrArray * vectorToFillIn);
   
    /**
     * Verifies that each element in v is within the bounds of 
     * [lowerBound,upperBound) (inclusive).  If bZero is TRUE, then
     * 0 is NOT a valid value, otherwise 0 is a valid value.  If
     * there is an error, log the error message and set bParseError
     * to TRUE.  Each valid element is added to the output list.
     * The return size of the list is retSize
     * @param           v               vector of string element
     * @param           lowerBound      lowerbound of valid range
     * @param           upperBound      upperbound of valid range
     * @param           error           log error message
     * @param           retSize         output size of parameter list
     * @param           bParseError     output parse error boolean
     * @param           bZero           TRUE is 0 OK, FALSE if 0 not OK
     *
     * @return          ptr to t_int32 (list of gntr parameter elements)
     */
    static t_int32 * verifyIntList(JulianPtrArray * v, t_int32 lowerBound, 
        t_int32 upperBound, UnicodeString & error, t_int32 & retSize, 
        t_bool & bParseError, t_bool bZero = FALSE);

    /**
     * Verifies that each element in v is a valid day element.
     * A day element is of form (-5MO) where the last two letters 
     * must be a recurrence day string with a optional modifier in front.
     * Depending of the type variable the modifier can be
     * from -53 to 53 for type=RT_YEARLY and -5 to 5 for type=RT_MONTHLY.
     * Each valid element is added to the output RRDay list.
     * The return size of the list is retSize.
     * @param           v           vector of string elements
     * @param           type        interval type
     * @param           retSize     output size of RRDay list
     * @param           bParseError output parse error boolean
     *
     * @return          ptr to RRDay * (list of RRDay objects)
     */
    static RRDay * createByDayList(JulianPtrArray * v, t_int32 type, 
        t_int32 & retSize, t_bool & bParseError);
    
    /**
     * Helper method gets the day and modifier from the string in.
     * @param           in          string to get day, modifier from
     * @param           day         output Calendar::EDaysOfWeek
     * @param           modifier    output day modifier
     * @param           bParseError output parse error boolean
     *
     */
    static void createByDayListHelper(UnicodeString & in, t_int32 & day, 
        t_int32 & modifier, t_bool & bParseError);
    
    /**
     * Cleanup vector of UnicodeString objects.  Delete each UnicodeString
     * in vector.
     * @param           strings     vector of elements to delete from
     */
    static void deleteUnicodeStringVector(JulianPtrArray * strings);
public:    
    
    /**
     * Cleanup vector of DateTime objects.  Delete each DateTime in vector.
     * @param           strings     vector of elements to delete from
     */
    static void deleteDateTimeVector(JulianPtrArray * datetimes);
    
    /**
     * Cleanup vector of Recurrence objects.  Delete each Recurrence
     * in vector
     * @param           strings     vector of elements to delete from
     */
    static void deleteRecurrenceVector(JulianPtrArray * recurrences);
private:
    
    /**
     * Cleanup vector of vector of DateTime objects.   Delete each
     * DateTime in each vector in vector
     * @param           strings     vector of elements to delete from
     */
    static void deleteDateTimeVectorVector(JulianPtrArray * vDatetimes);
    
    /**
     * Cleanup vector of ICalParameter objects.  Delete each ICalParameter
     * from vector.
     * @param           strings     vector of elements to delete from
     */
    static void deleteICalParameterVector(JulianPtrArray * parameters);

#if 0
    static void copyDateTimesInto(JulianPtrArray * from, DateTime * to);
#endif

    /**
     * Converts frequency string (i.e. YEARLY, WEEKLY, DAILY, etc.)
     * to Recurrence type constant.
     * @param           s               input iCal frequency string
     * @param           bParseError     output parse error boolean
     *
     * @return          output recurrence type
     */
    static t_int32 stringToType(UnicodeString & s, t_bool & bParseError);

    /**
     * Gets the length of the Generator Order Array given an index
     * to the array
     * @param           genOrderIndex   index in to generator order array
     *
     * @return          length of m_GntrVctr[genOrderIndex]
     */
    static t_int32 getGenOrderIndexLength(t_int32 genOrderIndex);
    
    /**
     * Concatentates a vector of vector of dates into a vector of dates.
     * @param           vvIn        input vector of vector of dates
     * @param           vOut        output vector of dates with vvIn's dates added in
     *
     */
    static void concat(JulianPtrArray * vvIn, JulianPtrArray * vOut);
    
    /**
     * Replaces the contents of vOut with the contents of vvIn 
     * @param           vvIn        input vector of vector of dates
     * @param           vOut        output vector of dates with vvIn's dates replacing old dates
     *
     * @return          static void 
     */
    static void replace(JulianPtrArray * vvIn, JulianPtrArray * vOut);
    
    /**
     * Removes dates in vDatesOut that are not in vDatesIn. 
     * @param           vDatesOut   output vector of dates that are also in vDatesIn
     * @param           vDatesIn    input vector of dates
     *
     */
    static void intersectOutList(JulianPtrArray * vDatesOut, JulianPtrArray * vDatesIn);


    /**
     * Subtract or intersects date lists.  If subtract is TRUE, then
     * subtract lists vvDates[1..n] from vvDates[0].  If intersects mode
     * intersect lists vvDates[0...n].  Resulting dates vector goes into
     * out.
     *
     * @param           vvDates     vector of vector of dates
     * @param           subtract    subtract = TRUE or intersect = FALSE mode
     * @param           out         output resulting dates vector
     *
     */
    static void intersectDateList(JulianPtrArray * vvDates, t_bool subtract,
        JulianPtrArray * out);
    
    /**
     * Checks whether datetimes vector contains date.  To contain date
     * means containing a date with the same year, month, day, hour, minute and second.
     * It does not have to have the same millesecond value.
     * @param           datetimes       vector of dates to check for date
     * @param           date            target date to look for
     *
     * @return          TRUE if date is in datetimes, FALSE otherwise.
     */
    static t_bool contains(JulianPtrArray * datetimes, DateTime * date);

    /**
     * Eliminates duplicate datetime object from datetimes
     * @param           datetimes   vector to remove duplicates from
     *
     */
    static void eliminateDuplicates(JulianPtrArray * datetimes);

    /**
     * DEBUGGING method to print out datetime elements in a vector
     * @param           x       datetime vector
     * @param           name    debug name of vector
     *
     */
    static void TRACE_DATETIMEVECTOR(JulianPtrArray * x, char * name);

    /**
     * DEBUGGING method to print out elements in a vector of vector of datetimes
     * @param           x       vector of vector of datetimes
     * @param           name    debug name of vector
     *
     */
    static void TRACE_DATETIMEVECTORVECTOR(JulianPtrArray * x, char * name);
    
};
#endif /* __RECURRENCE_H_ */

   
