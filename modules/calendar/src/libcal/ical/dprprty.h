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
 * dprprty.h
 * John Sun
 * 2/12/98 3:16:32 PM
 */

#include <unistring.h>
#include "sdprprty.h"
#include "datetime.h"

#ifndef __DATETIMEPROPERTY_H_
#define __DATETIMEPROPERTY_H_

/**
 * DateTimeProperty is a subclass of StandardProperty.  It implements the ICalProperty
 * interface for ICAL properties with TYPE=DATETIME or TYPE=DATE.
 */
class DateTimeProperty: public StandardProperty
{
private:

    /** the DateTime value of the property */
    DateTime m_DateTime;

    /**
     * Default constructor
     */
    DateTimeProperty();

    /**
     * Copy constructor
     * @param           that    property to copy
     */
    DateTimeProperty(const DateTimeProperty & that);
public:
    
    /**
     * Constructor that sets value of property to contents of value
     * and makes a copy of the contents of parameters ptr.
     * @param           value       value of property
     * @param           parameters  parameters of property
     */
    DateTimeProperty(DateTime value, JulianPtrArray * parameters);
    
    /**
     * Destructor
     */
    ~DateTimeProperty();

    /**
     * Return the value of the property 
     * @return      void * ptr to value of the property     
     */
    virtual void * getValue() const;

    /**
     * Set the value of the property
     * @param           value   the value to set property to
     */
    virtual void setValue(void * value);

    /**
     * Returns a clone of this property.  Clients are responsible for deleting
     * the returned object.
     *
     * @return          a clone of this property. 
     */
    virtual ICalProperty * clone(JLog * initLog);

    /**
     * Return TRUE if property is valid, FALSE otherwise
     *
     * @return          TRUE if is valid, FALSE otherwise
     */
    virtual t_bool isValid();

    /**
     * Return a string in human-readable form of the property.
     * @param           out     contains output human-readable string
     *
     * @return          the property in its human-readable string format
     */
    virtual UnicodeString & toString(UnicodeString & out);

    /**
     * Return a string in human-readable form of the property.  For DateTimes,
     * dateFmt would contain the a DateFormat string (i.e "MM/dd/yy hh:mm:ss a").
     * @param           dateFmt     format string options
     * @param           out         contains output human-readable string
     *
     * @return          the property in its human-readable string format
     */
    virtual UnicodeString & toString(UnicodeString & dateFmt, UnicodeString & out);

    /**
     * Return the property's string in the ICAL format.  For DateTimes, this
     * would be the ISO8601 UTC date format of the datetime (i.e. 19971110T112233Z).
     * @param           out     contains output ICAL string
     *
     * @return          the property in its ICAL string format
     */
    virtual UnicodeString & toExportString(UnicodeString & out);
};

#endif /* __DATETIMEPROPERTY_H_ */


