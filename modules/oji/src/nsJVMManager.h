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

#ifndef nsJVMManager_h___
#define nsJVMManager_h___

#include "jvmmgr.h"
#include "xp_core.h"    /* include first because of Bool problem */
#include "prtypes.h"
#include "nsCom.h"

#include "nsjvm.h"
#include "nsAgg.h"
#include "jsjava.h"
#include "nsVector.h"

class nsSymantecDebugManager;

struct ThreadLocalStorageAtIndex0 {
    PRUint32 refcount;
};
typedef struct ThreadLocalStorageAtIndex0 ThreadLocalStorageAtIndex0;

/*******************************************************************************
 * NsJVMManager is the interface to the JVM manager that the browser sees. All
 * files that want to include java services should include this header file.
 * nsIJVMManager is the more limited interface what the JVM plugin sees.
 ******************************************************************************/

struct nsJVMManager : public nsIJVMManager {
public:

    NS_DECL_AGGREGATED
    
    /* from nsIJVMManager: */

    /* NsJVMManager specific methods: */

    /* ====> From here on are things only called by the browser, not the plugin... */

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    nsIJVMPlugin* GetJVMPlugin(void) { return fJVM; }

    /* Unlike the nsIJVMPlugin::StartupJVM, this version handles putting
     * up any error dialog: */
    nsJVMStatus StartupJVM(void);
    nsJVMStatus ShutdownJVM(PRBool fullShutdown = PR_FALSE);
    nsJVMStatus GetJVMStatus(void);
    void SetJVMEnabled(PRBool enabled);
    
    void        ReportJVMError(nsresult err);
    const char* GetJavaErrorString(JNIEnv* env);

    nsresult    AddToClassPath(const char* dirPath);
    PRBool      MaybeStartupLiveConnect(void);
    PRBool      MaybeShutdownLiveConnect(void);
    PRBool      IsLiveConnectEnabled(void);
    JSJavaVM*   GetJSJavaVM(void) { return fJSJavaVM; }


protected:    
    nsJVMManager(nsISupports* outer);
    virtual ~nsJVMManager(void);

    void        EnsurePrefCallbackRegistered(void);
    const char* GetJavaErrorString(JRIEnv* env);

    nsIJVMPlugin*       fJVM;
    nsJVMStatus         fStatus;
    PRBool              fRegisteredJavaPrefChanged;
    nsISupports*        fDebugManager;
    JSJavaVM *          fJSJavaVM;  
    nsVector*           fClassPathAdditions;
};

/*******************************************************************************
 * Symantec Debugger Stuff
 ******************************************************************************/

class nsSymantecDebugManager : public nsISymantecDebugManager {
public:

    NS_DECL_AGGREGATED

    NS_IMETHOD
    SetDebugAgentPassword(PRInt32 pwd);

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
           nsJVMManager* nsJVMManager);

protected:
    nsSymantecDebugManager(nsISupports* outer, nsJVMManager* nsJVMManager);
    virtual ~nsSymantecDebugManager(void);

    nsJVMManager* fJVMMgr;

};

////////////////////////////////////////////////////////////////////////////////

#define NS_JVMMANAGER_CID                            \
{ /* 38e7ef10-58df-11d2-8164-006008119d7a */         \
    0x38e7ef10,                                      \
    0x58df,                                          \
    0x11d2,                                          \
    {0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // nsJVMManager_h___
