/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsBkgThread.h"
#include "prlog.h"

static void PR_CALLBACK RunFunction(void* arg);

static void PR_CALLBACK RunFunction(void* arg)
{
    nsBkgThread* pBT = (nsBkgThread*) arg;
    if (pBT)
    {
        pBT->Process();
    }
}

nsBkgThread::nsBkgThread(PRIntervalTime iSleepTime, PRBool bStart /* =PR_TRUE */)
{
    m_SleepTime = iSleepTime;
    m_bContinue = bStart;
    m_pThread = PR_CreateThread(
        PR_USER_THREAD,
        RunFunction,
        this,
        PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD,
        PR_JOINABLE_THREAD,
        0);
    PR_ASSERT(NULL != m_pThread);
}

nsBkgThread::~nsBkgThread()
{
    m_bContinue = PR_FALSE;
    if (m_pThread != NULL)
    {
        Stop();
    }
}
/*
nsrefcnt nsBkgThread::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsBkgThread::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsBkgThread::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}

*/
void nsBkgThread::Process(void)
{
    while (m_bContinue)
    {
        PR_Sleep(m_SleepTime);
        Run();
    }
}

void nsBkgThread::Stop(void)
{
    m_bContinue = PR_FALSE;
    PRStatus status = PR_Interrupt(m_pThread);
    PR_ASSERT(PR_SUCCESS == status);
}
