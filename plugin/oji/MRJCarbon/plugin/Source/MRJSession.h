/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
	MRJSession.h

	Encapsulates a session with the MacOS Runtime for Java.
	
	by Patrick C. Beard.
 */

#pragma once

#include <jni.h>

#include <vector>
#include <stringfwd>

class NativeMessage {
public:
	NativeMessage() : mNext(NULL) {}
	virtual ~NativeMessage() {}
	
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
	
	OSStatus open();
	OSStatus close();
	
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
	
	void idle(UInt32 milliseconds = 0x00000400);

	void sendMessage(NativeMessage* message, Boolean async = false);
	
	/**
	 * Used to prevent reentering the VM.
	 */
	void lock();
	void unlock();

private:
	void postMessage(NativeMessage* message);
	void dispatchMessage();
	
	std::string getClassPath();
	
private:
	OSStatus mStatus;
	
	typedef std::vector<FSRef> MRJClassPath;
	MRJClassPath mClassPath;

	JNIEnv* mMainEnv;
	JavaVM* mJavaVM;

	// Message queue.
	NativeMessage* mFirst;
	NativeMessage* mLast;
	Monitor* mMessageMonitor;
	
	UInt32 mLockCount;
};
