/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIToolkit_h__
#define nsIToolkit_h__

#include "nsISupports.h"
#include "prthread.h"

// {18032BD0-B265-11d1-AA2A-000000000000}
#define NS_ITOOLKIT_IID      \
{ 0x18032bd0, 0xb265, 0x11d1, \
    { 0xaa, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


/**
 *
 * Toolkit to associate with widgets.
 * Each widget that is created must belong to a specific toolkit
 * The toolkit is used to switch to the appropriate thread when
 * the message pump for the widget is processed.
 */

class nsIToolkit : public nsISupports {

  public:

    /**
     * Initialize this toolkit with aThread. 
     * @param aThread The thread passed in runs the message pump.
     *  NULL can be passed in, in which case a new thread gets created
     *  and a message pump will run in that thread
     *
     */
    virtual void Init(PRThread *aThread) = 0;

};

#endif
