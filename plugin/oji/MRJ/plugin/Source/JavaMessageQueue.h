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
	JavaMessageQueue.h
 */

#pragma once

#ifndef JNI_H
#include "jni.h"
#endif

class Monitor;

class JavaMessage {
public:
	JavaMessage() : mNext(NULL) {}
	virtual ~JavaMessage() {}

	void setNext(JavaMessage* next) { mNext = next; }
	JavaMessage* getNext() { return mNext; }

	virtual void execute(JNIEnv* env) = 0;

private:
	JavaMessage* mNext;
};

class JavaMessageQueue {
public:
	JavaMessageQueue(Monitor* monitor);

	void putMessage(JavaMessage* msg);
	JavaMessage* getMessage();

	void enter();
	void exit();

	void wait();
	void wait(long long millis);
	void notify();

private:
	// Message queue.
	JavaMessage* mFirst;
	JavaMessage* mLast;
	Monitor* mMonitor;
};
