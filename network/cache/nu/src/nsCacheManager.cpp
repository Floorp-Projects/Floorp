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
#include "prinrval.h"
#include "plstr.h"
#include "prprf.h" //PR_smprintf and PR_smprintf_free

#include "nsCacheManager.h"
#include "nsCacheTrace.h"
#include "nsCachePref.h"
#include "nsCacheModule.h"
#include "nsMemModule.h"
#include "nsDiskModule.h"
#include "nsCacheBkgThd.h"

/* TODO move this to InitNetLib */
static nsCacheManager TheManager;

PRUint32 NumberOfObjects(void);

nsCacheManager::nsCacheManager(): 
    m_pFirstModule(0), 
    m_bOffline(PR_FALSE),
    m_pPrefs(0)
{
}

nsCacheManager::~nsCacheManager()
{
    if (m_pPrefs)
    {
        delete m_pPrefs;
        m_pPrefs = 0;
    }
    if (m_pFirstModule)
    {
        delete m_pFirstModule;
        m_pFirstModule = 0;
    }
    if (m_pBkgThd)
    {
        m_pBkgThd->Stop();
        delete m_pBkgThd;
        m_pBkgThd = 0;
    }
}

nsCacheManager* 
nsCacheManager::GetInstance()
{
    return &TheManager;
}

#if 0
/* Caller must free returned char* */
const char* 
nsCacheManager::Trace() const
{

    char linebuffer[128];
    char* total;

    sprintf(linebuffer, "nsCacheManager: Modules = %d\n", Entries());

    total = new char[strlen(linebuffer) + 1];
    strcpy(total, linebuffer);
    return total;
}
#endif

PRInt32 
nsCacheManager::AddModule(nsCacheModule* pModule)
{
    MonitorLocker ml(this);
    if (pModule) 
    {
        if (m_pFirstModule)
            LastModule()->NextModule(pModule);
        else
            m_pFirstModule = pModule;

        return Entries()-1;
    }
    else
        return -1;
}

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
        /*
        char* extraBytes;
        char extraBytesSeparator;
        */
    }
    return bStatus;
}

PRBool
nsCacheManager::ContainsExactly(const char* i_url) const
{
    if (m_pFirstModule)
    {
        nsCacheModule* pModule = m_pFirstModule;
        while (pModule)
        {
            if (pModule->Contains(i_url))
            {
                return PR_TRUE;
            }
            pModule = pModule->NextModule();
        }
    }
    return PR_FALSE;
}

nsCacheObject* 
nsCacheManager::GetObj(const char* i_url) const
{
    MonitorLocker ml((nsMonitorable*)this);
    if (m_pFirstModule) 
    {
        nsCacheModule* pModule = m_pFirstModule;
        nsCacheObject* obj = 0;
        while (pModule)
        {
            obj = pModule->GetObject(i_url);
            if (obj)
                return obj;
            pModule = pModule->NextModule();
        }
    }
    return 0;
}

PRInt16 
nsCacheManager::Entries() const
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
    return 0;
}

nsCacheModule* 
nsCacheManager::GetModule(PRInt16 i_index) const
{
    MonitorLocker ml((nsMonitorable*)this);
    if ((i_index < 0) || (i_index >= Entries()))
        return 0;
    nsCacheModule* pModule = m_pFirstModule;
    PR_ASSERT(pModule);
    for (PRInt16 i=0; i<i_index; pModule = pModule->NextModule())
    {
        i++;
        PR_ASSERT(pModule);
    }
    return pModule;
}

void
nsCacheManager::InfoAsHTML(char* o_Buffer) const
{
	MonitorLocker ml((nsMonitorable*)this);

	char* tmpBuffer =/* TODO - make this cool */
		PR_smprintf("<HTML><h2>Your cache has %d modules</h2> \
			It has a total of %d cache objects. Hang in there for the details. </HTML>\0", 
			Entries(),
			NumberOfObjects());
	
	if (tmpBuffer)
	{
		PL_strncpyz(o_Buffer, tmpBuffer, PL_strlen(tmpBuffer)+1);
		PR_smprintf_free(tmpBuffer);
																					}
																				}

void 
nsCacheManager::Init() 
{
    MonitorLocker ml(this);
    
    //Init prefs
    if (!m_pPrefs)
        m_pPrefs = new nsCachePref();

    if (m_pFirstModule)
        delete m_pFirstModule;

    m_pFirstModule = new nsMemModule(nsCachePref::GetInstance()->MemCacheSize());
    PR_ASSERT(m_pFirstModule);
    if (m_pFirstModule)
    {
        nsDiskModule* pTemp = new nsDiskModule(nsCachePref::GetInstance()->DiskCacheSize());
        PR_ASSERT(pTemp);
        m_pFirstModule->NextModule(pTemp);
        m_pBkgThd = new nsCacheBkgThd(PR_SecondsToInterval(nsCachePref::GetInstance()->BkgSleepTime()));
        PR_ASSERT(m_pBkgThd);
    }
}

nsCacheModule* 
nsCacheManager::LastModule() const 
{
    MonitorLocker ml((nsMonitorable*)this);
    if (m_pFirstModule) 
    {
        nsCacheModule* pModule = m_pFirstModule;
        while(pModule->NextModule()) {
            pModule = pModule->NextModule();
        }
        return pModule;
    }
    return 0;
}

PRBool
nsCacheManager::Remove(const char* i_url)
{
    MonitorLocker ml(this);
    PRBool bStatus = PR_FALSE;
    if (m_pFirstModule)
    {
        nsCacheModule* pModule = m_pFirstModule;
        bStatus = pModule->Remove(i_url);
        if (bStatus)
            return bStatus;
        while(pModule->NextModule()) {
            pModule = pModule->NextModule();
            bStatus = pModule->Remove(i_url);
            if (bStatus)
                return bStatus;
        }
    }
    return bStatus;
}

PRUint32 
nsCacheManager::WorstCaseTime(void) const 
{
    PRIntervalTime start = PR_IntervalNow();
    if (this->Contains("a vague string that should not be in any of the modules"))
    {
        PR_ASSERT(0);
    }
    return PR_IntervalToMicroseconds(PR_IntervalNow() - start);
}

PRUint32
NumberOfObjects(void)
{
    PRUint32 objs = 0;
    int i = TheManager.Entries();
    while (i>0)
    {
        objs += TheManager.GetModule(--i)->Entries();
    }
    return objs;
}

