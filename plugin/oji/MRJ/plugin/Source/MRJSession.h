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
	MRJSession.h

	Encapsulates a session with the MacOS Runtime for Java.
	
	by Patrick C. Beard.
 */

#pragma once

#ifndef CALL_NOT_IN_CARBON
	#define CALL_NOT_IN_CARBON 1
#endif

#include "jni.h"
#include "JManager.h"

class NativeMessage {
public:
	NativeMessage() : mNext(NULL) {}
	
	virtual void execute() = 0;

	void setNext(NativeMessage* next) { mNext = next; }
	NativeMessage* getNext() { return mNext; }

private:
	NativeMessage* mNext;
};

// FIXME:  need an interface for setting security options, etc.

class MRJContext;
class Monitor;

class MRJSession {
public:
	MRJSession();
	virtual ~MRJSession();
	
	JMSessionRef getSessionRef();
	
	JNIEnv* getCurrentEnv();
	JNIEnv* getMainEnv();
	JavaVM* getJavaVM();
	
	Boolean onMainThread();
	
	Boolean addToClassPath(const FSSpec& fileSpec);
	Boolean addToClassPath(const char* dirPath);
	Boolean addURLToClassPath(const char* fileURL);

	char* getProperty(const char* propertyName);
	
	void setStatus(OSStatus status);
	OSStatus getStatus();
	
	void idle(UInt32 milliseconds = kDefaultJMTime);

	void sendMessage(NativeMessage* message, Boolean async = false);
	
	/**
	 * Used to prevent reentering the VM.
	 */
	void lock();
	void unlock();

private:
	void postMessage(NativeMessage* message);
	void dispatchMessage();
	
private:
	JMSessionRef mSession;
	OSStatus mStatus;

	JNIEnv* mMainEnv;

	// Message queue.
	NativeMessage* mFirst;
	NativeMessage* mLast;
	Monitor* mMessageMonitor;
	
	UInt32 mLockCount;
};
