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

/* nsCachePref. A class to separate the preference related code to 
 * one place. This is an incomplete implementation, I need to access
 * the libprefs directly, but that would add another dependency. And 
 * libpref comes with a JS dependency... so holding this off for the 
 * moment. 
 * 
 * -Gagan Saksena 09/15/98
 */
#ifndef nsCachePref_h__
#define nsCachePref_h__

//#include "nsISupports.h"
#include "prtypes.h"
#include "prlog.h"

class nsCachePref //: public nsISupports
{

public:
    static nsCachePref* GetInstance(void);
    
    nsCachePref(void);
    ~nsCachePref();

    enum Refresh 
    {
        ONCE,
        ALWAYS,
        NEVER
    };

    const PRUint32   BkgSleepTime(void);
    
    const char*      DiskCacheDBFilename(void); /* like Fat.db */
    const char*      DiskCacheFolder(void);   /* Cache dir */

    PRBool           DiskCacheSSL(void);
    void             DiskCacheSSL(PRBool bSet);

    PRUint32         DiskCacheSize(void);
    void             DiskCacheSize(const PRUint32 i_Size);

    PRUint32         MemCacheSize(void);
    void             MemCacheSize(const PRUint32 i_Size);

    nsCachePref::Refresh
                            Frequency(void);

    /* Revalidating in background, makes IMS calls in the bkg thread to 
       update cache entries. TODO, this should be at a bigger time period
       than the cache cleanup routine */
    PRBool          RevalidateInBkg(void);

    /* Setup all prefs */
    void            SetupPrefs(const char* i_Pref);

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

inline
PRUint32 nsCachePref::DiskCacheSize()
{
    return m_DiskCacheSize;
}

inline
void nsCachePref::DiskCacheSize(const PRUint32 i_Size)
{
    m_DiskCacheSize = i_Size;
}

inline
PRBool nsCachePref::DiskCacheSSL(void)
{
    return m_bDiskCacheSSL;
}

inline
void nsCachePref::DiskCacheSSL(PRBool bSet)
{
    m_bDiskCacheSSL = bSet;
}

inline
const char* 
nsCachePref::DiskCacheFolder(void)
{
    PR_ASSERT(m_DiskCacheFolder);
    return m_DiskCacheFolder;
}

inline
PRUint32 nsCachePref::MemCacheSize()
{
    return m_MemCacheSize;
}

inline
void nsCachePref::MemCacheSize(const PRUint32 i_Size)
{
    m_MemCacheSize = i_Size;
}

inline
PRBool nsCachePref::RevalidateInBkg(void)
{
    return m_bRevalidateInBkg;
}


#endif // nsCachePref_h__

