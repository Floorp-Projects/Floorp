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

#ifndef nsObject_h__
#define nsObject_h__

#include "nsdefs.h"
#include "nsCList.h"
#include "nsISupports.h"
#include "prmon.h"

/**
 * nsObject is the base class for all native widgets 
 */

class nsObject 
{

public:
                            nsObject();
    virtual                 ~nsObject();
    
protected:
    //
    // Data members:
    //
    CList m_link;

public:

    //
    // The following data members are used to maintain a chain of all
    // allocated nsObject instances. This chain is traversed at 
    // shutdown to clean up any dangling instances...
    //
    static CList      s_liveChain;
    static PRMonitor  *s_liveChainMutex;

#ifdef _DEBUG 
    static int32 s_nObjects;
#endif

    static void DeleteAllObjects(void);

};


#endif // nsObject_h__
