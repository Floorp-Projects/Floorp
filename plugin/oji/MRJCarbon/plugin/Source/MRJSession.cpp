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
	MRJSession.cpp

	Encapsulates a session with the MacOS Runtime for Java.
	
	by Patrick C. Beard.
 */

#include "MRJSession.h"
#include "MRJPlugin.h"
#include "MRJContext.h"
#include "MRJConsole.h"
#include "MRJMonitor.h"

#include <ControlDefinitions.h>
#include <string.h>
#include <Memory.h>
#include <Files.h>
#include <Dialogs.h>
#include <Appearance.h>
#include <Resources.h>
#include <Gestalt.h>
#include <Folders.h>
#include <Script.h>

#ifdef TARGET_CARBON
#include <JavaControl.h>
#endif

#include <string>

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
			myErr = SetEOF(refNum, 0);
			
			fileCreated = true;
			sprintf((char *)holder, "MRJ Console Output.\r");
			
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
		
		DialogPtr dialog = ::GetNewDialog(kAuthenticationDialog, NULL, WindowPtr(-1));
		if (dialog != NULL) {
			// set up default buttons.
			::SetDialogDefaultItem(dialog, kStdOkItemIndex);
			::SetDialogCancelItem(dialog, kStdCancelItemIndex);
			::SetDialogTracksCursor(dialog, true);

			// set up default keyboard focus.
			ControlHandle userNameControl = getItemControl(dialog, kUserNameIndex);
			if (userNameControl != NULL)
				::SetKeyboardFocus(::GetDialogWindow(dialog), userNameControl, kControlFocusNextPart);
			
			::ShowWindow(::GetDialogWindow(dialog));

			DialogItemIndex itemHit = 0;
			do {
				::ModalDialog(ModalFilterUPP(NULL), &itemHit);
			} while (itemHit != 1 && itemHit != 2);
			
			if (itemHit == 1) {
				getItemText(dialog, kUserNameIndex, kControlEditTextTextTag, userName);
				getItemText(dialog, kPasswordIndex, kControlEditTextPasswordTag, password);
				result = true;
			}
			
			::DisposeDialog(dialog);
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
	:	mStatus(noErr), mMainEnv(NULL), mJavaVM(NULL),
	    mFirst(NULL), mLast(NULL), mMessageMonitor(NULL), mLockCount(0)
{
}

MRJSession::~MRJSession()
{
    close();
}

OSStatus MRJSession::open()
{
#if TARGET_CARBON
    // Use vanilla JNI invocation API to fire up a fresh JVM.

    std::string classPath = getClassPath();
    JavaVMOption theOptions[] = {
    	{ (char*) classPath.c_str() }
    };

    JavaVMInitArgs theInitArgs = {
    	JNI_VERSION_1_2,
    	sizeof(theOptions) / sizeof(JavaVMOption),
    	theOptions,
    	JNI_TRUE
    };

    mStatus = ::JNI_CreateJavaVM(&mJavaVM, (void**) &mMainEnv, &theInitArgs);
    
    if (mStatus == noErr) {
       	// create a monitor for the message queue to unblock Java threads.
		mMessageMonitor = new MRJMonitor(this);
    }
#else
	// Make sure JManager exists.
	if (&::JMGetVersion != NULL && ::JMGetVersion() >= kJMVersion) {
		static JMSessionCallbacks callbacks = {
			kJMVersion,					/* should be set to kJMVersion */
			&java_stdout,				/* JM will route "stdout" to this function. */
			&java_stderr,				/* JM will route "stderr" to this function. */
			&java_stdin,				/* read from console - can be nil for default behavior (no console IO) */
			&java_exit,					/* handle System.exit(int) requests */
			&java_authenticate,		    /* present basic authentication dialog */
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
		}
	} else {
		mStatus = kJMVersionError;
	}
#endif /* !TARGET_CARBON */

    return mStatus;
}

OSStatus MRJSession::close()
{
    if (mJavaVM) {
    	if (mMessageMonitor != NULL) {
    		mMessageMonitor->notifyAll();
    		delete mMessageMonitor;
    		mMessageMonitor;
    	}

        mJavaVM->DestroyJavaVM();
        mJavaVM = NULL;
    }
    return noErr;
}

JNIEnv* MRJSession::getCurrentEnv()
{
	JNIEnv* env;
	if (mJavaVM->GetEnv((void**)&env, JNI_VERSION_1_2) == JNI_OK)
		return env;
    return NULL;
}

JNIEnv* MRJSession::getMainEnv()
{
	return mMainEnv;
}

JavaVM* MRJSession::getJavaVM()
{
    return mJavaVM;
}

Boolean MRJSession::onMainThread()
{
	return (getCurrentEnv() == mMainEnv);
}

inline StringPtr c2p(const char* cstr, StringPtr pstr)
{
	pstr[0] = (unsigned char)strlen(cstr);
	::BlockMoveData(cstr, pstr + 1, pstr[0]);
	return pstr;
}

Boolean MRJSession::addToClassPath(const FSSpec& fileSpec)
{
    // if the Java VM has started already, it's too late to do this (for now).
    if (mJavaVM)
        return false;

    // keep accumulating paths.
    FSRef ref;
    OSStatus status = FSpMakeFSRef(&fileSpec, &ref);
    if (status == noErr)
        mClassPath.push_back(ref);

    return true;
}

Boolean MRJSession::addToClassPath(const char* dirPath)
{
	// Need to convert the path into an FSSpec, and add it MRJ's class path.
	Str255 path;
	FSSpec pathSpec;
	OSStatus status = ::FSMakeFSSpec(0, 0, c2p(dirPath, path), &pathSpec);
	if (status == noErr)
		return addToClassPath(pathSpec);
	return false;
}

Boolean MRJSession::addURLToClassPath(const char* fileURL)
{
    Boolean success = false;
    // Use CFURL, FSRef and FSSpec?
    CFURLRef fileURLRef = CFURLCreateWithBytes(NULL, (UInt8*)fileURL, strlen(fileURL),
                                               kCFStringEncodingUTF8, NULL);
    if (fileURLRef) {
        FSRef fsRef;
        if (CFURLGetFSRef(fileURLRef, &fsRef)) {
            mClassPath.push_back(fsRef);
            success = true;
        }
        CFRelease(fileURLRef);
    }
    return success;
}

char* MRJSession::getProperty(const char* propertyName)
{
	char* result = NULL;
#if !TARGET_CARBON
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
#endif
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

#if !TARGET_CARBON
	// Guard against entering the VM multiple times.
	if (mLockCount == 0) {
		lock();
		mStatus = ::JMIdle(mSession, milliseconds);
		unlock();
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

static OSStatus ref2path(const FSRef& ref, char* path, UInt32 maxPathSize)
{
    return FSRefMakePath(&ref, (UInt8*)path, maxPathSize);
}

std::string MRJSession::getClassPath()
{
    std::string classPath("-Djava.class.path=");
    
    // keep appending paths make from FSSpecs.
    MRJClassPath::const_iterator i = mClassPath.begin();
    if (i != mClassPath.end()) {
        char path[1024];
        if (ref2path(*i, path, sizeof(path)) == noErr)
            classPath += path;
        ++i;
        while (i != mClassPath.end()) {
            if (ref2path(*i, path, sizeof(path)) == noErr) {
                classPath += ":";
                classPath += path;
            }    
            ++i;
        }
    }
    
    return classPath;
}
