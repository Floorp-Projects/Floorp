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

/**
 * nsILiveConnectManager provides the necessary interfaces for initializing LiveConnect
 * and provides services to the DOM.
 */

#ifndef nsILiveConnectManager_h___
#define nsILiveConnectManager_h___

#ifndef nsISupports_h___
#include "nsISupports.h"
#endif
#ifndef JNI_H
#include "jni.h"
#endif

// {d20c8081-cbcb-11d2-a5a0-e884aed9c9fc}
#define NS_ILIVECONNECTMANAGER_IID \
{ 0xd20c8081, 0xcbcb, 0x11d2, { 0xa5, 0xa0, 0xe8, 0x84, 0xae, 0xd9, 0xc9, 0xfc } }

// {d20c8083-cbcb-11d2-a5a0-e884aed9c9fc}
#define NS_LIVECONNECTMANAGER_CID \
{ 0xd20c8083, 0xcbcb, 0x11d2, { 0xa5, 0xa0, 0xe8, 0x84, 0xae, 0xd9, 0xc9, 0xfc } }

struct JSRuntime;
struct JSContext;
struct JSObject;

class nsILiveConnectManager : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILIVECONNECTMANAGER_IID)
	
	/**
	 * Attempts to start LiveConnect using the specified JSRuntime.
	 */
	NS_IMETHOD
    StartupLiveConnect(JSRuntime* runtime, PRBool& outStarted) = 0;
    
	/**
	 * Attempts to stop LiveConnect using the specified JSRuntime.
	 */
	NS_IMETHOD
    ShutdownLiveConnect(JSRuntime* runtime, PRBool& outShutdown) = 0;

	/**
	 * Indicates whether LiveConnect can be used.
	 */
	NS_IMETHOD
    IsLiveConnectEnabled(PRBool& outEnabled) = 0;
    
    /**
     * Initializes a JSContext with the proper LiveConnect support classes.
     */
	NS_IMETHOD
    InitLiveConnectClasses(JSContext* context, JSObject* globalObject) = 0;
    
    /**
     * Creates a JavaScript wrapper for a Java object.
     */
     NS_IMETHOD
     WrapJavaObject(JSContext* context, jobject javaObject, JSObject* *outJSObject) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsILiveConnectManager_h___ */
