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
 * attendee.h
 * John Sun
 * 2/6/98 4:33:43 PM
 */

#ifndef __ATTENDEE_H_
#define __ATTENDEE_H_

#include "ptrarray.h"
#include "icalcomp.h"
#include "prprty.h"
#include "jlog.h"
#include "datetime.h"

/**
 *  The Attendee class encapsulates the data in the iCalendar 
 *  ATTENDEE property.  This class must implement the interface defined 
 *  by the ICalProperty abstract class.  The keyword parameters in Attendee 
 *  are stored as t_int32.  Other parameters are stored as UnicodeStrings 
 *  or as JulianPtrArray's of UnicodeStrings.
 */
class Attendee : public ICalProperty
{
private:

    /**
     *  For now, hide default constructor.  
     *  Currently must pass in a Log object to create an Attendee.  
     */
    Attendee();


public:
    
    /* enums of attendee keywords */
    /** Enumeration of ROLE parameter*/
    enum ROLE {
        /*ROLE_ATTENDEE = 0, ROLE_ORGANIZER = 1, ROLE_OWNER = 2, 
        ROLE_DELEGATE = 3, ROLE_LENGTH = 4,
        
        ROLE_CHAIR = 0, ROLE_PARTICIPANT = 1, ROLE_NON_PARTICIPANT = 2,
        ROLE_LENGTH = 3, ROLE_INVALID = -1*/

        ROLE_REQ_PARTICIPANT = 0, ROLE_CHAIR = 1, ROLE_OPT_PARTICIPANT = 2,
            ROLE_NON_PARTICIPANT = 3, ROLE_LENGTH = 4, ROLE_INVALID = -1
    };

    /** Enumeration of TYPE parameter*/
    enum TYPE {
        TYPE_INDIVIDUAL = 0, TYPE_GROUP = 1, 
        TYPE_RESOURCE = 2, TYPE_ROOM = 3, TYPE_UNKNOWN = 4, 
        TYPE_LENGTH = 5, TYPE_INVALID = -1
    };
    
    /** Enumeration of STATUS parameter*/
    enum STATUS {
        /* For VEVENT, VTODO, VJOURNAL */
        STATUS_NEEDSACTION = 0, STATUS_ACCEPTED = 1, STATUS_DECLINED = 2, 

        /* For VEVENT and VTODO only */
        STATUS_TENTATIVE = 3, 
        STATUS_DELEGATED = 4,

        /* For VTODO only */
        STATUS_COMPLETED = 5, STATUS_INPROCESS = 6,

        STATUS_LENGTH = 7, STATUS_INVALID = -1
    };

    /**
     * Is the status variable valid for this component type.  For example, 
     * if compType is VEVENT and status is INPROCESS, return FALSE.
     * But if compType is VTODO and status is INPROCESS, return TRUE.
     * @param           compType    component type
     * @param           status      status 
     *
     * @return          TRUE if valid status for compType, FALSE otherwise
     */
    static t_bool isValidStatus(ICalComponent::ICAL_COMPONENT compType, STATUS status);

    /** Enumeration of RSVP parameter */
    enum RSVP { 
        RSVP_FALSE = 0, RSVP_TRUE = 1, 
        RSVP_LENGTH = 2, RSVP_INVALID = -1
    };

    /** Enumeration of EXPECT parameter */
    enum EXPECT {
        EXPECT_FYI = 0, EXPECT_REQUIRE = 1, EXPECT_REQUEST = 2,
        /*EXPECT_IMMEDIATE = 3, EXPECT_LENGTH = 4, */ 
        EXPECT_LENGTH = 3, EXPECT_INVALID = -1
    };


    /** Enumeration of all keyword properties in an Attendee */
    enum PROPS {
        PROPS_ROLE = 0, PROPS_TYPE = 1, 
        PROPS_STATUS = 2, PROPS_RSVP = 3, 
        PROPS_EXPECT = 4
    };

    /**
     * Converts ROLE string to a ROLE enumeration
     * @param           sRole   role string
     *
     * @return          ROLE that represents that string 
     */
    static Attendee::ROLE stringToRole(UnicodeString & sRole);
    
    /**
     * Converts TYPE string to a TYPE enumeration
     * @param           sType   type string
     *
     * @return          TYPE that represents that string 
     */
    static Attendee::TYPE stringToType(UnicodeString & sType);

    /**
     * Converts PARTSTAT string to a STATUS enumeration
     * @param           sStatus status string
     *
     * @return          STATUS  that represents that string 
     */
    static Attendee::STATUS stringToStatus(UnicodeString & sStatus);

    /**
     * Converts RSVP string to a RSVP enumeration
     * @param           sRSVP   rsvp string
     *
     * @return          RSVP that represents that string
     */
    static Attendee::RSVP stringToRSVP(UnicodeString & sRSVP);

    /**
     * Converts EXPECT string to a EXPECT enumeration
     * @param           sExpect expect string
     *
     * @return          EXPECT that represents that string
     */
    static Attendee::EXPECT stringToExpect(UnicodeString & sExpect);

    /**
     * Converts ROLE to string.  If bad role, return default (REQ_PARTICIPANT).
     * @param           role    ROLE enumeration
     * @param           out     output role string
     *
     * @return          output role string (out)
     */
    static UnicodeString & roleToString(Attendee::ROLE role, UnicodeString & out);
    
    /**
     * Converts TYPE to string.  If bad type, return default (INDIVIDUAL).
     * @param           type    TYPE enumeration
     * @param           out     output type string
     *
     * @return          output type string (out)
     */
    static UnicodeString & typeToString(Attendee::TYPE type, UnicodeString & out);
    
    /**
     * Converts STATUS to string.  If bad status, return default (NEEDS-ACTION).
     * @param           status      STATUS enumeration
     * @param           out         output partstat string
     *
     * @return          output partstat string (out)
     */
    static UnicodeString & statusToString(Attendee::STATUS status, UnicodeString & out);
    
    /**
     * Converts RSVP to string.  If bad rsvp, return default (FALSE).
     * @param           rsvp    RSVP enumeration
     * @param           out     output rsvp string
     *
     * @return          output rsvp string (out)
     */
    static UnicodeString & rsvpToString(Attendee::RSVP rsvp, UnicodeString & out);
    
    /**
     * Converts EXPECT to string.  If bad expect, return default (FYI).
     * @param           expect    EXPECT enumeration
     * @param           out     output expect string
     *
     * @return          output expect string (out)
     */
    static UnicodeString & expectToString(Attendee::EXPECT expect, UnicodeString & out);

    /**
     * Cleanup method.  Delete each Attendee object in the vector
     * attendees.
     * @param           attendees   vector containing Attendee's to delete
     *
     */
    static void deleteAttendeeVector(JulianPtrArray * attendees);

    /**
     * Constructor.  Also sets log file to write errors to.
     * @param           ICalComponent::ICAL_COMPONENT componentType
     * @param           initLog     log file to write errors to
     */
    Attendee(ICalComponent::ICAL_COMPONENT componentType, JLog * initLog = 0);  
    
    /**
     * Destructor 
     */
    ~Attendee();

    /**
     * Factory method that returns a new default Attendee
     *
     * @return          static Attendee *
     */
    static Attendee * getDefault(ICalComponent::ICAL_COMPONENT compType,
        JLog * initLog = 0);

    /**
     * Given attendee name and parameters, populates Attendee
     * data members.
     * @param           propVal     attendee name
     * @param           parameters  attendee parameters
     *
     */
    void parse(UnicodeString & propVal, JulianPtrArray * parameters = 0);
    
    /**
     * Sets parameter with name paramName with value paramVal.
     * For MEMBER, DELEGATED-TO, DELEGATED-FROM, DIR, SENT-BY, the
     * double quotes are stripped.  THE MAILTO: is not stripped.
     *
     * @param           paramName       parameter name to set
     * @param           paramVal        new value of parameter
     */
    void setParam(UnicodeString & paramName, UnicodeString & paramVal);

    /**
     * convert a character to the content of a parameter in 
     * human-readable string format
     *
     * @param c		a character represents a parameter
     * @return      parameter in a human-readable string
     */  
    UnicodeString toStringChar(t_int32 c);


    /**
     * Return the first attendee in vAttendees with the name matching sAttendee.
     * @param           vAttendees      vector of attendees
     * @param           sAttendee       attendee name to look for
     *
     * @return          first Attendee with name sAttendee, 0 if not found.
     */
    static Attendee * getAttendee(JulianPtrArray * vAttendees, UnicodeString sAttendee);

    /*---------- To satisfy ICalProperty interface ----------*/
    /**
     * Sets parameters.
     * @param           parameters  vector of parameters to set
     *
     */
    virtual void setParameters(JulianPtrArray * parameters);
    
    /**
     * Return value of property.  Never use.  Use getName().
     *
     * @return          value of property
     */
    virtual void * getValue() const { PR_ASSERT(FALSE); return (void*) (&m_sName); }
    
    /**
     * Sets value of property.  Never use.  Use setName().
     * @param           value   new value of property
     *
     */
    virtual void setValue(void * value) { PR_ASSERT(FALSE); if (value) {}} ;
    
    /**
     * Returns a clone of this property.
     * @param           initLog     the log file for clone to write to
     *
     * @return          clone of this ICalProperty object
     */
    virtual ICalProperty * clone(JLog * initLog);
    
    /**
     * Checks whether this property is valid or not.  Must have MAILTO: in
     * front of attendee name to be valid.
     *
     * @return          TRUE if property is valid, FALSE otherwise
     */
    virtual t_bool isValid();
    
    /**
     * Return property to human-readable string.
     * @param           dateFmt     formatting string
     * @param           out         output human-readable string
     *
     * @return          output human-readable string (out)
     */
    virtual UnicodeString & toString(UnicodeString & strFmt, UnicodeString & out);
    
    /**
     * Return property to human-readable string.
     * @param           out         output human-readable string
     *
     * @return          output human-readable string (out)
     */
    virtual UnicodeString & toString(UnicodeString & out);
    
    /**
     * Return property to iCal property string.
     * @param           out         output iCal string
     *
     * @return          output iCal string (out)
     */
    virtual UnicodeString & toICALString(UnicodeString & out);
    
    /**
     * Return property to iCal property string.  Inserts sProp to
     * front of output string.  sProp should be the property name of this
     * property.
     * @param           sProp       property name to append to insert at front of out
     * @param           out         output iCal string with sProp in front
     *
     * @return          output iCal string with sProp in front (out)
     */
    virtual UnicodeString & toICALString(UnicodeString & sProp, UnicodeString & out);
    /*----------End of ICalProperty interface----------*/

    /* ---- GETTERS AND SETTERS ---- */
  
    /**
     * Sets Attendee name.
     * @param           sName   new Attendee name
     *
     */
    void setName(UnicodeString sName);

    /**
     * Returns Attendee name.
     *
     * @return          Attendee name
     */
    UnicodeString getName() const { return m_sName; }
    
    /**
     * Sets ROLE.
     * @param           i       new ROLE value
     *
     */
    void setRole(Attendee::ROLE i) { m_iRole = i; }
    
    /**
     * Sets TYPE.
     * @param           i       new TYPE value
     *
     */
    void setType(Attendee::TYPE i) { m_iType = i; }
    
    /**
     * Sets STATUS.
     * @param           i       new STATUS value
     *
     */
    void setStatus(Attendee::STATUS i) { m_iStatus = i; }
    
    /**
     * Sets RSVP.
     * @param           i       new RSVP value
     *
     */
    void setRSVP(Attendee::RSVP i) { m_iRSVP = i; }
    
    /**
     * Sets EXPECT.
     * @param           i       new EXPECT value
     *
     */
    void setExpect(Attendee::EXPECT i) { m_iExpect = i; }
    
    /**
     * Returns ROLE value.
     *
     * @return          ROLE value
     */
    Attendee::ROLE getRole() const { return (Attendee::ROLE) m_iRole; }
    
    /**
     * Returns TYPE value.
     *
     * @return          TYPE value
     */
    Attendee::TYPE getType() const { return (Attendee::TYPE) m_iType; }
    
    /**
     * Returns STATUS value.
     *
     * @return          STATUS value
     */
    Attendee::STATUS getStatus() const { return (Attendee::STATUS) m_iStatus; }
    
    /**
     * Returns RSVP value.
     *
     * @return          RSVP value
     */
    Attendee::RSVP getRSVP() const { return (Attendee::RSVP) m_iRSVP; }
    
    /**
     * Returns EXPECT value.
     *
     * @return          EXPECT value
     */
    Attendee::EXPECT getExpect() const { return (Attendee::EXPECT) m_iExpect; }

    /**
     * Add a name to the Delegated-To vector.
     * @param           sDelegatedTo        name to add to delegated-to vector
     */
    void addDelegatedTo(UnicodeString sDelegatedTo);
    
    /**
     * Add a name to the Delegated-From vector.
     * @param           sDelegatedFrom      name to add to delegated-from vector
     */
    void addDelegatedFrom(UnicodeString sDelegatedFrom);
    
    /**
     * Add a name to the Member vector.
     * @param           sMember             name to add to member vector
     */
    void addMember(UnicodeString sMember);
    
    /**
     * Return ptr to Delegated-To vector 
     *
     * @return          Delegated-To vector
     */
    JulianPtrArray * getDelegatedTo() const { return m_vsDelegatedTo; }
    
    /**
     * Return ptr to Delegated-From vector 
     *
     * @return          Delegated-From vector
     */
    JulianPtrArray * getDelegatedFrom() const { return m_vsDelegatedFrom; }
    
    /**
     * Return ptr to Member vector 
     *
     * @return          Member vector
     */
    JulianPtrArray * getMember() const { return m_vsMember; }
        
    /**
     * Return CN (common-name) value
     *
     * @return          CN value
     */
    UnicodeString getCN() const { return m_CN; }

    /**
     * Return Language value
     *
     * @return          Language value
     */
    UnicodeString getLanguage() const { return m_Language; }

    /**
     * Return DIR (location of directory info) value
     *
     * @return          DIR value
     */
    UnicodeString getDir() const { return m_Dir; }
    
    /**
     * Return SENT-BY (calendar user acting on behalf of the attendee) value
     *
     * @return          SENT-BY value
     */
    UnicodeString getSentBy() const { return m_SentBy; }

    /**
     * Set CN (common-name) value
     * @param           new CN value
     */
    void setCN(UnicodeString u) { m_CN = u; }
    
    /**
     * Set Language value
     * @param           new Language value
     */
    void setLanguage(UnicodeString u) { m_Language = u; }

    /**
     * Set DIR (location of directory info) value
     * @param           new DIR value
     */
    void setDir(UnicodeString u) { m_Dir = u; }
    
    /**
     * Set SENT-BY (calendar user acting on behalf of the attendee) value
     * @param           new SENT-BY value
     */
    void setSentBy(UnicodeString u) { m_SentBy = u; }

    void setDTStamp(DateTime d) { m_DTStamp = d; }
    void setSequence(t_int32 i) { m_Sequence = i; }

    /* list the default values for properties 
       (always to 0th element of enumeration) */

    /** the default ROLE value */
    static const t_int32 ms_iDEFAULT_ROLE;

    /** the default TYPE value */
    static const t_int32 ms_iDEFAULT_TYPE;
    
    /** the default STATUS value */
    static const t_int32 ms_iDEFAULT_STATUS;

    /** the default RSVP value */
    static const t_int32 ms_iDEFAULT_RSVP;

    /** the default EXPECT value*/
    static const t_int32 ms_iDEFAULT_EXPECT;
  
    /* ---- END OF PUBLIC STATIC DATA ---- */
private:
    
    /**
     * Copy constructor
     * @param           Attendee & that
     */
    Attendee(Attendee & that);

    /**
     * Fixes data-members are so that they are valid.  If properties
     * are invalid, then it will set it to defaults.
     */
    void selfCheck();

    /**
     * Fixes keyword property so that it is valid.  If property is
     * invalid (param is out of range), then set property to default value.
     * @param           prop            keyword property to check
     * @param           t_int32 param   current value of keyword property
     */
    void selfCheckHelper(Attendee::PROPS prop, t_int32 param);
    
    /** 
     * format string to print Attendee with PARTSTAT NOT 
     * set to NEEDS-ACTION.
     * @return          output formatted iCal Attendee string
     */
    UnicodeString formatDoneAction();

    /** 
     * format string to print Attendee with PARTSTAT NOT 
     * set to NEEDS-ACTION and Delegated-To of attendee. 
     * @return          output formatted iCal Attendee string
     */
    UnicodeString formatDoneDelegateToOnly();

    /** 
     * format string to print Attendee with PARTSTAT NOT 
     * set to NEEDS-ACTION and Delegated-From of attendee. 
     * @return          output formatted iCal Attendee string
     */
    UnicodeString formatDoneDelegateFromOnly();

    /* BELOW METHODS ALL ARE NEEDS ACTION */
    /* no need for %S, assumed to be NEEDS-ACTION */
    
    /** 
     * format string to print Attendee with PARTSTAT
     * set to NEEDS-ACTION.
     * @return          output formatted iCal Attendee string
     */
    UnicodeString formatNeedsAction();
    
    /** 
     * format string to print Attendee with PARTSTAT
     * set to NEEDS-ACTION and Delegated-To of attendee. 
     * @return          output formatted iCal Attendee string
     */
    UnicodeString formatDelegateToOnly();
    
    /** 
     * format string to print Attendee with PARTSTAT
     * set to NEEDS-ACTION and Delegated-From of attendee.
     * @return          output formatted iCal Attendee string
     */
    UnicodeString formatDelegateFromOnly();

    /**
     * Method that takes a format string and returns
     * the correctly formatted iCal Attendee string
     * @param           strFmt      format string
     *
     * @return          output formatted iCal Attendee string
     */
    UnicodeString format(UnicodeString strFmt);

    /**
     * Method that takes a character and returns parameter
     * in iCal output format.
     * @param           c       character representing parameter to print
     *
     * @return          parameter in iCal output format
     */
    UnicodeString formatChar(t_int32 c);

    /**
     * Strip double quotest from the beginning and end of string
     * if it has double-quotes at beginning and end.
     * @param           u       string to strip double-quotes from
     */
    /*static void stripDoubleQuotes(UnicodeString & u);*/

    /**
     * Inserts double quotes at start and end of string if 
     * string != "".
     * @param           us      string to add double-quotes to
     *
     * @return          string with double-quotes at start, end (us)
     */
    /*static UnicodeString & addDoubleQuotes(UnicodeString & us);*/
    
    /**
     * Prints each element in a vector of MAILTO:'s 
     * (i.e. member,delegated-to,delegated-from).
     * @param           propName        name of property (i.e. MEMBER)
     * @param           mailto          vector of MAILTO: to print
     * @param           out             output string
     *
     * @return          output string (out)
     */
    UnicodeString & printMailToVector(UnicodeString & propName,
        JulianPtrArray * mailto, UnicodeString & out);

    /* ---- PRIVATE STATIC DATA ---- */

    /* characters for printing Attendee parameters */
    static const t_int32 ms_cAttendeeName;/*          = 'N';*/
    static const t_int32 ms_cAttendeeRole;/*          = 'R';*/
    static const t_int32 ms_cAttendeeStatus;/*        = 'S';*/
    static const t_int32 ms_cAttendeeRSVP;/*          = 'V';*/
    static const t_int32 ms_cAttendeeType;/*          = 'T';*/
    static const t_int32 ms_cAttendeeExpect;/*        = 'E';*/
    static const t_int32 ms_cAttendeeDelegatedTo;/*   = 'D';*/
    static const t_int32 ms_cAttendeeDelegatedFrom;/* = 'd';*/
    static const t_int32 ms_cAttendeeMember;/*        = 'M';*/
    static const t_int32 ms_cAttendeeDir; /*          = 'l';  'el'*/
    static const t_int32 ms_cAttendeeSentBy;/*        = 's';*/
    static const t_int32 ms_cAttendeeCN;/*            = 'C';*/
    static const t_int32 ms_cAttendeeLanguage;/*      = 'm';*/
    static const t_int32 ms_cAttendeeDisplayName;/*   = 'z';*/

    /* ---- END OF PRIVATE STATIC DATA ---- */

    /* ---- PRIVATE DATA MEMBERS ---- */
  
    /**
     * the name of the Attendee.  Whatever is to the right of the colon
     * the name must be a URL.  Field must be filled to be valid Attendee.
     */
    UnicodeString m_sName;

    /** ptr to log file to write errors to */
    JLog * m_Log;

    /** the attendee's role value.  Default is REQ_PARTICIPANT. */
    t_int32 m_iRole;

    /** the attendee's type value.  Default is INDIVIDUAL. */
    t_int32 m_iType;

    /** the attendee's status value.  Default is NEEDS-ACTION. */
    t_int32 m_iStatus;

    /** the attendee's RSVP value.  Default is FALSE. */
    t_int32 m_iRSVP;

    /** the attendee's expect value.  Default is FYI.*/
    t_int32 m_iExpect;

    /** 
     * the list of members the attendee belongs to 
     * each element in members must be a MAILTO:
     */
    JulianPtrArray * m_vsMember;

    /**
     * the list of delegated-to attendees.
     * each element in delegated-to must be a MAILTO:
     */
    JulianPtrArray * m_vsDelegatedTo;

    /**
     * the list of delegated-from attendees.
     * each element in delegated-from must be a MAILTO:
     */ 
    JulianPtrArray * m_vsDelegatedFrom;

    /* added newer properties 3-23-98 */

    /** the common name of attendee.  Optional, default is "". */
    UnicodeString m_CN;

    /** the language of the common name of the attendee.  Optional, default is "". */
    UnicodeString m_Language;

    /** the MAILTO: of a calendar user acting on behalf of the attendee.  
     * Optional, default is "". 
     */
    UnicodeString m_SentBy;

    /** the location of the Attendee's directory information.  
     * Optional, default is "". */
    UnicodeString m_Dir;

    /** the type of the attendee status:  STATUS depends on this */
    ICalComponent::ICAL_COMPONENT m_ComponentType;

    DateTime m_DTStamp;
    t_int32 m_Sequence;

    /* ---- END OF  DATA MEMBERS ---- */
};

#endif /* __ATTENDEE_H_ */
