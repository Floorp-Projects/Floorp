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

#include "nsCachePref.h"
#ifndef XP_UNIX
#include "xp.h" // This complains on unix. Works ok without there.
#endif
#include "prefapi.h"
#include "prmem.h"
#include "nsCacheManager.h"
#include "plstr.h"

#ifdef XP_MAC
#include "uprefd.h"
#endif


static const PRUint32 MEM_CACHE_SIZE_DEFAULT = 1024*1024;
static const PRUint32 DISK_CACHE_SIZE_DEFAULT = 5*MEM_CACHE_SIZE_DEFAULT;
static const PRUint32 BKG_THREAD_SLEEP = 60;//TODO- Testing 15*60; /*in seconds, 15 minutes */
static const PRUint16 BUGS_FOUND_SO_FAR = 0;

//Preferences
static const char* MEM_CACHE_PREF   = "browser.cache.memory_cache_size";
static const char* DISK_CACHE_PREF  = "browser.cache.disk_cache_size";
static const char* CACHE_DIR_PREF   = "browser.cache.directory";
static const char* FREQ_PREF        = "browser.cache.check_doc_frequency";

/* Find a bug in NU_CACHE, get these many chocolates */
static const PRUint16 CHOCOLATES_PER_BUG_FOUND = 2^BUGS_FOUND_SO_FAR; 

int PR_CALLBACK Cache_PrefChangedFunc(const char *pref, void *data) {
    nsCachePref::GetInstance()->SetupPrefs(pref);
	return PR_TRUE;
} 

nsCachePref::nsCachePref(void):
    m_BkgSleepTime(BKG_THREAD_SLEEP),
    m_DiskCacheDBFilename(new char[6+1]),
    m_DiskCacheFolder(0),
    m_DiskCacheSize(DISK_CACHE_SIZE_DEFAULT),
    m_MemCacheSize(MEM_CACHE_SIZE_DEFAULT),
    m_RefreshFreq(ONCE)
{
    PREF_RegisterCallback("browser.cache",Cache_PrefChangedFunc,0); 
    SetupPrefs(0);
}

nsCachePref::~nsCachePref()
{
    if (m_DiskCacheFolder)
    {
        delete m_DiskCacheFolder;
        m_DiskCacheFolder = 0;
    }
}

//Read all the stuff from pref here. 
void
nsCachePref::SetupPrefs(const char* i_Pref)
{
    PRBool bSetupAll = PR_FALSE;
    
    int32 nTemp;
    char* tempPref=0;

    if (!i_Pref)
        bSetupAll = PR_TRUE;

    if (bSetupAll || !PL_strcmp(i_Pref,MEM_CACHE_PREF)) 
    {
        if (PREF_OK == PREF_GetIntPref(MEM_CACHE_PREF,&nTemp))
            m_MemCacheSize = 1024*nTemp;
        else
            m_MemCacheSize = MEM_CACHE_SIZE_DEFAULT;
    }

    if (bSetupAll || !PL_strcmp(i_Pref,DISK_CACHE_PREF)) 
    {
        if (PREF_OK == PREF_GetIntPref(DISK_CACHE_PREF,&nTemp))
            m_DiskCacheSize = 1024*nTemp;
        else
            m_DiskCacheSize = DISK_CACHE_SIZE_DEFAULT;
    }

    if (bSetupAll || !PL_strcmp(i_Pref,FREQ_PREF)) 
    {
        if (PREF_OK == PREF_GetIntPref(FREQ_PREF,&nTemp))
        {
            m_RefreshFreq = (nsCachePref::Refresh)nTemp;
        }
        else
            m_RefreshFreq = ONCE;
    }

    if (bSetupAll || !PL_strcmp(i_Pref,CACHE_DIR_PREF)) 
    {
#ifdef XP_MAC
		tempPref = CPrefs::GetCachePath();
		if (tempPref)
		{
			if (m_DiskCacheFolder)
				delete m_DiskCacheFolder;
			m_DiskCacheFolder =  new char[PL_strlen(tempPref)+2];
		
	        if (!m_DiskCacheFolder)
	        {
	            if (tempPref)
	                PR_Free(tempPref);
	            return;
	        }
	        /* Changing the mac path to unix because nspr converts 
	           it the other way round and cant deal with it being a mac path */
	        m_DiskCacheFolder[0]='/';
	        m_DiskCacheFolder[1]='\0';
	        PL_strcat(m_DiskCacheFolder, tempPref);
	        char *tempPref2 = m_DiskCacheFolder+1;
		    while (*tempPref2)
		    {
		    	if (*tempPref2 == ':')
		    		*tempPref2 = '/';
    			tempPref2++;
    		}
		     
	   }
#else
	    if (PREF_OK == PREF_CopyCharPref(CACHE_DIR_PREF,&tempPref))
        {
            PR_ASSERT(tempPref);
            if (m_DiskCacheFolder)
                delete m_DiskCacheFolder;
            m_DiskCacheFolder = new char[PL_strlen(tempPref)+1];
            if (!m_DiskCacheFolder)
            {
                if (tempPref)
                    PR_Free(tempPref);
                return;
            }
            PL_strcpy(m_DiskCacheFolder, tempPref);
        }
#endif
        else //TODO set to temp folder
        {
            m_DiskCacheFolder = new char [1];
            if (!m_DiskCacheFolder)
                return;
            *m_DiskCacheFolder = '\0';
        }

    }

    if (tempPref)
        PR_Free(tempPref);
}

const PRUint32  
nsCachePref::BkgSleepTime(void)
{
    return BKG_THREAD_SLEEP; 
}

const char* nsCachePref::DiskCacheDBFilename(void)
{
    return "nufat.db";
}

nsCachePref* nsCachePref::GetInstance()
{
    PR_ASSERT(nsCacheManager::GetInstance()->GetPrefs());
    return nsCacheManager::GetInstance()->GetPrefs();
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


