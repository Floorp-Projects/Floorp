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
 * sdprprty.h
 * John Sun
 * 3/2/98 9:37:03 AM
 */

#include "prprty.h"
#include "icalprm.h"
#include <unistring.h>

#ifndef __STANDARDPROPERTY_H_
#define __STANDARDPROPERTY_H_

/**
 * StandardProperty is an abstract class that encapsulates the similar state of the
 * BooleanProperty, DateTimeProperty, DurationProperty, IntegerProperty,
 * PeriodProperty, and StringProperty classes.
 * This mainly includes holding the ICAL parameters in a JulianPtrArray.
 * It also defines a few methods to set, get, print parameters.
 */
class StandardProperty: public ICalProperty 
{
protected:
    /** the parameters list of the property */
    JulianPtrArray * m_vParameters;

#if 0
    /** ptr to log.  Don't delete it */
    JLog * m_Log;
#endif

    /**
     * Add a parameter to the parameter list
     * @param           parameter   parameter to add
     */
    void addParameter(ICalParameter * parameter);
    
    /**
     * Constructor.  Hidden from clients.
     */
    StandardProperty();

    /**
     * Constructor.  Makes copy of parameters.
     * @param           parameters   parameter array to copy
     */
    StandardProperty(JulianPtrArray * parameters);
public:

    /**
     * Virtual destructor deletes parameters.
     */
    virtual ~StandardProperty();

    /*---------------------------------------------------------------------*/
    /*-- ICALPROPERTY interface --*/

    /**
     * Pure virtual method. Return the value of the property 
     * @return      void * ptr to value of the property
     */    
    virtual void * getValue() const { PR_ASSERT(FALSE); return 0;}
    
    /**
     * Pure virtual method.  Set the value of the property
     * @param           value   the value to set property to
     */
    virtual void setValue(void * value) { PR_ASSERT(FALSE); if (value != 0) {} ; }
   
    /**
     * Pure virtual method.  Returns a clone of this property.  
     * Clients are responsible for deleting the returned object.
     *
     * @return          a clone of this property. 
     */
    virtual ICalProperty * clone(JLog * initLog) { PR_ASSERT(FALSE); if (initLog) {} return 0; }

    /**
     * Pure virtual method.  Return TRUE if property is valid, FALSE otherwise
     *
     * @return          TRUE if is valid, FALSE otherwise
     */
    virtual t_bool isValid() { PR_ASSERT(FALSE); return FALSE; }
 
    /**
     * Pure virtual method.  Return a string in human-readable form of the property.
     * @param           out     contains output human-readable string
     *
     * @return          the property in its human-readable string format
     */
    virtual UnicodeString & toString(UnicodeString & dateFmt, 
        UnicodeString & out) 
    { PR_ASSERT(FALSE); if (dateFmt.size() > 0) {} ; return out; }
    
    /**
     * Pure virtual method.  Return a string in human-readable form of the property.
     * @param           dateFmt     format string options
     * @param           out         contains output human-readable string
     *
     * @return          the property in its human-readable string format
     */
    virtual UnicodeString & toString(UnicodeString & out) { PR_ASSERT(FALSE); return out; }

    /*-- End of ICALPROPERTY interface --*/
    /*---------------------------------------------------------------------*/

    /**
     * Pure virtual method.  Return the property's string in the ICAL format.
     * @param           out     contains output ICAL string
     *
     * @return          the property in its ICAL string format
     */
    virtual UnicodeString & toExportString(UnicodeString & out) { PR_ASSERT(FALSE); return out; }

    /**
     * Given a parameter name, return the parameter value in paramVal. 
     * Return a status of 1 if paramName not found in parameters array.
     * @param           paramName  input parameter name
     * @param           paramVal   output parameter value if found
     * @param           status     ZERO_ERROR if OK, 1 is empty parameters, 2 is not found
     *
     * @return          output parameter value (paramVal)
     */
    UnicodeString & getParameterValue(UnicodeString & paramName, /* IN */
        UnicodeString & paramVal, ErrorCode & status);  /* OUT, OUT */

    /*void setParameter(UnicodeString & paramName, UnicodeString & paramValue)*/

    /**
     * Parse a line and populate data.
     * @param           in      line to parse.
     */
    void parse(UnicodeString & in);

    /**
     * Copies contents of parameters into this property's parameters
     * array.
     * @param           parameters  parameters to copy
     */
    void setParameters(JulianPtrArray * parameters);

    /*
    virtual void setParameter(UnicodeString & paramName, UnicodeString & paramValue)
    {
        PR_ASSERT(FALSE); if (paramName.size() > 0 && paramValue.size() > 0) {}   
    }
    */

    /**
     * Returns the property's ICAL format strings, 
     * to output string of the form
     * :value or ;param1;param2;param3(etc.):value
     *
     * @param           out    output ICAL string
     *
     * @return          output ICAL string (out)
     */
    UnicodeString & toICALString(UnicodeString & out);

    /**
     * Returns the property's ICAL format strings, inserting
     * sProp (property name) to output string of the form
     * sProp:value or sProp;param1;param2;param3(etc.):value
     *
     * @param           sProp  property name of property
     * @param           out    output ICAL string
     *
     * @return          output ICAL string (out)
     */
    UnicodeString & toICALString(UnicodeString & sProp,
        UnicodeString & out);
};

#endif /* __STANDARDPROPERTY_H_ */

