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
 * vfrbsy.h
 * John Sun
 * 2/19/98 4:17:54 PM
 */

#ifndef __VFREEBUSY_H_
#define __VFREEBUSY_H_

#include <unistring.h>
#include "datetime.h"
#include "period.h"
#include "icalcomp.h"
#include "attendee.h"
#include "freebusy.h"
#include "jlog.h"

class VFreebusy : public ICalComponent
{
private:
#if 0

    /**
     * Default Constructor.  Hide from clients. 
     */
    VFreebusy();
#endif
protected:

    /** 
     *  Checks private data members so they are valid.
     */
    void selfCheck();

    /*public static VFreebusy * createVFreebusy(UnicodeString & string);*/

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
    t_bool storeData(UnicodeString & strLine, UnicodeString & propName, 
        UnicodeString & propVal, JulianPtrArray * parameters, 
        JulianPtrArray * vTimeZones);

    UnicodeString formatHelper(UnicodeString & strFmt, 
        UnicodeString sFilterAttendee = "");

    /**
     * Sorts vector of Freebusy periods by start time.
     */
    void sortFreebusy();

    /**
     * Copy constructor. 
     * @param           that        VFreebusy to copy
     */
    VFreebusy(VFreebusy & that);

public:

    /**
     * Constructor.  Creates VFreebusy with initial log file initLog.
     * @param           JLog * initLog = 0
     */
    VFreebusy(JLog * initLog = 0);

    /**
     * Destructor. 
     */
    ~VFreebusy();   

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
     * @param   sType           name of component (i.e. VEVENT, VTODO, VTIMEZONE)
     * @param   parseStatus     return parse error status string (normally return "OK")
     * @param   vTimeZones      vector of VTimeZones to apply to datetimes
     * @param   bIgnoreBeginError   TRUE = ignore first "BEGIN:VFREEBUSY", FALSE otherwise
     * @param   encoding            the encoding of the stream,  default is 7bit.
     *
     * @return  parse error status string (parseStatus)
     */
    virtual UnicodeString & parse(ICalReader * brFile, UnicodeString & method, 
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
        t_bool isRecurring = FALSE);

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
    virtual ICAL_COMPONENT GetType() const { return ICAL_COMPONENT_VFREEBUSY; }

    /* OVERRIDES ICalComponent::MatchUID_seqNO */
    virtual t_bool MatchUID_seqNO(UnicodeString uid, t_int32 iSeqNo);
   
    /*  -- End of ICALComponent interface -- */

    /* -- GETTERS AND SETTERS -- */

    /* ATTENDEE */ 
    void addAttendee(Attendee * a);
    JulianPtrArray * getAttendees() const { return m_AttendeesVctr ; }
   
    /* COMMENT */
    void addComment(UnicodeString s, JulianPtrArray * parameters = 0);
    void addCommentProperty(ICalProperty * prop);
    JulianPtrArray * getComment() const { return m_CommentVctr; }

    /* CREATED */
    DateTime getCreated() const;
    void setCreated(DateTime s, JulianPtrArray * parameters = 0);
    ICalProperty * getCreatedProperty() const { return m_Created; }

    /* DURATION */
    Duration getDuration() const;
    void setDuration(Duration s, JulianPtrArray * parameters = 0);
    ICalProperty * getDurationProperty() const { return m_Duration; }
 
    /* DTSTART */
    DateTime getDTStart() const;
    void setDTStart(DateTime s, JulianPtrArray * parameters = 0);
    ICalProperty * getDTStartProperty() const { return m_DTStart; }

    /* DTSTAMP */
    DateTime getDTStamp() const;
    void setDTStamp(DateTime s, JulianPtrArray * parameters = 0);
    ICalProperty * getDTStampProperty() const { return m_DTStamp; }

    /* DTEND */
    DateTime getDTEnd() const;
    void setDTEnd(DateTime s, JulianPtrArray * parameters = 0);
    ICalProperty * getDTEndProperty() const { return m_DTEnd; }

    /* FREEBUSY */
    void addFreebusy(Freebusy * f); 
    JulianPtrArray * getFreebusy() const { return m_FreebusyVctr ; }
    void removeAllFreebusy();

    /* LAST-MODIFIED */
    DateTime getLastModified() const;
    void setLastModified(DateTime s, JulianPtrArray * parameters = 0);
    ICalProperty * getLastModifiedProperty() const { return m_LastModified; }
    
    /* ORGANIZER */
    UnicodeString getOrganizer() const;
    void setOrganizer(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getOrganizerProperty() const { return m_Organizer; }

    /* RELATED-TO */
    /*void addRelatedTo(UnicodeString s, JulianPtrArray * parameters = 0);
    void addRelatedToProperty(ICalProperty * prop);
    JulianPtrArray * getRelatedTo() const { return m_RelatedToVctr; }*/
    
    /* REQUEST-STATUS */
    UnicodeString getRequestStatus() const;
    void setRequestStatus(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getRequestStatusProperty() const { return m_RequestStatus; }

    /* SEQUENCE */
    t_int32 getSequence() const;
    void setSequence(t_int32 i, JulianPtrArray * parameters = 0);
    ICalProperty * getSequenceProperty() const { return m_Sequence; }

    /* UID */
    UnicodeString getUID() const;
    void setUID(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getUIDProperty() const { return m_UID; }

    /* URL */
    /*void addURL(UnicodeString s, JulianPtrArray * parameters = 0);
    void addURLProperty(ICalProperty * prop);
    JulianPtrArray * getURL() const { return m_URLVctr; }*/
    UnicodeString getURL() const;
    void setURL(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getURLProperty() const { return m_URL; }

    /* XTOKENS: NOTE: a vector of strings, not a vector of ICalProperties */
    void addXTokens(UnicodeString s);
    JulianPtrArray * getXTokens() { return m_XTokensVctr; }

    /* Method */
    void setMethod(UnicodeString & s) { m_sMethod = s; }
    UnicodeString & getMethod()       { return m_sMethod; }
    
    /**
     * Sets Attendee with the name sAttendeeFilter to status.
     * If the status is Delegated, then adds each element in delegatedTo
     * to the delegatedTo vector of that attendee.
     * If that attendee does not exist in the attendee list, then add
     * a new Attendee to the attendee list with his/her PARTSTAT set to status.
     * @param           sAttendeeFilter     attendee to set status on
     * @param           status              status to set 
     * @param           delegatedTo         vector of delegatedTo names to set to 
     */
    void setAttendeeStatus(UnicodeString & sAttendeeFilter, Attendee::STATUS status,
        JulianPtrArray * delegatedTo = 0);

    /**
     * Return the Attendee in attendee vector with name equal
     * to sAttendee.  Return 0 if no attendee with that name.
     * @param           sAttendee   name of attendee
     *
     * @return          Attendee whose name is sAttendee, 0 if not found
     */
    Attendee * getAttendee(UnicodeString sAttendee);

    /**
     * Sets DTStamp to current time value. 
     */
    void stamp();

    /*void update(VFreebusy * vfUpdated);*/
    /*t_bool IsIntersectingTimePeriod(DateTime start, DateTime end);*/

    

    /**
     * Alters the freebusy vector from
     * FB1: p1, p2, p3
     * FB2: p4, p5
     * FB3: p6, p7
     * to
     * FB1: p1
     * FB2: p2
     * FB3: p3
     * FB4: p4
     * FB5: p5
     * FB6: p6
     * FB7: p7
     * where FB1-7 will be sorted chronologically by start and normalized
     */
    void normalize();
    
    /**
     * Compare VFreebusy by UID.  Used by JulianPtrArray::QuickSort method.
     * @param           a       first VFreebusy
     * @param           b       second VFreebusy
     *
     * @return          a.getUID().compareTo(b.getUID())
     */
    static int CompareVFreebusyByUID(const void * a, const void * b);
    
    /**
     * Compare VFreebusy by DTSTART.  Used by JulianPtrArray::QuickSort method.
     * @param           a       first VFreebusy
     * @param           b       second VFreebusy
     *
     * @return          a.getDTSTART().compareTo(b.getDTSTART())
     */
    static int CompareVFreebusyByDTStart(const void * a, const void * b);

private:
    UnicodeString m_sMethod;

    /* -- MEMBERS -- */
    JulianPtrArray * m_AttendeesVctr;
    JulianPtrArray * m_CommentVctr;
    ICalProperty * m_Created;
    ICalProperty * m_Duration;
    ICalProperty * m_DTEnd;
    ICalProperty * m_DTStart;
    ICalProperty * m_DTStamp;
    JulianPtrArray * m_FreebusyVctr;
    ICalProperty * m_LastModified;
    ICalProperty * m_Organizer;
    ICalProperty * m_RequestStatus;
    /*JulianPtrArray * m_RelatedToVctr;*/
    ICalProperty * m_Sequence;
    ICalProperty * m_UID;
    
    /*JulianPtrArray * m_URLVctr;*/
    ICalProperty * m_URL;
    
    JulianPtrArray * m_XTokensVctr;
    /*NSCalendar m_Parent;*/

    JLog * m_Log;

};

#endif /* __VFREEBUSY_H_ */

