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
 * vjournal.h
 * John Sun
 * 4/23/98 10:24:32 AM
 */
#ifndef __VJOURNAL_H_
#define __VJOURNAL_H_

#include "datetime.h"
#include "ptrarray.h"
#include "tmbevent.h"
#include "jlog.h"

class JULIAN_PUBLIC VJournal: public TimeBasedEvent
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/

    /**
     * store the data, depending on property name, property value
     * parameter names, parameter values, and the current line.
     * If this method returns FALSE, property not valid.
     *
     * @param       strLine         current line to process
     * @param       propName        name of property
     * @param       propVal         value of property
     * @param       parameters      property's parameters
     * @param       vTimeZones      vector of timezones
     * @return      TRUE if property handled, FALSE otherwise
     */
    t_bool storeData(UnicodeString & strLine, 
        UnicodeString & propName, UnicodeString & propVal,
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);

    /**
     * Sets default data.  Currently does following:
     * 1) Sets DTEnd to correct value, depending on DURATION, DTSTART
     *    (see spec on rules)
     * 2) Sets TRANSP to TRANSPARENT if anniversary event
     */
    void selfCheck();
    
    /**
     * Copy constructor.
     * @param           that        VJournal to copy.
     */
    VJournal(VJournal & that);

public:
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/

    /**
     * Constructor.  Create VJournal with initial log file set to initLog.
     * @param           initLog     initial log file pointer
     */
    VJournal(JLog * initLog = 0);
    
    /**
     * Destructor.
     */
    ~VJournal();

    /*  -- Start of ICALComponent interface -- */

    /**
     * The parse method is the standard interface for ICalComponent
     * subclasses to parse from an ICalReader object 
     * and from the ITIP method type (i.e. PUBLISH, REQUEST, CANCEL, etc.).
     * The method type can be set to \"\" if there is no method to be loaded.
     * Also accepts a vector of VTimeZone objects to apply to
     * the DateTime properties of the component.
     * 4-2-98:  Added bIgnoreBeginError.  If desired to start parsing in Component level,
     * it is useful to ignore first "BEGIN:".   Setting bIgnoreBeginError to TRUE allows for
     * this.
     *     
     * @param   brFile              ICalReader to load from
     * @param   sType               name of component (VJOURNAL)
     * @param   parseStatus         return parse error status string (normally return "OK")
     * @param   vTimeZones          vector of VTimeZones to apply to datetimes
     * @param   bIgnoreBeginError   TRUE = ignore first "BEGIN:VJOURNAL", FALSE otherwise
     * @param   encoding            the encoding of the stream,  default is 7bit.
     *
     * @return  parse error status string (parseStatus)
     */
    UnicodeString & parse(ICalReader * brFile, UnicodeString & sMethod,
        UnicodeString & parseStatus, JulianPtrArray * vTimeZones = 0,
        t_bool bIgnoreBeginError = FALSE,
        JulianUtility::MimeEncoding encoding = JulianUtility::MimeEncoding_7bit);

    /**
     * Returns a clone of this object
     * @param           initLog     log file to write errors to
     *
     * @return          clone of this object
     */
    virtual ICalComponent * clone(JLog * initLog);

    /**
     * Return TRUE if component is valid, FALSE otherwise
     *
     * @return          TRUE if is valid, FALSE otherwise
     */
    t_bool isValid();

    /**
     * Print all contents of ICalComponent to human-readable string.
     *
     * @return          string containing human-readable format of ICalComponent
     */
    virtual UnicodeString toString();

    /**
     * Depending on character passed in, returns a string that represents
     * the ICAL export string of that property that the character represents, if 
     * other paramaters not null, then print out information is filtered in several ways
     * Attendee print out can be filtered so that only attendee with certain name is printed
     * also, can print out the relevant attendees in a delegation message
     * (ie. owner, delegate-to, delegate-from) 
     *
     * @param       c                   char of what property to print
     * @param       sFilterAttendee     name of attendee to print
     * @param       bDelegateRequest    TRUE if a delegate request, FALSE if not
     * @return                          ICAL export format of that property
     */
    virtual UnicodeString formatChar(t_int32 c, 
        UnicodeString sFilterAttendee = "", t_bool delegateRequest = FALSE);

    /**
     * convert a character to the content of a property in string 
     * human-readable format
     * @param           c           a character represents a property
     * @param           dateFmt     for formatting datetimes
     *
     * @return          property in human-readable string
     */
    virtual UnicodeString toStringChar(t_int32 c, UnicodeString & dateFmt);

    /**
     * Returns the ICAL_COMPONENT enumeration value of this ICalComponent.
     * Each ICalComponent subclass must return a unique ICAL_COMPONENT value.
     *
     * @return          ICAL_COMPONENT value of this component
     */
    virtual ICAL_COMPONENT GetType() const { return ICAL_COMPONENT_VJOURNAL; }

    /*  -- End of ICALComponent interface -- */

    /**
     * Overridden virtual method used as wrapper to ICalComponent::format method. 
     * @param           strFmt              iCal format string 
     * @param           sFilterAttendee     attendee to filter
     * @param           delegateRequest     delegate request = TRUE, FALSE otherwise
     *
     * @return          output iCal formatted export string
     */
    virtual UnicodeString formatHelper(UnicodeString & strFmt,
            UnicodeString sFilterAttendee = "", t_bool delegateRequest = FALSE);

    /**
     * Helper method.  Overriden virtual method.  Used by subclasses to
     * populate recurrence-dependent data.  For example, a recurring
     * VJournal needs to have DTEnd calculated from recurring 
     * DTStart.  VJournal will overwrite method set DTEnd correctly.
     * Abstract difference() method is used to calculate ldiff.
     * Need to pass vector of RDate periods in case end value must
     * be adjusted in RDATE is a PERIOD value.
     * @param           start       recurrence instance starting time
     * @param           ldiff       abstract difference value
     * @param           vPer        vector of RDate periods
     */
    void populateDatesHelper(DateTime start, Date ldiff, JulianPtrArray * vPer);

    /* overridden message methods */
    virtual UnicodeString cancelMessage();
    virtual UnicodeString requestMessage();
    virtual UnicodeString requestRecurMessage();
    virtual UnicodeString counterMessage();
    virtual UnicodeString declineCounterMessage();
    virtual UnicodeString addMessage();
    virtual UnicodeString refreshMessage(UnicodeString sAttendeeFilter);
    virtual UnicodeString allMessage();
    virtual UnicodeString replyMessage(UnicodeString sAttendeeFilter);
    virtual UnicodeString publishMessage();
    virtual UnicodeString publishRecurMessage();

    /**
     * Overridden method.  Calculate difference from start, end time
     * For VJournal, this would 0.
     *
     * @return          virtual Date 
     */
    Date difference();

    /**
     * Sets default human-readable event format pattern to s. 
     * @param           s   new event format pattern
     */
    static void setDefaultFmt(UnicodeString s);

    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 
    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
    /*----------------------------- 
    ** STATIC METHODS 
    **---------------------------*/ 
    /*----------------------------- 
    ** OVERLOADED OPERATORS 
    **---------------------------*/ 
};

#endif /* __VJOURNAL_H_ */

