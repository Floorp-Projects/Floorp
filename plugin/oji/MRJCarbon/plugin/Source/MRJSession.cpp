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

//
//	This function allocates a block of CFM glue code which contains the instructions to call CFM routines
//
static UInt32 CFM_glue[6] = {0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420};

static void* NewMachOFunctionPointer(void* cfmfp)
{
    UInt32	*mfp = (UInt32*) NewPtr(sizeof(CFM_glue));		//	Must later dispose of allocated memory
    if (mfp) {
	    BlockMoveData(CFM_glue, mfp, sizeof(CFM_glue));
	    mfp[0] |= ((UInt32)cfmfp >> 16);
	    mfp[1] |= ((UInt32)cfmfp & 0xFFFF);
	    MakeDataExecutable(mfp, sizeof(CFM_glue));
	}
	return mfp;
}

static jint JNICALL java_vfprintf(FILE *fp, const char *format, va_list args)
{
    DebugStr("\pjava_vfprintf here.");
    return 0;
}

MRJSession::MRJSession()
	:	mStatus(noErr), mMainEnv(NULL), mJavaVM(NULL), mSession(NULL),
	    mFirst(NULL), mLast(NULL), mMessageMonitor(NULL), mLockCount(0)
{
}

MRJSession::~MRJSession()
{
    close();
}

OSStatus MRJSession::open(const char* consolePath)
{
    // Use vanilla JNI invocation API to fire up a fresh JVM.

    std::string classPath = getClassPath();
    JavaVMOption theOptions[] = {
    	{ (char*) classPath.c_str() },
    	{ "vfprintf", NewMachOFunctionPointer(&java_vfprintf) }
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
    
    JNIEnv* env = mMainEnv;
    jclass session = env->FindClass("netscape.oji.MRJSession");
    if (session) {
        mSession = (jclass) env->NewGlobalRef(session);
        jmethodID openMethod = env->GetStaticMethodID(session, "open", "(Ljava/lang/String;)V");
        if (openMethod) {
            jstring path = env->NewStringUTF(consolePath);
            if (path) {
                env->CallStaticVoidMethod(session, openMethod, path);
                env->DeleteLocalRef(path);
            }
        }
        env->DeleteLocalRef(session);
    }

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
    	
    	if (mSession) {
    	    jclass session = mSession;
    	    JNIEnv* env = mMainEnv;
            jmethodID closeMethod = env->GetStaticMethodID(session, "close", "()V");
            if (closeMethod)
                env->CallStaticVoidMethod(session, closeMethod);
    	    env->DeleteGlobalRef(mSession);
    	    mSession = NULL;
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
