/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Implementation of misc. xpti stuff. */

#include "xptiprivate.h"

struct xptiFileTypeEntry
{
    char*               name;
    int                 len;
    xptiFileType::Type  type;
};

static const xptiFileTypeEntry g_Entries[] = 
    {
        {".xpt", 4, xptiFileType::XPT},            
        {".zip", 4, xptiFileType::ZIP},            
        {".jar", 4, xptiFileType::ZIP},            
        {nsnull, 0, xptiFileType::UNKNOWN}            
    };

// static 
xptiFileType::Type xptiFileType::GetType(const char* name)
{
    NS_ASSERTION(name, "loser!");
    for(const xptiFileTypeEntry* p = g_Entries; p->name; p++)
    {
        int len = PL_strlen(name);
        if(len > p->len && 0 == PL_strcasecmp(p->name, &(name[len - p->len])))
            return p->type;
    }
    return UNKNOWN;        
}        

/***************************************************************************/

MOZ_DECL_CTOR_COUNTER(xptiAutoLog);

xptiAutoLog::xptiAutoLog(xptiInterfaceInfoManager* mgr, 
                         nsILocalFile* logfile, PRBool append)
    : mMgr(nsnull), mOldFileDesc(nsnull)
{
    MOZ_COUNT_CTOR(xptiAutoLog);

    if(mgr && logfile)
    {
        PRFileDesc* fd;
        if(NS_SUCCEEDED(logfile->
                    OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_APPEND |
                                             (append ? 0 : PR_TRUNCATE),
                                             0666, &fd)) && fd)
        {
#ifdef DEBUG
            m_DEBUG_FileDesc = fd;
#endif
            mMgr = mgr;
            mOldFileDesc = mMgr->SetOpenLogFile(fd);
            if(append)
                PR_Seek(fd, 0, PR_SEEK_END);
            WriteTimestamp(fd, "++++ start logging ");

        }
        else
        {
#ifdef DEBUG
        printf("xpti failed to open log file for writing\n");
#endif
        }
    }
};

xptiAutoLog::~xptiAutoLog()
{
    MOZ_COUNT_DTOR(xptiAutoLog);

    if(mMgr)
    {
        PRFileDesc* fd = mMgr->SetOpenLogFile(mOldFileDesc);
        NS_ASSERTION(fd == m_DEBUG_FileDesc, "bad unravel");
        if(fd)
        {
            WriteTimestamp(fd, "---- end logging   ");
            PR_Close(fd);
        }
    }
}

void xptiAutoLog::WriteTimestamp(PRFileDesc* fd, const char* msg)
{
    PRExplodedTime expTime;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &expTime);
    char time[128];
    PR_FormatTimeUSEnglish(time, 128, "%Y-%m-%d-%H:%M:%S", &expTime);
    PR_fprintf(fd, "\n%s %s\n\n", msg, time);
}

