/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
