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
 * prprtyfy.h
 * John Sun
 * 2/12/98 5:17:48 PM
 */

#include "prprty.h"

#ifndef __ICALPROPERTYFACTORY_H_
#define __ICALPROPERTYFACTORY_H_

/**
 * ICalPropertyFactory is a factory class that creates 
 * ICalProperty instances.
 *
 * @see             ICalProperty 
 */
class ICalPropertyFactory
{
private:

    /**
     * Hide default constructor from use.
     */
    ICalPropertyFactory(); 

public:

    /**
     * Factory method that creates ICalProperty objects.
     * Clients should call this method to create ICalProperty 
     * objects.  Clients are responsible for de-allocating returned
     * object.
     *
     * @param           aType       the type of ICALProperty
     * @param           value       the initial value of the property   
     * @param           parameters  the initial parameters of the property
     *
     * @return          ptr to the  newly created ICalProperty object
     * @see             ICalProperty 
    */
    static ICalProperty * Make(ICalProperty::PropertyTypes aType,
        void * value, JulianPtrArray * parameters);
};

#endif /* __ICALPROPERTYFACTORY_H_ */



