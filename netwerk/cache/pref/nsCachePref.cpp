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

#ifndef XP_UNIX
#include "xp.h" // This complains on unix. Works ok without there.
#endif
//#include "prefapi.h"
#include "nsIPref.h"
#include "prmem.h"
#include "prprf.h"
#include "nsICacheManager.h"
#include "plstr.h"
#if defined(XP_PC)
#include <direct.h>
#endif /* XP_PC */
#include "nspr.h"
#include "nscore.h"
#include "nsRepository.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#ifdef XP_MAC
#include "uprefd.h"
#endif

#include "nsCachePref.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICachePrefIID, NS_ICACHEPREF_IID);  
static NS_DEFINE_IID(kCachePrefCID, NS_CACHEPREF_CID);  
static NS_DEFINE_IID(kICACHEMANAGERIID, NS_ICACHEMANAGER_IID);  
static NS_DEFINE_IID(kCacheManagerCID, NS_CACHEMANAGER_CID);  
static NS_DEFINE_IID(knsPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kInsPrefIID, NS_IPREF_IID);

static const PRUint32 MEM_CACHE_SIZE_DEFAULT = 1024*1024;
static const PRUint32 DISK_CACHE_SIZE_DEFAULT = 5*MEM_CACHE_SIZE_DEFAULT;
static const PRUint32 BKG_THREAD_SLEEP = 15*60; /*in seconds, 15 minutes */
static const PRUint16 BUGS_FOUND_SO_FAR = 0;

//Preferences
static const char * const MEM_CACHE_PREF   = "browser.cache.memory_cache_size";
static const char * const DISK_CACHE_PREF  = "browser.cache.disk_cache_size";
static const char * const CACHE_DIR_PREF   = "browser.cache.directory";
static const char * const FREQ_PREF        = "browser.cache.check_doc_frequency";
//Not implemented as yet...
static const char * const BKG_CLEANUP      = "browser.cache.cleanup_period"; //Default for BKG_THREAD_SLEEP	

/* Find a bug in NU_CACHE, get these many chocolates */
static const PRUint16 CHOCOLATES_PER_BUG_FOUND = 2^BUGS_FOUND_SO_FAR; 

static int PR_CALLBACK Cache_PrefChangedFunc(const char *pref, void *data);

static PRInt32 gInstanceCnt = 0 ;
static PRInt32 gLockCnt = 0 ;

nsCachePref::nsCachePref(void):
    m_RefreshFreq(ONCE) ,
    m_MemCacheSize(MEM_CACHE_SIZE_DEFAULT),
    m_DiskCacheSize(DISK_CACHE_SIZE_DEFAULT),
    m_DiskCacheDBFilename(new char[6+1]),
    m_DiskCacheFolder(0),
    m_BkgSleepTime(BKG_THREAD_SLEEP)
{
	nsIPref * pref=nsnull ;
    nsresult rv = nsServiceManager::GetService(knsPrefCID,
	                                           kInsPrefIID,
											   (nsISupports **)&pref) ;

    rv = pref->RegisterCallback("browser.cache",Cache_PrefChangedFunc,0); 

	m_DiskCacheDBFilename = "nufat.db" ;
    SetupPrefs(0);

	NS_INIT_ISUPPORTS() ;
	PR_AtomicIncrement(&gInstanceCnt) ;
}

nsCachePref::~nsCachePref()
{
    if (m_DiskCacheFolder)
    {
        delete m_DiskCacheFolder;
        m_DiskCacheFolder = 0;
    }

	NS_ASSERTION(mRefCnt == 0, "Wrong ref count") ;
	PR_AtomicDecrement(&gInstanceCnt) ;
}

//Read all the stuff from pref here. 
NS_IMETHODIMP 
nsCachePref::SetupPrefs(const char* i_Pref)
{
    PRBool bSetupAll = PR_FALSE;
    
    PRInt32 nTemp;
    char* tempPref=0;

    if (!i_Pref)
        bSetupAll = PR_TRUE;

	nsIPref * pref ;

	nsServiceManager::GetService(knsPrefCID,
	                                           kInsPrefIID,
											   (nsISupports **)&pref) ;

    if (bSetupAll || !PL_strcmp(i_Pref,MEM_CACHE_PREF)) 
    {
        if (NS_SUCCEEDED(pref->GetIntPref(MEM_CACHE_PREF,&nTemp)) ) {
            m_MemCacheSize = 1024*nTemp;
		}
        else {
            m_MemCacheSize = MEM_CACHE_SIZE_DEFAULT;
		}
    }

    if (bSetupAll || !PL_strcmp(i_Pref,DISK_CACHE_PREF)) 
    {
        if (NS_SUCCEEDED(pref->GetIntPref(DISK_CACHE_PREF,&nTemp)))
            m_DiskCacheSize = 1024*nTemp;
        else
            m_DiskCacheSize = DISK_CACHE_SIZE_DEFAULT;
    }

    if (bSetupAll || !PL_strcmp(i_Pref,FREQ_PREF)) 
    {
        if (NS_SUCCEEDED(pref->GetIntPref(FREQ_PREF,&nTemp)))
        {
            m_RefreshFreq = (nsCachePref::Refresh)nTemp;
        }
        else
            m_RefreshFreq = ONCE;
    }

    if (bSetupAll || !PL_strcmp(i_Pref,CACHE_DIR_PREF)) 
    {
		/* the CopyPathPref is not in nsIPref, is CopyCharPref ok? -xyz */

		nsresult rv = pref->CopyCharPref(CACHE_DIR_PREF,&tempPref) ;

	    if (NS_SUCCEEDED(rv))
        {
            PR_ASSERT(tempPref);

            delete [] m_DiskCacheFolder;
			m_DiskCacheFolder =  new char[PL_strlen(tempPref)+2];
	        if (!m_DiskCacheFolder)
	        {
	            if (tempPref)
	                PR_Free(tempPref);
	            return NS_OK ;
	        }
            PL_strcpy(m_DiskCacheFolder, tempPref);
	    }
        else //TODO set to temp folder
        {
#if defined(XP_PC)
            char tmpBuf[_MAX_PATH];
            PRFileInfo dir;
            PRStatus status;
            char *cacheDir = new char [_MAX_PATH];
            if (!cacheDir)
                return NS_OK;
            cacheDir = _getcwd(cacheDir, _MAX_PATH);

            // setup the cache dir.
            PL_strcpy(tmpBuf, cacheDir);
            sprintf(cacheDir,"%s%s%s%s", tmpBuf, "\\", "cache", "\\");
            status = PR_GetFileInfo(cacheDir, &dir);
            if (PR_FAILURE == status) {
                // Create the dir.
                status = PR_MkDir(cacheDir, 0600);
                if (PR_SUCCESS != status) {
                    m_DiskCacheFolder = new char[1];
					*m_DiskCacheFolder = '\0';
                    return NS_OK;
                }
            }
            m_DiskCacheFolder = cacheDir;
#endif /* XP_PC */

			delete [] m_DiskCacheFolder;
			m_DiskCacheFolder = new char[5] ;
			PL_strcpy(m_DiskCacheFolder,  "/tmp") ;
        }

    }

    if (tempPref)
        PR_Free(tempPref);

	return NS_OK ;
}

NS_IMETHODIMP
nsCachePref::GetBkgSleepTime(PRUint32* o_time)
{
    *o_time = BKG_THREAD_SLEEP; 
	return NS_OK ;
}

NS_IMETHODIMP 
nsCachePref::GetDiskCacheDBFilename(char** o_name)
{
    *o_name = "nufat.db";
	return NS_OK ;
}

nsICachePref* 
nsCachePref::GetInstance()
{
    nsICacheManager * cachemanager ;
	nsICachePref * cachepref ;

	nsresult rv = nsServiceManager::GetService(kCacheManagerCID,
	                                           kICACHEMANAGERIID,
											   (nsISupports **) &cachemanager) ;

    rv = cachemanager->GetPrefs(&cachepref);
	return cachepref ;
}

/*
nsrefcnt nsCachePref::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsCachePref::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsCachePref::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}
*/

NS_IMPL_ISUPPORTS(nsCachePref, nsCOMTypeInfo<nsICachePref>::GetIID() ) ;

int PR_CALLBACK Cache_PrefChangedFunc(const char *pref, void *data) {
    
    // nsCachePref::GetInstance()->SetupPrefs(pref);
	nsICachePref* cachepref ;

	nsresult rv = nsServiceManager::GetService(kCachePrefCID,
	                                           kICachePrefIID,
											   (nsISupports **)&cachepref) ;

	if (NS_FAILED(rv))
	  return PR_FALSE ;
	else 
	{
	  cachepref->SetupPrefs(pref) ;
	  return PR_TRUE;
	}
} 


/////////////////////////////////////////////////////////////////////
// Exported functions. With these in place we can compile
// this module into a dynamically loaded and registered
// component.

//static NS_DEFINE_CID(kCacheObjectCID, NS_CACHEOBJECT_CID);
/*
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nspr.h"
#include "nscore.h"
#include "nsRepository.h"
#include "nsICacheManager.h"

#include "nsICachePref.h"
#include "nsCachePref.h"
*/

// static NS_DEFINE_IID(kICachePrefIID, NS_ICACHEPREF_IID);  
//static NS_DEFINE_IID(kCachePrefCID, NS_CACHEPREF_CID);  
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
// static NS_DEFINE_IID(kICACHEMANAGERIID, NS_ICACHEMANAGER_IID);  
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);


// The "name" of our component
static const char* g_desc = "Cache Prefrence XPCOM Module";

nsCachePrefFactory::nsCachePrefFactory ()
{
	NS_INIT_ISUPPORTS();
	PR_AtomicIncrement(&gInstanceCnt);
}

nsCachePrefFactory::~nsCachePrefFactory ()
{
	NS_ASSERTION(mRefCnt == 0,"Wrong ref count");
	PR_AtomicDecrement(&gInstanceCnt);
}

/*
NS_IMETHODIMP 
nsCacheObjectFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;

	if (aIID.Equals(kISupportsIID)) {
		*aResult = NS_STATIC_CAST(nsISupports*,this);
	} else if (aIID.Equals(kIFactoryIID)) {
		*aResult = NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(nsIFactory*,this));
	} else {
		*aResult = nsnull;
		return NS_ERROR_NO_INTERFACE;
	}

	NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*,*aResult));

	return NS_OK;
}

NS_IMPL_ADDREF(nsCacheObjectFactory)
NS_IMPL_RELEASE(nsCacheObjectFactory)
*/

NS_IMPL_ISUPPORTS(nsCachePrefFactory, kIFactoryIID) ;

NS_IMETHODIMP
nsCachePrefFactory::CreateInstance(nsISupports *aOuter, 
                                const nsIID &aIID, 
                                void **aResult) 
{ 
	if (!aResult)
		return NS_ERROR_NULL_POINTER; 

	*aResult = nsnull; 

	nsISupports *inst = new nsCachePref(); 

	if (!inst) { 
		return NS_ERROR_OUT_OF_MEMORY; 
	} 

	// All XPCOM methods return nsresult
	nsresult rv = inst->QueryInterface(aIID, aResult); 

	// There are handy macros, NS_SUCCEEDED and NS_FAILED
	// to test the return value of an XPCOM method.
	if (NS_FAILED(rv)) { 
		// We didn't get the right interface, so clean up 
		delete inst; 
	} 

	return rv; 
} 

NS_IMETHODIMP
nsCachePrefFactory::LockFactory(PRBool aLock) 
{ 
	if (aLock) { 
		PR_AtomicIncrement(&gLockCnt); 
	} else { 
		PR_AtomicDecrement(&gLockCnt); 
	} 

	return NS_OK;
}

extern "C" NS_EXPORT nsresult 
NSGetFactory(nsISupports *serviceMgr,
			 const nsCID &aCID,
			 const char *aClassName,
			 const char *aProgID,
		     nsIFactory **aResult)  
{ 
	if (!aResult)
		return NS_ERROR_NULL_POINTER; 

	*aResult = nsnull; 

	nsISupports *inst; 

	if (aCID.Equals(kCachePrefCID)) {
		// Ok, we know this CID and here is the factory
		// that can manufacture the objects
		inst = new nsCachePrefFactory(); 
	} else { 
		return NS_ERROR_NO_INTERFACE; 
	} 

	if (!inst)
		return NS_ERROR_OUT_OF_MEMORY; 

	nsresult rv = inst->QueryInterface(kIFactoryIID, 
							(void **) aResult); 

	if (NS_FAILED(rv)) { 
		delete inst; 
	} 

	return rv; 
} 

extern "C" NS_EXPORT PRBool 
NSCanUnload(nsISupports* serviceMgr) 
{
	return PRBool(gInstanceCnt == 0 && gLockCnt == 0); 
}

extern "C" NS_EXPORT nsresult 
NSRegisterSelf(nsISupports *aServMgr, const char *path)
{
	nsresult rv = NS_OK;

	// We will use the service manager to obtain the component
	// manager, which will enable us to register a component
	// with a ProgID (text string) instead of just the CID.

	nsIServiceManager *sm;

	// We can get the IID of an interface with the static GetIID() method as
	// well.
	rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void **)&sm);

	if (NS_FAILED(rv))
		return rv;

	nsIComponentManager *cm;

	rv = sm->GetService(kComponentManagerCID, nsIComponentManager::GetIID(), (nsISupports **)&cm);

	if (NS_FAILED(rv)) {
		NS_RELEASE(sm);

		return rv;
	}

	// Note the text string, we can access the hello component with just this
	// string without knowing the CID
	rv = cm->RegisterComponent(kCachePrefCID, g_desc, "component://cachePref",
		path, PR_TRUE, PR_TRUE);

	sm->ReleaseService(kComponentManagerCID, cm);

	NS_RELEASE(sm);

#ifdef NS_DEBUG
	printf("*** %s registered\n",g_desc);
#endif

	return rv;
}

extern "C" NS_EXPORT nsresult 
NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
	nsresult rv = NS_OK;

	nsIServiceManager *sm;

	rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void **)&sm);

	if (NS_FAILED(rv))
		return rv;

	nsIComponentManager *cm;

	rv = sm->GetService(kComponentManagerCID, nsIComponentManager::GetIID(), (nsISupports **)&cm);

	if (NS_FAILED(rv)) {
		NS_RELEASE(sm);

		return rv;
	}

	rv = cm->UnregisterComponent(kCachePrefCID, path);

	sm->ReleaseService(kComponentManagerCID, cm);

	NS_RELEASE(sm);

	return rv;
}



