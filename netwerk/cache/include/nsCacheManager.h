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

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsICacheManager.h"

#include "prlog.h"

#include "nsMonitorable.h"
#include "nsICacheModule.h"
#include "nsICacheObject.h"
#include "nsCacheBkgThd.h"
#include <stdio.h>

// class nsMemModule;
// class nsDiskModule;
// class nsCachePref;
// class nsCacheBkgThd;


class nsCacheManager : public nsICacheManager , public nsMonitorable 
{

public:

    nsCacheManager();
    virtual ~nsCacheManager();

	NS_DECL_ISUPPORTS

    NS_IMETHOD Contains(const char* i_url ) const;
	// PRBool Contains(const char* i_url) const ;
    
    /* Number of modules in the cache manager */
    NS_IMETHOD Entries(PRInt16 * n_Entries) const;
	// PRInt16 				Entries( ) const ;

    NS_IMETHOD AddModule(PRInt16 * n_Index, nsICacheModule* pModule);
                            // InsertModule

    NS_IMETHOD GetModule (PRInt16 i_index, nsICacheModule** pmodule) const ;
    // nsCacheModule*          GetModule(PRInt16 i_index) const;

    NS_IMETHOD GetDiskModule(nsICacheModule** pmodule) const ;
    // nsDiskModule*           GetDiskModule() const;

    NS_IMETHOD GetMemModule(nsICacheModule** pmodule) const ;
    // nsMemModule*            GetMemModule() const;
 
    NS_IMETHOD GetPrefs(nsICachePref** pPref) const ;
    // nsCachePref*            GetPrefs(void) const;

    NS_IMETHOD GetObj(const char* i_url, void ** o_pObject) const;

    NS_IMETHOD InfoAsHTML(char** o_Buffer) const;

    NS_IMETHOD IsOffline(PRBool *bOffline) const;

    NS_IMETHOD Offline(PRBool bSet);

    NS_IMETHOD Remove(const char* i_url);

    /* Performance measure- microseconds */
    NS_IMETHOD WorstCaseTime(PRUint32 * o_Time) const;
   
    /* Singleton */
    static nsCacheManager*  GetInstance();

    // Initialize the cache manager. Constructor doesn't call this. 
    // So you must specify this separately. 

	// GetInstance will call it automatically. - yixiong
    void                    Init();

    // const char*             Trace() const;

protected:
    
    NS_IMETHOD ContainsExactly(const char* i_url, PRBool * bContain) const;
  
    nsICacheModule*          LastModule() const;
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
    nsCacheBkgThd*       m_pBkgThd;
    nsICacheModule*      m_pFirstModule;
    PRMonitor*           m_pMonitor;
    PRBool               m_bOffline;
    nsICachePref*        m_pPrefs;

    nsCacheManager(const nsCacheManager& cm);
    nsCacheManager& operator=(const nsCacheManager& cm);
	static nsCacheManager * gInstance ;
};

class nsCacheManagerFactory : public nsIFactory 
{
  public:
    nsCacheManagerFactory( ) ;
	virtual ~nsCacheManagerFactory( ) ;

	NS_DECL_ISUPPORTS

	NS_IMETHOD CreateInstance(nsISupports *aOuter, 
	                          const nsIID &aIID,
							  void **aResult) ;

	NS_IMETHOD LockFactory(PRBool aLock) ;
} ;

inline NS_IMETHODIMP
nsCacheManager::GetDiskModule( nsICacheModule ** pModule) const
{
    PR_ASSERT(m_pFirstModule);

	nsresult rv ;
	*pModule = nsnull ;

    // return (m_pFirstModule) ? (nsDiskModule*) m_pFirstModule->NextModule() : NULL;
	if (m_pFirstModule) {

	  rv = m_pFirstModule->GetNextModule(pModule) ;

	  if (NS_SUCCEEDED(rv) && *pModule){
        return NS_OK ;
	  }
//      else 
//	    return NS_ERROR_FAILURE ;
	}

	return NS_ERROR_FAILURE ;
}

inline NS_IMETHODIMP
nsCacheManager::GetMemModule(nsICacheModule ** pModule ) const
{
    PR_ASSERT(m_pFirstModule);
    *pModule = m_pFirstModule;

	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheManager::GetPrefs(nsICachePref ** pPref) const
{
    PR_ASSERT(m_pPrefs);
    *pPref =  m_pPrefs;

	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheManager::IsOffline(PRBool * bOffline) const
{
    *bOffline = m_bOffline;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheManager::Offline(PRBool i_bSet) 
{
    m_bOffline = i_bSet;
	return NS_OK ;
}

#endif
