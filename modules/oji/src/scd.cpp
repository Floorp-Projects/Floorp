/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Methods to support the JVM Plugin API
////////////////////////////////////////////////////////////////////////////////

#include "jvmmgr.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kISymantecDebugManagerIID, NP_ISYMANTECDEBUGMANAGER_IID);

NS_IMPL_AGGREGATED(nsSymantecDebugManager);

nsSymantecDebugManager::nsSymantecDebugManager(nsISupports* outer, JVMMgr* jvmMgr)
    : fJVMMgr(jvmMgr)
{
    NS_INIT_AGGREGATED(outer);
}

nsSymantecDebugManager::~nsSymantecDebugManager()
{
}

NS_METHOD
nsSymantecDebugManager::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aIID.Equals(kISymantecDebugManagerIID)) {
        *aInstancePtr = this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_METHOD
nsSymantecDebugManager::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr, 
                               JVMMgr* jvmMgr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsSymantecDebugManager* dbgr = new nsSymantecDebugManager(outer, jvmMgr);
    nsresult result = dbgr->QueryInterface(aIID, aInstancePtr);
    if (result != NS_OK) {
        delete dbgr;
    }
    return result;
}

#if defined(XP_PC) && defined(_WIN32)
extern "C" HWND FindNavigatorHiddenWindow(void);
#endif

NS_METHOD_(PRBool)
nsSymantecDebugManager::SetDebugAgentPassword(PRInt32 pwd)
{
#if defined(XP_PC) && defined(_WIN32)
    HWND win = FindNavigatorHiddenWindow();
    HANDLE sem;
    long err;

    /* set up by aHiddenFrameClass in CNetscapeApp::InitInstance */
    err = SetWindowLong(win, 0, pwd);	
    if (err == 0) {
//        PR_LOG(NSJAVA, PR_LOG_ALWAYS,
//               ("SetWindowLong returned %ld (err=%d)\n", err, GetLastError()));
        /* continue so that we try to wake up the DebugManager */
    }
    sem = OpenSemaphore(SEMAPHORE_MODIFY_STATE, FALSE, "Netscape-Symantec Debugger");
    if (sem) {
        ReleaseSemaphore(sem, 1, NULL);
        CloseHandle(sem);
    }
    return PR_TRUE;
#else
    return PR_FALSE;
#endif
}

////////////////////////////////////////////////////////////////////////////////
