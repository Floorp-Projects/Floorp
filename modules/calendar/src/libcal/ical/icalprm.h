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
 * icalprm.h
 * John Sun
 * 2/10/98 3:41:52 PM
 */

#include <unistring.h>
#include "ptrarray.h"

#ifndef __ICALPARAMETER_H_
#define __ICALPARAMETER_H_

/**
 * This is a simple class holding ICAL parameters in an ICalProperty.
 * NOTE: this class may eventually become abstract
 * since it limits only to have one parameter value,
 * so multiple parameter value aren't allowed
 * also only allows for string parameter values
 */
class ICalParameter
{
private:
    /** the parameter name of the parameter */
    UnicodeString m_ParameterName;

    /** the parameter value of the parameter */
    UnicodeString m_ParameterValue;  
public:

    /** default constructor, returns ("","") */
    ICalParameter(); 

    /**
     * Copy constructor
     * @param           that    ICalParameter to copy
     */
    ICalParameter(ICalParameter & that);

    /**
     * Normal constructor
     * @param           parameterName       initial parameter name
     * @param           parameterValue      initial parameter value
     */
    ICalParameter(const UnicodeString & parameterName, 
        const UnicodeString & parameterValue);


    /**
     * Returns a copy of this ICalParameter.  Clients responsible
     * for de-allocating returned ICalParameter *.
     *
     * @return          a clone of this ICalParameter
     */
    ICalParameter * clone();


    /**
     * Sets the parameter name.
     * @param           parameterName   new parameter name
     */
    void setParameterName(const UnicodeString & parameterName);

    /**
     * Sets the parameter value.
     * @param           parameterValue   new parameter value
     */
    void setParameterValue(const UnicodeString & parameterValue);


    /**
     * Return parameter name
     * @param           retName     return storage for parameter name
     *
     * @return          retName (storage for parameter name)
     */
    UnicodeString & getParameterName(UnicodeString & retName) const;

    /**
     * Return parameter value
     * @param           retValue     return storage for parameter value
     *
     * @return          retValue (storage for parameter value)
     */
    UnicodeString & getParameterValue(UnicodeString & retValue) const;


    /**
     * Returns parameter in string of form ";PARAMETERNAME=PARAMETERVALUE
     * @param           result  output string
     *
     * @return          result (output string)  
     */
    UnicodeString & toICALString(UnicodeString & result) const;

    /**
     * Assignment operator.
     * @param           d   ICalParameter to assign to.
     *
     * @return          
     */
    const ICalParameter & operator=(const ICalParameter & d);


    /**
     * Searches parameters for a first occurrence of ICalParameter with
     * parameter name equal to paramName.  Returns the value of parameter
     * in retVal.  Returns "", if no match.
     * @param           paramName   parameter name to look for
     * @param           retVal      return value of parameter
     * @param           parameters  parameters to search from
     *
     * @return          retVal (value of parameter)
     */
    static UnicodeString & GetParameterFromVector(UnicodeString & paramName,
        UnicodeString & retVal, JulianPtrArray * parameters);
};

#endif /* __ICALPARAMETER_H_ */


