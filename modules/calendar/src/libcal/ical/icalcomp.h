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
 * icalcomp.h
 * John Sun
 * 2/5/98 2:57:16 PM
 */

#include <unistring.h>
#include "ptrarray.h"
#include "icalredr.h"
#include "prprty.h"
#include "jlog.h"
#include "jutility.h"

#ifndef __ICALCOMPONENT_H_
#define __ICALCOMPONENT_H_

/**
 *  ICalComponent defines a Calendar Component in iCal.  A calendar
 *  component can contain iCalendar properties as well as
 *  other iCalendar components.  
 *  Examples of iCalendar components include VEvent, VTodo,
 *  VJournal, VFreebusy, VTimeZone, and VAlarms.
 *  TZPart, althought not an iCal component, subclasses from
 *  ICalComponent.
 */
class JULIAN_PUBLIC ICalComponent
{
protected:
    
    /**
     * Constructor.  Hide from clients except subclasses.
     */
    ICalComponent();

public:

    /**
     * An enumeration of Calendar Components.  TZPart is not
     * actually a Calendar component.  However, it is useful to add
     * it here because it implements this class' interface.
     */
    enum ICAL_COMPONENT
    {
      ICAL_COMPONENT_VEVENT = 0, 
      ICAL_COMPONENT_VTODO = 1, 
      ICAL_COMPONENT_VJOURNAL = 2,
      ICAL_COMPONENT_VFREEBUSY = 3,
      ICAL_COMPONENT_VTIMEZONE = 4,
      ICAL_COMPONENT_VALARM = 5,
      ICAL_COMPONENT_TZPART = 6
    };


    /**
     * Converts ICAL_COMPONENT to string. If bad ic, then return "".
     * @param           ICAL_COMPONENT ic
     *
     * @return          output name of component 
     */
    static UnicodeString componentToString(ICAL_COMPONENT ic);

    static ICAL_COMPONENT stringToComponent(UnicodeString & s, t_bool & error);

    /**
     * Destructor.
     *
     * @return          virtual 
     */
    virtual ~ICalComponent();


    /*  -- Start of ICALComponent interface (PURE VIRTUAL METHODS)-- */
    
    /**
     * The parse method is the standard interface for ICalComponent
     * subclasses to parse from an ICalReader object 
     * and from the ITIP method type (i.e. PUBLISH, REQUEST, CANCEL, etc.).
     * The method type can be set to \"\" if there is no method to be loaded.
     * Also accepts a vector of VTimeZone objects to apply to
     * the DateTime properties of the component.
     * TODO: maybe instead of passing a UnicodeString for method, why not a enum METHOD
     * 4-2-98:  Added bIgnoreBeginError.  If desired to start parsing in Component level,
     * it is useful to ignore first "BEGIN:".   Setting bIgnoreBeginError to TRUE allows for
     * this.
     *     
     * @param   brFile              ICalReader to load from
     * @param   sMethod             method of component (i.e. PUBLISH, REQUEST, REPLY)
     * @param   parseStatus         return parse error status string (normally return "OK")
     * @param   vTimeZones          vector of VTimeZones to apply to datetimes
     * @param   bIgnoreBeginError   TRUE = ignore first "BEGIN:VEVENT", FALSE otherwise
     * @param   encoding            the encoding of the stream,  default is 7bit.
     *
     * @return  parse error status string (parseStatus)
     */
    virtual UnicodeString & parse(ICalReader * brFile, UnicodeString & sMethod, 
        UnicodeString & parseStatus, JulianPtrArray * vTimeZones = 0,
        t_bool bIgnoreBeginError = FALSE, JulianUtility::MimeEncoding encoding =
        JulianUtility::MimeEncoding_7bit)
    {
        PR_ASSERT(FALSE);
        if (brFile != NULL && vTimeZones != NULL 
            && sMethod.size() > 0 && parseStatus.size() > 0 && bIgnoreBeginError)
        {}
        return parseStatus;
    }

    /**
     * Returns a clone of this object
     * @param           initLog     log file to write errors to
     *
     * @return          clone of this object
     */
    virtual ICalComponent * clone(JLog * initLog) { PR_ASSERT(FALSE); if (initLog) {} return 0; }
                                                                                               
    /**
     * Return TRUE if component is valid, FALSE otherwise
     *
     * @return          TRUE if is valid, FALSE otherwise
     */
    virtual t_bool isValid() { PR_ASSERT(FALSE); return FALSE; }

    /**
     * Print all contents of ICalComponent to iCal export format.
     *
     * @return          string containing iCal export format of ICalComponent
     */
    virtual UnicodeString toICALString() { PR_ASSERT(FALSE); return "";}

    /**
     * Print all contents of ICalComponent to iCal export format, depending
     * on attendee name, attendee delegated-to, and where to include recurrence-id or not
     *
     * @param           method          method name (REQUEST,PUBLISH, etc.)
     * @param           name            attendee name to filter with
     * @param           isRecurring     TRUE = no recurrenceid, FALSE = include recurrenceid
     *
     * @return          string containing iCal export format of ICalComponent
     */
    virtual UnicodeString toICALString(UnicodeString method, UnicodeString name, 
        t_bool isRecurring) 
    {
        PR_ASSERT(FALSE);
        if (method.size() == 0 && name.size() == 0 && isRecurring)
        {}
        return "";
    }
    
    /**
     * Print all contents of ICalComponent to human-readable string.
     *
     * @return          string containing human-readable format of ICalComponent
     */
    virtual UnicodeString toString() {PR_ASSERT(FALSE); return "";}

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
        UnicodeString sFilterAttendee = "", 
        t_bool delegateRequest = FALSE) 
    {
        PR_ASSERT(FALSE);   
#if 0
        if (c == 8 || sFilterAttendee.size() == 0 || delegateRequest) {} 
#endif   
        return "";
    }

    /**
     * convert a character to the content of a property in string 
     * human-readable format
     * @param           c           a character represents a property
     * @param           dateFmt     for formatting datetimes
     *
     * @return          property in human-readable string
     */
    virtual UnicodeString toStringChar(t_int32 c, UnicodeString & dateFmt) 
    {
        PR_ASSERT(FALSE);

#if 0 /* fix HPUX build busgage - this is dead code anyway - alecf 4/30/98 */
        if (c == 8) {}
#endif 
        return "";
#if 0
        if (dateFmt.size() > 0) {}
#endif
    }

    /**
     * Returns the ICAL_COMPONENT enumeration value of this ICalComponent.
     * Each ICalComponent subclass must return a unique ICAL_COMPONENT value.
     *
     * @return          ICAL_COMPONENT value of this component
     */
    virtual ICAL_COMPONENT GetType() const { PR_ASSERT(FALSE); return ICAL_COMPONENT_VEVENT; }

    /*  -- End of ICALComponent PURE VIRTUAL interface -- */


    /*-------------------------------------------------------------------*/
    /* NOT PURE
     * TRYING TO MERGE SIMILARITIES IN VFREEBUSY AND TIMEBASEDEVENT TOGETHER
     */

    /**
     * Check if this component matches a UID and a sequence number.
     * TimeBasedEvent and VFreebusy should override this method.
     * @param           sUID        uid to look for
     * @param           iSeqNo      sequence number to look for
     *
     * @return          TRUE if match, FALSE otherwise
     */
    virtual t_bool MatchUID_seqNO(UnicodeString sUID, t_int32 iSeqNo = -1)
    {
        /* NOTE: remove later, just to get rid of compiler warnings */
        if (sUID.size() > 0 && iSeqNo > 0) {}
        return FALSE;
    }

    /**
     * Return the UID of this component.  TimeBasedEvent and VFreebusy
     * should override this method.
     *
     * @return          the UID of this component.
     */
    virtual UnicodeString getUID() const { return ""; }

    /**
     * Return the sequence number of this component.  
     * TimeBasedEvent and VFreebusy should override this method.
     *
     * @return          the sequence number of this component.
     */
    virtual t_int32 getSequence() const { return -1; }

    /*---------------------------------------------------------------------*/

    /*  -- End of ICALComponent interface -- */


    /**
     * Given a component name, a component format string,  print the component in
     * following manner with properties specified by the format string.  It does
     * this by calling abstract formatChar method.
     * (i.e. sComponentName = VEVENT)
     *
     * BEGIN:VEVENT
     * DTSTART:19980331T112233Z (properties dependent on format string)
     * ...
     * END:VEVENT
     *
     * Optionally, the caller may pass in an sFilterAttendee and delegateRequest args.
     * If sFilterAttendee != "", then filter on sFilterAttendee, meaning that
     * only the attendee with the name sFilterAttendee should be printed.  If the
     * delegateRequest is set to TRUE, 
     * also print attendees who are delegates or related-to the delegates.
     * @param           sComponentName          name of ICalComponent
     * @param           strFmt                  property format string
     * @param           sFilterAttendee         attendee to filter on
     * @param           delegateRequest         whether to print delegates
     *
     * @return          iCal export string of component
     */
    UnicodeString format(UnicodeString & sComponentName, UnicodeString & strFmt, 
        UnicodeString sFilterAttendee = "", t_bool delegateRequest = FALSE);

    /**
     * Given a format string, prints the component to human-readable format
     * with properties specified by format string.  It does this by calling
     * abstract toStringChar method.
     * @param           strFmt          property format string
     *
     * @return          human-readable string of component
     */
    UnicodeString toStringFmt(UnicodeString & strFmt); 

    /* STATIC METHODS */
   
    /**
     * Cleanup vector of DateTime objects.  Delete each DateTime in vector.
     * @param           strings     vector of elements to delete from
     */
    static void deleteDateVector(JulianPtrArray * dateVector);
    
    /**
     * Cleanup vector of UnicodeString objects.   Delete each UnicodeString
     * in vector.
     * @param           strings     vector of elements to delete from
     */
    static void deleteUnicodeStringVector(JulianPtrArray * stringVector);
    
    /**
     * Cleanup vector of ICalComponent objects.  Delete each ICalComponent
     * in vector
     * @param           strings     vector of elements to delete from
     */
    static void deleteICalComponentVector(JulianPtrArray * componentVector);

    /**
     * prints Vector of string properties to ICAL format for printing
     * @param           sProp       property name
     * @param           v           property value
     * @param           sResult     output string
     *
     * @return          output string (sResult)
     */
    static UnicodeString & propertyVToCalString(UnicodeString & sProp, 
        JulianPtrArray * val, UnicodeString & sResult);

};

#endif /* __ICALCOMPONENT_H_ */


