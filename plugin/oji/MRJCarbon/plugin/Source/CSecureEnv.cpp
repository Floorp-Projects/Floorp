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
	CSecureEnv.cpp

	Rewritten for use with MRJ plugin by Patrick C. Beard. Just forwards all
	calls through the underlying JNIEnv that this wraps. Eventually, it will
	communicate with the underlying threads using a message queue.
 */	

#include "CSecureEnv.h"
#include "nsISecurityContext.h"

#include "MRJPlugin.h"
#include "MRJSession.h"
#include "nsIThreadManager.h"
#include "nsIJVMManager.h"

#include "MRJMonitor.h"
#include "NativeMonitor.h"
#include "JavaMessageQueue.h"

#if 0
static NS_DEFINE_IID(kISecureEnvIID, NS_ISECUREENV_IID);
static NS_DEFINE_IID(kIRunnableIID, NS_IRUNNABLE_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
#endif

JavaMessageQueue::JavaMessageQueue(Monitor* monitor)
	:	mFirst(NULL), mLast(NULL), mMonitor(monitor)
{
}

void JavaMessageQueue::putMessage(JavaMessage* msg)
{
	if (mFirst == NULL) {
		mFirst = mLast = msg;
	} else {
		mLast->setNext(msg);
		mLast = msg;
	}
	msg->setNext(NULL);
}

JavaMessage* JavaMessageQueue::getMessage()
{
	JavaMessage* msg = mFirst;
	if (msg != NULL) {
		mFirst = mFirst->getNext();
		if (mFirst == NULL) mLast = NULL;
	}
	return msg;
}

void JavaMessageQueue::enter()
{
	mMonitor->enter();
}

void JavaMessageQueue::exit()
{
	mMonitor->exit();
}

void JavaMessageQueue::wait()
{
	mMonitor->wait();
}

void JavaMessageQueue::wait(long long millis)
{
	mMonitor->wait(millis);
}

void JavaMessageQueue::notify()
{
	mMonitor->notify();
}

/**
 * Native run method that communicates with LiveConnect from a Java thread.
 */
static void netscape_oji_JNIThread_run(JNIEnv* env, jobject self)
{
	CSecureEnv* secureEnv = NULL;
	jmethodID yieldMethod = NULL;
	jmethodID sleepMethod = NULL;
	
	jclass clazz = env->GetObjectClass(self);
	if (clazz != NULL) {
		// the field fSecureEnv contains a pointer to a CSecureEnv.
		jfieldID fSecureEnvField = env->GetFieldID(clazz, "fSecureEnv", "I");
		if (fSecureEnvField != NULL) {
			secureEnv = (CSecureEnv*) env->GetIntField(self, fSecureEnvField);
		}
		yieldMethod = env->GetStaticMethodID(clazz, "yield", "()V");
		sleepMethod = env->GetStaticMethodID(clazz, "sleep", "(J)V");
	}
	
	// notify the secure JNI that we are here, and wait for messages to arrive.
	if (secureEnv != NULL) {
		jboolean isRunning = true;
		MRJSession* session = secureEnv->getSession();
		MRJMonitor requestMonitor(session, self);
		MRJMonitor replyMonitor(session);
		// NativeMonitor replyMonitor(secureEnv->getThreadManager());
		JavaMessageQueue requests(&requestMonitor), replies(&replyMonitor);
		secureEnv->initialize(env, &isRunning, &requests, &replies);
		
		// when this thread is running, no other thread can enter the request queue monitor.
		requests.enter();
		
		while (isRunning) {
			// the protocol for now is dead simple:  get a message from the
			// requests message queue, process it, and then put it back in
			// the replies queue. This will get more elaborate to handle
			// upcall requests. 
			JavaMessage* msg = requests.getMessage();
			if (msg != NULL) {
				msg->execute(env);
				replies.putMessage(msg);
				replies.notify();
			} else {
				// should we do sleep, timed wait, or what?
				// env->CallStaticVoidMethod(clazz, yieldMethod);
				// env->CallStaticVoidMethod(clazz, sleepMethod, jlong(kDefaultJMTime));
				requests.wait();
			}
		}
		
		requests.exit();
	}
}

/**
 * Called from browser side, starts the Java thread that calls from LiveConnect to Java
 * are processed in.
 */
static void CreateJNIThread(CSecureEnv* secureEnv)
{
	nsIThreadManager* manager = secureEnv->getThreadManager();
	MRJSession* session = secureEnv->getSession();
	JNIEnv* env = session->getCurrentEnv();

	jclass java_lang_System = env->FindClass("java.lang.System");
	if (java_lang_System)
	    DebugStr("\pjava_lang_System OK");
    else
	    DebugStr("\pjava_lang_System FAILED");
	    
	jclass JNIThreadClass = env->FindClass("netscape.oji.JNIThread");
	if (JNIThreadClass != NULL) {
	    DebugStr("\pCreateJNIThread: found JNIThreadClass.");
		JNINativeMethod method = { "run", "()V", &netscape_oji_JNIThread_run };
		env->RegisterNatives(JNIThreadClass, &method, 1);
		jmethodID constructorID = env->GetMethodID(JNIThreadClass, "<init>", "(I)V");
		if (constructorID != NULL) {
			jobject javaThread = env->NewObject(JNIThreadClass, constructorID, secureEnv);
			for (;;) {
				// give time to Java, to allow the thread to come up.
				session->idle();
				// has the thread made contact?
				if (secureEnv->isInitialized())
					break;
				// give time to NSPR, to avoid hanging too long.
				manager->Sleep();
			}
		}
	} else {
	    DebugStr("\pCreateJNIThread: failed.");
	}
}

/**
 * Creates a new native thread in MRJ's main thread, to avoid deadlock problems.
 */
class CreateNativeThreadMessage : public NativeMessage {
	nsresult* mResult;
	PRUint32* mThreadID;
	CSecureEnv* mSecureEnv;
public:
	CreateNativeThreadMessage(nsresult* outResult, PRUint32* outThreadID, CSecureEnv* secureEnv)
		:	mResult(outResult), mThreadID(outThreadID), mSecureEnv(secureEnv)
	{
	}

	virtual void execute()
	{
		nsIThreadManager* manager = mSecureEnv->getThreadManager();
		*mResult = manager->CreateThread(mThreadID, mSecureEnv);
	}
};

/**
 * Called from a Java thread that wants to communicate with the browser.
 */
static void CreateNativeThread(CSecureEnv* secureEnv)
{
	nsresult result;
	PRUint32 threadID;
	MRJSession* session = secureEnv->getSession();
	
	// cause a native thread to be created on our behalf. Perhaps this should be a message we send to the
	// session itself. otherwise we could have reentrancy problems.
	CreateNativeThreadMessage message(&result, &threadID, secureEnv);
	session->sendMessage(&message);

	if (session->onMainThread()) {
		// give time to other native threads, so the new thread can come up.
		nsIThreadManager* manager = secureEnv->getThreadManager();
		while (!secureEnv->isInitialized()) {
			manager->Sleep();
		}
	} else {
		// sleep the current Java thread until we rendezvous with the Native thread.
		JNIEnv* env = session->getCurrentEnv();
		jclass threadClass = env->FindClass("java/lang/Thread");
		if (threadClass != NULL) {
			jmethodID sleepMethod = env->GetStaticMethodID(threadClass, "sleep", "(J)V");
			if (sleepMethod != NULL) {
				while (!secureEnv->isInitialized())
					env->CallStaticVoidMethod(threadClass, sleepMethod, jlong(1024));
			}
			env->DeleteLocalRef(threadClass);
		}
	}
}

/**
 * Runs the server thread for LiveConnect upcalls from spontaneous Java threads.
 */
NS_IMETHODIMP CSecureEnv::Run()
{
	jboolean isRunning = true;
	NativeMonitor requestMonitor(mSession, mThreadManager);
	MRJMonitor replyMonitor(mSession);
	JavaMessageQueue requests(&requestMonitor), replies(&replyMonitor);
	// initialize(env, self, &isRunning, &requests, &replies);
	
	// we have to create the Proxy JNI here, so it associated with this thread.
	nsIJVMManager* manager = mPlugin->getManager();
	manager->CreateProxyJNI(this, &mProxyEnv);
	
	mIsRunning = &isRunning;
	mNativeQueue = &requests;
	mJavaQueue = &replies;
	
	// when this thread is running, no other thread can enter the request queue monitor.
	requests.enter();
	
	while (isRunning) {
		// the protocol for now is dead simple:  get a message from the
		// requests message queue, process it, and then put it back in
		// the replies queue. This will get more elaborate to handle
		// upcall requests. 
		JavaMessage* msg = requests.getMessage();
		if (msg != NULL) {
			msg->execute(mProxyEnv);
			replies.putMessage(msg);
			replies.notify();
		} else {
			// should we do sleep, timed wait, or what?
			// env->CallStaticVoidMethod(clazz, yieldMethod);
			// env->CallStaticVoidMethod(clazz, sleepMethod, jlong(kDefaultJMTime));
			requests.wait();
		}
	}
	
	requests.exit();

	return NS_OK;
}

/**
 * Used to send a message from Native to Java threads.
 */
void CSecureEnv::sendMessageToJava(JavaMessage* msg)
{
	messageLoop(mProxyEnv, msg, mJavaQueue, mNativeQueue, true);
}

/**
 * Used to send a message from Java to Native threads.
 */
void CSecureEnv::sendMessageFromJava(JNIEnv* javaEnv, JavaMessage* msg, Boolean busyWaiting)
{
	messageLoop(javaEnv, msg, mNativeQueue, mJavaQueue, busyWaiting);
}

////////////////////////////////////////////////////////////////////////////
// from nsISupports and AggregatedQueryInterface:

// Thes macro expands to the aggregated query interface scheme.

#if 0
NS_IMPL_AGGREGATED(CSecureEnv);

NS_METHOD
CSecureEnv::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aIID.Equals(kISupportsIID)) {
      *aInstancePtr = GetInner();
      AddRef();
      return NS_OK;
    }
    if (aIID.Equals(kISecureEnvIID)) {
        *aInstancePtr = (nsISecureEnv*) this;
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIRunnableIID)) {
        *aInstancePtr = (nsIRunnable*) this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
#endif

const InterfaceInfo CSecureEnv::sInterfaces[] = {
	{ NS_ISECUREENV_IID, INTERFACE_OFFSET(CSecureEnv, nsISecureEnv) },
	{ NS_IRUNNABLE_IID, INTERFACE_OFFSET(CSecureEnv, nsIRunnable) },
};
const UInt32 CSecureEnv::kInterfaceCount = sizeof(sInterfaces) / sizeof(InterfaceInfo);

///=--------------------------------------------------------------------------=
// CSecureEnv::CSecureEnv
///=--------------------------------------------------------------------------=
// Implements the CSecureJNI object for creating object, invoking method, 
// getting/setting field in JNI with security context.
//
// parameters :
//
// return :
// 
// notes :
//
CSecureEnv::CSecureEnv(MRJPlugin* plugin, JNIEnv* proxyEnv, JNIEnv* javaEnv)
	:	SupportsMixin(this, sInterfaces, kInterfaceCount),
		mPlugin(plugin), mProxyEnv(proxyEnv), mJavaEnv(javaEnv),
		mSession(plugin->getSession()), mThreadManager(plugin->getThreadManager()),
		mIsRunning(NULL), mJavaQueue(NULL), mNativeQueue(NULL)
{
	// need to create the JNIThread for communicating with Java.
	if (mJavaEnv != NULL)
        CreateNativeThread(this);
	else
	    CreateJNIThread(this);
}


///=--------------------------------------------------------------------------=
// CSecureEnv::~CSecureEnv
///=--------------------------------------------------------------------------=
// Implements the CSecureEnv object for creating object, invoking method, 
// getting/setting field in JNI with security context.
//
// parameters :
//
// return :
// 
// notes :
//
CSecureEnv::~CSecureEnv()  
{
	// Tell the Java thread to die.
	if (mIsRunning != NULL) {
		*mIsRunning = false;
		mJavaQueue->notify();
	}
}

void CSecureEnv::initialize(JNIEnv* javaEnv, jboolean* isRunning, JavaMessageQueue* javaQueue, JavaMessageQueue* nativeQueue)
{
	mJavaEnv = javaEnv;
	mIsRunning = isRunning;
	mJavaQueue = javaQueue;
	mNativeQueue = nativeQueue;
}

///=--------------------------------------------------------------------------=
// CSecureEnv::Create
///=--------------------------------------------------------------------------=
// Create the CSecureEnv object for creating object, invoking method, 
// getting/setting field in JNI with security context.
//
// parameters :
//
// return :
// 
// notes :
//
NS_METHOD
CSecureEnv::Create(MRJPlugin* plugin, JNIEnv* proxyEnv, const nsIID& aIID, void* *aInstancePtr)
{
	CSecureEnv* secureEnv = new CSecureEnv(plugin, proxyEnv);
	if (secureEnv == NULL)
	    return NS_ERROR_OUT_OF_MEMORY;
	NS_ADDREF(secureEnv);
	nsresult rv = secureEnv->QueryInterface(aIID, aInstancePtr);
	NS_RELEASE(secureEnv);
	return rv;
}

////////////////////////////////////////////////////////////////////////////
// from nsISecureJNI:
//


///=--------------------------------------------------------------------------=
// CSecureEnv::NewObject
///=--------------------------------------------------------------------------=
// Create new Java object in LiveConnect.
//
// @param env        -- JNIEnv pointer.
// @param clazz      -- Java Class object.
// @param methodID   -- Method id
// @param args       -- arguments for invoking the constructor.
// @param result     -- return new Java object.
// @param ctx        -- security context 
//

class NewObjectMessage : public JavaMessage {
	jclass clazz;
	jmethodID methodID;
	jvalue* args;
	jobject* result;

public:
	NewObjectMessage(jclass clazz,  jmethodID methodID, jvalue *args, jobject* result)
	{
		this->clazz = clazz;
		this->methodID = methodID;
		this->args = args;
		this->result = result;
	}
	
	virtual void execute(JNIEnv* env)
	{
		*result = env->NewObjectA(clazz, methodID, args);
	}
};

NS_IMETHODIMP CSecureEnv::NewObject(/*[in]*/  jclass clazz, 
                                     /*[in]*/  jmethodID methodID, 
                                     /*[in]*/  jvalue *args, 
                                     /*[out]*/ jobject* result,
                                     /*[in]*/  nsISecurityContext* ctx)
{
    if (clazz == NULL || methodID == NULL)
        return NS_ERROR_NULL_POINTER;

    // Call method on Java side
    NewObjectMessage msg(clazz, methodID, args, result);
    sendMessageToJava(&msg);
	// *result = m_env->NewObjectA(clazz, methodID, args);
	
	return NS_OK;
}


///=--------------------------------------------------------------------------=
// CSecureEnv::CallMethod
///=--------------------------------------------------------------------------=
// Invoke method on Java object in LiveConnect.
//
// @param type       -- Return type
// @param obj        -- Java object.
// @param methodID   -- Method id
// @param result     -- return result of invocation.
// @param ctx        -- security context 
//

class CallMethodMessage : public JavaMessage {
	jni_type type;
	jobject obj;
	jmethodID methodID;
	jvalue* args;
	jvalue* result;

public:
	CallMethodMessage(jni_type type, jobject obj, jmethodID methodID, jvalue *args, jvalue* result)
	{
		this->type = type;
		this->obj = obj;
		this->methodID = methodID;
		this->args = args;
		this->result = result;
	}
	
	virtual void execute(JNIEnv* env)
	{
		switch (type) {
		case jobject_type:
			result->l = env->CallObjectMethodA(obj, methodID, args);
			break;
		case jboolean_type:
			result->z = env->CallBooleanMethodA(obj, methodID, args);
			break;
		case jbyte_type:
			result->b = env->CallByteMethodA(obj, methodID, args);
			break;
		case jchar_type:
			result->c = env->CallCharMethodA(obj, methodID, args);
			break;
		case jshort_type:
			result->s = env->CallShortMethodA(obj, methodID, args);
			break;
		case jint_type:
			result->i = env->CallIntMethodA(obj, methodID, args);
			break;
		case jlong_type:
			result->j = env->CallLongMethodA(obj, methodID, args);
			break;
		case jfloat_type:
			result->f = env->CallFloatMethodA(obj, methodID, args);
			break;
		case jdouble_type:
			result->d = env->CallDoubleMethodA(obj, methodID, args);
			break;
		case jvoid_type:
			env->CallVoidMethodA(obj, methodID, args);
			break;
		}
	}
};

NS_IMETHODIMP CSecureEnv::CallMethod(/*[in]*/  jni_type type,
                                      /*[in]*/  jobject obj, 
                                      /*[in]*/  jmethodID methodID, 
                                      /*[in]*/  jvalue *args, 
                                      /*[out]*/ jvalue* result,
                                      /*[in]*/  nsISecurityContext* ctx)
{
    if (obj == NULL || methodID == NULL)
        return NS_ERROR_NULL_POINTER;

	// Call method on Java side
	// return CallJavaMethod(obj, method, args, ctx, result);
	CallMethodMessage msg(type, obj, methodID, args, result);
	sendMessageToJava(&msg);
	
	return NS_OK;
}


///=--------------------------------------------------------------------------=
// CSecureEnv::CallNonvirtualMethod
///=--------------------------------------------------------------------------=
// Invoke non-virtual method on Java object in LiveConnect.
//
// @param obj        -- Java object.
// @param methodID   -- Method id
// @param args       -- arguments for invoking the constructor.
// @param result     -- return result of invocation.
// @param ctx        -- security context 
//

class CallNonvirtualMethodMessage : public JavaMessage {
	jni_type type;
	jobject obj;
	jclass clazz;
	jmethodID methodID;
	jvalue* args;
	jvalue* result;

public:
	CallNonvirtualMethodMessage(jni_type type, jobject obj, jclass clazz, jmethodID methodID, jvalue *args, jvalue* result)
	{
		this->type = type;
		this->obj = obj;
		this->clazz = clazz;
		this->methodID = methodID;
		this->args = args;
		this->result = result;
	}
	
	virtual void execute(JNIEnv* env)
	{
		switch (type) {
		case jobject_type:
			result->l = env->CallNonvirtualObjectMethodA(obj, clazz, methodID, args);
			break;
		case jboolean_type:
			result->z = env->CallNonvirtualBooleanMethodA(obj, clazz, methodID, args);
			break;
		case jbyte_type:
			result->b = env->CallNonvirtualByteMethodA(obj, clazz, methodID, args);
			break;
		case jchar_type:
			result->c = env->CallNonvirtualCharMethodA(obj, clazz, methodID, args);
			break;
		case jshort_type:
			result->s = env->CallNonvirtualShortMethodA(obj, clazz, methodID, args);
			break;
		case jint_type:
			result->i = env->CallNonvirtualIntMethodA(obj, clazz, methodID, args);
			break;
		case jlong_type:
			result->j = env->CallNonvirtualLongMethodA(obj, clazz, methodID, args);
			break;
		case jfloat_type:
			result->f = env->CallNonvirtualFloatMethodA(obj, clazz, methodID, args);
			break;
		case jdouble_type:
			result->d = env->CallNonvirtualDoubleMethodA(obj, clazz, methodID, args);
			break;
		case jvoid_type:
			env->CallNonvirtualVoidMethodA(obj, clazz, methodID, args);
			break;
		}
	}
};

NS_IMETHODIMP CSecureEnv::CallNonvirtualMethod(/*[in]*/  jni_type type,
                                                /*[in]*/  jobject obj, 
                                                /*[in]*/  jclass clazz,
                                                /*[in]*/  jmethodID methodID,
                                                /*[in]*/  jvalue *args, 
                                                /*[out]*/ jvalue* result,
                                                /*[in]*/  nsISecurityContext* ctx)
{
    if (obj == NULL || clazz == NULL || methodID == NULL)
        return NS_ERROR_NULL_POINTER;

	// Call non-virtual method on Java side
	// return CallJavaMethod(obj, method, args, ctx, result);
	CallNonvirtualMethodMessage msg(type, obj, clazz, methodID, args, result);
	sendMessageToJava(&msg);
	
	return NS_OK;
}


///=--------------------------------------------------------------------------=
// CSecureEnv::GetField
///=--------------------------------------------------------------------------=
// Get a field on Java object in LiveConnect.
//
// @param obj        -- Java object.
// @param fieldID    -- field id
// @param result     -- return field value
// @param ctx        -- security context 
//

class GetFieldMessage : public JavaMessage {
	jni_type type;
	jobject obj;
	jfieldID fieldID;
	jvalue* args;
	jvalue* result;

public:
	GetFieldMessage(jni_type type, jobject obj, jfieldID fieldID, jvalue *args, jvalue* result)
	{
		this->type = type;
		this->obj = obj;
		this->fieldID = fieldID;
		this->args = args;
		this->result = result;
	}
	
	virtual void execute(JNIEnv* env)
	{
		switch (type) {
		case jobject_type:
			result->l = env->GetObjectField(obj, fieldID);
			break;
		case jboolean_type:
			result->z = env->GetBooleanField(obj, fieldID);
			break;
		case jbyte_type:
			result->b = env->GetByteField(obj, fieldID);
			break;
		case jchar_type:
			result->c = env->GetCharField(obj, fieldID);
			break;
		case jshort_type:
			result->s = env->GetShortField(obj, fieldID);
			break;
		case jint_type:
			result->i = env->GetIntField(obj, fieldID);
			break;
		case jlong_type:
			result->j = env->GetLongField(obj, fieldID);
			break;
		case jfloat_type:
			result->f = env->GetFloatField(obj, fieldID);
			break;
		case jdouble_type:
			result->d = env->GetDoubleField(obj, fieldID);
			break;
		}
	}
};

NS_IMETHODIMP CSecureEnv::GetField(/*[in]*/  jni_type type,
                                    /*[in]*/  jobject obj, 
                                    /*[in]*/  jfieldID fieldID,
                                    /*[out]*/ jvalue* result,
                                    /*[in]*/  nsISecurityContext* ctx)
{
    if (mJavaEnv == NULL || obj == NULL || fieldID == NULL)
        return NS_ERROR_NULL_POINTER;

    // Get field on Java side
    // return GetJavaField(obj, field, ctx, result);
    
	JNIEnv* env = mJavaEnv;
	switch (type) {
	case jobject_type:
		result->l = env->GetObjectField(obj, fieldID);
		break;
	case jboolean_type:
		result->z = env->GetBooleanField(obj, fieldID);
		break;
	case jbyte_type:
		result->b = env->GetByteField(obj, fieldID);
		break;
	case jchar_type:
		result->c = env->GetCharField(obj, fieldID);
		break;
	case jshort_type:
		result->s = env->GetShortField(obj, fieldID);
		break;
	case jint_type:
		result->i = env->GetIntField(obj, fieldID);
		break;
	case jlong_type:
		result->j = env->GetLongField(obj, fieldID);
		break;
	case jfloat_type:
		result->f = env->GetFloatField(obj, fieldID);
		break;
	case jdouble_type:
		result->d = env->GetDoubleField(obj, fieldID);
		break;
	}
	
	return NS_OK;
}


///=--------------------------------------------------------------------------=
// CSecureEnv::SetField
///=--------------------------------------------------------------------------=
//
// Set a field on Java object in LiveConnect.
//
// @param obj        -- Java object.
// @param fieldID    -- field id
// @param result     -- field value to set
// @param ctx        -- security context 
//
NS_IMETHODIMP CSecureEnv::SetField(/*[in]*/ jni_type type,
                                    /*[in]*/ jobject obj, 
                                    /*[in]*/ jfieldID fieldID,
                                    /*[in]*/ jvalue val,
                                    /*[in]*/ nsISecurityContext* ctx)
{
    if (mJavaEnv == NULL || obj == NULL || fieldID == NULL)
        return NS_ERROR_NULL_POINTER;

    // Set field on Java side
    // return SetJavaField(obj, field, val, ctx);

	JNIEnv* env = mJavaEnv;
	switch (type) {
	case jobject_type:
		env->SetObjectField(obj, fieldID, val.l);
		break;
	case jboolean_type:
		env->SetBooleanField(obj, fieldID, val.z);
		break;
	case jbyte_type:
		env->SetByteField(obj, fieldID, val.b);
		break;
	case jchar_type:
		env->SetCharField(obj, fieldID, val.c);
		break;
	case jshort_type:
		env->SetShortField(obj, fieldID, val.s);
		break;
	case jint_type:
		env->SetIntField(obj, fieldID, val.i);
		break;
	case jlong_type:
		env->SetLongField(obj, fieldID, val.j);
		break;
	case jfloat_type:
		env->SetFloatField(obj, fieldID, val.f);
		break;
	case jdouble_type:
		env->SetDoubleField(obj, fieldID, val.d);
		break;
	}
	
	return NS_OK;
}


///=--------------------------------------------------------------------------=
// CSecureEnv::CallStaticMethod
///=--------------------------------------------------------------------------=
//
// Invoke static method on Java object in LiveConnect.
//
// @param obj        -- Java object.
// @param methodID   -- method id
// @param args       -- arguments for invoking the constructor.
// @param result     -- return result of invocation.
// @param ctx        -- security context 
//

class CallStaticMethodMessage : public JavaMessage {
	jni_type type;
	jclass clazz;
	jmethodID methodID;
	jvalue* args;
	jvalue* result;

public:
	CallStaticMethodMessage(jni_type type, jclass clazz, jmethodID methodID, jvalue *args, jvalue* result)
	{
		this->type = type;
		this->clazz = clazz;
		this->methodID = methodID;
		this->args = args;
		this->result = result;
	}
	
	virtual void execute(JNIEnv* env)
	{
		switch (type) {
		case jobject_type:
			result->l = env->CallStaticObjectMethodA(clazz, methodID, args);
			break;
		case jboolean_type:
			result->z = env->CallStaticBooleanMethodA(clazz, methodID, args);
			break;
		case jbyte_type:
			result->b = env->CallStaticByteMethodA(clazz, methodID, args);
			break;
		case jchar_type:
			result->c = env->CallStaticCharMethodA(clazz, methodID, args);
			break;
		case jshort_type:
			result->s = env->CallStaticShortMethodA(clazz, methodID, args);
			break;
		case jint_type:
			result->i = env->CallStaticIntMethodA(clazz, methodID, args);
			break;
		case jlong_type:
			result->j = env->CallStaticLongMethodA(clazz, methodID, args);
			break;
		case jfloat_type:
			result->f = env->CallStaticFloatMethodA(clazz, methodID, args);
			break;
		case jdouble_type:
			result->d = env->CallStaticDoubleMethodA(clazz, methodID, args);
			break;
		case jvoid_type:
			env->CallStaticVoidMethodA(clazz, methodID, args);
			break;
		}
	}
};

NS_IMETHODIMP CSecureEnv::CallStaticMethod(/*[in]*/  jni_type type,
                                            /*[in]*/  jclass clazz,
                                            /*[in]*/  jmethodID methodID,
                                            /*[in]*/  jvalue *args, 
                                            /*[out]*/ jvalue* result,
                                            /*[in]*/  nsISecurityContext* ctx)
{
    if (clazz == NULL || methodID == NULL)
        return NS_ERROR_NULL_POINTER;

	// Call method on Java side
	// return CallJavaMethod(NULL, method, args, ctx, result);
	CallStaticMethodMessage msg(type, clazz, methodID, args, result);
	sendMessageToJava(&msg);

	return NS_OK;
}


///=--------------------------------------------------------------------------=
// CSecureEnv::GetStaticField
///=--------------------------------------------------------------------------=
// Get a static field on Java object in LiveConnect.
//
// @param obj        -- Java object.
// @param fieldID    -- field id
// @param result     -- return field value
// @param ctx        -- security context 
//


class GetStaticFieldMessage : public JavaMessage {
	jni_type type;
	jclass clazz;
	jfieldID fieldID;
	jvalue* args;
	jvalue* result;

public:
	GetStaticFieldMessage(jni_type type, jclass clazz, jfieldID fieldID, jvalue* result)
	{
		this->type = type;
		this->clazz = clazz;
		this->fieldID = fieldID;
		this->args = args;
		this->result = result;
	}
	
	virtual void execute(JNIEnv* env)
	{
		switch (type) {
		case jobject_type:
			result->l = env->GetStaticObjectField(clazz, fieldID);
			break;
		case jboolean_type:
			result->z = env->GetStaticBooleanField(clazz, fieldID);
			break;
		case jbyte_type:
			result->b = env->GetStaticByteField(clazz, fieldID);
			break;
		case jchar_type:
			result->c = env->GetStaticCharField(clazz, fieldID);
			break;
		case jshort_type:
			result->s = env->GetStaticShortField(clazz, fieldID);
			break;
		case jint_type:
			result->i = env->GetStaticIntField(clazz, fieldID);
			break;
		case jlong_type:
			result->j = env->GetStaticLongField(clazz, fieldID);
			break;
		case jfloat_type:
			result->f = env->GetStaticFloatField(clazz, fieldID);
			break;
		case jdouble_type:
			result->d = env->GetStaticDoubleField(clazz, fieldID);
			break;
		}
	}
};

NS_IMETHODIMP CSecureEnv::GetStaticField(/*[in]*/  jni_type type,
                                          /*[in]*/  jclass clazz, 
                                          /*[in]*/  jfieldID fieldID,
                                          /*[out]*/ jvalue* result,
                                          /*[in]*/  nsISecurityContext* ctx)
{
    if (mJavaEnv == NULL || clazz == NULL || fieldID == NULL)
        return NS_ERROR_NULL_POINTER;

    // Get static field on Java side
	// return GetJavaField(NULL, field, ctx, result);
	// GetStaticFieldMessage msg(type, clazz, fieldID, result);
	// sendMessageToJava(&msg);

	// should be able to perform in Java env.
	JNIEnv* env = mJavaEnv;
	switch (type) {
	case jobject_type:
		result->l = env->GetStaticObjectField(clazz, fieldID);
		break;
	case jboolean_type:
		result->z = env->GetStaticBooleanField(clazz, fieldID);
		break;
	case jbyte_type:
		result->b = env->GetStaticByteField(clazz, fieldID);
		break;
	case jchar_type:
		result->c = env->GetStaticCharField(clazz, fieldID);
		break;
	case jshort_type:
		result->s = env->GetStaticShortField(clazz, fieldID);
		break;
	case jint_type:
		result->i = env->GetStaticIntField(clazz, fieldID);
		break;
	case jlong_type:
		result->j = env->GetStaticLongField(clazz, fieldID);
		break;
	case jfloat_type:
		result->f = env->GetStaticFloatField(clazz, fieldID);
		break;
	case jdouble_type:
		result->d = env->GetStaticDoubleField(clazz, fieldID);
		break;
	}
	
	return NS_OK;
}


///=--------------------------------------------------------------------------=
// CSecureEnv::SetStaticField
///=--------------------------------------------------------------------------=
// Set a static field on Java object in LiveConnect.
//
// @param obj        -- Java object.
// @param fieldID    -- field id
// @param result     -- field value to set
// @param ctx        -- security context 
//
NS_IMETHODIMP CSecureEnv::SetStaticField(/*[in]*/  jni_type type,
                                          /*[in]*/ jclass clazz, 
                                          /*[in]*/ jfieldID fieldID,
                                          /*[in]*/ jvalue val,
                                          /*[in]*/ nsISecurityContext* ctx)
{
    if (mJavaEnv == NULL || clazz == NULL || fieldID == NULL)
        return NS_ERROR_NULL_POINTER;

	// Set static field on Java side
	// return SetJavaField(NULL, field, val, ctx);

	JNIEnv* env = mJavaEnv;
	switch (type) {
	case jobject_type:
		env->SetStaticObjectField(clazz, fieldID, val.l);
		break;
	case jboolean_type:
		env->SetStaticBooleanField(clazz, fieldID, val.z);
		break;
	case jbyte_type:
		env->SetStaticByteField(clazz, fieldID, val.b);
		break;
	case jchar_type:
		env->SetStaticCharField(clazz, fieldID, val.c);
		break;
	case jshort_type:
		env->SetStaticShortField(clazz, fieldID, val.s);
		break;
	case jint_type:
		env->SetStaticIntField(clazz, fieldID, val.i);
		break;
	case jlong_type:
		env->SetStaticLongField(clazz, fieldID, val.j);
		break;
	case jfloat_type:
		env->SetStaticFloatField(clazz, fieldID, val.f);
		break;
	case jdouble_type:
		env->SetStaticDoubleField(clazz, fieldID, val.d);
		break;
	}
	
	return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetVersion(/*[out]*/ jint* version) 
{
    if (version == NULL)
        return NS_ERROR_NULL_POINTER;
    
 	JNIEnv* env = mSession->getCurrentEnv();
	*version = env->GetVersion();

    return NS_OK;
}

/**
 * To give proper "local" refs, need to run this in the Java thread.
 */
class DefineClassMessage : public JavaMessage {
	const char* name;
	jobject loader;
	const jbyte *buf;
	jsize len;
	jclass* clazz;
public:
	DefineClassMessage(const char* name, jobject loader, const jbyte *buf, jsize len, jclass* clazz)
	{
		this->name = name;
		this->loader = loader;
		this->buf = buf;
		this->len = len;
		this->clazz = clazz;
	}
	
	virtual void execute(JNIEnv* env)
	{
    	*clazz = env->DefineClass(name, loader, buf, len);
	}
};

NS_IMETHODIMP CSecureEnv::DefineClass(/*[in]*/  const char* name,
                                       /*[in]*/  jobject loader,
                                       /*[in]*/  const jbyte *buf,
                                       /*[in]*/  jsize len,
                                       /*[out]*/ jclass* clazz) 
{
    if (clazz == NULL)
        return NS_ERROR_NULL_POINTER;
    
    DefineClassMessage msg(name, loader, buf, len, clazz);
    sendMessageToJava(&msg);

    return NS_OK;
}

/**
 * To give proper "local" refs, need to run this in the Java thread.
 */
class FindClassMessage : public JavaMessage {
	const char* name;
	jclass* result;
public:
	FindClassMessage(const char* name, jclass* result)
	{
		this->name = name;
		this->result = result;
	}

	virtual void execute(JNIEnv* env)
	{
		*result = env->FindClass(name);
	}
};

NS_IMETHODIMP CSecureEnv::FindClass(/*[in]*/  const char* name, 
                                     /*[out]*/ jclass* clazz) 
{
    if (clazz == NULL)
        return NS_ERROR_NULL_POINTER;
    
	// JNIEnv* env = mSession->getCurrentEnv();
	// *clazz = env->FindClass(name);
	FindClassMessage msg(name, clazz);
	sendMessageToJava(&msg);

    return NS_OK;
}

/**
 * To give proper "local" refs, need to run this in the Java thread.
 */
class GetSuperclassMessage : public JavaMessage {
	jclass sub;
	jclass* super;
public:
	GetSuperclassMessage(jclass sub, jclass* super)
	{
		this->sub = sub;
		this->super = super;
	}

	virtual void execute(JNIEnv* env)
	{
    	*super = env->GetSuperclass(sub);
	}
};

NS_IMETHODIMP CSecureEnv::GetSuperclass(/*[in]*/  jclass sub,
                                         /*[out]*/ jclass* super) 
{
    if (mJavaEnv == NULL || super == NULL)
        return NS_ERROR_NULL_POINTER;
    
	// GetSuperclassMessage msg(sub, super);
	// sendMessageToJava(&msg);
	*super = mJavaEnv->GetSuperclass(sub);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::IsAssignableFrom(/*[in]*/  jclass sub,
                                            /*[in]*/  jclass super,
                                            /*[out]*/ jboolean* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
	// JNIEnv* env = mSession->getCurrentEnv();
	// *result = env->IsAssignableFrom(sub, super);
	*result = mJavaEnv->IsAssignableFrom(sub, super);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::Throw(/*[in]*/  jthrowable obj,
                                 /*[out]*/ jint* result) 
{
	if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
	// Huh? This doesn't really make sense.
    
    *result = mJavaEnv->Throw(obj);

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::ThrowNew(/*[in]*/  jclass clazz,
                                    /*[in]*/  const char *msg,
                                    /*[out]*/ jint* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->ThrowNew(clazz, msg);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::ExceptionOccurred(/*[out]*/ jthrowable* result)
{
	if (mJavaEnv == NULL || result == NULL)
		return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->ExceptionOccurred();

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::ExceptionDescribe(void)
{
	if (mJavaEnv == NULL)
		return NS_ERROR_NULL_POINTER;

    mJavaEnv->ExceptionDescribe();

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::ExceptionClear(void)
{
    mJavaEnv->ExceptionClear();

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::FatalError(/*[in]*/ const char* msg)
{
    mJavaEnv->FatalError(msg);

    return NS_OK;
}


/**
 * To give proper "local" refs, need to run this in the true thread.
 */
NS_IMETHODIMP CSecureEnv::NewGlobalRef(/*[in]*/  jobject localRef, 
                                        /*[out]*/ jobject* result)
{
    if (result == NULL)
        return NS_ERROR_NULL_POINTER;
    
	// *result = mJavaEnv->NewGlobalRef(localRef);
	class NewGlobalRefMessage : public JavaMessage {
		jobject localRef;
		jobject* result;
	public:
		NewGlobalRefMessage(jobject localRef, jobject* result)
		{
			this->localRef = localRef;
			this->result = result;
		}

		virtual void execute(JNIEnv* env)
		{
			*result = env->NewGlobalRef(localRef);
		}
	} msg(localRef, result);
 	sendMessageToJava(&msg);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::DeleteGlobalRef(/*[in]*/ jobject globalRef) 
{
	JNIEnv* env = mSession->getCurrentEnv();
    env->DeleteGlobalRef(globalRef);
    return NS_OK;
}

/**
 * To give proper "local" refs, need to run this in the true thread.
 */
class DeleteLocalRefMessage : public JavaMessage {
	jobject localRef;
public:
	DeleteLocalRefMessage(jobject localRef)
	{
		this->localRef = localRef;
	}

	virtual void execute(JNIEnv* env)
	{
		env->DeleteLocalRef(localRef);
	}
};

NS_IMETHODIMP CSecureEnv::DeleteLocalRef(/*[in]*/ jobject obj)
{
    mJavaEnv->DeleteLocalRef(obj);
    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::IsSameObject(/*[in]*/  jobject obj1,
                                        /*[in]*/  jobject obj2,
                                        /*[out]*/ jboolean* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->IsSameObject(obj1, obj2);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::AllocObject(/*[in]*/  jclass clazz,
                                       /*[out]*/ jobject* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->AllocObject(clazz);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetObjectClass(/*[in]*/  jobject obj,
                                          /*[out]*/ jclass* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->GetObjectClass(obj);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::IsInstanceOf(/*[in]*/  jobject obj,
                                        /*[in]*/  jclass clazz,
                                        /*[out]*/ jboolean* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->IsInstanceOf(obj, clazz);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetMethodID(/*[in]*/  jclass clazz, 
                                       /*[in]*/  const char* name,
                                       /*[in]*/  const char* sig,
                                       /*[out]*/ jmethodID* result) 
{
    if (result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    // run this on the *CURRENT* thread.
    JNIEnv* env = mSession->getCurrentEnv();
    *result = env->GetMethodID(clazz, name, sig);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetFieldID(/*[in]*/  jclass clazz, 
                                      /*[in]*/  const char* name,
                                      /*[in]*/  const char* sig,
                                      /*[out]*/ jfieldID* result) 
{
    if (result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    // run this on the *CURRENT* thread.
    JNIEnv* env = mSession->getCurrentEnv();
    *result = env->GetFieldID(clazz, name, sig);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetStaticMethodID(/*[in]*/  jclass clazz, 
                                             /*[in]*/  const char* name,
                                             /*[in]*/  const char* sig,
                                             /*[out]*/ jmethodID* result)
{
    if (result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    // run this on the *CURRENT* thread.
    JNIEnv* env = mSession->getCurrentEnv();
    *result = env->GetStaticMethodID(clazz, name, sig);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetStaticFieldID(/*[in]*/  jclass clazz, 
                                            /*[in]*/  const char* name,
                                            /*[in]*/  const char* sig,
                                            /*[out]*/ jfieldID* result) 
{
    if (result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    // run this on the *CURRENT* thread.
    JNIEnv* env = mSession->getCurrentEnv();
    *result = env->GetStaticFieldID(clazz, name, sig);

    return NS_OK;
}

/**
 * To give proper "local" refs, need to run this in the true thread.
 */
NS_IMETHODIMP CSecureEnv::NewString(/*[in]*/  const jchar* unicode,
                                     /*[in]*/  jsize len,
                                     /*[out]*/ jstring* result) 
{
    if (result == NULL)
        return NS_ERROR_NULL_POINTER;
    
	// *result = mJavaEnv->NewString(unicode, len);
	class NewStringMessage : public JavaMessage {
		const jchar* unicode;
		jsize len;
		jstring* result;
	public:
		NewStringMessage(const jchar* unicode, jsize len, jstring* result)
		{
			this->unicode = unicode;
			this->len = len;
			this->result = result;
		}

		virtual void execute(JNIEnv* env)
		{
	    	*result = env->NewString(unicode, len);
		}
	} msg(unicode, len, result);
    sendMessageToJava(&msg);

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::GetStringLength(/*[in]*/  jstring str,
                                           /*[out]*/ jsize* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->GetStringLength(str);

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::GetStringChars(/*[in]*/  jstring str,
                                          /*[in]*/  jboolean *isCopy,
                                          /*[out]*/ const jchar** result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->GetStringChars(str, isCopy);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::ReleaseStringChars(/*[in]*/  jstring str,
                                              /*[in]*/  const jchar *chars) 
{
    if (mJavaEnv == NULL)
        return NS_ERROR_NULL_POINTER;
    
    mJavaEnv->ReleaseStringChars(str, chars);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::NewStringUTF(/*[in]*/  const char *utf,
                                        /*[out]*/ jstring* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
	// *result = mJavaEnv->NewStringUTF(utf);
	class NewStringUTFMessage : public JavaMessage {
		const char *utf;
		jstring* result;
	public:
		NewStringUTFMessage(const char *utf, jstring* result)
		{
			this->utf = utf;
			this->result = result;
		}

		virtual void execute(JNIEnv* env)
		{
	    	*result = env->NewStringUTF(utf);
		}
	} msg(utf, result);
    sendMessageToJava(&msg);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetStringUTFLength(/*[in]*/  jstring str,
                                              /*[out]*/ jsize* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->GetStringUTFLength(str);

    return NS_OK;
}

    
NS_IMETHODIMP CSecureEnv::GetStringUTFChars(/*[in]*/  jstring str,
                                             /*[in]*/  jboolean *isCopy,
                                             /*[out]*/ const char** result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->GetStringUTFChars(str, isCopy);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::ReleaseStringUTFChars(/*[in]*/  jstring str,
                                                 /*[in]*/  const char *chars) 
{
    if (mJavaEnv == NULL)
        return NS_ERROR_NULL_POINTER;
    
    mJavaEnv->ReleaseStringUTFChars(str, chars);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetArrayLength(/*[in]*/  jarray array,
                                          /*[out]*/ jsize* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = mJavaEnv->GetArrayLength(array);

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::NewObjectArray(/*[in]*/  jsize len,
										/*[in]*/  jclass clazz,
					                    /*[in]*/  jobject init,
					                    /*[out]*/ jobjectArray* result)
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = mJavaEnv->NewObjectArray(len, clazz, init);

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::GetObjectArrayElement(/*[in]*/  jobjectArray array,
                                                 /*[in]*/  jsize index,
                                                 /*[out]*/ jobject* result)
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = mJavaEnv->GetObjectArrayElement(array, index);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::SetObjectArrayElement(/*[in]*/  jobjectArray array,
                                                 /*[in]*/  jsize index,
                                                 /*[in]*/  jobject val) 
{
    if (mJavaEnv == NULL)
        return NS_ERROR_NULL_POINTER;

    mJavaEnv->SetObjectArrayElement(array, index, val);

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::NewArray(/*[in]*/ jni_type element_type,
                        			/*[in]*/  jsize len,
                        			/*[out]*/ jarray* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (element_type) {
    case jboolean_type:
        result = (jarray*) mJavaEnv->NewBooleanArray(len);
        break;
    case jbyte_type:
        result = (jarray*) mJavaEnv->NewByteArray(len);
        break;
    case jchar_type:
        result = (jarray*) mJavaEnv->NewCharArray(len);
        break;
    case jshort_type:
        result = (jarray*) mJavaEnv->NewShortArray(len);
        break;
    case jint_type:
        result = (jarray*) mJavaEnv->NewIntArray(len);
        break;
    case jlong_type:
        result = (jarray*) mJavaEnv->NewLongArray(len);
        break;
    case jfloat_type:
        result = (jarray*) mJavaEnv->NewFloatArray(len);
        break;
    case jdouble_type:
        result = (jarray*) mJavaEnv->NewDoubleArray(len);
        break;
    default:
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::GetArrayElements(/*[in]*/  jni_type type,
                                            /*[in]*/  jarray array,
                                            /*[in]*/  jboolean *isCopy,
                                            /*[out]*/ void* result)
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (type) {
	case jboolean_type:
	    result = (void*) mJavaEnv->GetBooleanArrayElements((jbooleanArray)array, isCopy);
	    break;
	case jbyte_type:
	    result = (void*) mJavaEnv->GetByteArrayElements((jbyteArray)array, isCopy);
	    break;
	case jchar_type:
	    result = (void*) mJavaEnv->GetCharArrayElements((jcharArray)array, isCopy);
	    break;
	case jshort_type:
	    result = (void*) mJavaEnv->GetShortArrayElements((jshortArray)array, isCopy);
	    break;
	case jint_type:
	    result = (void*) mJavaEnv->GetIntArrayElements((jintArray)array, isCopy);
	    break;
	case jlong_type:
	    result = (void*) mJavaEnv->GetLongArrayElements((jlongArray)array, isCopy);
	    break;
	case jfloat_type:
	    result = (void*) mJavaEnv->GetFloatArrayElements((jfloatArray)array, isCopy);
	    break;
	case jdouble_type:
	    result = (void*) mJavaEnv->GetDoubleArrayElements((jdoubleArray)array, isCopy);
	    break;
	default:
	    return NS_ERROR_FAILURE;
	}

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::ReleaseArrayElements(/*[in]*/ jni_type type,
                                                /*[in]*/ jarray array,
                                                /*[in]*/ void *elems,
                                                /*[in]*/ jint mode) 
{
	if (mJavaEnv == NULL)
		return NS_ERROR_NULL_POINTER;
	
	switch (type)
	{
	case jboolean_type:
		mJavaEnv->ReleaseBooleanArrayElements((jbooleanArray)array, (jboolean*)elems, mode);
		break;
	case jbyte_type:
		mJavaEnv->ReleaseByteArrayElements((jbyteArray)array, (jbyte*)elems, mode);
		break;
	case jchar_type:
		mJavaEnv->ReleaseCharArrayElements((jcharArray)array, (jchar*)elems, mode);
		break;
	case jshort_type:
		mJavaEnv->ReleaseShortArrayElements((jshortArray)array, (jshort*)elems, mode);
		break;
	case jint_type:
		mJavaEnv->ReleaseIntArrayElements((jintArray)array, (jint*)elems, mode);
		break;
	case jlong_type:
		mJavaEnv->ReleaseLongArrayElements((jlongArray)array, (jlong*)elems, mode);
		break;
	case jfloat_type:
		mJavaEnv->ReleaseFloatArrayElements((jfloatArray)array, (jfloat*)elems, mode);
		break;
	case jdouble_type:
		mJavaEnv->ReleaseDoubleArrayElements((jdoubleArray)array, (jdouble*)elems, mode);
		break;
	default:
		return NS_ERROR_FAILURE;
	}

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::GetArrayRegion(/*[in]*/  jni_type type,
                                          /*[in]*/  jarray array,
                                          /*[in]*/  jsize start,
                                          /*[in]*/  jsize len,
                                          /*[out]*/ void* buf)
{
    if (mJavaEnv == NULL || buf == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (type) {
    case jboolean_type:
        mJavaEnv->GetBooleanArrayRegion((jbooleanArray)array, start, len, (jboolean*)buf);
        break;
    case jbyte_type:
        mJavaEnv->GetByteArrayRegion((jbyteArray)array, start, len, (jbyte*)buf);
        break;
    case jchar_type:
        mJavaEnv->GetCharArrayRegion((jcharArray)array, start, len, (jchar*)buf);
        break;
    case jshort_type:
        mJavaEnv->GetShortArrayRegion((jshortArray)array, start, len, (jshort*)buf);
        break;
    case jint_type:
        mJavaEnv->GetIntArrayRegion((jintArray)array, start, len, (jint*)buf);
        break;
    case jlong_type:
        mJavaEnv->GetLongArrayRegion((jlongArray)array, start, len, (jlong*)buf);
        break;
    case jfloat_type:
        mJavaEnv->GetFloatArrayRegion((jfloatArray)array, start, len, (jfloat*)buf);
        break;
    case jdouble_type:
        mJavaEnv->GetDoubleArrayRegion((jdoubleArray)array, start, len, (jdouble*)buf);
        break;
    default:
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::SetArrayRegion(/*[in]*/  jni_type type,
                                          /*[in]*/  jarray array,
                                          /*[in]*/  jsize start,
                                          /*[in]*/  jsize len,
                                          /*[in]*/  void* buf) 
{
    if (mJavaEnv == NULL || buf == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (type) {
    case jboolean_type:
        mJavaEnv->SetBooleanArrayRegion((jbooleanArray)array, start, len, (jboolean*)buf);
        break;
    case jbyte_type:
        mJavaEnv->SetByteArrayRegion((jbyteArray)array, start, len, (jbyte*)buf);
        break;
    case jchar_type:
        mJavaEnv->SetCharArrayRegion((jcharArray)array, start, len, (jchar*)buf);
        break;
    case jshort_type:
        mJavaEnv->SetShortArrayRegion((jshortArray)array, start, len, (jshort*)buf);
        break;
    case jint_type:
        mJavaEnv->SetIntArrayRegion((jintArray)array, start, len, (jint*)buf);
        break;
    case jlong_type:
        mJavaEnv->SetLongArrayRegion((jlongArray)array, start, len, (jlong*)buf);
        break;
    case jfloat_type:
        mJavaEnv->SetFloatArrayRegion((jfloatArray)array, start, len, (jfloat*)buf);
        break;
    case jdouble_type:
        mJavaEnv->SetDoubleArrayRegion((jdoubleArray)array, start, len, (jdouble*)buf);
        break;
    default:
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::RegisterNatives(/*[in]*/  jclass clazz,
                                           /*[in]*/  const JNINativeMethod *methods,
                                           /*[in]*/  jint nMethods,
                                           /*[out]*/ jint* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = mJavaEnv->RegisterNatives(clazz, methods, nMethods);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::UnregisterNatives(/*[in]*/  jclass clazz,
                                             /*[out]*/ jint* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = mJavaEnv->UnregisterNatives(clazz);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::MonitorEnter(/*[in]*/  jobject obj,
                                        /*[out]*/ jint* result) 
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

	// *result = mJavaEnv->MonitorEnter(obj);
	class MonitorEnterMessage : public JavaMessage {
		jobject obj;
		jint* result;
	public:
		MonitorEnterMessage(jobject obj, jint* result)
		{
			this->obj = obj;
			this->result = result;
		}

		virtual void execute(JNIEnv* env)
		{
			*result = env->MonitorEnter(obj);
		}
	} msg(obj, result);
	sendMessageToJava(&msg);

    return NS_OK;
}


NS_IMETHODIMP CSecureEnv::MonitorExit(/*[in]*/  jobject obj,
                                       /*[out]*/ jint* result)
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

	// *result = mJavaEnv->MonitorExit(obj);
	class MonitorExitMessage : public JavaMessage {
		jobject obj;
		jint* result;
	public:
		MonitorExitMessage(jobject obj, jint* result)
		{
			this->obj = obj;
			this->result = result;
		}

		virtual void execute(JNIEnv* env)
		{
			*result = env->MonitorExit(obj);
		}
	} msg(obj, result);
	sendMessageToJava(&msg);

    return NS_OK;
}

NS_IMETHODIMP CSecureEnv::GetJavaVM(/*[in]*/  JavaVM **vm,
                                     /*[out]*/ jint* result)
{
    if (mJavaEnv == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = mJavaEnv->GetJavaVM(vm);

    return NS_OK;
}

/**
 * Runs a message loop. Puts the message in the sendQueue, then waits on the receiveQueue for the reply.
 */

void CSecureEnv::messageLoop(JNIEnv* env, JavaMessage* msg, JavaMessageQueue* sendQueue, JavaMessageQueue* receiveQueue, Boolean busyWaiting)
{
	// put the message in the send queue, and wait for a reply.
	sendQueue->putMessage(msg);
	sendQueue->notify();
	JavaMessage* replyMsg = receiveQueue->getMessage();
	for (;;) {
		// Fully synchronized approach.
		if (replyMsg != NULL) {
			if (replyMsg == msg)
				break;
			// must be a message that needs executing.
			replyMsg->execute(env);
			sendQueue->putMessage(replyMsg);
			sendQueue->notify();
			// notify can cause a yield, so look again.
			replyMsg = receiveQueue->getMessage();
			if (replyMsg != NULL)
				continue;
		}
		// when busy waiting, must give time to browser between timed waits.
		if (busyWaiting) {
			receiveQueue->wait(1024);
			replyMsg = receiveQueue->getMessage();
			if (replyMsg != NULL)
				continue;
			// mSession->lock();
			mThreadManager->Sleep();
			// mSession->unlock();
		} else {
			// wait for a message to arrive.
			receiveQueue->wait();
		}
		replyMsg = receiveQueue->getMessage();
	}
}

CSecureEnv* GetSecureJNI(JNIEnv* env, jobject thread)
{
	CSecureEnv* secureJNI = NULL;
	
	jclass threadClass = env->GetObjectClass(thread);
	if (threadClass != NULL) {
		jfieldID fSecureEnvField = env->GetFieldID(threadClass, "fSecureEnv", "I");
		if (fSecureEnvField != NULL) {
			secureJNI = (CSecureEnv*) env->GetIntField(thread, fSecureEnvField);
		} else {
			env->ExceptionClear();
		}
		env->DeleteLocalRef(threadClass);
	}
	
	return secureJNI;
}
