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

#include "MRJPlugin.h"
#include "MRJContext.h"
#include "MRJSession.h"
#include "CSecureEnv.h"
#include "JavaMessageQueue.h"
#include "MRJMonitor.h"
#include "NativeMonitor.h"

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

static MRJPluginInstance* GetCurrentPlugin(JNIEnv* env)
{
	MRJPluginInstance* pluginInstance = NULL;
	jobject classLoader = GetCurrentClassLoader(env);
	if (classLoader != NULL) {
		pluginInstance = MRJPluginInstance::getInstances();
		while (pluginInstance != NULL) {
			jobject applet;
			pluginInstance->GetJavaObject(&applet);
			jobject appletClassLoader = GetObjectClassLoader(env, applet);
			if (env->IsSameObject(appletClassLoader, classLoader))
				break;
			pluginInstance = pluginInstance->getNextInstance();
		}
	}
	return pluginInstance;
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
			sharedEnv->sendMessageFromJava(env, msg);
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

	jobject member = NULL;
	nsresult result = theLiveConnectManager->GetMember(env, Unwrap_JSObject(env, java_wrapper_obj), property_name_ucs2, property_name_len,
														NULL, 0, NULL, &member);
	if (result != NS_OK) member = NULL;
	
	env->ReleaseStringChars(property_name_jstr, property_name_ucs2);

    return member;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getSlot
 * Signature: (I)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_getSlot(JNIEnv* env,
                                          jobject java_wrapper_obj,
                                          jint slot)
{
	jobject member = NULL;
	nsresult result = theLiveConnectManager->GetSlot(env, Unwrap_JSObject(env, java_wrapper_obj), slot, NULL, 0, NULL, &member);
    return member;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    setMember
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)V
 */
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

	jobject member = NULL;
	nsresult result = theLiveConnectManager->SetMember(env, Unwrap_JSObject(env, java_wrapper_obj), property_name_ucs2, property_name_len, NULL, 0, NULL, java_obj);
	if (result != NS_OK) member = NULL;
	
	env->ReleaseStringChars(property_name_jstr, property_name_ucs2);
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    setSlot
 * Signature: (ILjava/lang/Object;)V
 */
JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_setSlot(JNIEnv* env,
                                          jobject java_wrapper_obj,
                                          jint slot,
                                          jobject java_obj)
{
	nsresult result = theLiveConnectManager->SetSlot(env, Unwrap_JSObject(env, java_wrapper_obj), slot, NULL, 0, NULL, java_obj);
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
	RemoveMemberMessage(jsobject obj, const jchar* propertyName, jsize length);

	virtual void execute(JNIEnv* env);
};

RemoveMemberMessage::RemoveMemberMessage(jsobject obj, const jchar* propertyName, jsize length)
	:	mObject(obj), mPropertyName(propertyName), mLength(length)
{
}

void RemoveMemberMessage::execute(JNIEnv* env)
{
	nsresult result = theLiveConnectManager->RemoveMember(env, mObject, mPropertyName, mLength, NULL, 0, NULL);
}

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
	CallMessage(jsobject obj, const jchar* functionName, jsize length, jobjectArray java_args, jobject* javaResult, const char* codebaseURL);

	virtual void execute(JNIEnv* env);
};

CallMessage::CallMessage(jsobject obj, const jchar* functionName, jsize length, jobjectArray javaArgs, jobject* javaResult, const char* codebaseURL)
	:	mObject(obj), mFunctionName(functionName), mLength(length),
		mJavaArgs(javaArgs), mJavaResult(javaResult), mCodebaseURL(codebaseURL)
{
}

void CallMessage::execute(JNIEnv* env)
{
	/* If we have an applet, try to create a codebase principle. */
	nsIPrincipal* principal = NULL;
	nsISecurityContext* context = NULL;
	if (mCodebaseURL != NULL) {
		principal = newCodebasePrincipal(mCodebaseURL);
		context = newSecurityContext();
	}
	nsresult status = theLiveConnectManager->Call(env, mObject, mFunctionName, mLength, mJavaArgs, &principal, (principal != NULL ? 1 : 0), context, mJavaResult);
	NS_IF_RELEASE(principal);
	NS_IF_RELEASE(context);
}

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
	MRJPluginInstance* pluginInstance = GetCurrentPlugin(env);
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

	return jresult;
}

class EvalMessage : public JavaMessage {
	JNIEnv* mEnv;
	jsobject mObject;
	const jchar* mScript;
	jsize mLength;
	jobject* mJavaResult;
public:
	EvalMessage(jsobject obj, const jchar* script, jsize length, jobject* javaResult);

	virtual void execute(JNIEnv* env);
};

EvalMessage::EvalMessage(jsobject obj, const jchar* script, jsize length, jobject* javaResult)
	:	mObject(obj), mScript(script), mLength(length), mJavaResult(javaResult)
{
}

void EvalMessage::execute(JNIEnv* env)
{
	nsresult status = theLiveConnectManager->Eval(env, mObject, mScript, mLength, NULL, 0, NULL, mJavaResult);
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
	/* determine the plugin instance so we can obtain its codebase. */
	MRJPluginInstance* pluginInstance = theJVMPlugin->getPluginInstance(env);
	if (pluginInstance == NULL) {
		env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
		return NULL;
	}

	EvalMessage msg(js_obj, script_ucs2, script_len, &jresult);
	sendMessage(env, &msg);

	// Make sure it gets released!
	pluginInstance->Release();
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

class GetWindowMessage : public JavaMessage {
	nsIPluginInstance* mPluginInstance;
	jsobject* mWindowResult;
public:
	GetWindowMessage(nsIPluginInstance* pluginInstance, jsobject* windowResult);

	virtual void execute(JNIEnv* env);
};

GetWindowMessage::GetWindowMessage(nsIPluginInstance* pluginInstance, jsobject* windowResult)
	:	mPluginInstance(pluginInstance), mWindowResult(windowResult)
{
}

void GetWindowMessage::execute(JNIEnv* env)
{
	nsresult status = theLiveConnectManager->GetWindow(env,mPluginInstance, NULL, 0, NULL, mWindowResult);
}

JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_getWindow(JNIEnv* env,
                                            jclass js_object_class,
                                            jobject java_applet_obj)
{
	MRJPluginInstance* pluginInstance = theJVMPlugin->getPluginInstance(java_applet_obj);
	if (pluginInstance != NULL) {
#ifdef MRJPLUGIN_4X
		jobject jwindow = Wrap_JSObject(env, jsobject(pluginInstance));
		pluginInstance->Release();
		return jwindow;
#else
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
}
