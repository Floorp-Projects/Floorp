/*
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
	MRJConsole.h
	
	Implements the JVM console interface.
	
	by Patrick C. Beard.
 */

#include "nsIJVMConsole.h"
#include "nsIEventHandler.h"
#include "SupportsMixin.h"

#include <jni.h>
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
	NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
	NS_IMETHOD_(nsrefcnt) AddRef(void);
	NS_IMETHOD_(nsrefcnt) Release(void);

	// nsIJVMConsole methods.

    NS_IMETHOD
    ShowConsole(void);

    NS_IMETHOD
    HideConsole(void);

    NS_IMETHOD
    IsConsoleVisible(PRBool* isVisible);

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

	// support for nsISupports.
	static nsID sInterfaceIDs[];
};
