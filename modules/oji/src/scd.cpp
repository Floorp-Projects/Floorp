/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Methods to support the JVM Plugin API
////////////////////////////////////////////////////////////////////////////////

#include "nsJVMManager.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kISymantecDebugManagerIID, NS_ISYMANTECDEBUGMANAGER_IID);

NS_IMPL_AGGREGATED(nsSymantecDebugManager);

nsSymantecDebugManager::nsSymantecDebugManager(nsISupports* outer, nsJVMManager* jvmMgr)
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
	 if (!aInstancePtr)
	     return NS_ERROR_INVALID_POINTER;

	 if (aIID.Equals(NS_GET_IID(nsISupports)))
	     *aInstancePtr = GetInner();
    else if (aIID.Equals(kISymantecDebugManagerIID))
        *aInstancePtr = NS_STATIC_CAST(nsISymantecDebugManager*, this);
	 else {
	     *aInstancePtr = nsnull;
		  return NS_NOINTERFACE;
	 }

	 NS_ADDREF((nsISupports*)*aInstancePtr);
	 return NS_OK;
}

NS_METHOD
nsSymantecDebugManager::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr, 
                               nsJVMManager* jvmMgr)
{
	 if (!aInstancePtr)
	     return NS_ERROR_INVALID_POINTER;
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_ERROR_INVALID_ARG;
    nsSymantecDebugManager* dbgr = new nsSymantecDebugManager(outer, jvmMgr);
    if (dbgr == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

	 nsresult rv = dbgr->AggregatedQueryInterface(aIID, aInstancePtr);
	 if (NS_FAILED(rv)) {
	     delete dbgr;
		  return rv;
	 }
    return rv;
}

#if defined(XP_PC) && defined(_WIN32)
extern "C" HWND FindNavigatorHiddenWindow(void);
#endif

NS_METHOD
nsSymantecDebugManager::SetDebugAgentPassword(PRInt32 pwd)
{
#if defined(XP_PC) && defined(_WIN32)
    HWND win = NULL;
    /*
    ** TODO:amusil Get to a hidden window for symantec debugger to get its password from.
    HWND win = FindNavigatorHiddenWindow();
    */
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
    return NS_OK;
#else
    return NS_ERROR_FAILURE;
#endif
}

////////////////////////////////////////////////////////////////////////////////
