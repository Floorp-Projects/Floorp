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
 * icompfy.h
 * John Sun
 * 2/20/98 10:44:52 AM
 */

#include "icalcomp.h"
#include "jlog.h"

#ifndef __ICALCOMPONENTFACTORY_H_
#define __ICALCOMPONENTFACTORY_H_

/**
 * ICalComponentFactory is a factory class that creates 
 * ICalComponent instances.
 *
 * @see             ICalComponent 
 */
class ICalComponentFactory
{
private:
    
    /**
     * Hide default constructor from use.
     */
    ICalComponentFactory(); 

public:

    /**
     * Factory method that creates ICalComponent objects.
     * Clients are responsible for calling parse method
     * to populate the returned ICalComponent.
     * Clients are also responsible for de-allocating 
     * return ICalComponent.
     * TODO: instead of passing string, why not pass a enum.
     *
     * @param           name        the name of the ICalComponent
     * @param           initLog     the log of the ICalComponent
     *
     * @return          ptr to the newly created ICalComponent object
     * @see             ICalComponent 
     */
    static ICalComponent * Make(UnicodeString & name, JLog * initLog);
};

#endif /* __ICALCOMPONENTFACTORY_H_ */
