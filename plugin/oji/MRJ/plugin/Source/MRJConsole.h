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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
	MRJConsole.h
	
	Implements the JVM console interface.
	
	by Patrick C. Beard.
 */

#ifndef CALL_NOT_IN_CARBON
	#define CALL_NOT_IN_CARBON 1
#endif

#include "nsIJVMConsole.h"
#include "nsIEventHandler.h"
#include "SupportsMixin.h"

#include "jni.h"
#include <JManager.h>

class MRJPlugin;
class MRJSession;
class TopLevelFrame;

class MRJConsole :	public nsIJVMConsole,
					public nsIEventHandler,
					public SupportsMixin {
public:
	MRJConsole(MRJPlugin* plugin);
	virtual ~MRJConsole();

	// NS_DECL_ISUPPORTS
	// DECL_SUPPORTS_MIXIN
	NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
	NS_IMETHOD_(nsrefcnt) AddRef(void);
	NS_IMETHOD_(nsrefcnt) Release(void);

	// nsIJVMConsole methods.

    NS_IMETHOD
    Show(void);

    NS_IMETHOD
    Hide(void);

    NS_IMETHOD
    IsVisible(PRBool* isVisible);

    // Prints a message to the Java console. The encodingName specifies the
    // encoding of the message, and if NULL, specifies the default platform
    // encoding.
    NS_IMETHOD
    Print(const char* msg, const char* encodingName = NULL);

    NS_IMETHOD
    HandleEvent(nsPluginEvent* event, PRBool* eventHandled);
    
    // Private implementation methods.

	void setFrame(TopLevelFrame* frame) { mFrame = frame; }
	
	void write(const void *message, UInt32 messageLengthInBytes);

private:
	// Private implementation methods.
	OSStatus CallConsoleMethod(jmethodID method);
	OSStatus CallConsoleMethod(jmethodID method, jobject arg);

	void Initialize();
	
private:
	MRJPlugin* mPlugin;
	MRJSession* mSession;
	PRBool mIsInitialized;

	jclass mConsoleClass;
	jmethodID mInitMethod;
	jmethodID mDisposeMethod;
	jmethodID mShowMethod;
	jmethodID mHideMethod;
	jmethodID mVisibleMethod;
	jmethodID mPrintMethod;
	jmethodID mFinishMethod;

	jbooleanArray mResults;

	JMAWTContextRef mContext;
	TopLevelFrame* mFrame;

	// support for SupportsMixin.
	static const InterfaceInfo sInterfaces[];
	static const UInt32 kInterfaceCount;
};
