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
 * recid.h
 * John Sun
 * 3/20/98 5:08:46 PM
 */

#ifndef __RECURRENCEID_H_
#define __RECURRENCEID_H_

#include <unistring.h>
#include "datetime.h"
#include "prprty.h"
#include "jlog.h"

/**
 *  JulianRecurrenceID implements the RecurrenceID property.  The RecurrenceID
 *  must have a DateTime value.  It can optionally have a RANGE value set to
 *  THISANDPRIOR or THISANDFUTURE.  A RecurrenceID without a RANGE has its
 *  range value set to NONE.
 *  The JulianRecurrenceID must implement the ICalProperty interface.
 * 
 *  @see ICalProperty
 */
class JulianRecurrenceID : public ICalProperty
{
public:
    /** an enumeration of the RANGE parameter */
    enum RANGE{RANGE_NONE = 0, RANGE_THISANDPRIOR = 1, RANGE_THISANDFUTURE = 2};
private:
    
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    
    /** range parameter value */
    JulianRecurrenceID::RANGE m_Range;
    
    /** datetime value */
    DateTime m_DateTime;

    /** log to write errors to */
    JLog * m_Log;

    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    
    /**
     * Sets parameter with name paramName with value paramVal.
     * @param           paramName       parameter name to set
     * @param           paramVal        new value of parameter
     *
     */
    void setParam(UnicodeString & paramName, UnicodeString & paramVal);

#if 0
    /* hide constructor from clients */
    JulianRecurrenceID(); 
#endif 

    /**
     * Copy constructor.
     * @param           that        JulianRecurrenceID to copy
     */
    JulianRecurrenceID(JulianRecurrenceID & that);

public:
    
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/

    /**
     * Constructor.  Create a RecurrenceID with log set to initLog
     * @param           initLog     log file to write errors to
     */
    JulianRecurrenceID(JLog * initLog = 0);

    /**
     * Constructor.     Create a RecurrenceID with log set to initLog 
     * @param           datetime        initial DateTime value
     * @param           initLog         log file to write errors to
     * @param           range           initial RANGE value 
     */
    JulianRecurrenceID(DateTime datetime, JLog * initLog, RANGE range = RANGE_NONE);

    /**
     * Destructor
     */
    ~JulianRecurrenceID();
    
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 

    /**
     * Returns RANGE value.
     *
     * @return          current RANGE value
     */
    JulianRecurrenceID::RANGE getRange() const { return m_Range; }

    /**
     * Sets RANGE value 
     * @param           range   new range value
     *
     */
    void setRange(JulianRecurrenceID::RANGE range) { m_Range = range; }

    /**
     * Sets DateTime value.
     * @param           d       new DateTime value
     *
     */
    void setDateTime(DateTime d) { m_DateTime = d; }

    /**
     * Returns DateTime value
     *
     * @return          current DateTime value 
     */
    DateTime getDateTime() { return m_DateTime; }

    /*---------- To satisfy ICalProperty interface ----------*/
    /**
     * Sets parameters.
     * @param           parameters  vector of parameters to set
     *
     */
    virtual void setParameters(JulianPtrArray * parameters);
    
    /**
     * Return value of property.  Never use.  Use getDateTime().
     *
     * @return          value of property
     */
    virtual void * getValue() const; 
    
    /**
     * Sets value of property.  Never use.  Use setDateTime().
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
     * Checks whether this property is valid or not.  DateTime value 
     * must be valid in order to be TRUE.
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
    ** STATIC METHODS 
    **---------------------------*/

    /**
     * Converts string to RANGE enumeration.  Returns NONE if error. 
     * @param           sRange      range string
     *
     * @return          RANGE enumeration value of sRange
     */
    static JulianRecurrenceID::RANGE stringToRange(UnicodeString & sRange);

    /**
     * Converts RANGE to string.    Returns "" for NONE.
     * @param           range       RANGE value
     * @param           out         output range string
     *
     * @return          output range string
     */
    static UnicodeString & rangeToString(JulianRecurrenceID::RANGE range, UnicodeString & out);
    
};

#endif /* __RECURRENCEID_H_ */


