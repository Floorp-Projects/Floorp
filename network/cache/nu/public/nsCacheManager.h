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

#ifndef _CacheManager_H_
#define _CacheManager_H_
/* 
 * nsCacheManager
 * Design and original implementation 
 * by Gagan Saksena 02/02/98
 * 
 */

#if 0
#   include "nsISupports.h"
#endif

#include <prlog.h>

#include "nsCacheModule.h"
#include "nsCacheObject.h"

class nsMemModule;
class nsDiskModule;
class nsCachePref;
class nsCacheBkgThd;

class nsCacheManager //: public nsISupports
{

public:

    //Reserved modules
    enum modules
    {
       MEM =0,
       DISK=1
    };

    nsCacheManager();
    ~nsCacheManager();

    PRInt32         AddModule(nsCacheModule* i_cacheModule);
    
    PRBool          Contains(const char* i_url) const;
    
    /* Number of modules in the cache manager */
    PRInt32         Entries() const;
    
    /* Singleton */
    static nsCacheManager* 
                    GetInstance();
    
    nsCacheObject*  GetObject(const char* i_url) const;
    nsCacheModule*  GetModule(PRInt32 i_index) const;

    nsMemModule*    GetMemModule() const;
    nsDiskModule*   GetDiskModule() const;

    PRBool          IsOffline(void) const;

    void            Offline(PRBool bSet);

    const char*     Trace() const;

    /* Performance measure- microseconds */
    PRUint32        WorstCaseTime(void) const;

protected:
    void            Init();
    nsCacheModule*
                    LastModule() const;

private:
    nsCacheModule*  m_pFirstModule;

    nsCacheManager(const nsCacheManager& cm);
    nsCacheManager& operator=(const nsCacheManager& cm);
    nsCacheBkgThd*  m_pBkgThd;
    PRBool          m_bOffline;
};

inline
nsDiskModule* nsCacheManager::GetDiskModule() const
{
    PR_ASSERT(m_pFirstModule && m_pFirstModule->Next());
    return (m_pFirstModule) ? (nsDiskModule*) m_pFirstModule->Next() : NULL;
}

inline
nsMemModule* nsCacheManager::GetMemModule() const
{
    PR_ASSERT(m_pFirstModule);
    return (nsMemModule*) m_pFirstModule;
}

inline
PRBool nsCacheManager::IsOffline(void) const
{
    return m_bOffline;
}

inline
void  nsCacheManager::Offline(PRBool i_bSet) 
{
    m_bOffline = i_bSet;
}

#endif
