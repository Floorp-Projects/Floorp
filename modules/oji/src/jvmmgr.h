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

#ifndef jvmmgr_h___
#define jvmmgr_h___

#include "nsjvm.h"
#include "nsscd.h"
#include "nsAgg.h"

class nsSymantecDebugManager;

////////////////////////////////////////////////////////////////////////////////
// JVMMgr is the interface to the JVM manager that the browser sees. All
// files that want to include java services should include this header file.
// NPIJVMPluginManager is the more limited interface what the JVM plugin sees.

class JVMMgr : public NPIJVMPluginManager {
public:

    NS_DECL_AGGREGATED
    
    ////////////////////////////////////////////////////////////////////////////
    // from NPIJVMPluginManager:

    // ====> These are usually only called by the plugin, not the browser...

    NS_IMETHOD_(void)
    BeginWaitCursor(void);

    NS_IMETHOD_(void)
    EndWaitCursor(void);

    NS_IMETHOD_(const char*)
    GetProgramPath(void);

    NS_IMETHOD_(const char*)
    GetTempDirPath(void);

    NS_IMETHOD_(nsresult)
    GetFileName(const char* fn, FileNameType type,
                char* resultBuf, PRUint32 bufLen);

    NS_IMETHOD_(nsresult)
    NewTempFileName(const char* prefix, char* resultBuf, PRUint32 bufLen);

    NS_IMETHOD_(PRBool)
    HandOffJSLock(PRThread* oldOwner, PRThread* newOwner);

    NS_IMETHOD_(void)
    ReportJVMError(JVMEnv* env, JVMError err);

    NS_IMETHOD_(PRBool)
    SupportsURLProtocol(const char* protocol);

    ////////////////////////////////////////////////////////////////////////////
    // JVMMgr specific methods:

    // ====> From here on are things only called by the browser, not the plugin...

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    NPIJVMPlugin* GetJVM(void);

    // Unlike the NPIJVMPlugin::StartupJVM, this version handles putting
    // up any error dialog:
    JVMStatus   StartupJVM(void);
    JVMStatus   ShutdownJVM(PRBool fullShutdown = PR_TRUE);

    PRBool      GetJVMEnabled(void);
    void        SetJVMEnabled(PRBool enable);
    JVMStatus   GetJVMStatus(void);

    void SetProgramPath(const char* path) { fProgramPath = path; }

protected:    
    JVMMgr(nsISupports* outer);
    virtual ~JVMMgr(void);

    void        EnsurePrefCallbackRegistered(void);
    const char* GetJavaErrorString(JVMEnv* env);

    NPIJVMPlugin*       fJVM;
    PRUint16            fWaiting;
    void*               fOldCursor;
    const char*         fProgramPath;
    PRBool              fRegisteredJavaPrefChanged;
    nsISupports*        fDebugManager;
 
};

////////////////////////////////////////////////////////////////////////////////
// Symantec Debugger Stuff

class nsSymantecDebugManager : public NPISymantecDebugManager {
public:

    NS_DECL_AGGREGATED

    NS_IMETHOD_(PRBool)
    SetDebugAgentPassword(PRInt32 pwd);

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
           JVMMgr* jvmMgr);

protected:
    nsSymantecDebugManager(nsISupports* outer, JVMMgr* jvmMgr);
    virtual ~nsSymantecDebugManager(void);

    JVMMgr*             fJVMMgr;

};

////////////////////////////////////////////////////////////////////////////////
// JVMInstancePeer: The browser makes one of these when it sees an APPLET or
// appropriate OBJECT tag.

class JVMInstancePeer : public NPIJVMPluginInstancePeer {
public:

    NS_DECL_AGGREGATED

    ////////////////////////////////////////////////////////////////////////////
    // Methods specific to NPIPluginInstancePeer:

    // ====> These are usually only called by the plugin, not the browser...

    NS_IMETHOD_(NPIPlugin*)
    GetClass(void);

    // (Corresponds to NPP_New's MIMEType argument.)
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void);

    // (Corresponds to NPP_New's mode argument.)
    NS_IMETHOD_(NPPluginType)
    GetMode(void);

    // Get a ptr to the paired list of attribute names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD_(NPPluginError)
    GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values);

    // Get the value for the named attribute.  Returns null
    // if the attribute was not set.
    NS_IMETHOD_(const char*)
    GetAttribute(const char* name);

    // Get a ptr to the paired list of parameter names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD_(NPPluginError)
    GetParameters(PRUint16& n, const char*const*& names, const char*const*& values);

    // Get the value for the named parameter.  Returns null
    // if the parameter was not set.
    NS_IMETHOD_(const char*)
    GetParameter(const char* name);

    // Get the complete text of the HTML tag that was
    // used to instantiate this plugin
    NS_IMETHOD_(const char*)
    GetTagText(void);

    // Get the type of the HTML tag that was used ot instantiate this
    // plugin.  Currently supported tags are EMBED, OBJECT and APPLET.
    // 
    NS_IMETHOD_(NPTagType) 
    GetTagType(void);

    NS_IMETHOD_(NPIPluginManager*)
    GetPluginManager(void);

    // (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
    //   notifyData: When present, URLNotify is called passing the notifyData back
    //          to the client. When NULL, this call behaves like NPN_GetURL.
    // New arguments:
    //   altHost: An IP-address string that will be used instead of the host
    //          specified in the URL. This is used to prevent DNS-spoofing attacks.
    //          Can be defaulted to NULL meaning use the host in the URL.
    //   referrer: 
    //   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
    //          even if the user currently has JavaScript disabled. 
    NS_IMETHOD_(NPPluginError)
    GetURL(const char* url, const char* target, void* notifyData = NULL,
           const char* altHost = NULL, const char* referrer = NULL,
           PRBool forceJSEnabled = PR_FALSE);

    // (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
    //   notifyData: When present, URLNotify is called passing the notifyData back
    //          to the client. When NULL, this call behaves like NPN_GetURL.
    // New arguments:
    //   altHost: An IP-address string that will be used instead of the host
    //          specified in the URL. This is used to prevent DNS-spoofing attacks.
    //          Can be defaulted to NULL meaning use the host in the URL.
    //   referrer: 
    //   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
    //          even if the user currently has JavaScript disabled. 
    //   postHeaders: A string containing post headers.
    //   postHeadersLength: The length of the post headers string.
    NS_IMETHOD_(NPPluginError)
    PostURL(const char* url, const char* target, PRUint32 bufLen, 
            const char* buf, PRBool file, void* notifyData = NULL,
            const char* altHost = NULL, const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, const char* postHeaders = NULL);

    // (Corresponds to NPN_NewStream.)
    NS_IMETHOD_(NPPluginError)
    NewStream(nsMIMEType type, const char* target,
              NPIPluginManagerStream* *result);

    // (Corresponds to NPN_Status.)
    NS_IMETHOD_(void)
    ShowStatus(const char* message);

    // (Corresponds to NPN_UserAgent.)
    NS_IMETHOD_(const char*)
    UserAgent(void);

    // (Corresponds to NPN_GetValue.)
    NS_IMETHOD_(NPPluginError)
    GetValue(NPPluginManagerVariable variable, void *value);

    // (Corresponds to NPN_SetValue.)
    NS_IMETHOD_(NPPluginError)
    SetValue(NPPluginVariable variable, void *value);

    // (Corresponds to NPN_InvalidateRect.)
    NS_IMETHOD_(void)
    InvalidateRect(nsRect *invalidRect);

    // (Corresponds to NPN_InvalidateRegion.)
    NS_IMETHOD_(void)
    InvalidateRegion(nsRegion invalidRegion);

    // (Corresponds to NPN_ForceRedraw.)
    NS_IMETHOD_(void)
    ForceRedraw(void);

    // New top-level window handling calls for Mac:
    
    NS_IMETHOD_(void)
    RegisterWindow(void* window);
    
    NS_IMETHOD_(void)
    UnregisterWindow(void* window);

	// Menu ID allocation calls for Mac:
    NS_IMETHOD_(PRInt16)
	AllocateMenuID(PRBool isSubmenu);

    ////////////////////////////////////////////////////////////////////////////
    // Methods specific to NPIJVMPluginInstancePeer:

    NS_IMETHOD_(const char *)
    GetCode(void);

    NS_IMETHOD_(const char *)
    GetCodeBase(void);

    NS_IMETHOD_(const char *)
    GetArchive(void);

    NS_IMETHOD_(PRBool)
    GetMayScript(void);

    NS_IMETHOD_(const char *)
    GetID(void);

    // XXX reload method?

    // Returns a unique id for the current document on which the
    // plugin is displayed.
    NS_IMETHOD_(PRUint32)
    GetDocumentID(void);

    NS_IMETHOD_(const char*)
    GetDocumentBase(void);
    
    NS_IMETHOD_(INTL_CharSetInfo)
    GetDocumentCharSetInfo(void);
    
    NS_IMETHOD_(const char*)
    GetAlignment(void);
    
    NS_IMETHOD_(PRUint32)
    GetWidth(void);
    
    NS_IMETHOD_(PRUint32)
    GetHeight(void);
    
    NS_IMETHOD_(PRUint32)
    GetBorderVertSpace(void);
    
    NS_IMETHOD_(PRUint32)
    GetBorderHorizSpace(void);

    NS_IMETHOD_(PRBool)
    Tickle(void);
    
    ////////////////////////////////////////////////////////////////////////////
    // Methods specific to JVMPluginInstancePeer:
    
    // ====> From here on are things only called by the browser, not the plugin...

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
           MWContext* cx, LO_CommonPluginStruct* lo);

protected:

    JVMInstancePeer(nsISupports* outer, MWContext* cx, LO_CommonPluginStruct* lo);
    virtual ~JVMInstancePeer(void);

    // Instance Variables:
    NPIPluginInstancePeer*      fPluginInstancePeer;
    MWContext*                  fContext;
    LO_CommonPluginStruct*      fLayoutElement;
    PRUint32                    fUniqueID;
    char*                       fSimulatedCodebase;
};

////////////////////////////////////////////////////////////////////////////////

// Returns the JVM manager. You must do a Release on the
// pointer returned when you're done with it.
extern "C" PR_EXTERN(JVMMgr*)
JVM_GetJVMMgr(void);

////////////////////////////////////////////////////////////////////////////////
#endif /* jvmmgr_h___ */
