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
 * Gagan Saksena  02/02/98
 * 
 */

#include "prtypes.h"
#include "pratom.h"
#include "nscore.h"
#include "prinrval.h"
#include "plstr.h"
#include "prprf.h" //PR_smprintf and PR_smprintf_free

#include "nsISupports.h"

#include "nsRepository.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

// #include "nsCacheTrace.h"
// #include "nsCachePref.h"
#include "nsICacheModule.h"
//#include "nsMemModule.h"
//#include "nsDiskModule.h"
#include "nsCacheBkgThd.h"

// #include "nsMemModule.h"
// #include "nsDiskModule.h"
#include "nsCacheManager.h"

/* TODO move this to InitNetLib */
// static nsCacheManager TheManager;
nsCacheManager * nsCacheManager::gInstance = NULL ;

static PRInt32 gLockCnt = 0 ;
static PRInt32 gInstanceCnt = 0 ;

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID) ;
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID) ;
static NS_DEFINE_IID(kICacheManagerIID, NS_ICACHEMANAGER_IID) ;
static NS_DEFINE_IID(kCacheManagerCID, NS_CACHEMANAGER_CID) ;

static NS_DEFINE_IID(kICachePrefIID, NS_ICACHEPREF_IID) ;
static NS_DEFINE_IID(kCachePrefCID, NS_CACHEPREF_CID) ;

static NS_DEFINE_IID(kICacheModuleIID, NS_ICACHEMODULE_IID) ;
static NS_DEFINE_IID(kCacheDiskModuleCID, NS_DISKMODULE_CID) ;
static NS_DEFINE_IID(kCacheMemModuleCID, NS_MEMMODULE_CID) ;

PRUint32 NumberOfObjects(void);

nsCacheManager::nsCacheManager(): 
    m_pFirstModule(0), 
    m_bOffline(PR_FALSE),
    m_pPrefs(0)
{
  NS_INIT_ISUPPORTS( ) ;
  PR_AtomicIncrement(&gInstanceCnt) ;
}

nsCacheManager::~nsCacheManager()
{
    if (m_pPrefs)
    {
        // delete m_pPrefs;
		NS_RELEASE(m_pPrefs) ;
        m_pPrefs = 0;
    }
    if (m_pFirstModule)
    {
        // delete m_pFirstModule;
		NS_RELEASE(m_pFirstModule) ;
        m_pFirstModule = 0;
    }
    if (m_pBkgThd)
    {
        m_pBkgThd->Stop();
        delete m_pBkgThd;
        m_pBkgThd = 0;
    }

    // Shutdown( ) ;

    NS_ASSERTION(mRefCnt==0, "wrong Ref count" ) ;
    PR_AtomicDecrement(&gInstanceCnt) ;

}


nsCacheManager* 
nsCacheManager::GetInstance()
{
  if (!gInstance)
  {
	gInstance = new nsCacheManager( ) ;
	gInstance->Init( ) ;
  }
  return gInstance ;
//    return &TheManager;
}

#if 0
/* Caller must free returned char* */
const char* 
nsCacheManager::Trace() const
{

    char linebuffer[128];
    char* total;
	PRInt16 n_entries=0 ;

	Entries(&n_entries) ;

    sprintf(linebuffer, "nsCacheManager: Modules = %d\n", n_entries);

    total = new char[strlen(linebuffer) + 1];
    strcpy(total, linebuffer);
    return total;
}
#endif

NS_IMETHODIMP
nsCacheManager::AddModule(PRInt16 * n_Index, nsICacheModule* pModule)
{
  // PRInt16 *n_Entries ;
  // nsresult rv ;

	if (!n_Index)
	  return NS_ERROR_NULL_POINTER ;
	  
    MonitorLocker ml(this);
    if (pModule) 
    {
        if (m_pFirstModule)
            LastModule()->SetNextModule(pModule);
        else
            m_pFirstModule = pModule;

		Entries(n_Index) ;
		(*n_Index)-- ;

		return NS_OK ;
    }
    else
        return NS_ERROR_FAILURE ;
}

NS_IMETHODIMP
nsCacheManager::Contains(const char* i_url) const
{
    MonitorLocker ml((nsMonitorable*)this);
    // Add logic to check for IMAP type URLs, byteranges, and search with / appended as well...
    // TODO 
	PRBool bContain ;

    nsresult rv = ContainsExactly(i_url, &bContain);

    if (!bContain)
    {
     // try alternate stuff
     //  char* extraBytes;
     //  char extraBytesSeparator;
    }
#if !defined(NDEBUG) && defined(DEBUG_gagan)
		fputs(i_url, stdout);
		fputs(NS_SUCCEEDED(bStatus) ? " is " : " is not ", stdout);
		fputs("in cache\n", stdout);
#endif
    return rv ;
}


/*
PRBool
nsCacheManager::Contains(const char* i_url) const
{
    MonitorLocker ml((nsMonitorable*)this);
    // Add logic to check for IMAP type URLs, byteranges, and search with / appended as well...
    // TODO 
    PRBool bStatus = ContainsExactly(i_url);
    if (!bStatus)
    {
        // try alternate stuff
       //  char* extraBytes;
      //  char extraBytesSeparator;
    }
#if !defined(NDEBUG) && defined(DEBUG_gagan)
		fputs(i_url, stdout);
		fputs(bStatus ? " is " : " is not ", stdout);
		fputs("in cache\n", stdout);
#endif
	return bStatus ;
}
*/

NS_IMETHODIMP
nsCacheManager::ContainsExactly(const char* i_url, PRBool * contains) const
{
    nsresult rv ;

	*contains = PR_FALSE ;

    if (m_pFirstModule)
    {
        nsICacheModule* pModule = m_pFirstModule;
        while (pModule)
        {
		  rv = pModule->ContainsURL(i_url, contains) ;

            if ( contains )
            {
                return NS_OK;
            }
            rv = pModule->GetNextModule(&pModule);
        }
    }
    return NS_ERROR_FAILURE ; 
}

NS_IMETHODIMP
nsCacheManager::GetObj(const char* i_url, void ** o_pObject) const
{
	nsresult rv ;

    if ( !o_pObject)
	  return NS_ERROR_NULL_POINTER ;

	*o_pObject = nsnull ;

    MonitorLocker ml((nsMonitorable*)this);

    if (m_pFirstModule) 
    {
        nsICacheModule* pModule = m_pFirstModule;

        while (pModule)
        {
            rv = pModule->GetObjectByURL(i_url, (nsICacheObject**) o_pObject);

			if (*o_pObject)
			  return NS_OK ;

            rv = pModule->GetNextModule(&pModule);
        }
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCacheManager::Entries(PRInt16 * n_Entries) const
{
    nsresult rv ;

    if (!n_Entries)
	  return NS_ERROR_NULL_POINTER ;

    MonitorLocker ml((nsMonitorable*)this);
    if (m_pFirstModule) 
    {
        PRInt16 count=1;
        nsICacheModule* pModule ;

		rv = m_pFirstModule->GetNextModule(&pModule);

        while (pModule)
        {
            count++;
			rv = pModule->GetNextModule(&pModule);
        }
		*n_Entries = count ;
        return NS_OK;
    }
    return NS_ERROR_FAILURE ;
}

/*
PRInt16
nsCacheManager::Entries(void) const
{
    MonitorLocker ml((nsMonitorable*)this);
    if (m_pFirstModule) 
    {
        PRInt16 count=1;
        nsCacheModule* pModule = m_pFirstModule->NextModule();
        while (pModule)
        {
            count++;
			pModule = pModule->NextModule();
        }
        return count;
    }
    return -1 ;
}
*/

// todo: use PROGID instead of index
NS_IMETHODIMP
nsCacheManager::GetModule(PRInt16 i_index, nsICacheModule** pModule) const
{
	nsresult rv ;

    if (!pModule)
	  return NS_ERROR_NULL_POINTER ;

    MonitorLocker ml((nsMonitorable*)this);

	PRInt16 n_entries = 0 ;

	rv = Entries(&n_entries) ;

    if ((i_index < 0) || (i_index >= n_entries))
        return NS_ERROR_FAILURE ;

    *pModule = m_pFirstModule;

    PR_ASSERT(*pModule);
    for (PRInt16 i=0; i<i_index; (*pModule)->GetNextModule(pModule))
    {
        i++;
        PR_ASSERT(*pModule);
    }
    return NS_OK ;
}

NS_IMETHODIMP
nsCacheManager::InfoAsHTML(char** o_Buffer) const
{
	PRInt16 n_entries = 0;
	Entries(&n_entries) ;

	if (!o_Buffer)
	  return NS_ERROR_NULL_POINTER ;

	MonitorLocker ml((nsMonitorable*)this);

	char* tmpBuffer =/* TODO - make this cool */
		PR_smprintf("<HTML><h2>Your cache has %d modules</h2> \
			It has a total of %d cache objects. Hang in there for the details. </HTML>\0", 
			n_entries,
			NumberOfObjects());
	
	if (tmpBuffer)
	{
		PL_strncpyz(*o_Buffer, tmpBuffer, PL_strlen(tmpBuffer)+1);
		PR_smprintf_free(tmpBuffer);
	}
	return NS_OK ;
}

void
nsCacheManager::Init() 
{
    nsresult rv=nsnull ;
	PRUint32 size ;

    MonitorLocker ml(this);
    
    //Init prefs
    if (!m_pPrefs)
    //    m_pPrefs = new nsCachePref();
		
		rv = nsComponentManager::CreateInstance(kCachePrefCID,
		                                        nsnull,
												kICachePrefIID, 
												(void**)&m_pPrefs) ;

		rv=nsnull ;

    if (m_pFirstModule)
        delete m_pFirstModule;

    // m_pFirstModule = new nsMemModule(nsCachePref::GetInstance()->MemCacheSize());
	rv = nsComponentManager::CreateInstance(kCacheMemModuleCID,
	                                        nsnull,
											kICacheModuleIID,
											(void**)&m_pFirstModule) ;

	rv = m_pPrefs->GetMemCacheSize(&size) ;

	rv = m_pFirstModule -> SetSize(size) ;

    PR_ASSERT(m_pFirstModule);

    if (m_pFirstModule)
    {
		rv = m_pPrefs->GetDiskCacheSize(&size) ;

        nsICacheModule* p_Temp ;
		// = new nsDiskModule(nsCachePref::GetInstance()->DiskCacheSize());

    	rv = nsComponentManager::CreateInstance(kCacheDiskModuleCID,
	                                        nsnull,
											kICacheModuleIID,
											(void**)&p_Temp) ;

		rv = m_pPrefs->SetDiskCacheSize(size) ;

		rv = p_Temp->SetSize(size) ;

        PR_ASSERT(p_Temp);
        rv = m_pFirstModule->SetNextModule(p_Temp);

		PRUint32 time ;

        rv = m_pPrefs->GetBkgSleepTime(&time) ;

        m_pBkgThd = new nsCacheBkgThd(PR_SecondsToInterval(time));

        PR_ASSERT(m_pBkgThd);
    }
}

nsICacheModule* 
nsCacheManager::LastModule() const 
{
    MonitorLocker ml((nsMonitorable*)this);

	nsresult rv ;

    if (m_pFirstModule) 
    {
        nsICacheModule* temp = m_pFirstModule;
		nsICacheModule* pModule=temp ; 

		rv = temp->GetNextModule(&temp) ;

        while(temp) {
		    pModule = temp ; 
			rv = temp->GetNextModule(&temp) ;
        }

        return pModule;
    }
    return 0;
}

NS_IMETHODIMP
nsCacheManager::Remove(const char* i_url)
{
    MonitorLocker ml(this);
    nsresult rv ; 

    if (m_pFirstModule)
    {
        nsICacheModule* pModule = m_pFirstModule;

		do {
		  rv = pModule ->RemoveByURL(i_url) ;
		  if ( NS_SUCCEEDED(rv) ) return rv ;

		  rv = pModule ->GetNextModule(&pModule) ;
		} while (pModule) ;
	}

    return NS_ERROR_FAILURE ;
}

NS_IMETHODIMP
nsCacheManager::WorstCaseTime(PRUint32 * o_Time) const 
{
    PRIntervalTime start = PR_IntervalNow();

	o_Time = nsnull ;

    if (NS_SUCCEEDED(this->Contains("a vague string that should not be in any of the modules")))
    {
        PR_ASSERT(0);
    }
    *o_Time = PR_IntervalToMicroseconds(PR_IntervalNow() - start);

	return NS_OK ;
}

PRUint32
NumberOfObjects(void)
{
    PRUint32 objs = 0, temp; 
	nsresult rv ;
	nsICacheModule* pModule ;
	nsICacheManager* cacheManager ;

    PRInt16 i ;

	rv = nsServiceManager::GetService(kCacheManagerCID,
	                                  kICacheManagerIID,
									  (nsISupports **)&cacheManager) ; 

	rv = cacheManager->Entries(&i);

    while (i>0)
    {
        rv = cacheManager->GetModule(--i, &pModule) ;
		pModule->GetNumOfEntries(&temp);
		objs+=temp ;
    }
    return objs;
}

/*
NS_IMETHODIMP
nsCacheManager::QueryInterface(const nsIID &aIID, void **aResult)
{
  if(!aResult)
	return NS_ERROR_NULL_POINTER ;

  if (aIID.Equals(kISupportsIID)) {
	*aResult = NS_STATIC_CAST(nsISupports*, this) ;
  }
  else if (aIID.Equals(kICacheManagerIID)) {
	*aResult = NS_STATIC_CAST(nsISupports*, 
	           NS_STATIC_CAST(nsICacheManager*, this)) ;
  }
  else {
	*aResult = nsnull ;
	return NS_ERROR_NO_INTERFACE ;
  }

  NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult)) ;

  return NS_OK ;
}

NS_IMPL_ADDREF(nsCacheManager) 
NS_IMPL_RELEASE(nsCacheManager)
*/

NS_IMPL_ISUPPORTS(nsCacheManager, kICacheManagerIID) ;

nsCacheManagerFactory::nsCacheManagerFactory( )
{
  NS_INIT_ISUPPORTS( ) ;
  PR_AtomicIncrement(&gInstanceCnt) ;
}

nsCacheManagerFactory::~nsCacheManagerFactory( )
{
  NS_ASSERTION(mRefCnt == 0, "Wrong ref count") ;
  PR_AtomicDecrement(&gInstanceCnt) ;
}

/*
NS_IMETHODIMP
nsCacheManagerFactory::QueryInterface(const nsIID &aIID, void ** aResult)
{
  if (!aResult)
	return NS_ERROR_NULL_POINTER ;

  if (aIID.Equals(kISupportsIID)) {
	*aResult = NS_STATIC_CAST(nsISupports*, this) ;
  }
  else if (aIID.Equals(kIFactoryIID)) {
	*aResult = NS_STATIC_CAST(nsISupports*, 
	           NS_STATIC_CAST(nsIFactory*, this)) ;
  }
  else {
	*aResult = nsnull ;
	return NS_ERROR_NO_INTERFACE ;
  }

  NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult)) ;

  return NS_OK ;
}

NS_IMPL_ADDREF(nsCacheManagerFactory)
NS_IMPL_RELEASE(nsCacheManagerFactory)
*/

NS_IMPL_ISUPPORTS(nsCacheManagerFactory, kIFactoryIID) ;

// Todo, needs to enforce singleton

NS_IMETHODIMP
nsCacheManagerFactory::CreateInstance(nsISupports *aOuter, 
                                      const nsIID &aIID,
									  void **aResult)
{
  if (!aResult)
	return NS_ERROR_NULL_POINTER ;

  *aResult = nsnull ;

  nsISupports *inst = nsCacheManager::GetInstance( ) ;

  if (!inst) {
	return NS_ERROR_OUT_OF_MEMORY ;
  }

  nsresult rv = inst -> QueryInterface(aIID, aResult) ;

  if (NS_FAILED(rv)) {
	delete inst ;
  }

  return rv ;
}

NS_IMETHODIMP
nsCacheManagerFactory::LockFactory(PRBool aLock)
{
  if (aLock) {
	PR_AtomicIncrement (&gLockCnt) ;
  }
  else {
	PR_AtomicDecrement (&gLockCnt) ;
  }

  return NS_OK ;
}

// static NS_DEFINE_CID (kCacheManagerCID, NS_CACHEMANAGER_CID) ;
static NS_DEFINE_CID (kComponentManagerCID, NS_COMPONENTMANAGER_CID) ;

static const char * g_desc = "Mozilla xpcom cache manager" ;

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports *serviceMgr, 
             const nsCID &aCID,
			 const char * aClassName,
			 const char * aProgID,
			 nsIFactory **aResult)
{
  if (!aResult)
	return NS_ERROR_NULL_POINTER ;

  *aResult = nsnull ;

  nsISupports *inst ;

  if (aCID.Equals(kCacheManagerCID)) {
	inst = new nsCacheManagerFactory( ) ;
  }
  else {
	return NS_ERROR_NO_INTERFACE ;
  }

  if (!inst)
	return NS_ERROR_OUT_OF_MEMORY ;

  nsresult rv = inst->QueryInterface(kIFactoryIID, (void **)aResult) ;

  if (NS_FAILED(rv)) {
	delete inst ;
  }

  return rv ;
}

extern "C" NS_EXPORT PRBool
NSCanUnload (nsISupports* serviceMgr)
{
  return PRBool(gInstanceCnt == 0 && gLockCnt == 0) ;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports *aServMgr, const char *path)
{
  nsresult rv = NS_OK ;

  nsIServiceManager *sm ;

  rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void**)&sm) ;

  if(NS_FAILED(rv)) {
	return rv;
  }

  nsIComponentManager *cm ;

  rv = sm->GetService(kComponentManagerCID, 
                      nsIComponentManager::GetIID(), 
                      (nsISupports **) &cm) ;
  if (NS_FAILED(rv)) {
	NS_RELEASE(sm) ;
	return rv ;
  }

  rv = cm->RegisterComponent(kCacheManagerCID, g_desc, "component://nucache",
                             path, PR_TRUE, PR_TRUE) ;

  sm->ReleaseService(kComponentManagerCID, cm) ;

  NS_RELEASE(sm) ;

#ifdef NS_DEBUG
  printf("*** %s registered\n", g_desc) ;
#endif

  return rv ;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv = NS_OK ;

  nsIServiceManager *sm ;

  rv = aServMgr ->QueryInterface(nsIServiceManager::GetIID(),
                                 (void **)&sm) ;

  if(NS_FAILED(rv)) {
	return rv ;
  }

  nsIComponentManager *cm ;

  rv = sm->GetService(kComponentManagerCID, nsIComponentManager::GetIID(), 
                      (nsISupports**) &cm) ;

  if(NS_FAILED(rv)) {
	NS_RELEASE(sm) ;
	return rv ;
  }

  rv = cm->UnregisterComponent(kCacheManagerCID, path);

  sm->ReleaseService(kComponentManagerCID, cm) ;

  NS_RELEASE(sm) ;

  return rv ;
}

