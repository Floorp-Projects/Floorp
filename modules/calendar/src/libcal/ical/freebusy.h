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
 * freebusy.h
 * John Sun
 * 2/18/98 1:35:51 PM
 */

#ifndef __FREEBUSY_H_
#define __FREEBUSY_H_

#include <unistring.h>
#include "datetime.h"
#include "period.h"
#include "prprty.h"
#include "jlog.h"

/**
 *  The Freebusy class encapsulates the data in the iCalendar
 *  FREEBUSY property.  A Freebusy contains two parameters, a TYPE and BSTAT
 *  parameters.  These are repesented by the enumerations (FB_TYPE).
 *  (FB_STATUS ifdef'd out 4-28-98).
 *  The Freebusy must contain periods, which are contained in a vector of periods.
 */
class Freebusy: public ICalProperty
{
public:

    /* 4-28-98 combine TYPE and STATUS into just TYPE */
#if 0
    /** enumeration of TYPE parameters */
    enum FB_TYPE{FB_TYPE_BUSY = 0, FB_TYPE_FREE = 1, 
        FB_TYPE_LENGTH = 2, FB_TYPE_INVALID = -1
    };

    /** enumeration of BSTAT parameters */
    enum FB_STATUS{FB_STATUS_BUSY = 0, FB_STATUS_UNAVAILABLE = 1, 
        FB_STATUS_TENTATIVE = 2,
        FB_STATUS_LENGTH = 3, FB_STATUS_INVALID = -1
    };

    /** list acceptable keywords properties in a FREEBUSY */
    enum FB_PROPS{FB_PROPS_TYPE = 0, FB_PROPS_STATUS};
#else
    /** enumeration of TYPE parameters */
    enum FB_TYPE{FB_TYPE_BUSY = 0, FB_TYPE_FREE = 1, 
        FB_TYPE_BUSY_UNAVAILABLE, FB_TYPE_BUSY_TENTATIVE,
        FB_TYPE_LENGTH = 2, FB_TYPE_INVALID = -1};

    /** list acceptable keywords properties in a FREEBUSY */
    enum FB_PROPS{FB_PROPS_TYPE = 0};
#endif

    /**
     * Converts TYPE string to a FB_TYPE enumeration value
     * @param           sType   type string
     *
     * @return          FB_TYPE that represents that string 
     */
    static Freebusy::FB_TYPE stringToType(UnicodeString & sType); 
    
    /**
     * Converts FB_TYPE to string.  If bad type, return default (BUSY).
     * @param           type    FB_TYPE enumeration value
     * @param           out     output type string
     *
     * @return          output type string (out)
     */
    static UnicodeString & typeToString(Freebusy::FB_TYPE type, UnicodeString & out);

#if 0
    /**
     * Converts BSTAT string to a FB_STATUS enumeration
     * @param           sStatus status string
     *
     * @return          FB_STATUS that represents that string 
     */
    static Freebusy::FB_STATUS stringToStatus(UnicodeString & sType); 
    
    /**
     * Converts FB_STATUS to string.  If bad status, return default (BUSY).
     * @param           attstat    FB_STATUS enumeration value
     * @param           out     output BSTAT string
     *
     * @return          output BSTAT string (out)
     */
    static UnicodeString & statusToString(Freebusy::FB_STATUS status, UnicodeString & out);
#endif

private:

#if 0
    /** default FB_STATUS value */
    static const t_int32 ms_iDEFAULT_STATUS;
#endif

    /** default FB_TYPE value */
    static const t_int32 ms_iDEFAULT_TYPE;

    /* DATA-MEMBERS */
    /* storing type and status as ints instead of strings */

    /** type value.  Default is FB_TYPE_BUSY. */
    t_int32 m_Type;

#if 0
    /** bstat value.  Default is FB_STATUS_BUSY. */
    t_int32 m_Status;
#endif

    /** vector of freebusy periods */
    JulianPtrArray * m_vPeriod;

    /** ptr to log file to write errors to */
    JLog * m_Log;

    /* characters for printing Freebusy parameters */
    static const t_int32 ms_cFreebusyType;/*   = 'T';*/
    static const t_int32 ms_cFreebusyPeriod;/* = 'P';*/
#if 0
    static const t_int32 ms_cFreebusyStatus;/* = 'S';*/
#endif 


    /* -- private methods -- */

    /**
     * convert a character to the content of a parameter in 
     * human-readable string format
     *
     * @param c		a character represents a parameter
     * @return      parameter in a string
     */  
    UnicodeString & toStringChar(t_int32 c, UnicodeString & out);

    /**
     * Helper method to parse a string of periods.
     * @param           s           string containing periods
     * @param           vTimeZones  vector of timezones (usually 0)
     */
    void parsePeriod(UnicodeString & s, JulianPtrArray * vTimeZones);
    
    /**
     * Sets parameter with name paramName with value paramVal.
     * @param           paramName       parameter name to set
     * @param           paramVal        new value of parameter
     */
    void setParam(UnicodeString & paramName, UnicodeString & paramVal);

    /**
     * Fixes data-members are so that they are valid.  If properties
     * are invalid, then it will set it to defaults.
     */   
    void selfCheck();
    
    /**
     * Sets default value for property
     * @param           prop        property to set default value
     */
    void setDefaultProps(Freebusy::FB_PROPS prop);

    /**
     * Sort the periods vector by the starting time of periods.
     */
    void sortPeriods();

    /**
     * Sort the periods vector by the ending time of the periods.
     */
    void sortPeriodsByEndTime();

#if 0
    /**
     * Default constructor.  Hide from clients. 
     */
    Freebusy();
#endif

    /**
     * Copy constructor.
     * @param       that        Freebusy to copy
     */
    Freebusy(Freebusy & that);

public:

    /**
     * Constructor.  Also sets log file to write errors to.
     * @param           initLog     log file to write errors to
     */
    Freebusy(JLog * initLog = 0);
    
    /**
     * Destructor 
     */
    ~Freebusy();

    /*---------- To satisfy ICalProperty interface ----------*/
    /**
     * Sets parameters.
     * @param           parameters  vector of parameters to set
     *
     */
    virtual void setParameters(JulianPtrArray * parameters);
    
    /**
     * Return value of property.  Never use.  Use getPeriod().
     *
     * @return          value of property
     */
    virtual void * getValue() const { PR_ASSERT(FALSE); return 0; } 
    
    /**
     * Sets value of property.  Never use.  Use addPeriod().
     * @param           value   new value of property
     *
     */
    virtual void setValue(void * value) {PR_ASSERT(FALSE); if(value){} } 
    
    /**
     * Returns a clone of this property.
     * @param           initLog     the log file for clone to write to
     *
     * @return          clone of this ICalProperty object
     */
    virtual ICalProperty * clone(JLog * initLog);
    
    /**
     * Checks whether this property is valid or not.  Period vector size 
     * must be greater than 0 to be TRUE.
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

    /**
     * Returns FB_TYPE value.
     *
     * @return          FB_TYPE value
     */
    Freebusy::FB_TYPE getType() const { return (Freebusy::FB_TYPE) m_Type; }
    
    /**
     * Sets FB_TYPE.
     * @param           i       new FB_TYPE value
     *
     */
    void setType(FB_TYPE type) {m_Type = type;}
    
#if 0
    /**
     * Returns FB_STATUS value.
     *
     * @return          FB_STATUS value
     */
    Freebusy::FB_STATUS getStatus() const { return (Freebusy::FB_STATUS) m_Status; }
    
    /**
     * Sets FB_STATUS.
     * @param           i       new FB_STATUS value
     *
     */
    void setStatus(FB_STATUS status) {m_Status = status;}
#endif

    /**
     * Return vector of periods. 
     *
     * @return          vector of periods
     */
    JulianPtrArray * getPeriod() const { return m_vPeriod; }
    

    /**
     * Adds a period to period vector.  It makes a copy of the period.
     * So client must delete p.  Freebusy does NOT take ownership of p.  
     * @param           p       period to add to vector
     */
    void addPeriod(Period * p);

    /**
     * Adds each period in argument vector to this Freebusy's 
     * period vector
     * @param           periods     vector of periods to add
     */
    void addAllPeriods(JulianPtrArray * periods);

    /*
    void removePeriod(Period *p)
    {
        PR_ASSERT(p != 0);
        PR_ASSERT(m_vPeriod != 0);
        m_vPeriod->Remove(p);
        delete p;
    }
    */

    /**
     * Return starting time of first period chronologically.
     *
     * @return          starting time of first period.
     */
    DateTime startOfFreebusyPeriod();
    
    /**
     * Return ending time of last period chronologically.
     *
     * @return          starting time of first period.
     */
    DateTime endOfFreebusyPeriod();

    /**
     * Parse a Freebusy property line and populate data.
     * @param           in      line to parse.
     */
    void parse(UnicodeString & in);
    
    /**
     * Given freebusy period values in string, parameters, and timezones 
     * populates Freebusy data members.
     * @param           propVal     freebusy period string
     * @param           parameters  freebusy parameters
     * @param           vTimeZones  vector of timezones to adjust dates to
     *
     */
    void parse(UnicodeString & sPeriods, JulianPtrArray * parameters, 
        JulianPtrArray * vTimeZones = 0);

    /**
     * Compresses overlapping periods in vector.
     */
    void normalizePeriods();

    /**
     * Compare method to pass into JulianPtrArray::QuickSort method.
     * Currently compares against first period in a vs. first period in b.
     * @param           a       first freebusy 
     * @param           b       second freebusy
     *
     * @return          ComparePeriods(a[0], b[0])
     */
    static int CompareFreebusy(const void * a, const void * b);

    /**
     * Cleanup method.  Deletes each Freebusy object in vector
     * @param           freebusy        vector of Freebusy objects
     */
    static void deleteFreebusyVector(JulianPtrArray * freebusy);

    /**
     * Check if two freebusy objects have the same TYPE and BSTAT.  If they 
     * have the same TYPE and BSTAT, return TRUE, otherwise return FALSE.
     * @param           f1          first freebusy
     * @param           f2          second freebusy
     *
     * @return          TRUE if TYPE and BSTAT are same in both freebusy, FALSE otherwise
     */
    static t_bool HasSameParams(Freebusy * f1, Freebusy * f2);
   
};

#endif /* __FREEBUSY_H_ */
