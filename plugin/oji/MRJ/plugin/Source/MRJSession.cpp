/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
	MRJSession.cpp

	Encapsulates a session with the MacOS Runtime for Java.
	
	by Patrick C. Beard.
 */

#include "MRJSession.h"
#include "MRJPlugin.h"
#include "MRJContext.h"
#include "MRJConsole.h"
#include "MRJMonitor.h"
#include "MRJNetworking.h"

#include <ControlDefinitions.h>
#include <string.h>
#include <Memory.h>
#include <Files.h>
#include <Dialogs.h>
#include <Appearance.h>
#include <Resources.h>

#if (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
#include <ControlDefinitions.h>
#endif

extern MRJConsole* theConsole;
extern short thePluginRefnum;

static Str255 consoleOutFilename = "\pMRJPluginAppletOutput";
static bool fileCreated = false;

static SInt32 java_stdin(JMSessionRef session, void *buffer, SInt32 maxBufferLength)
{
	return -1;
}

static Boolean java_exit(JMSessionRef session, SInt32 status)
{
	return false;					/* not allowed in a plugin. */
}

static void getItemText(DialogPtr dialog, DialogItemIndex index, ResType textTag, char str[256])
{
	ControlHandle control;
	if (::GetDialogItemAsControl(dialog, index, &control) == noErr) {
		Size textSize;
		::GetControlDataSize(control, kControlNoPart, textTag, &textSize);
		if (textSize > 255) textSize = 255;
		::GetControlData(control, kControlNoPart, textTag, textSize, (Ptr)str, &textSize);
		str[textSize] = '\0';
	}
}

static void miniEgg () {

#if 0
	
	long count = 0;
	OSErr myErr;
	short myVRef;
	long myDirID;
	FSSpec mySpec;
	short refNum;
	Str255 holder;

	myErr = FindFolder(kOnSystemDisk, kPreferencesFolderType, kDontCreateFolder, &myVRef, &myDirID);

	if (myErr == noErr) {
	
		myErr = FSMakeFSSpec(myVRef, myDirID, "\pFileName", &mySpec);
		
		if ((myErr != noErr) && (myErr != fnfErr)) {
			return;
		}

		myErr = FSpCreate(&mySpec, 'ttxt', 'TEXT', smSystemScript);

		// if it exists just exit.
		if (myErr != noErr) {
			return;
		}
		
		//we care if this errs, but not enough to impede mozilla running.
		myErr = FSpOpenDF(&mySpec, fsWrPerm, &refNum);

		if (myErr != noErr) {
			return;
		}

		sprintf((char *)holder, "egg line1\r");
		count = strlen((char *)holder);
		myErr = FSWrite(refNum, &count, holder);
			        
		sprintf((char *)holder, "egg line2\r");
		count = strlen((char *)holder);
		myErr = FSWrite(refNum, &count, holder);

		sprintf((char *)holder, "egg line3\r");
		count = strlen((char *)holder);
		myErr = FSWrite(refNum, &count, holder);

	    // ...

		FlushVol("\p", refNum);
        myErr = FSClose(refNum);
	}

#endif

}

static void debug_out(const void *label, const void *message, UInt32 messageLengthInBytes)
{
	long count = 0;
	OSErr myErr;
	short myVRef;
	long myDirID;
	FSSpec mySpec;
	short refNum;
	Str255 holder;
	
	myErr = Gestalt(gestaltFindFolderAttr, &count);
	
	if ((myErr != noErr) || (! (count & (1 << gestaltFindFolderPresent)))) {
		return;
	}
	
	myErr = FindFolder(kOnSystemDisk, kPreferencesFolderType, kDontCreateFolder, &myVRef, &myDirID);

	if (myErr == noErr) {
	
		myErr = FSMakeFSSpec(myVRef, myDirID, consoleOutFilename, &mySpec);
		
		if ((myErr != noErr) && (myErr != fnfErr)) {
			return;
		}

		//will err if file exists, we don't care.
		myErr = FSpCreate(&mySpec, 'ttxt', 'TEXT', smSystemScript);

		//we care if this errs, but not enough to impede mozilla running.
		myErr = FSpOpenDF(&mySpec, fsWrPerm, &refNum);

		if (myErr != noErr) {
			return;
		}

		if (! fileCreated) {
			//write over
			miniEgg();
			myErr = SetEOF(refNum, 0);
			
			fileCreated = true;
			sprintf((char *)holder, "MRJ Console Output.\rMRJ Plugin Version: %s\r--------------------\r", MRJPlugin::PLUGIN_VERSION);
			
			count = strlen((char *)holder);
			myErr = FSWrite(refNum, &count, holder);
			
		} else {
			//append
			myErr = SetFPos(refNum, fsFromLEOF, 0);
		}

		count = strlen((char *)label);
		myErr = FSWrite(refNum, &count, label);

		count = messageLengthInBytes;
        myErr = FSWrite(refNum, &count, message);
        
		count = 1;
        myErr = FSWrite(refNum, &count, "\r");
        
		FlushVol("\p", refNum);
        myErr = FSClose(refNum);
	}


//	Str255 pmsg;
//if (myErr != noErr) {
//Str255 bla;
//sprintf((char *)bla, "FSpOpenDF error: %d", myErr);
//pmsg[0] = strlen((char *)bla);
//::BlockMoveData(bla, &pmsg[1], pmsg[0]);
//::DebugStr(pmsg);
//}

}

static void java_stdout(JMSessionRef session, const void *message, SInt32 messageLengthInBytes)
{
	if (theConsole != NULL) {
		theConsole->write(message, messageLengthInBytes);
	} else {
		debug_out("[System.out] ", message, messageLengthInBytes);
	}
}

static void java_stderr(JMSessionRef session, const void *message, SInt32 messageLengthInBytes)
{
	if (theConsole != NULL) {
		theConsole->write(message, messageLengthInBytes);
	} else {
		debug_out("[System.err] ", message, messageLengthInBytes);
	}
}

static ControlHandle getItemControl(DialogPtr dialog, DialogItemIndex index)
{
	ControlHandle control;
	if (::GetDialogItemAsControl(dialog, index, &control) == noErr)
		return control;
	else
		return NULL;
}

enum {
	kUserNameIndex = 3,
	kPasswordIndex,
	kAuthenticationDialog = 128
};

static Boolean java_authenticate(JMSessionRef session, const char *url, const char *realm, char userName[255], char password[255])
{
	Boolean result = false;
	if (thePluginRefnum != -1) {
		// ensure resources come from the plugin (yuck!).
		short oldRefnum = ::CurResFile();
		::UseResFile(thePluginRefnum);
		
		DialogRecord storage;
		DialogPtr dialog = ::GetNewDialog(kAuthenticationDialog, &storage, WindowPtr(-1));
		if (dialog != NULL) {
			// set up default buttons.
			::SetDialogDefaultItem(dialog, kStdOkItemIndex);
			::SetDialogCancelItem(dialog, kStdCancelItemIndex);
			::SetDialogTracksCursor(dialog, true);

			// set up default keyboard focus.
			ControlHandle userNameControl = getItemControl(dialog, kUserNameIndex);
			if (userNameControl != NULL)
				::SetKeyboardFocus(dialog, userNameControl, kControlFocusNextPart);
			
			::ShowWindow(dialog);

			DialogItemIndex itemHit = 0;
			do {
				::ModalDialog(ModalFilterUPP(NULL), &itemHit);
			} while (itemHit != 1 && itemHit != 2);
			
			if (itemHit == 1) {
				getItemText(dialog, kUserNameIndex, kControlEditTextTextTag, userName);
				getItemText(dialog, kPasswordIndex, kControlEditTextPasswordTag, password);
				result = true;
			}
			
			::CloseDialog(dialog);
			::UseResFile(oldRefnum);
		}
	}
	return result;
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

		JMTextRef nameRef = NULL;
		JMTextRef valueRef = NULL;
		OSStatus status = noErr;

		mStatus = ::JMOpenSession(&mSession, eJManager2Defaults, eCheckRemoteCode,
											&callbacks, kTextEncodingMacRoman, NULL);

		// capture the main environment, so it can be distinguished from true Java threads.
		if (mStatus == noErr) {
			mMainEnv = ::JMGetCurrentEnv(mSession);
		
			// create a monitor for the message queue to unblock Java threads.
			mMessageMonitor = new MRJMonitor(this);

#ifdef MRJPLUGIN_4X
            // hook up MRJ networking layer, to permit SSL, etc.
            ::OpenMRJNetworking(this);
#endif
		}
	} else {
		mStatus = kJMVersionError;
	}
}

MRJSession::~MRJSession()
{
#ifdef MRJPLUGIN_4X
    // is this perhaps too late?
    ::CloseMRJNetworking(this);
#endif

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
