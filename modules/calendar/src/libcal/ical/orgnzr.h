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
 * orgnzr.h
 * John Sun
 * 4/7/98 11:16:49 AM
 */

#ifndef __JULIANORGANIZER_H_
#define __JULIANORGANIZER_H_

#include "jlog.h"
#include "prprty.h"

/**
 *  Encapsulates a Calendar Component's organizer.
 *  Similar to the Attendee property, but doesn't
 *  have as much state.
 */
class JulianOrganizer : public ICalProperty
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/

    /**
     * the name of the JulianOrganizer.  Whatever is to the right of the colon
     * the name must be a URL.  Field must be filled to be valid Attendee.
     */
    UnicodeString m_Name;

    /** the common name of attendee.  Optional, default is "". */
    UnicodeString m_CN;

    /** the MAILTO: of a calendar user acting on behalf of the attendee.  
     * Optional, default is "". 
     */
    UnicodeString m_SentBy;

    /** the location of the Attendee's directory information.  
     * Optional, default is "". */
    UnicodeString m_Dir;

    /** the language of the common name of the attendee.  Optional, default is "". */
    UnicodeString m_Language;

    /** ptr to log file to write errors to */
    JLog * m_Log;

    static const t_int32 ms_cJulianOrganizerName;/*          = 'N';*/
    static const t_int32 ms_cJulianOrganizerDir; /*          = 'l';  'el'*/
    static const t_int32 ms_cJulianOrganizerSentBy;/*        = 's';*/
    static const t_int32 ms_cJulianOrganizerCN;/*            = 'C';*/
    static const t_int32 ms_cJulianOrganizerLanguage;/*      = 'm';*/
    static const t_int32 ms_cJulianOrganizerDisplayName;/*   = 'z';*/

    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    JulianOrganizer(JulianOrganizer & that);

public:

    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    JulianOrganizer(JLog * initLog = 0);
    ~JulianOrganizer();    

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
    virtual void * getValue() const;
    
    /**
     * Sets value of property.  Never use.  Use setName().
     * @param           value   new value of property
     *
     */
    virtual void setValue(void * value);
    
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

    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/
    
    /**
     * Sets JulianOrganizer name.
     * @param           sName   new Attendee name
     *
     */
    void setName(UnicodeString sName);

    /**
     * Returns JulianOrganizer name.
     *
     * @return          Attendee name
     */
    UnicodeString getName() const { return m_Name; }

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
     * Return SENT-BY (calendar user acting on behalf of the organizer) value
     *
     * @return          SENT-BY value
     */
    UnicodeString getSentBy() const { return m_SentBy; }

    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
    
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

    /*----------------------------- 
    ** STATIC METHODS 
    **---------------------------*/ 
    /*----------------------------- 
    ** OVERLOADED OPERATORS 
    **---------------------------*/ 
};

#endif /* __JULIANORGANIZER_H_ */

