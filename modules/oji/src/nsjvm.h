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
// NETSCAPE JAVA VM PLUGIN EXTENSIONS
// 
// This interface allows a Java virtual machine to be plugged into
// Communicator to implement the APPLET tag and host applets.
// 
// Note that this is the C++ interface that the plugin sees. The browser
// uses a specific implementation of this, nsJVMPlugin, found in navjvm.h.
////////////////////////////////////////////////////////////////////////////////

#ifndef nsjvm_h___
#define nsjvm_h___

#include "nsIPlug.h"
#include "jri.h"        // XXX for now
#include "jni.h"
#include "prthread.h"

#include "libi18n.h"    // XXX way too much browser dependence
#include "intl_csi.h"

////////////////////////////////////////////////////////////////////////////////

#define NPJVM_MIME_TYPE         "application/x-java-vm"

enum JVMError {
    JVMError_Ok                 = NPPluginError_NoError,
    JVMError_Base               = 0x1000,
    JVMError_InternalError      = JVMError_Base,
    JVMError_NoClasses,
    JVMError_WrongClasses,
    JVMError_JavaError,
    JVMError_NoDebugger
};

#ifdef XP_MAC
typedef JNIEnv JVMEnv;
#else
typedef JRIEnv JVMEnv;
#endif


////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Manager
// This interface defines additional entry points that are available
// to JVM plugins for browsers that support JVM plugins.

class NPIJVMPlugin;

class NPIJVMPluginManager : public nsISupports {
public:

    NS_IMETHOD_(void)
    BeginWaitCursor(void) = 0;

    NS_IMETHOD_(void)
    EndWaitCursor(void) = 0;

    NS_IMETHOD_(const char*)
    GetProgramPath(void) = 0;

    NS_IMETHOD_(const char*)
    GetTempDirPath(void) = 0;

    enum FileNameType { SIGNED_APPLET_DBNAME, TEMP_FILENAME };

    NS_IMETHOD_(nsresult)
    GetFileName(const char* fn, FileNameType type,
                char* resultBuf, PRUint32 bufLen) = 0;

    NS_IMETHOD_(nsresult)
    NewTempFileName(const char* prefix, char* resultBuf, PRUint32 bufLen) = 0;

    NS_IMETHOD_(PRBool)
    HandOffJSLock(PRThread* oldOwner, PRThread* newOwner) = 0;

    NS_IMETHOD_(void)
    ReportJVMError(JVMEnv* env, JVMError err) = 0;      // XXX JNIEnv*

    NS_IMETHOD_(PRBool)
    SupportsURLProtocol(const char* protocol) = 0;

};

#define NP_IJVMPLUGINMANAGER_IID                     \
{ /* a1e5ed50-aa4a-11d1-85b2-00805f0e4dfe */         \
    0xa1e5ed50,                                      \
    0xaa4a,                                          \
    0x11d1,                                          \
    {0x85, 0xb2, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////

enum JVMStatus {
    JVMStatus_Enabled,  // but not Running
    JVMStatus_Disabled, // explicitly disabled
    JVMStatus_Running,  // enabled and started
    JVMStatus_Failed    // enabled but failed to start
};

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Interface
// This interface defines additional entry points that a plugin developer needs
// to implement in order to implement a Java virtual machine plugin. 

class NPIJVMPlugin : public NPIPlugin {
public:

    // QueryInterface may be used to obtain a JRIEnv or JNIEnv
    // from an NPIPluginManager.
    // (Corresponds to NPN_GetJavaEnv.)

    NS_IMETHOD_(JVMStatus)
    StartupJVM(void) = 0;

    NS_IMETHOD_(JVMStatus)
    ShutdownJVM(PRBool fullShutdown = PR_TRUE) = 0;

    NS_IMETHOD_(PRBool)
    GetJVMEnabled(void) = 0;

    NS_IMETHOD_(void)
    SetJVMEnabled(PRBool enable) = 0;

    NS_IMETHOD_(JVMStatus)
    GetJVMStatus(void) = 0;

    // Find or create a JNIEnv for the specified thread. The thread
    // parameter may be NULL indicating the current thread.
    // XXX JNIEnv*
    NS_IMETHOD_(JVMEnv*)
    EnsureExecEnv(PRThread* thread = NULL) = 0;

    NS_IMETHOD_(void)
    AddToClassPath(const char* dirPath) = 0;

    NS_IMETHOD_(void)
    ShowConsole(void) = 0;

    NS_IMETHOD_(void)
    HideConsole(void) = 0;

    NS_IMETHOD_(PRBool)
    IsConsoleVisible(void) = 0;

    NS_IMETHOD_(void)
    PrintToConsole(const char* msg) = 0;

    NS_IMETHOD_(const char*)
    GetClassPath(void) = 0;

    NS_IMETHOD_(const char*)
    GetSystemJARPath(void) = 0;
    
};

#define NP_IJVMPLUGIN_IID                            \
{ /* da6f3bc0-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xda6f3bc0,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Instance Interface

class NPIJVMPluginInstance : public NPIPluginInstance {
public:

    // This method is called when LiveConnect wants to find the Java object
    // associated with this plugin instance, e.g. the Applet or JavaBean object.
    NS_IMETHOD_(jobject) 
    GetJavaObject(void) = 0;

};

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Instance Peer Interface
// This interface provides additional hooks into the plugin manager that allow 
// a plugin to implement the plugin manager's Java virtual machine.

enum NPTagAttributeName {
    NPTagAttributeName_Width,
    NPTagAttributeName_Height,
    NPTagAttributeName_Classid,
    NPTagAttributeName_Code,
    NPTagAttributeName_Codebase,
    NPTagAttributeName_Docbase,
    NPTagAttributeName_Archive,
    NPTagAttributeName_Name,
    NPTagAttributeName_MayScript
};

class NPIJVMPluginInstancePeer : public NPIPluginInstancePeer {
public:

    // XXX reload method?

    // Returns a unique id for the current document on which the
    // plugin is displayed.
    NS_IMETHOD_(PRUint32)
    GetDocumentID(void) = 0;

    NS_IMETHOD_(const char *) 
    GetCode(void) = 0;

    NS_IMETHOD_(const char *) 
    GetCodeBase(void) = 0;

    NS_IMETHOD_(const char *) 
    GetArchive(void) = 0;

    NS_IMETHOD_(const char *) 
    GetID(void) = 0;

    NS_IMETHOD_(PRBool) 
    GetMayScript(void) = 0;
    
    NS_IMETHOD_(const char*)
    GetDocumentBase(void) = 0;
    
    NS_IMETHOD_(INTL_CharSetInfo)
    GetDocumentCharSetInfo(void) = 0;
    
    NS_IMETHOD_(const char*)
    GetAlignment(void) = 0;
    
    NS_IMETHOD_(PRUint32)
    GetWidth(void) = 0;
    
    NS_IMETHOD_(PRUint32)
    GetHeight(void) = 0;
    
    NS_IMETHOD_(PRUint32)
    GetBorderVertSpace(void) = 0;
    
    NS_IMETHOD_(PRUint32)
    GetBorderHorizSpace(void) = 0;

    NS_IMETHOD_(PRBool)
    Tickle(void) = 0;

};

#define NP_IJVMPLUGININSTANCEPEER_IID                \
{ /* 27b42df0-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x27b42df0,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
#endif /* nsjvm_h___ */
