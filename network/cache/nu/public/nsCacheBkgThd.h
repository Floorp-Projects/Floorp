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

#ifndef nsCacheBkgThd_h__
#define nsCacheBkgThd_h__

#include "nsBkgThread.h"

class nsCacheBkgThd: public nsBkgThread
{

public:
            nsCacheBkgThd(PRIntervalTime iSleepTime);
    virtual ~nsCacheBkgThd();
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
    void        Run(void);

protected:

private:
    nsCacheBkgThd(const nsCacheBkgThd& o);
    nsCacheBkgThd& operator=(const nsCacheBkgThd& o);
};

#endif // nsCacheBkgThd_h__

