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

/* 
 * nsCacheManager- The boss of cache architecture. Contains all "external"
 * functions for use by people who don't care/want to know the internals
 * of the cache architecture. 
 *
 * - Gagan Saksena 09/15/98
 *
 * Design and original implementation by Gagan Saksena 02/02/98
 * 
 */

#ifndef _CacheManager_H_
#define _CacheManager_H_

#if 0
#   include "nsISupports.h"
#endif

#include "prlog.h"

#include "nsMonitorable.h"
#include "nsCacheModule.h"
#include "nsCacheObject.h"

class nsMemModule;
class nsDiskModule;
class nsCachePref;
class nsCacheBkgThd;

class nsCacheManager : public nsMonitorable //: public nsISupports
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

    PRInt32                 AddModule(nsCacheModule* i_cacheModule);
                            // InsertModule
    PRBool                  Contains(const char* i_url) const;
    
    /* Number of modules in the cache manager */
    PRInt16                 Entries() const;
    
    /* Singleton */
    static nsCacheManager*  GetInstance();
    
    nsDiskModule*           GetDiskModule() const;

    nsMemModule*            GetMemModule() const;

    nsCacheModule*          GetModule(PRInt16 i_index) const;

    nsCacheObject*          GetObj(const char* i_url) const;

    nsCachePref*            GetPrefs(void) const;

    // Initialize the cache manager. Constructor doesn't call this. 
    // So you must specify this separately. 
    void                    Init();

    void                    InfoAsHTML(char* o_Buffer) const;

    PRBool                  IsOffline(void) const;

    void                    Offline(PRBool bSet);

    PRBool                  Remove(const char* i_url);

    const char*             Trace() const;

    /* Performance measure- microseconds */
    PRUint32                WorstCaseTime(void) const;

protected:
    
    PRBool                  ContainsExactly(const char* i_url) const;
  
    nsCacheModule*          LastModule() const;
    //PRBool                  Lock(void);
    //void                    Unlock(void);

/*
    class MgrMonitor
    {
    public:
        MgrMonitor() { nsCacheManager::GetInstance()->Lock();}
        ~MgrMonitor() { nsCacheManager::GetInstance()->Unlock();}
    };

    friend MgrMonitor;
*/

private:
    nsCacheBkgThd*      m_pBkgThd;
    nsCacheModule*      m_pFirstModule;
    PRMonitor*          m_pMonitor;
    PRBool              m_bOffline;
    nsCachePref*        m_pPrefs;

    nsCacheManager(const nsCacheManager& cm);
    nsCacheManager& operator=(const nsCacheManager& cm);
};

inline
nsDiskModule* nsCacheManager::GetDiskModule() const
{
    PR_ASSERT(m_pFirstModule && m_pFirstModule->NextModule());
    return (m_pFirstModule) ? (nsDiskModule*) m_pFirstModule->NextModule() : NULL;
}

inline
nsMemModule* nsCacheManager::GetMemModule() const
{
    PR_ASSERT(m_pFirstModule);
    return (nsMemModule*) m_pFirstModule;
}

inline
nsCachePref* nsCacheManager::GetPrefs(void) const
{
    PR_ASSERT(m_pPrefs);
    return m_pPrefs;
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
