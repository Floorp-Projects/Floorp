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

#ifndef OBJECT_H
#define OBJECT_H

#include "nsdefs.h"
#include "nsCList.h"
#include "nsISupports.h"
#include "prmon.h"

/**
 * nsObject is the base class for all native widgets 
 */

class nsObject : public nsISupports
{

public:
                            nsObject(nsISupports *aOuter);
    virtual                 ~nsObject();

    // Implementation of those, always forward to the outer object.
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);

    //
    // Derived classes have to implement this one
    //
    virtual nsresult        QueryObject(const nsIID& aIID, void** aInstancePtr);

protected:
    //
    // Data members:
    //
    CList m_link;

    nsISupports *mOuter;

private:

    class InnerSupport : public nsISupports {
    public:
        InnerSupport() {}

#define INNER_OUTER \
            ((nsObject*)((char*)this - offsetof(nsObject, mInner)))

        NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr)
                                { return INNER_OUTER->QueryObject(aIID, aInstancePtr); }
        NS_IMETHOD_(nsrefcnt) AddRef(void)
                                { return INNER_OUTER->AddRefObject(); }
        NS_IMETHOD_(nsrefcnt) Release(void)
                                { return INNER_OUTER->ReleaseObject(); }
    } mInner;
    friend InnerSupport;

    nsrefcnt mRefCnt;
    nsrefcnt AddRefObject(void);
    nsrefcnt ReleaseObject(void);

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

#define BASE_SUPPORT                                                                      \
          NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr)               \
                                { return nsObject::QueryInterface(aIID, aInstancePtr); }  \
          NS_IMETHOD_(nsrefcnt) AddRef(void)                                              \
                                { return nsObject::AddRef(); }                            \
          NS_IMETHOD_(nsrefcnt) Release(void)                                             \
                                { return nsObject::Release(); } 


#endif  // OBJECT_H

