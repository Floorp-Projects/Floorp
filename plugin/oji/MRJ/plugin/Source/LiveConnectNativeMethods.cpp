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
	LiveConnectNativeMethods.cpp
 */

#include "LiveConnectNativeMethods.h"

#include "nsIServiceManager.h"
#include "nsIPluginManager.h"
#include "nsIJVMManager.h"
#include "nsILiveconnect.h"
#include "nsICapsManager.h"
#include "nsISecurityContext.h"
#include "nsIPluginInstancePeer2.h"

#include "MRJPlugin.h"
#include "MRJContext.h"
#include "MRJSession.h"
#include "CSecureEnv.h"
#include "JavaMessageQueue.h"
#include "MRJMonitor.h"
#include "NativeMonitor.h"
#include "RunnableMixin.h"

#include "netscape_javascript_JSObject.h"   /* javah-generated headers */

extern nsIPluginManager* thePluginManager;
extern nsIServiceManager* theServiceManager;	// needs to be in badaptor.cpp.

static MRJPlugin* theJVMPlugin = NULL;
static nsILiveconnect* theLiveConnectManager = NULL;

static jclass netscape_javascript_JSObject = NULL;
static jmethodID netscape_javascript_JSObject_JSObject;
static jfieldID netscape_javascript_JSObject_internal;

static jclass netscape_oji_JNIUtils = NULL;
static jmethodID netscape_oji_JNIUtils_NewLocalRef = NULL;
static jmethodID netscape_oji_JNIUtils_GetCurrentThread = NULL;
static jmethodID netscape_oji_JNIUtils_GetCurrentClassLoader = NULL;
static jmethodID netscape_oji_JNIUtils_GetObjectClassLoader = NULL;

nsresult InitLiveConnectSupport(MRJPlugin* jvmPlugin)
{
	theJVMPlugin = jvmPlugin;

	NS_DEFINE_IID(kLiveConnectCID, NS_CLIVECONNECT_CID);
	NS_DEFINE_IID(kILiveConnectIID, NS_ILIVECONNECT_IID);
	nsresult result = theServiceManager->GetService(kLiveConnectCID, kILiveConnectIID,
													(nsISupports**)&theLiveConnectManager);
	if (result != NS_OK)
		return result;
	
    // Manually load the required native methods.
    static JNINativeMethod nativeMethods[] = {
        "getMember", "(Ljava/lang/String;)Ljava/lang/Object;", (void*)&Java_netscape_javascript_JSObject_getMember,
        "getSlot", "(I)Ljava/lang/Object;", (void*)&Java_netscape_javascript_JSObject_getSlot,
        "setMember", "(Ljava/lang/String;Ljava/lang/Object;)V", (void*)&Java_netscape_javascript_JSObject_setMember,
        "setSlot", "(ILjava/lang/Object;)V", (void*)&Java_netscape_javascript_JSObject_setSlot,
        "removeMember", "(Ljava/lang/String;)V", (void*)&Java_netscape_javascript_JSObject_removeMember,
        "call", "(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;", (void*)&Java_netscape_javascript_JSObject_call,
        "eval", "(Ljava/lang/String;)Ljava/lang/Object;", (void*)&Java_netscape_javascript_JSObject_eval,
        "toString", "()Ljava/lang/String;", (void*)&Java_netscape_javascript_JSObject_toString,
        "getWindow", "(Ljava/applet/Applet;)Lnetscape/javascript/JSObject;", (void*)&Java_netscape_javascript_JSObject_getWindow,
        "finalize", "()V", (void*)&Java_netscape_javascript_JSObject_finalize,
    };
    
    JNIEnv* env = NULL;
    nsrefcnt count = jvmPlugin->GetJNIEnv(&env);
    if (count > 0 && env != NULL) {
    	jclass classJSObject = env->FindClass("netscape/javascript/JSObject");
    	if (classJSObject != NULL) {
    		// register LiveConnect native methods.
    		netscape_javascript_JSObject = (jclass) env->NewGlobalRef(classJSObject);
    		env->DeleteLocalRef(classJSObject);
    		
    		netscape_javascript_JSObject_JSObject = env->GetMethodID(netscape_javascript_JSObject, "<init>", "(I)V");
    		netscape_javascript_JSObject_internal = env->GetFieldID(netscape_javascript_JSObject, "internal", "I");

		    env->RegisterNatives(netscape_javascript_JSObject, nativeMethods, sizeof(nativeMethods) / sizeof(JNINativeMethod));
		    if (env->ExceptionOccurred()) {
				env->ExceptionClear();
				result = NS_ERROR_FAILURE;
		    }
	    }
	    
	    // load netscape.oji.JNIUtils class.
		jclass classJNIUtils = env->FindClass("netscape/oji/JNIUtils");
		if (classJNIUtils != NULL) {
		    netscape_oji_JNIUtils = (jclass) env->NewGlobalRef(classJNIUtils);
    		env->DeleteLocalRef(classJNIUtils);
		    netscape_oji_JNIUtils_NewLocalRef = env->GetStaticMethodID(netscape_oji_JNIUtils, "NewLocalRef", "(Ljava/lang/Object;)Ljava/lang/Object;");
		    netscape_oji_JNIUtils_GetCurrentThread = env->GetStaticMethodID(netscape_oji_JNIUtils, "GetCurrentThread", "()Ljava/lang/Object;");
		    netscape_oji_JNIUtils_GetCurrentClassLoader = env->GetStaticMethodID(netscape_oji_JNIUtils, "GetCurrentClassLoader", "()Ljava/lang/Object;");
		    netscape_oji_JNIUtils_GetObjectClassLoader = env->GetStaticMethodID(netscape_oji_JNIUtils, "GetObjectClassLoader", "(Ljava/lang/Object;)Ljava/lang/Object;");
	    }
	    
	    jvmPlugin->ReleaseJNIEnv(env);
    }
    
    return result;
}

nsresult ShutdownLiveConnectSupport()
{
	
	if (theLiveConnectManager != NULL) {
		theLiveConnectManager->Release();
		theLiveConnectManager = NULL;
	}
	
	if (theJVMPlugin != NULL) {
		theJVMPlugin = NULL;
	}
	
	return NS_OK;
}

jobject Wrap_JSObject(JNIEnv* env, jsobject js_obj)
{
    jmethodID constructorID = env->GetMethodID(netscape_javascript_JSObject, "<init>", "(I)V");
	return env->NewObject(netscape_javascript_JSObject, constructorID, js_obj);
}

jsobject Unwrap_JSObject(JNIEnv* env, jobject java_wrapper_obj)
{
	return env->GetIntField(java_wrapper_obj, netscape_javascript_JSObject_internal);
}

static jobject NewLocalRef(JNIEnv* env, jobject global_ref)
{
	return env->CallStaticObjectMethod(netscape_oji_JNIUtils, netscape_oji_JNIUtils_NewLocalRef, global_ref);
}

static jobject ToGlobalRef(JNIEnv* env, jobject localRef)
{
	jobject globalRef = env->NewGlobalRef(localRef);
	env->DeleteLocalRef(localRef);
	return globalRef;
}

static jobject ToLocalRef(JNIEnv* env, jobject globalRef)
{
	jobject localRef = NewLocalRef(env, globalRef);
	env->DeleteGlobalRef(globalRef);
	return localRef;
}

static jobject GetCurrentThread(JNIEnv* env)
{
	return env->CallStaticObjectMethod(netscape_oji_JNIUtils, netscape_oji_JNIUtils_GetCurrentThread);
}

/**
 * Security Considerations.
 */

class MRJSecurityContext : public nsISecurityContext {
public:
	MRJSecurityContext();

	NS_DECL_ISUPPORTS
    
	NS_IMETHOD Implies(const char* target, const char* action, PRBool *bAllowedAccess);
};

MRJSecurityContext::MRJSecurityContext()
{
	NS_INIT_REFCNT();
}

// work around a bug in Metrowerks pre-processor.
static NS_DEFINE_IID(kISecurityContextIID, NS_ISECURITYCONTEXT_IID);
NS_IMPL_ISUPPORTS(MRJSecurityContext, kISecurityContextIID)

NS_METHOD MRJSecurityContext::Implies(const char* target, const char* action, PRBool *bAllowedAccess)
{
	*bAllowedAccess = (target != NULL && action == NULL);
	return NS_OK;
}

static nsISecurityContext* newSecurityContext()
{
	MRJSecurityContext* context = new MRJSecurityContext();
	NS_IF_ADDREF(context);
	return context;
}

static jobject GetCurrentClassLoader(JNIEnv* env)
{
	return env->CallStaticObjectMethod(netscape_oji_JNIUtils, netscape_oji_JNIUtils_GetCurrentClassLoader);
}

static jobject GetObjectClassLoader(JNIEnv* env, jobject object)
{
	return env->CallStaticObjectMethod(netscape_oji_JNIUtils, netscape_oji_JNIUtils_GetObjectClassLoader, object);
}

/**
 * Maps the given JNIEnv to a given plugin instance by comparing the current class loader
 * with the class loader of the applet of each plugin instance. This could be made
 * faster, but it's probably good enough. Note:  the reference count of the plugin
 * instance isn't affected by this call.
 */
static MRJPluginInstance* GetCurrentInstance(JNIEnv* env)
{
	MRJPluginInstance* pluginInstance = NULL;
	jobject classLoader = GetCurrentClassLoader(env);
	if (classLoader != NULL) {
		pluginInstance = MRJPluginInstance::getInstances();
		while (pluginInstance != NULL) {
			jobject applet;
			pluginInstance->GetJavaObject(&applet);
			jobject appletClassLoader = GetObjectClassLoader(env, applet);
			jboolean sameClassLoader = env->IsSameObject(appletClassLoader, classLoader);
			env->DeleteLocalRef(appletClassLoader);
			if (sameClassLoader)
				break;
			pluginInstance = pluginInstance->getNextInstance();
		}
		env->DeleteLocalRef(classLoader);
	}
	return pluginInstance;
}

/**
 * Wraps a JavaMessage in an nsIRunnable form, so that it runs on the correct native thread.
 */
class MessageRunnable : public JavaMessage, public RunnableMixin {
public:
	MessageRunnable(PRUint32 threadID, JavaMessage* msg);
	
	virtual void execute(JNIEnv* env);
	
	NS_IMETHOD Run();

private:
	PRUint32 mThreadID;
	JavaMessage* mMessage;
};

MessageRunnable::MessageRunnable(PRUint32 threadID, JavaMessage* msg)
	: mThreadID(threadID), mMessage(msg)
{
}

void MessageRunnable::execute(JNIEnv* env)
{
	// because a spontaneous Java thread called us, we have to switch to the JavaScript thread
	// to handle this request.
	nsIThreadManager* threadManager = NULL;
	if (theServiceManager->GetService(nsIJVMManager::GetCID(), nsIThreadManager::GetIID(), (nsISupports**)&threadManager) == NS_OK) {
		threadManager->PostEvent(mThreadID, this, PR_FALSE);
		theServiceManager->ReleaseService(nsIJVMManager::GetCID(), threadManager);
	}
}

NS_IMETHODIMP MessageRunnable::Run()
{
	nsIJVMManager* javaManager = NULL;
	if (theServiceManager->GetService(nsIJVMManager::GetCID(), nsIJVMManager::GetIID(), (nsISupports**)&javaManager) == NS_OK) {
		JNIEnv* proxyEnv = NULL;
		if (javaManager->GetProxyJNI(&proxyEnv) == NS_OK && proxyEnv != NULL)
			mMessage->execute(proxyEnv);
		theServiceManager->ReleaseService(nsIJVMManager::GetCID(), javaManager);
	}
	return NS_OK;
}

static PRUint32 getJavaScriptThread(JNIEnv* env)
{
	PRUint32 threadID = 0;
	MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
	if (pluginInstance != NULL) {
		nsIPluginInstancePeer* peer;
		if (pluginInstance->GetPeer(&peer) == NS_OK) {
			nsIPluginInstancePeer2* peer2 = NULL;
			if (peer->QueryInterface(nsIPluginInstancePeer2::GetIID(), &peer2) == NS_OK) {
				if (peer2->GetJSThread(&threadID) != NS_OK)
					threadID = 0;
				NS_RELEASE(peer2);
			}
			NS_RELEASE(peer);
		}
	}
	return threadID;
}

/**
 * Sends a message by rendezvousing with an existing JavaScript thread, or creates a new one
 * if none exists for this thread already.
 */
static void sendMessage(JNIEnv* env, JavaMessage* msg)
{
	// the main thread gets its own secure env, so it won't contend with other threads. this
	// is needed to handle finalization, which seems to get called from the main thread sometimes.
	if (env == theJVMPlugin->getSession()->getMainEnv()) {
		static CSecureEnv* mainEnv = NULL;
		if (mainEnv == NULL) {
			mainEnv = new CSecureEnv(theJVMPlugin, NULL, env);
			mainEnv->AddRef();
		}
		mainEnv->setJavaEnv(env);
		mainEnv->sendMessageFromJava(env, msg, true);
		return;
	}
	
	// If this is a call back into JavaScript from Java, there will be a secureEnv associated with this thread.
	jobject thread = GetCurrentThread(env);
	CSecureEnv* secureEnv = GetSecureJNI(env, thread);
	env->DeleteLocalRef(thread);
	if (secureEnv != NULL) {
		secureEnv->sendMessageFromJava(env, msg);
	} else {
		// spontaneous call in from Java. this communicates with a shared server thread. this is *VERY* slow right now.
		static MRJMonitor sharedMonitor(theJVMPlugin->getSession());
		// only 1 Java thread can use this at a time.
		sharedMonitor.enter();
		{
			static CSecureEnv* sharedEnv = NULL;
			if (sharedEnv == NULL) {
				sharedEnv = new CSecureEnv(theJVMPlugin, NULL, env);
				sharedEnv->AddRef();
			}
			sharedEnv->setJavaEnv(env);

			// In the current Seamonkey architecture, there's really only one thread that JavaScript
			// can execute in. We take advantage of that fact here. When we have a more multithreaded
			// system, this will have to be revisited.
			static PRUint32 theJavaScriptThread = getJavaScriptThread(env);
			
			// if the JavaScript thread is known, wrap the message in a MessageRunnable to handle
			// the message in the JavaScript thread.
			if (theJavaScriptThread != 0) {
				MessageRunnable* runnableMsg = new MessageRunnable(theJavaScriptThread, msg);
				NS_ADDREF(runnableMsg);
				sharedEnv->sendMessageFromJava(env, runnableMsg);
				NS_IF_RELEASE(runnableMsg);
			}
		}
		sharedMonitor.exit();
	}
}

static nsIPrincipal* newCodebasePrincipal(const char* codebaseURL)
{
	nsIPrincipal* principal = NULL;
	nsICapsManager* capsManager = NULL;
	static NS_DEFINE_IID(kICapsManagerIID, NS_ICAPSMANAGER_IID);
	if (thePluginManager->QueryInterface(kICapsManagerIID, &capsManager) == NS_OK) {
		if (capsManager->CreateCodebasePrincipal(codebaseURL, &principal) != NS_OK)
			principal = NULL;
		capsManager->Release();
	}
	return principal;
}

/****************** Implementation of methods of JSObject *******************/

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getMember
 * Signature: (Ljava/lang/String;)Ljava/lang/Object;
 */

class GetMemberMessage : public JavaMessage {
	jsobject mObject;
	const jchar* mPropertyName;
	jsize mLength;
	jobject* mResultObject;
public:
	GetMemberMessage(jsobject js_obj, const jchar* propertyName, jsize nameLength, jobject* member)
		:	mObject(js_obj), mPropertyName(propertyName), mLength(nameLength), mResultObject(member)
	{
	}

	virtual void execute(JNIEnv* env)
	{
		jobject member;
		nsresult result = theLiveConnectManager->GetMember(env, mObject, mPropertyName, mLength, NULL, 0, NULL, &member);
		if (result == NS_OK) {
			// convert reference to a global reference, in case we're switching threads.
			*mResultObject = ToGlobalRef(env, member);
		}
	}
};

JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_getMember(JNIEnv* env,
                                            jobject java_wrapper_obj,
                                            jstring property_name_jstr)
{
	if (property_name_jstr == NULL) {
		env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal null member name");
		return NULL;
	}

	/* Get the Unicode string for the JS property name */
	jboolean is_copy;
	const jchar* property_name_ucs2 = env->GetStringChars(property_name_jstr, &is_copy);
	jsize property_name_len = env->GetStringLength(property_name_jstr);

	jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
	jobject member = NULL;
	GetMemberMessage msg(js_obj, property_name_ucs2, property_name_len, &member);
	
	sendMessage(env, &msg);

	// convert the resulting reference back to a local reference.
	if (member != NULL)
		member = ToLocalRef(env, member);
	
	env->ReleaseStringChars(property_name_jstr, property_name_ucs2);

    return member;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getSlot
 * Signature: (I)Ljava/lang/Object;
 */

class GetSlotMessage : public JavaMessage {
	jsobject mObject;
	jint mSlot;
	jobject* mResultObject;
public:
	GetSlotMessage(jsobject js_obj, jint slot, jobject* member)
		:	mObject(js_obj), mSlot(slot), mResultObject(member)
	{
	}

	virtual void execute(JNIEnv* env)
	{
		jobject member;
		nsresult result = theLiveConnectManager->GetSlot(env, mObject, mSlot, NULL, 0, NULL, &member);
		if (result == NS_OK) {
			// convert reference to a global reference, in case we're switching threads.
			*mResultObject = ToGlobalRef(env, member);
		}
	}
};

JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_getSlot(JNIEnv* env,
                                          jobject java_wrapper_obj,
                                          jint slot)
{
	jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
	jobject member = NULL;
	GetSlotMessage msg(js_obj, slot, &member);
	sendMessage(env, &msg);
	// convert the resulting reference back to a local reference.
	if (member != NULL)
		member = ToLocalRef(env, member);
    return member;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    setMember
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)V
 */

class SetMemberMessage : public JavaMessage {
	jsobject mObject;
	const jchar* mPropertyName;
	jsize mLength;
	jobject mJavaObject;
public:
	SetMemberMessage(jsobject js_obj, const jchar* propertyName, jsize nameLength, jobject java_obj)
		:	mObject(js_obj), mPropertyName(propertyName), mLength(nameLength), mJavaObject(java_obj)
	{
	}

	virtual void execute(JNIEnv* env)
	{
		nsresult result = theLiveConnectManager->SetMember(env, mObject, mPropertyName, mLength, NULL, 0, NULL, mJavaObject);
	}
};

JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_setMember(JNIEnv* env,
                                            jobject java_wrapper_obj,
                                            jstring property_name_jstr,
                                            jobject java_obj)
{
	if (property_name_jstr == NULL) {
		env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal null member name");
		return;
	}

	/* Get the Unicode string for the JS property name */
	jboolean is_copy;
	const jchar* property_name_ucs2 = env->GetStringChars(property_name_jstr, &is_copy);
	jsize property_name_len = env->GetStringLength(property_name_jstr);
	
	jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
	java_obj = ToGlobalRef(env, java_obj);
	SetMemberMessage msg(js_obj, property_name_ucs2, property_name_len, java_obj);
	
	sendMessage(env, &msg);
	
	env->DeleteGlobalRef(java_obj);
	env->ReleaseStringChars(property_name_jstr, property_name_ucs2);
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    setSlot
 * Signature: (ILjava/lang/Object;)V
 */

class SetSlotMessage : public JavaMessage {
	jsobject mObject;
	jint mSlot;
	jobject mJavaObject;
public:
	SetSlotMessage(jsobject js_obj, jint slot, jobject java_obj)
		:	mObject(js_obj), mSlot(slot), mJavaObject(java_obj)
	{
	}

	virtual void execute(JNIEnv* env)
	{
		nsresult result = theLiveConnectManager->SetSlot(env, mObject, mSlot, NULL, 0, NULL, mJavaObject);
	}
};

JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_setSlot(JNIEnv* env,
                                          jobject java_wrapper_obj,
                                          jint slot,
                                          jobject java_obj)
{
	jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
	java_obj = ToGlobalRef(env, java_obj);
	SetSlotMessage msg(js_obj, slot, java_obj);
	sendMessage(env, &msg);
	env->DeleteGlobalRef(java_obj);
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    removeMember
 * Signature: (Ljava/lang/String;)V
 */

class RemoveMemberMessage : public JavaMessage {
	jsobject mObject;
	const jchar* mPropertyName;
	jsize mLength;
public:
	RemoveMemberMessage(jsobject obj, const jchar* propertyName, jsize length)
		:	mObject(obj), mPropertyName(propertyName), mLength(length)
	{
	}

	virtual void execute(JNIEnv* env)
	{
		nsresult result = theLiveConnectManager->RemoveMember(env, mObject, mPropertyName, mLength, NULL, 0, NULL);
	}
};

JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_removeMember(JNIEnv* env,
                                               jobject java_wrapper_obj,
                                               jstring property_name_jstr)
{
	if (property_name_jstr == NULL) {
		env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal null member name");
		return;
	}

	/* Get the Unicode string for the JS property name */
	jboolean is_copy;
	const jchar* property_name_ucs2 = env->GetStringChars(property_name_jstr, &is_copy);
	jsize property_name_len = env->GetStringLength(property_name_jstr);

	jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
	RemoveMemberMessage msg(js_obj, property_name_ucs2, property_name_len);
	
	sendMessage(env, &msg);
	
	env->ReleaseStringChars(property_name_jstr, property_name_ucs2);
}

class CallMessage : public JavaMessage {
	jsobject mObject;
	const jchar* mFunctionName;
	jsize mLength;
	jobjectArray mJavaArgs;
	jobject* mJavaResult;
	const char* mCodebaseURL;
public:
	CallMessage(jsobject obj, const jchar* functionName, jsize length, jobjectArray javaArgs, jobject* javaResult, const char* codebaseURL)
		:	mObject(obj), mFunctionName(functionName), mLength(length),
			mJavaArgs(javaArgs), mJavaResult(javaResult), mCodebaseURL(codebaseURL)
	{
	}

	virtual void execute(JNIEnv* env)
	{
		/* If we have an applet, try to create a codebase principle. */
		nsIPrincipal* principal = NULL;
		nsISecurityContext* context = NULL;
		if (mCodebaseURL != NULL) {
			principal = newCodebasePrincipal(mCodebaseURL);
			context = newSecurityContext();
		}
		jobject jresult = NULL;
		nsresult result = theLiveConnectManager->Call(env, mObject, mFunctionName, mLength, mJavaArgs, &principal, (principal != NULL ? 1 : 0), context, &jresult);
		NS_IF_RELEASE(principal);
		NS_IF_RELEASE(context);
		if (result == NS_OK)
			*mJavaResult = ToGlobalRef(env, jresult);
	}
};

/*
 * Class:     netscape_javascript_JSObject
 * Method:    call
 * Signature: (Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_call(JNIEnv* env, jobject java_wrapper_obj,
                                       jstring function_name_jstr, jobjectArray java_args)
{
	if (function_name_jstr == NULL) {
		env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal null function name");
		return NULL;
	}

	/* Try to determine which plugin instance is responsible for this thread. This is done by checking class loaders. */
	const char* codebaseURL = NULL;
	MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
	if (pluginInstance != NULL)
		codebaseURL = pluginInstance->getContext()->getDocumentBase();

	/* Get the Unicode string for the JS function name */
	jboolean is_copy;
	const jchar* function_name_ucs2 = env->GetStringChars(function_name_jstr, &is_copy);
	jsize function_name_len = env->GetStringLength(function_name_jstr);

	jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
	jobject jresult = NULL;

	CallMessage msg(js_obj, function_name_ucs2, function_name_len, java_args, &jresult, codebaseURL);

	sendMessage(env, &msg);

	env->ReleaseStringChars(function_name_jstr, function_name_ucs2);

	if (jresult != NULL)
		jresult = ToLocalRef(env, jresult);

	return jresult;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    eval
 * Signature: (Ljava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_eval(JNIEnv* env,
                                       jobject java_wrapper_obj,
                                       jstring script_jstr)
{
	/* Get the Unicode string for the JS function name */
	if (script_jstr == NULL) {
		env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal null script string");
		return NULL;
	}
	jboolean is_copy;
	const jchar* script_ucs2 = env->GetStringChars(script_jstr, &is_copy);
	jsize script_len = env->GetStringLength(script_jstr);

	/* unwrap the JS object from the Java object. */
	jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
	jobject jresult = NULL;
	
#ifdef MRJPLUGIN_4X
	nsresult status = theLiveConnectManager->Eval(env, js_obj, script_ucs2, script_len, NULL, 0, NULL, &jresult);
#else
	class EvalMessage : public JavaMessage {
		JNIEnv* mEnv;
		jsobject mObject;
		const jchar* mScript;
		jsize mLength;
		jobject* mJavaResult;
	public:
		EvalMessage(jsobject obj, const jchar* script, jsize length, jobject* javaResult)
			:	mObject(obj), mScript(script), mLength(length), mJavaResult(javaResult)
		{
		}

		virtual void execute(JNIEnv* env)
		{
			jobject jresult = NULL;
			nsresult result = theLiveConnectManager->Eval(env, mObject, mScript, mLength, NULL, 0, NULL, &jresult);
			if (result == NS_OK && jresult != NULL) {
				*mJavaResult = ToGlobalRef(env, jresult);
			}
		}
	};

	/* determine the plugin instance so we can obtain its codebase. */
	// beard: should file a bug with Apple that JMJNIToAWTContext doesn't work.
	// MRJPluginInstance* pluginInstance = theJVMPlugin->getPluginInstance(env);
	MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
	if (pluginInstance == NULL) {
		env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
		return NULL;
	}

	EvalMessage msg(js_obj, script_ucs2, script_len, &jresult);
	sendMessage(env, &msg);
	
	if (jresult != NULL)
		jresult = ToLocalRef(env, jresult);
	
#endif

	env->ReleaseStringChars(script_jstr, script_ucs2);

	return jresult;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    toString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_netscape_javascript_JSObject_toString(JNIEnv* env, jobject java_wrapper_obj)
{
	/* unwrap the JS object from the Java object. */
	jstring jresult = NULL;
	jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);

	class ToStringMessage : public JavaMessage {
		jsobject mObject;
		jstring* mStringResult;
	public:
		ToStringMessage(jsobject js_obj, jstring* stringResult)
			:	mObject(js_obj), mStringResult(stringResult)
		{
		}

		virtual void execute(JNIEnv* env)
		{
			nsresult status = theLiveConnectManager->ToString(env, mObject, mStringResult);
		}
	} msg(js_obj, &jresult);
	
	sendMessage(env, &msg);
	
	return jresult;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getWindow
 * Signature: (Ljava/applet/Applet;)Lnetscape/javascript/JSObject;
 */

JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_getWindow(JNIEnv* env,
                                            jclass js_object_class,
                                            jobject java_applet_obj)
{
	MRJPluginInstance* pluginInstance = theJVMPlugin->getPluginInstance(java_applet_obj);
	if (pluginInstance != NULL) {
#ifdef MRJPLUGIN_4X
		// keep an extra reference to the plugin instance, until it is finalized.
		jobject jwindow = Wrap_JSObject(env, jsobject(pluginInstance));
		return jwindow;
#else
		class GetWindowMessage : public JavaMessage {
			nsIPluginInstance* mPluginInstance;
			jsobject* mWindowResult;
		public:
			GetWindowMessage(nsIPluginInstance* pluginInstance, jsobject* windowResult)
				:	mPluginInstance(pluginInstance), mWindowResult(windowResult)
			{
			}

			virtual void execute(JNIEnv* env)
			{
				nsresult status = theLiveConnectManager->GetWindow(env,mPluginInstance, NULL, 0, NULL, mWindowResult);
			}
		};

		jsobject jswindow = NULL;
		GetWindowMessage msg(pluginInstance, &jswindow);
		sendMessage(env, &msg);
		pluginInstance->Release();
		if (jswindow != NULL)
			return Wrap_JSObject(env, jswindow);
#endif
	}
	return NULL;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    finalize
 * Signature: ()V
 */

JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_finalize(JNIEnv* env, jobject java_wrapper_obj)
{
	jsobject jsobj = Unwrap_JSObject(env, java_wrapper_obj);

#ifdef MRJPLUGIN_4X
	MRJPluginInstance* pluginInstance = (MRJPluginInstance*)jsobj;
	NS_IF_RELEASE(pluginInstance);
#else
	class FinalizeMessage : public JavaMessage {
		jsobject m_jsobj;
	public:
		FinalizeMessage(jsobject jsobj)
			:	m_jsobj(jsobj)
		{
		}

		virtual void execute(JNIEnv* env)
		{
			nsresult result = theLiveConnectManager->FinalizeJSObject(env, m_jsobj);
		}
	};
	
	FinalizeMessage msg(jsobj);
	sendMessage(env, &msg);
#endif
}
