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
    
    nsCachePref(void);
    ~nsCachePref();

    enum Refresh 
    {
        NEVER,
        ONCE,
        ALWAYS
    } r;

    static const PRUint32   BkgSleepTime(void);
    
    static const char*      DiskCacheDBFilename(void); /* like Fat.db */
    static const char*      DiskCacheFolder(void);   /* Cache dir */

    static PRBool           DiskCacheSSL(void);
    static void             DiskCacheSSL(PRBool bSet);

    static PRUint32         DiskCacheSize(void);
    static PRUint32         MemCacheSize(void);

    static nsCachePref::Refresh
                            Frequency(void);

    /* Revalidating in background, makes IMS calls in the bkg thread to 
       update cache entries. TODO, this should be at a bigger time period
       than the cache cleanup routine */
    static PRBool           RevalidateInBkg(void);

/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
private:
    nsCachePref(const nsCachePref& o);
    nsCachePref& operator=(const nsCachePref& o);

    PRBool                  m_bRevalidateInBkg;
    PRBool                  m_bDiskCacheSSL;
    nsCachePref::Refresh    m_RefreshFreq;
    PRUint32                m_MemCacheSize;
    PRUint32                m_DiskCacheSize;
    char*                   m_DiskCacheDBFilename;
    char*                   m_DiskCacheFolder;
    PRUint32                m_BkgSleepTime;
};

#endif // nsCachePref_h__

