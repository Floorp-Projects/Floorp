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
    LiveConnectNativeMethods.cpp
 */

#include <memory>

#include "LiveConnectNativeMethods.h"

#include "nsIComponentManager.h"
#include "nsIPluginManager.h"
#include "nsIJVMManager.h"
#include "nsILiveconnect.h"
#include "nsIPluginInstancePeer2.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsNetCID.h"

#include "MRJPlugin.h"
#include "MRJContext.h"
#include "MRJSession.h"
#include "MRJSecurityContext.h"
#include "CSecureEnv.h"
#include "JavaMessageQueue.h"
#include "MRJMonitor.h"
#include "NativeMonitor.h"
#include "RunnableMixin.h"
#include "StringUtils.h"

#include "netscape_javascript_JSObject.h"   /* javah-generated headers */

extern nsIPluginManager* thePluginManager;

static MRJPlugin* theJVMPlugin = NULL;
static nsILiveconnect* theLiveConnectManager = NULL;
static nsIComponentManager* theComponentManager = NULL;

static jclass netscape_javascript_JSObject = NULL;
static jmethodID netscape_javascript_JSObject_JSObject;
static jfieldID netscape_javascript_JSObject_internal;

static jclass netscape_oji_JNIUtils = NULL;
static jmethodID netscape_oji_JNIUtils_NewLocalRef = NULL;
static jmethodID netscape_oji_JNIUtils_GetCurrentThread = NULL;
static jmethodID netscape_oji_JNIUtils_GetCurrentClassLoader = NULL;
static jmethodID netscape_oji_JNIUtils_GetObjectClassLoader = NULL;

static NS_DEFINE_IID(kLiveConnectCID, NS_CLIVECONNECT_CID);
static NS_DEFINE_IID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static nsresult getGlobalComponentManager(nsIComponentManager* *result)
{
    return MRJPlugin::GetService(kComponentManagerCID, NS_GET_IID(nsIComponentManager), (void**)result);
}

nsresult InitLiveConnectSupport(MRJPlugin* jvmPlugin)
{
    theJVMPlugin = jvmPlugin;

    getGlobalComponentManager(&theComponentManager);

    nsresult result = MRJPlugin::GetService(kLiveConnectCID, NS_GET_IID(nsILiveconnect),
                                            (void**)&theLiveConnectManager);
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
    NS_IF_RELEASE(theLiveConnectManager);

    NS_IF_RELEASE(theComponentManager);
    
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

static nsresult
NS_NewURI(nsIURI* *result, 
          const char* spec, 
          nsIURI* baseURI = nsnull)     // pass in nsIIOService to optimize callers
{
    nsIIOService* ioService;
    static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
    nsresult rv = MRJPlugin::GetService(kIOServiceCID, NS_GET_IID(nsIIOService), (void**)&ioService);
    if (rv == NS_OK)
        rv = ioService->NewURI(spec, baseURI, result);
    NS_RELEASE(ioService);
    return rv;
}

MRJSecurityContext::MRJSecurityContext(const char* location)
    :   mLocation(nsnull), mConnection(nsnull)
{
    NS_INIT_REFCNT();
    
    NS_NewURI(&mLocation, location, nsnull);

    if (theComponentManager) {
        theComponentManager->CreateInstance(kLiveConnectCID, nsnull, NS_GET_IID(nsILiveconnect),
                                            (void**)&mConnection);
    } else {
        mConnection = theLiveConnectManager;
        NS_IF_ADDREF(mConnection);
    }
}

MRJSecurityContext::~MRJSecurityContext()
{
    NS_IF_RELEASE(mLocation);
    NS_IF_RELEASE(mConnection);
}

// work around a bug in Metrowerks pre-processor.
NS_IMPL_ISUPPORTS1(MRJSecurityContext, nsISecurityContext)

NS_METHOD MRJSecurityContext::Implies(const char* target, const char* action, PRBool *bAllowedAccess)
{
    *bAllowedAccess = (target != NULL && action == NULL);
    return NS_OK;
}

NS_METHOD 
MRJSecurityContext::GetOrigin(char* buf, int len)
{
    char* origin = nsnull;
    if (mLocation && NS_SUCCEEDED(mLocation->GetPrePath(&origin))) {
        ::strncpy(buf, origin, len);
        delete[] origin;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_METHOD 
MRJSecurityContext::GetCertificateID(char* buf, int len)
{
    // ACTION: Implement me.

    return NS_ERROR_NOT_IMPLEMENTED;
}

// NOTE:  this a weak reference to the MRJSecurityContext associated with this
// plugin instance. The MRJSecurityContext is owned by the MRJContext.

static MRJSecurityContext* getSecurityContext(MRJPluginInstance* pluginInstance)
{
    if (pluginInstance != NULL) {
        MRJContext* context = pluginInstance->getContext();
        MRJSecurityContext* securityContext = context->getSecurityContext();
        if (securityContext == NULL) {
            securityContext = new MRJSecurityContext(context->getDocumentBase());
            context->setSecurityContext(securityContext);
        }
        return securityContext;
    }
    return NULL;
}

inline nsILiveconnect* getLiveconnectInstance(MRJSecurityContext* securityContext)
{
    return (securityContext ? securityContext->getConnection() : theLiveConnectManager);
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
    if (MRJPlugin::GetService(nsIJVMManager::GetCID(), NS_GET_IID(nsIThreadManager), (void**)&threadManager) == NS_OK) {
        threadManager->PostEvent(mThreadID, this, PR_FALSE);
        NS_RELEASE(threadManager);
    }
}

NS_IMETHODIMP MessageRunnable::Run()
{
    nsIJVMManager* javaManager = NULL;
    if (MRJPlugin::GetService(nsIJVMManager::GetCID(), NS_GET_IID(nsIJVMManager), (void**)&javaManager) == NS_OK) {
        JNIEnv* proxyEnv = NULL;
        if (javaManager->GetProxyJNI(&proxyEnv) == NS_OK && proxyEnv != NULL)
            mMessage->execute(proxyEnv);
        NS_RELEASE(javaManager);
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
            if (peer->QueryInterface(NS_GET_IID(nsIPluginInstancePeer2), (void**)&peer2) == NS_OK) {
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
#if 0
    nsICapsManager* capsManager = NULL;
    static NS_DEFINE_IID(kICapsManagerIID, NS_ICAPSMANAGER_IID);
    if (thePluginManager->QueryInterface(kICapsManagerIID, &capsManager) == NS_OK) {
        if (capsManager->CreateCodebasePrincipal(codebaseURL, &principal) != NS_OK)
            principal = NULL;
        capsManager->Release();
    }
#endif
    return principal;
}

/****************** Implementation of methods of JSObject *******************/

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getMember
 * Signature: (Ljava/lang/String;)Ljava/lang/Object;
 */

class GetMemberMessage : public JavaMessage {
    MRJPluginInstance* mPluginInstance;
    jsobject mObject;
    const jchar* mPropertyName;
    jsize mLength;
    jobject* mResultObject;
public:
    GetMemberMessage(MRJPluginInstance* pluginInstance, jsobject js_obj,
                     const jchar* propertyName, jsize nameLength, jobject* member)
        :   mPluginInstance(pluginInstance), mObject(js_obj), mPropertyName(propertyName),
            mLength(nameLength), mResultObject(member)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        jobject member;
        nsresult result = connection->GetMember(env, mObject, mPropertyName, mLength, NULL, 0, securityContext, &member);
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

    MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
#if 0
    if (pluginInstance == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
        return NULL;
    }
#endif

    /* Get the Unicode string for the JS property name */
    jboolean is_copy;
    const jchar* property_name_ucs2 = env->GetStringChars(property_name_jstr, &is_copy);
    jsize property_name_len = env->GetStringLength(property_name_jstr);

    jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
    jobject member = NULL;

    GetMemberMessage msg(pluginInstance, js_obj, property_name_ucs2, property_name_len, &member);
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
    MRJPluginInstance* mPluginInstance;
    jsobject mObject;
    jint mSlot;
    jobject* mResultObject;
public:
    GetSlotMessage(MRJPluginInstance* pluginInstance, jsobject js_obj,
                   jint slot, jobject* member)
        :   mPluginInstance(pluginInstance), mObject(js_obj), mSlot(slot), mResultObject(member)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        jobject member;
        nsresult result = connection->GetSlot(env, mObject, mSlot, NULL, 0, securityContext, &member);
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
    MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
#if 0
    if (pluginInstance == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
        return NULL;
    }
#endif

    jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
    jobject member = NULL;
    
    GetSlotMessage msg(pluginInstance, js_obj, slot, &member);
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
    MRJPluginInstance* mPluginInstance;
    jsobject mObject;
    const jchar* mPropertyName;
    jsize mLength;
    jobject mJavaObject;
public:
    SetMemberMessage(MRJPluginInstance* pluginInstance, jsobject js_obj, const jchar* propertyName,
                     jsize nameLength, jobject java_obj)
        :   mPluginInstance(pluginInstance), mObject(js_obj), mPropertyName(propertyName), mLength(nameLength), mJavaObject(java_obj)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        nsresult result = connection->SetMember(env, mObject, mPropertyName, mLength, mJavaObject, 0, NULL, securityContext);
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

    MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
#if 0
    if (pluginInstance == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
        return;
    }
#endif

    /* Get the Unicode string for the JS property name */
    jboolean is_copy;
    const jchar* property_name_ucs2 = env->GetStringChars(property_name_jstr, &is_copy);
    jsize property_name_len = env->GetStringLength(property_name_jstr);
    
    jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
    java_obj = ToGlobalRef(env, java_obj);

    SetMemberMessage msg(pluginInstance, js_obj, property_name_ucs2, property_name_len, java_obj);
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
    MRJPluginInstance* mPluginInstance;
    jsobject mObject;
    jint mSlot;
    jobject mJavaObject;
public:
    SetSlotMessage(MRJPluginInstance* pluginInstance, jsobject js_obj, jint slot, jobject java_obj)
        :   mPluginInstance(pluginInstance), mObject(js_obj), mSlot(slot), mJavaObject(java_obj)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        nsresult result = connection->SetSlot(env, mObject, mSlot, mJavaObject, 0, NULL, securityContext);
    }
};

JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_setSlot(JNIEnv* env,
                                          jobject java_wrapper_obj,
                                          jint slot,
                                          jobject java_obj)
{
    MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
#if 0
    if (pluginInstance == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
        return;
    }
#endif

    jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
    java_obj = ToGlobalRef(env, java_obj);
    
    SetSlotMessage msg(pluginInstance, js_obj, slot, java_obj);
    sendMessage(env, &msg);
    env->DeleteGlobalRef(java_obj);
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    removeMember
 * Signature: (Ljava/lang/String;)V
 */

class RemoveMemberMessage : public JavaMessage {
    MRJPluginInstance* mPluginInstance;
    jsobject mObject;
    const jchar* mPropertyName;
    jsize mLength;
public:
    RemoveMemberMessage(MRJPluginInstance* pluginInstance, jsobject obj,
                        const jchar* propertyName, jsize length)
        :   mPluginInstance(pluginInstance), mObject(obj),
            mPropertyName(propertyName), mLength(length)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        nsresult result = connection->RemoveMember(env, mObject, mPropertyName, mLength, NULL, 0, securityContext);
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

    MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
#if 0
    if (pluginInstance == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
        return;
    }
#endif

    /* Get the Unicode string for the JS property name */
    jboolean is_copy;
    const jchar* property_name_ucs2 = env->GetStringChars(property_name_jstr, &is_copy);
    jsize property_name_len = env->GetStringLength(property_name_jstr);

    jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
    RemoveMemberMessage msg(pluginInstance, js_obj, property_name_ucs2, property_name_len);
    
    sendMessage(env, &msg);
    
    env->ReleaseStringChars(property_name_jstr, property_name_ucs2);
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    call
 * Signature: (Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;
 */
class CallMessage : public JavaMessage {
    MRJPluginInstance* mPluginInstance;
    jsobject mObject;
    const jchar* mFunctionName;
    jsize mLength;
    jobjectArray mJavaArgs;
    jobject* mJavaResult;
public:
    CallMessage(MRJPluginInstance* pluginInstance, jsobject obj, const jchar* functionName,
                jsize length, jobjectArray javaArgs, jobject* javaResult)
        :   mPluginInstance(pluginInstance), mObject(obj), mFunctionName(functionName),
            mLength(length), mJavaArgs(javaArgs), mJavaResult(javaResult)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        /* If we have an applet, try to create a codebase principle. */
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        jobject jresult = NULL;
        nsresult result = connection->Call(env, mObject, mFunctionName, mLength, mJavaArgs, NULL, 0, securityContext, &jresult);
        if (result == NS_OK)
            *mJavaResult = ToGlobalRef(env, jresult);
    }
};

JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_call(JNIEnv* env, jobject java_wrapper_obj,
                                       jstring function_name_jstr, jobjectArray java_args)
{
    if (function_name_jstr == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal null function name");
        return NULL;
    }

    /* Try to determine which plugin instance is responsible for this thread. This is done by checking class loaders. */
    MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
#if 0
    if (pluginInstance == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
        return NULL;
    }
#endif

    /* Get the Unicode string for the JS function name */
    jboolean is_copy;
    const jchar* function_name_ucs2 = env->GetStringChars(function_name_jstr, &is_copy);
    jsize function_name_len = env->GetStringLength(function_name_jstr);

    jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);
    jobject jresult = NULL;

    CallMessage msg(pluginInstance, js_obj, function_name_ucs2, function_name_len, java_args, &jresult);
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
class EvalMessage : public JavaMessage {
    MRJPluginInstance* mPluginInstance;
    jsobject mObject;
    const jchar* mScript;
    jsize mLength;
    jobject* mJavaResult;
public:
    EvalMessage(MRJPluginInstance* pluginInstance, jsobject obj, const jchar* script, jsize length, jobject* javaResult)
        :   mPluginInstance(pluginInstance), mObject(obj), mScript(script), mLength(length), mJavaResult(javaResult)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        jobject jresult = NULL;
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        nsresult result = connection->Eval(env, mObject, mScript, mLength, NULL, 0, securityContext, &jresult);
        if (result == NS_OK && jresult != NULL)
            *mJavaResult = ToGlobalRef(env, jresult);
    }
};

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
    // beard: should file a bug with Apple that JMJNIToAWTContext doesn't work.
    // MRJPluginInstance* pluginInstance = theJVMPlugin->getPluginInstance(env);
    MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
#if 0
    if (pluginInstance == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
        return NULL;
    }
#endif

    EvalMessage msg(pluginInstance, js_obj, script_ucs2, script_len, &jresult);
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
class ToStringMessage : public JavaMessage {
    MRJPluginInstance* mPluginInstance;
    jsobject mObject;
    jstring* mStringResult;
public:
    ToStringMessage(MRJPluginInstance* pluginInstance, jsobject js_obj, jstring* stringResult)
        :   mPluginInstance(pluginInstance), mObject(js_obj), mStringResult(stringResult)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        jstring jresult = NULL;
        nsresult status = connection->ToString(env, mObject, &jresult);
        if (status == NS_OK && jresult != NULL)
            *mStringResult = (jstring) ToGlobalRef(env, jresult);
    }
};

JNIEXPORT jstring JNICALL
Java_netscape_javascript_JSObject_toString(JNIEnv* env, jobject java_wrapper_obj)
{
    /* unwrap the JS object from the Java object. */
    jstring jresult = NULL;
    jsobject js_obj = Unwrap_JSObject(env, java_wrapper_obj);

    MRJPluginInstance* pluginInstance = GetCurrentInstance(env);
#if 0
    if (pluginInstance == NULL) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "illegal JNIEnv (can't find plugin)");
        return NULL;
    }
#endif

    ToStringMessage msg(pluginInstance, js_obj, &jresult);
    sendMessage(env, &msg);
    
    if (jresult != NULL)
        jresult = (jstring) ToLocalRef(env, jresult);
    
    return jresult;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getWindow
 * Signature: (Ljava/applet/Applet;)Lnetscape/javascript/JSObject;
 */
class GetWindowMessage : public JavaMessage {
    MRJPluginInstance* mPluginInstance;
    jsobject* mWindowResult;
public:
    GetWindowMessage(MRJPluginInstance* pluginInstance, jsobject* windowResult)
        :   mPluginInstance(pluginInstance), mWindowResult(windowResult)
    {
    }
    
    virtual void execute(JNIEnv* env)
    {
        MRJSecurityContext* securityContext = getSecurityContext(mPluginInstance);
        nsILiveconnect* connection = getLiveconnectInstance(securityContext);
        nsresult status = connection->GetWindow(env, mPluginInstance, NULL, 0, securityContext, mWindowResult);
    }
};

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
        jsobject jswindow = NULL;
        GetWindowMessage msg(pluginInstance, &jswindow);
        sendMessage(env, &msg);
        
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
class FinalizeMessage : public JavaMessage {
    jsobject m_jsobj;
public:
    FinalizeMessage(jsobject jsobj)
        :   m_jsobj(jsobj)
    {
    }

    virtual void execute(JNIEnv* env)
    {
        nsresult result = theLiveConnectManager->FinalizeJSObject(env, m_jsobj);
    }
};

JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_finalize(JNIEnv* env, jobject java_wrapper_obj)
{
    jsobject jsobj = Unwrap_JSObject(env, java_wrapper_obj);

#ifdef MRJPLUGIN_4X
    MRJPluginInstance* pluginInstance = (MRJPluginInstance*)jsobj;
    NS_IF_RELEASE(pluginInstance);
#else    
    FinalizeMessage msg(jsobj);
    sendMessage(env, &msg);
#endif
}
