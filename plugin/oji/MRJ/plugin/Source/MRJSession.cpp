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
	MRJSession.cpp

	Encapsulates a session with the MacOS Runtime for Java.
	
	by Patrick C. Beard.
 */

#include "MRJSession.h"
#include "MRJContext.h"
#include "MRJConsole.h"
#include "MRJMonitor.h"

#include <string.h>
#include <Memory.h>
#include <Files.h>
#include <Dialogs.h>
#include <Appearance.h>

extern MRJConsole* theConsole;

static Boolean appearanceManagerExists()
{
	long response = 0;
	return (Gestalt(gestaltAppearanceAttr, &response) == noErr && (response & (1 << gestaltAppearanceExists)));
}

static void debug_out(StringPtr stream, const void *message, UInt32 messageLengthInBytes)
{
	Str255 pmsg;
	pmsg[0] = messageLengthInBytes;
	::BlockMoveData(message, &pmsg[1], pmsg[0]);

	// why not display this using the Appearance Manager's wizzy new alert?
	if (appearanceManagerExists()) {
		static AlertStdAlertParamRec params = {
			false, 	// Boolean movable;					/* Make alert movable modal */
			false, 	// Boolean helpButton;				/* Is there a help button? */
			NULL, 	// ModalFilterUPP filterProc;		/* Event filter */
			"\pOK",	// StringPtr defaultText;			/* Text for button in OK position */
			NULL,		// StringPtr cancelText;			/* Text for button in cancel position */
			NULL,		// StringPtr otherText;				/* Text for button in left position */
			1, 		// SInt16 defaultButton;			/* Which button behaves as the default */
			0,			// SInt16 cancelButton;				/* Which one behaves as cancel (can be 0) */
			kWindowDefaultPosition
						// UInt16 position;					/* Position (kWindowDefaultPosition in this case */
																	/* equals kWindowAlertPositionParentWindowScreen) */
		};
		SInt16 itemHit = 0;
		OSErr result = ::StandardAlert(kAlertPlainAlert, stream, pmsg, &params, &itemHit);
	} else {
		::DebugStr(pmsg);
	}
}

static void java_stdout(JMSessionRef session, const void *message, SInt32 messageLengthInBytes)
{
	// if (theConsole != NULL)
	//	theConsole->write(message, messageLengthInBytes);
	/* else
		debug_out("\pSystem.out:", message, messageLengthInBytes); */
}

static void java_stderr(JMSessionRef session, const void *message, SInt32 messageLengthInBytes)
{
	// if (theConsole != NULL)
	//	theConsole->write(message, messageLengthInBytes);
	/* else
		debug_out("\pSystem.err:", message, messageLengthInBytes); */
}

static SInt32 java_stdin(JMSessionRef session, void *buffer, SInt32 maxBufferLength)
{
	return -1;
}

static Boolean java_exit(JMSessionRef session, SInt32 status)
{
	return false;					/* not allowed in a plugin. */
}

static Boolean java_authenticate(JMSessionRef session, const char *url, const char *realm, char userName[255], char password[255])
{
	return false;
}

static void java_lowmem(JMSessionRef session)
{
	/* can ask Netscape to purge some memory. */
	// NPN_MemFlush(512 * 1024);
}

MRJSession::MRJSession()
	:	mSession(NULL), mStatus(noErr), mMainEnv(NULL), mFirst(NULL), mLast(NULL), mMessageMonitor(NULL), mLockCount(0)
{
	// Make sure JManager exists.
	if (&::JMGetVersion != NULL && ::JMGetVersion() >= kJMVersion) {
		static JMSessionCallbacks callbacks = {
			kJMVersion,					/* should be set to kJMVersion */
			&java_stdout,				/* JM will route "stdout" to this function. */
			&java_stderr,				/* JM will route "stderr" to this function. */
			&java_stdin,				/* read from console - can be nil for default behavior (no console IO) */
			&java_exit,					/* handle System.exit(int) requests */
			&java_authenticate,		/* present basic authentication dialog */
			&java_lowmem				/* Low Memory notification Proc */
		};
		
		mStatus = ::JMOpenSession(&mSession, eJManager2Defaults, eCheckRemoteCode,
											&callbacks, kTextEncodingMacRoman, NULL);
		
		// capture the main environment, so it can be distinguished from true Java threads.
		if (mStatus == noErr) mMainEnv = ::JMGetCurrentEnv(mSession);
		
		// create a monitor for the message queue to unblock Java threads.
		mMessageMonitor = new MRJMonitor(this);
	} else {
		mStatus = kJMVersionError;
	}
}

MRJSession::~MRJSession()
{
	if (mMessageMonitor != NULL) {
		mMessageMonitor->notifyAll();
		delete mMessageMonitor;
	}

	if (mSession != NULL) {
		mStatus = ::JMCloseSession(mSession);
		mSession = NULL;
	}
}

JMSessionRef MRJSession::getSessionRef()
{
	return mSession;
}

JNIEnv* MRJSession::getCurrentEnv()
{
	return ::JMGetCurrentEnv(mSession);
}

JNIEnv* MRJSession::getMainEnv()
{
	return mMainEnv;
}

JavaVM* MRJSession::getJavaVM()
{
	JNIEnv* env = ::JMGetCurrentEnv(mSession);
	JavaVM* vm = NULL;
	env->GetJavaVM(&vm);
	return vm;
}

Boolean MRJSession::onMainThread()
{
	JNIEnv* env = ::JMGetCurrentEnv(mSession);
	return (env == mMainEnv);
}

inline StringPtr c2p(const char* cstr, StringPtr pstr)
{
	pstr[0] = (unsigned char)strlen(cstr);
	::BlockMoveData(cstr, pstr + 1, pstr[0]);
	return pstr;
}

Boolean MRJSession::addToClassPath(const FSSpec& fileSpec)
{
	return (::JMAddToClassPath(mSession, &fileSpec) == noErr);
}

Boolean MRJSession::addToClassPath(const char* dirPath)
{
	// Need to convert the path into an FSSpec, and add it MRJ's class path.
	Str255 path;
	FSSpec pathSpec;
	OSStatus status = ::FSMakeFSSpec(0, 0, c2p(dirPath, path), &pathSpec);
	if (status == noErr)
		status = ::JMAddToClassPath(mSession, &pathSpec);
	return (status == noErr);
}

Boolean MRJSession::addURLToClassPath(const char* fileURL)
{
	OSStatus status = noErr;
	// Need to convert the URL into an FSSpec, and add it MRJ's class path.
	JMTextRef urlRef = NULL;
	status = ::JMNewTextRef(mSession, &urlRef, kTextEncodingMacRoman, fileURL, strlen(fileURL));
	if (status == noErr) {
		FSSpec pathSpec;
		status = ::JMURLToFSS(mSession, urlRef, &pathSpec);
		if (status == noErr)
			status = ::JMAddToClassPath(mSession, &pathSpec);
		::JMDisposeTextRef(urlRef);
	}
	
	return (status == noErr);
}

char* MRJSession::getProperty(const char* propertyName)
{
	char* result = NULL;
	OSStatus status = noErr;
	JMTextRef nameRef = NULL, valueRef = NULL;
	status = ::JMNewTextRef(mSession, &nameRef, kTextEncodingMacRoman, propertyName, strlen(propertyName));
	if (status == noErr) {
		status = ::JMGetSessionProperty(mSession, nameRef, &valueRef);
		::JMDisposeTextRef(nameRef);
		if (status == noErr && valueRef != NULL) {
			UInt32 valueLength = 0;
			status = ::JMGetTextLengthInBytes(valueRef, kTextEncodingMacRoman, &valueLength);
			if (status == noErr) {
				result = new char[valueLength + 1];
				if (result != NULL) {
					UInt32 actualLength;
					status = ::JMGetTextBytes(valueRef, kTextEncodingMacRoman, result, valueLength, &actualLength);
					result[valueLength] = '\0';
				}
				::JMDisposeTextRef(valueRef);
			}
		}
	}
	return result;
}

void MRJSession::setStatus(OSStatus status)
{
	mStatus = status;
}

OSStatus MRJSession::getStatus()
{
	return mStatus;
}

void MRJSession::idle(UInt32 milliseconds)
{
	// Each call to idle processes a single message.
	dispatchMessage();

	// Guard against entering the VM multiple times.
	if (mLockCount == 0) {
		lock();
		mStatus = ::JMIdle(mSession, milliseconds);
		unlock();
	}
#if 0
	 else {
		// sleep the current thread.
		JNIEnv* env = getCurrentEnv();
		jclass threadClass = env->FindClass("java/lang/Thread");
		if (threadClass != NULL) {
			jmethodID sleepMethod = env->GetStaticMethodID(threadClass, "sleep", "(J)V");
			env->CallStaticVoidMethod(threadClass, sleepMethod, jlong(milliseconds));
			env->DeleteLocalRef(threadClass);
		}
	}
#endif
}

void MRJSession::sendMessage(NativeMessage* message, Boolean async)
{
	// can't block the main env, otherwise messages will never be processed!
	if (onMainThread()) {
		message->execute();
	} else {
		postMessage(message);
		if (!async)
			mMessageMonitor->wait();
	}
}

/**
 * Put a message on the queue.
 */
void MRJSession::postMessage(NativeMessage* message)
{
	if (mFirst == NULL) {
		mFirst = mLast = message;
	} else {
		mLast->setNext(message);
		mLast = message;
	}
	message->setNext(NULL);
}

void MRJSession::dispatchMessage()
{
	if (mFirst != NULL) {
		NativeMessage* message = mFirst;
		mFirst = message->getNext();
		if (mFirst == NULL) mLast = NULL;
		
		message->setNext(NULL);
		message->execute();
		mMessageMonitor->notify();
	}
}

void MRJSession::lock()
{
	++mLockCount;
}

void MRJSession::unlock()
{
	--mLockCount;
}
