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
 * vtimezne.h
 * John Sun
 * 2/24/98 2:28:21 PM
 */

#ifndef __VTIMEZONE_H_
#define __VTIMEZONE_H_

#include <unistring.h>
#include <timezone.h>
#include <simpletz.h>

#include "icalcomp.h"
#include "tzpart.h"
#include "jlog.h"

class VTimeZone : public ICalComponent
{
private:
#if 0

    /**
     * Default constructor.  Hide from clients.
     */
    VTimeZone();
#endif
protected:

    /**
     * Copy constructor.
     * @param           that    VTimeZone to copy
     */
    VTimeZone(VTimeZone & that);

public:

    /**
     * Constructor.  Creates VTimeZone with initial log file initLog. 
     * @param           initLog     initial log file pointer
     */
    VTimeZone(JLog * initLog = 0);

    /**
     * Destructor.
     */
    ~VTimeZone();

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
     * @param   brFile          ICalReader to load from
     * @param   sType           name of component (VTIMEZONE)
     * @param   parseStatus     return parse error status string (normally return "OK")
     * @param   vTimeZones      vector of VTimeZones to apply to datetimes
     * @param   bIgnoreBeginError   TRUE = ignore first "BEGIN:VTIMEZONE", FALSE otherwise
     * @param   encoding            the encoding of the stream,  default is 7bit.
     *
     * @return  parse error status string (parseStatus)
     */
    virtual UnicodeString & parse(ICalReader * brFile, UnicodeString & sType, 
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
    virtual t_bool isValid();
    
    /**
     * Print all contents of ICalComponent to iCal export format.
     *
     * @return          string containing iCal export format of ICalComponent
     */
    virtual UnicodeString toICALString();
    
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
        t_bool isRecurring);

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
    virtual UnicodeString formatChar(t_int32 c, UnicodeString sFilterAttendee = "",
        t_bool delegateRequest = FALSE);

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
    virtual ICAL_COMPONENT GetType() const { return ICAL_COMPONENT_VTIMEZONE; }

    /*  -- End of ICALComponent interface -- */

    /* -- GETTERS AND SETTERS -- */

    /**
     * Return the STANDARD TimeZone part of this VTimeZone.
     *
     * @return          ptr to STANDARD TZPart
     */
    TZPart * getStandardPart();
    
    /**
     * Return the DAYLIGHT TimeZone part of this VTimeZone.
     *
     * @return          ptr to DAYLIGHT TZPart
     */
    TZPart * getDaylightPart();


    /**
     * Return pointer to vector of TZParts
     *
     * @return          pointer to vector of TZParts.
     */
    JulianPtrArray * getTZParts() { return m_TZPartVctr; };

    /* TZPART */
    void addTZPart(TZPart * part);
    TZPart * getPart(UnicodeString & u);

    /* TZID */
    UnicodeString getTZID() const;
    void setTZID(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getTZIDProperty() const { return m_TZID; }
    
    /* LastModified */
    DateTime getLastModified() const;
    void setLastModified(DateTime s, JulianPtrArray * parameters = 0);
    ICalProperty * getLastModifiedProperty() const { return m_LastModified; }

    /* TZURL */
    UnicodeString getTZURL() const;
    void setTZURL(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getTZURLProperty() const { return m_TZURL; }

    /**
     * Given vector of timezones, search for VTimeZone with TZID equal to id.
     * Return 0 if not found.
     * @param           id              TZID to look for
     * @param           timezones       vector of timezones to search through
     *
     * @return          first matching VTimeZone in vector, 0 if not found 
     */
    static VTimeZone * getTimeZone(UnicodeString & id, JulianPtrArray * timezones);


    /**
     * Given a DateTime string, a vector of timezones, and a vector of
     * ICalParameters, apply a timezone to the string if there is a TZID
     * in the parameters vector.  
     * For example, if time = "19971110T112233", timezones has a EST timezone,
     * and TZID = EST, then the method would return the datetime for time with
     * the EST timezone applied to it.  
     * @param           time            datetime string to apply timezone to
     * @param           vTimeZones      vector of timezones
     * @param           parameters      parameters to search for TZID
     *
     * @return          DateTime representing time with timezone applied to it. 
     */
    static DateTime DateTimeApplyTimeZone(UnicodeString & time,
        JulianPtrArray * vTimeZones, JulianPtrArray * parameters);

    /**
     * Return the libnls TimeZone that represents this VTimeZone object.
     * @return          pointer to TimeZone 
     */
    TimeZone * getNLSTimeZone() const { return (TimeZone *) &*m_NLSTimeZone; } 

private:

    /**
     * Creates NLS TimeZone from start month,day,week,time,until.  
     * TODO: possible bug, may need to call everytime before calling getNLSTimeZone().
     */
    void selfCheck();
    
    /**
     * store the data, depending on property name, property value
     * parameter names, parameter values, and the current line.
     *
     * @param       strLine         current line to process
     * @param       propName        name of property
     * @param       propVal         value of property
     * @param       parameters      property's parameters
     */
    void storeData(UnicodeString & strLine, UnicodeString & propName,
        UnicodeString & propVal, JulianPtrArray * parameters);

    /* whether VTimeZone has more that two parts, currently must
     * have exactly two parts (DAYLIGHT AND STANDARD)
     */
    static t_bool ms_bMORE_THAN_TWO_TZPARTS;

    /* -- MEMBERS -- */
    /*TimeZone * m_NLSTimeZone;*/
    SimpleTimeZone *m_NLSTimeZone;

    JulianPtrArray *    m_TZPartVctr;
    ICalProperty *      m_TZID;
    ICalProperty *      m_LastModified;
    ICalProperty *      m_TZURL;
    
    JLog * m_Log;
};

#endif /* __VTIMEZONE_H_ */

