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

#ifndef nsCachePref_h__
#define nsCachePref_h__

//#include "nsISupports.h"
#include <prtypes.h>

class nsCachePref //: public nsISupports
{

public:
    static nsCachePref* GetInstance(void);
    
    // should really be protected/private
    nsCachePref(void);
    ~nsCachePref();

    enum Refresh 
    {
        NEVER,
        ONCE,
        ALWAYS
    } r;

    static const char*     DiskCacheDBFilename(void); /* like Fat.db */
    static const char*     DiskCacheFolder(void);   /* Cache dir */

    static PRUint32        DiskCacheSize(void);
    static PRUint32        MemCacheSize(void);

    static nsCachePref::Refresh
                    Frequency(void);

/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
private:
    nsCachePref(const nsCachePref& o);
    nsCachePref& operator=(const nsCachePref& o);
    static nsCachePref* m_pPref;
};

#endif // nsCachePref_h__

