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
 * prprty.h
 * John Sun
 * 2/10/98 4:03:56 PM
 */

#include <unistring.h>
#include "jdefines.h"
#include "ptrarray.h"
#include "icalprm.h"
#include "jatom.h"
#include "jlog.h"

#ifndef __ICALPROPERTY_H_
#define __ICALPROPERTY_H_

/**
 *  The ICalProperty class defines the interface for methods that
 *  all iCal Calendar properties must implement.  Also defines some
 *  static helper methods.
 */
class ICalProperty
{
public:
    /** Enumeration defines ICalProperty types */ 
    enum PropertyTypes{TEXT, DATETIME, INTEGER, PERIOD, DURATION, BOOLEAN};

protected:
    
    /**
     * Default constructor.  Hide from clients.  
     * Only subclasses allowed to use it. 
     */
    ICalProperty();

    /**
     * Copy constructor.  
     * @param           that    ICalProperty to copy
     */
    ICalProperty(ICalProperty & that);

public:
    
    /**
     * Virtual Destructor.
     */
    virtual ~ICalProperty() {}

    /* -- interface of methods that subclasses must implement (pure-virtual) -- */
 
    /**
     * Sets parameters.
     * @param           parameters  vector of parameters to set
     *
     */
    virtual void setParameters(JulianPtrArray * parameters) 
    { PR_ASSERT(FALSE); if (parameters) {} }

    /**
     * Return value of property
     *
     * @return          value of property
     */
    virtual void * getValue() const { PR_ASSERT(FALSE); return 0;}

    /**
     * Sets value of property.
     * @param           value   new value of property
     *
     */
    virtual void setValue(void * value) { PR_ASSERT(FALSE); if (value != 0) {} ; }
   
    /**
     * Returns a clone of this property.
     * @param           initLog     the log file for clone to write to
     *
     * @return          clone of this ICalProperty object
     */
    virtual ICalProperty * clone(JLog * initLog) 
    { PR_ASSERT(FALSE); if (initLog) {} return 0; }
    
    /**
     * Checks whether this property is valid or not
     *
     * @return          TRUE if property is valid, FALSE otherwise
     */
    virtual t_bool isValid() { PR_ASSERT(FALSE); return FALSE; }
 
    /**
     * Return property to human-readable string.
     * @param           dateFmt     formatting string
     * @param           out         output human-readable string
     *
     * @return          output human-readable string (out)
     */
    virtual UnicodeString & toString(UnicodeString & dateFmt, 
        UnicodeString & out) 
    { PR_ASSERT(FALSE); if (dateFmt.size() > 0) {} ; return out; }
    
    /**
     * Return property to human-readable string.
     * @param           out         output human-readable string
     *
     * @return          output human-readable string (out)
     */
    virtual UnicodeString & toString(UnicodeString & out) 
    { PR_ASSERT(FALSE); return out; }
    

    /**
     * Return property to iCal property string.
     * @param           out         output iCal string
     *
     * @return          output iCal string (out)
     */
    virtual UnicodeString & toICALString(UnicodeString & out) { PR_ASSERT(FALSE); return out; }

    /**
     * Return property to iCal property string.  Inserts sProp to
     * front of output string.  sProp should be the property name of this
     * property.
     * @param           sProp       property name to append to insert at front of out
     * @param           out         output iCal string with sProp in front
     *
     * @return          output iCal string with sProp in front (out)
     */
    virtual UnicodeString & toICALString(UnicodeString & sProp,
        UnicodeString & out)
    {  PR_ASSERT(FALSE); if (sProp.size() > 0) {} return out; }

#if 0 
    virtual UnicodeString & getParameterValue(UnicodeString & paramName, /* IN */
        UnicodeString & paramVal, ErrorCode & status)  /* OUT, OUT */
    {
        PR_ASSERT(FALSE); if (paramName.size() > 0 && paramVal.size() > 0 &&
            status) {}
        return paramVal;
    }
    
    virtual void setParameter(UnicodeString & paramName, UnicodeString & paramValue)
    {
        PR_ASSERT(FALSE); if (paramName.size() > 0 && paramValue.size() > 0) {}   
    }
    
    void parse(UnicodeString & in) { PR_ASSERT(FALSE); if (in.size() > 0) {}}
#endif

    /* -- END of interface of methods that subclasses must implement (pure-virtual) -- */

    
    /*----------------------------
    ** STATIC METHODS
    **--------------------------*/

    /**
     * Given propertyName, propertyValue, and parameters, returns the following string
     * - propertyName:parameters[0];parameters[1]...:propertyValue
     * @param   sProp       the property name
     * @param   sVal        the property value
     * @param   parameters  the parameters
     * @param   retVal      the output string
     * @return  retVal (the output string)
     */
    static UnicodeString & propertyToICALString(UnicodeString & sProp, UnicodeString & sVal,
        JulianPtrArray * parameters, UnicodeString & retVal);

    /**
     * prints string parameters to ICAL export format
     * @param           sParam      parameter name
     * @param           sVal        string parameter value
     * @param           out         output string
     *
     * @return          output string (out)
     */
    static UnicodeString & parameterToCalString(UnicodeString sParam, 
        UnicodeString & sVal, UnicodeString & out);
    
    /**
     * prints string parameters to ICAL export format 
     * @param           sParam      parameter name
     * @param           v           vector of strings parameter value
     * @param           out         output string
     *
     * @return          output string (out)
     */
    static UnicodeString & parameterVToCalString(UnicodeString param, 
        JulianPtrArray * v, UnicodeString & out);

    /**
     * Prints the property to ICAL format of form
     * propName;property.parameters:property.value.  
     * A null property returns "".
     * @param           propName        property name
     * @param           property        ICalProperty to print
     * @param           retVal          output string
     *
     * @return          output string (retVal)
     */
    static UnicodeString & propertyToICALString(UnicodeString & propName, 
        ICalProperty * property, UnicodeString & retVal);
    
    /**
     * Prints the properties to ICAL format of form
     * propName;property1.parameters:property1.value.  
     * propName;property2.parameters:property2.value.  
     * propName;property3.parameters:property3.value.  
     * A null property returns "".   
     * @param           propName        property name
     * @param           properties      vector of ICalProperty's to print
     * @param           retVal          output string
     *
     * @return          output string (retVal)
     */
    static UnicodeString & propertyVectorToICALString(UnicodeString & propName,
        JulianPtrArray * properties, UnicodeString & retVal);

    /**
     * Prints the strings to output.  Used for printing X-Tokens
     * @param           strings         vector of UnicodeStrings to print
     * @param           retVal          output string
     *
     * @return          output string (retVal)
     */
    static UnicodeString & vectorToICALString(JulianPtrArray * strings, UnicodeString & retVal);

    /**
     * Prints the property to human-readable format.  If property is null return "".
     * @param           property        ICalProperty to print   
     * @param           dateFmt         Formatting string to pass to property
     * @param           retVal          output string
     *
     * @return          output string (retVal)
     */
    static UnicodeString & propertyToString(ICalProperty * property, UnicodeString & dateFmt,
        UnicodeString & retVal);

    /**
     * Prints the property to human-readable format.  If property is null return "".
     * @param           property        ICalProperty to print
     * @param           retVal          Formatting string to pass to property
     *
     * @return          output string (retVal)
     */
    static UnicodeString & propertyToString(ICalProperty * property, UnicodeString & retVal);
    
    /**
     * Prints each ICalProperty in properties to human-readable format.
     * @param           properties      vector of ICalProperty's to print
     * @param           retVal          output string
     *
     * @return          output string (retVal)
     */
    static UnicodeString & propertyVectorToString(JulianPtrArray * properties,
        UnicodeString & retVal);


    /**
     * Prints each ICalProperty in properties to human-readable format.
     * @param           properties      vector of ICalProperty's to print
     * @param           dateFmt         Formatting string to pass to property
     * @param           retVal          output string
     *
     * @return          static UnicodeString 
     */
    static UnicodeString & propertyVectorToString(JulianPtrArray * properties,
        UnicodeString & dateFmt, UnicodeString & retVal);
    
    /**
     * Given a property name, a parameter name and parameter value,
     * make sure that the parameter name is valid for the property.
     * Doesn't do anything for the ATTENDEE, FREEBUSY, Recurrence-ID special
     * properties.
     * @param           propName        property name
     * @param           paramName       parameter name
     * @param           paramVal        parameter value
     *
     * @return          TRUE if parameter name is valid for property, FALSE otherwise
     */
    static t_bool checkParam(UnicodeString & propName, UnicodeString & paramName,
        UnicodeString & paramVal);

    /**
     * Given a vector of ICalParameters, an array of valid parameter names,
     * and the size of array, return TRUE if all parameters are in the range,
     * otherwise return FALSE.
     * @param           parameters              vector of ICalParameter's
     * @param           validParamNameRange[]   array of valid parameter name atoms
     * @param           validParamNameRangeSize size of validParamNameRange[]
     *
     * @return          TRUE if all parameters in range, FALSE otherwise.
     */
    static t_bool CheckParams(JulianPtrArray * parameters,
                               JAtom validParamNameRange[], 
                               t_int32 validParamNameRangeSize);

    /**
     * given a vector of ICalParameters, an array of valid parameter names,
     * and the size of array, an array of valid parameter values for the valueParamName 
     * type(usually VALUE, but can be RELTYPE),
     * return TRUE if all parameters are in the range and the VALUE parameter value
     * is in the validValueRange.  Otherwise return FALSE;
     * @param           parameters              vector of ICalParameter's
     * @param           validParamNameRange[]   array of valid parameter name atoms
     * @param           validParamNameRangeSize size of validParamNameRange[]
     * @param           validValueRange[]       array of valid parameter value atoms
     * @param           validValueRangeSize     size of validValudRange[]
     *
     * @return          TRUE if all parameter name and value in range, FALSE otherwise
     */
      static t_bool CheckParams(JulianPtrArray * parameters,
                              JAtom validParamNameRange[], 
                              t_int32 validParamNameRangeSize,
                              JAtom validValueRange[],
                              t_int32 validValueRangeSize);

    /**
     * Parse a line of iCal input and return output into outName, outVal,
     * and fill in parameters vector with parsed parameters of line.
     * @param           aLine       input iCal line
     * @param           outName     output property name
     * @param           outVal      output property value
     * @param           parameters  fill-in vector of parameters
     */
    static void parsePropertyLine(UnicodeString & aLine,
        UnicodeString & outName, UnicodeString & outVal,
        JulianPtrArray * parameters);


    /**
     * Trims UnicodeStrings.  The UnicodeString::trim() method
     * seems to only trim space-separators(ASCII 12 and ' ').  
     * This method also trims line-seperators(ASCII 13).
     * @param           string to trim.
     *
     * @return          trimmed string in s
     */
    static UnicodeString & Trim(UnicodeString & s);

    /**
     * Cleanup method.  Deletes each ICalParameter object in vector.
     * Doesn't delete actual vector though.
     * @param           parameters      vector of ICalParameter objects
     *
     */
    static void deleteICalParameterVector(JulianPtrArray * parameters);
    
    /**
     * Cleanup method.  Deletes each ICalProperty object in vector.
     * Doesn't delete actual vector though.
     * @param           parameters      vector of ICalProperty objects
     *
     */static void deleteICalPropertyVector(JulianPtrArray * properties);
    
    /**
     * Cleanup method.  Deletes each Period object in vector.
     * Doesn't delete actual vector though.
     * @param           parameters      vector of Period objects
     *
     */
    static void deletePeriodVector(JulianPtrArray * periods);

    static UnicodeString & multiLineFormat(UnicodeString & s);
    static t_bool IsXToken(UnicodeString & s);

    /**
     * Fills out vector with the clone of each ICalProperty in
     * the propertiesToClone vector.
     * @param           propertiesToClone   vector of properties to clone
     * @param           out                 output vector of clones    
     * @param           initLog             log file for clones to write to
     *
     */
    static void CloneICalPropertyVector(JulianPtrArray * propertiesToClone, JulianPtrArray * out,
        JLog * initLog);

    /**
     * Fills out vector with the clone of each UnicodeString in
     * the stringsToClone vector.
     * @param           stringsToClone      vector of strings to clone
     * @param           out                 output vector of clones of strings
     *
     */
    static void CloneUnicodeStringVector(JulianPtrArray * stringsToClone, JulianPtrArray * out);

};

#endif /* __ICALPROPERTY_H_ */
