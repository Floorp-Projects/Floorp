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
    CSecureEnv.h
    
    Rewritten for use with MRJ plugin by Patrick C. Beard.
 */

#ifndef CSecureJNI2_h___
#define CSecureJNI_h___

#include "nsISecureEnv.h"
#include "nsIThreadManager.h"
#include "SupportsMixin.h"

class MRJPlugin;
class MRJSession;

class Monitor;
class nsIThreadManager;
class nsIJVMManager;

class JavaMessage;
class JavaMessageQueue;

class CSecureEnv : public nsISecureEnv, public nsIRunnable, private SupportsMixin {
public:
    DECL_SUPPORTS_MIXIN

    static NS_METHOD Create(MRJPlugin* plugin, JNIEnv* proxyEnv, const nsIID& aIID, void* *aInstancePtr);

    ////////////////////////////////////////////////////////////////////////////
    // from nsISecureJNI2:


    /**
     * Create new Java object in LiveConnect.
     *
     * @param clazz      -- Java Class object.
     * @param methodID   -- Method id
     * @param args       -- arguments for invoking the constructor.
     * @param result     -- return new Java object.
     * @param ctx        -- security context 
     */
    NS_IMETHOD NewObject(/*[in]*/  jclass clazz, 
                         /*[in]*/  jmethodID methodID,
                         /*[in]*/  jvalue *args, 
                         /*[out]*/ jobject* result,
                         /*[in]*/  nsISecurityContext* ctx = NULL);
   
    /**
     * Invoke method on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param obj        -- Java object.
     * @param methodID   -- method id
     * @param args       -- arguments for invoking the constructor.
     * @param result     -- return result of invocation.
     * @param ctx        -- security context 
     */
    NS_IMETHOD CallMethod(/*[in]*/  jni_type type,
                          /*[in]*/  jobject obj, 
                          /*[in]*/  jmethodID methodID,
                          /*[in]*/  jvalue *args, 
                          /*[out]*/ jvalue* result,
                          /*[in]*/  nsISecurityContext* ctx = NULL);

    /**
     * Invoke non-virtual method on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param obj        -- Java object.
     * @param clazz      -- Class object
     * @param methodID   -- method id
     * @param args       -- arguments for invoking the constructor.
     * @param ctx        -- security context 
     * @param result     -- return result of invocation.
     */
    NS_IMETHOD CallNonvirtualMethod(/*[in]*/  jni_type type,
                                    /*[in]*/  jobject obj, 
                                    /*[in]*/  jclass clazz,
                                    /*[in]*/  jmethodID methodID,
                                    /*[in]*/  jvalue *args, 
                                    /*[out]*/ jvalue* result,
                                    /*[in]*/  nsISecurityContext* ctx = NULL);

    /**
     * Get a field on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param obj        -- Java object.
     * @param fieldID    -- field id
     * @param result     -- return field value
     * @param ctx        -- security context 
     */
    NS_IMETHOD GetField(/*[in]*/  jni_type type,
                        /*[in]*/  jobject obj, 
                        /*[in]*/  jfieldID fieldID,
                        /*[out]*/ jvalue* result,
                        /*[in]*/  nsISecurityContext* ctx = NULL);

    /**
     * Set a field on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param obj        -- Java object.
     * @param fieldID    -- field id
     * @param val        -- field value to set
     * @param ctx        -- security context 
     */
    NS_IMETHOD SetField(/*[in]*/ jni_type type,
                        /*[in]*/ jobject obj, 
                        /*[in]*/ jfieldID fieldID,
                        /*[in]*/ jvalue val,
                        /*[in]*/ nsISecurityContext* ctx = NULL);

    /**
     * Invoke static method on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param clazz      -- Class object.
     * @param methodID   -- method id
     * @param args       -- arguments for invoking the constructor.
     * @param result     -- return result of invocation.
     * @param ctx        -- security context 
     */
    NS_IMETHOD CallStaticMethod(/*[in]*/  jni_type type,
                                /*[in]*/  jclass clazz,
                                /*[in]*/  jmethodID methodID,
                                /*[in]*/  jvalue *args, 
                                /*[out]*/ jvalue* result,
                                /*[in]*/  nsISecurityContext* ctx = NULL);

    /**
     * Get a static field on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param clazz      -- Class object.
     * @param fieldID    -- field id
     * @param result     -- return field value
     * @param ctx        -- security context 
     */
    NS_IMETHOD GetStaticField(/*[in]*/  jni_type type,
                              /*[in]*/  jclass clazz, 
                              /*[in]*/  jfieldID fieldID, 
                              /*[out]*/ jvalue* result,
                              /*[in]*/  nsISecurityContext* ctx = NULL);


    /**
     * Set a static field on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param clazz      -- Class object.
     * @param fieldID    -- field id
     * @param val        -- field value to set
     * @param ctx        -- security context 
     */
    NS_IMETHOD SetStaticField(/*[in]*/ jni_type type,
                              /*[in]*/ jclass clazz, 
                              /*[in]*/ jfieldID fieldID,
                              /*[in]*/ jvalue val,
                              /*[in]*/ nsISecurityContext* ctx = NULL);


    NS_IMETHOD GetVersion(/*[out]*/ jint* version);

    NS_IMETHOD DefineClass(/*[in]*/  const char* name,
                           /*[in]*/  jobject loader,
                           /*[in]*/  const jbyte *buf,
                           /*[in]*/  jsize len,
                           /*[out]*/ jclass* clazz);

    NS_IMETHOD FindClass(/*[in]*/  const char* name, 
                         /*[out]*/ jclass* clazz);

    NS_IMETHOD GetSuperclass(/*[in]*/  jclass sub,
                             /*[out]*/ jclass* super);

    NS_IMETHOD IsAssignableFrom(/*[in]*/  jclass sub,
                                /*[in]*/  jclass super,
                                /*[out]*/ jboolean* result);

    NS_IMETHOD Throw(/*[in]*/  jthrowable obj,
                     /*[out]*/ jint* result);

    NS_IMETHOD ThrowNew(/*[in]*/  jclass clazz,
                        /*[in]*/  const char *msg,
                        /*[out]*/ jint* result);

    NS_IMETHOD ExceptionOccurred(/*[out]*/ jthrowable* result);

    NS_IMETHOD ExceptionDescribe(void);

    NS_IMETHOD ExceptionClear(void);

    NS_IMETHOD FatalError(/*[in]*/ const char* msg);

    NS_IMETHOD NewGlobalRef(/*[in]*/  jobject lobj, 
                            /*[out]*/ jobject* result);

    NS_IMETHOD DeleteGlobalRef(/*[in]*/ jobject gref);

    NS_IMETHOD DeleteLocalRef(/*[in]*/ jobject obj);

    NS_IMETHOD IsSameObject(/*[in]*/  jobject obj1,
                            /*[in]*/  jobject obj2,
                            /*[out]*/ jboolean* result);

    NS_IMETHOD AllocObject(/*[in]*/  jclass clazz,
                           /*[out]*/ jobject* result);

    NS_IMETHOD GetObjectClass(/*[in]*/  jobject obj,
                              /*[out]*/ jclass* result);

    NS_IMETHOD IsInstanceOf(/*[in]*/  jobject obj,
                            /*[in]*/  jclass clazz,
                            /*[out]*/ jboolean* result);

    NS_IMETHOD GetMethodID(/*[in]*/  jclass clazz, 
                           /*[in]*/  const char* name,
                           /*[in]*/  const char* sig,
                           /*[out]*/ jmethodID* id);

    NS_IMETHOD GetFieldID(/*[in]*/  jclass clazz, 
                          /*[in]*/  const char* name,
                          /*[in]*/  const char* sig,
                          /*[out]*/ jfieldID* id);

    NS_IMETHOD GetStaticMethodID(/*[in]*/  jclass clazz, 
                                 /*[in]*/  const char* name,
                                 /*[in]*/  const char* sig,
                                 /*[out]*/ jmethodID* id);

    NS_IMETHOD GetStaticFieldID(/*[in]*/  jclass clazz, 
                                /*[in]*/  const char* name,
                                /*[in]*/  const char* sig,
                                /*[out]*/ jfieldID* id);

    NS_IMETHOD NewString(/*[in]*/  const jchar* unicode,
                         /*[in]*/  jsize len,
                         /*[out]*/ jstring* result);

    NS_IMETHOD GetStringLength(/*[in]*/  jstring str,
                               /*[out]*/ jsize* result);
    
    NS_IMETHOD GetStringChars(/*[in]*/  jstring str,
                              /*[in]*/  jboolean *isCopy,
                              /*[out]*/ const jchar** result);

    NS_IMETHOD ReleaseStringChars(/*[in]*/  jstring str,
                                  /*[in]*/  const jchar *chars);

    NS_IMETHOD NewStringUTF(/*[in]*/  const char *utf,
                            /*[out]*/ jstring* result);

    NS_IMETHOD GetStringUTFLength(/*[in]*/  jstring str,
                                  /*[out]*/ jsize* result);
    
    NS_IMETHOD GetStringUTFChars(/*[in]*/  jstring str,
                                 /*[in]*/  jboolean *isCopy,
                                 /*[out]*/ const char** result);

    NS_IMETHOD ReleaseStringUTFChars(/*[in]*/  jstring str,
                                     /*[in]*/  const char *chars);

    NS_IMETHOD GetArrayLength(/*[in]*/  jarray array,
                              /*[out]*/ jsize* result);

    NS_IMETHOD NewObjectArray(/*[in]*/  jsize len,
                        /*[in]*/  jclass clazz,
                        /*[in]*/  jobject init,
                        /*[out]*/ jobjectArray* result);

    NS_IMETHOD GetObjectArrayElement(/*[in]*/  jobjectArray array,
                                     /*[in]*/  jsize index,
                                     /*[out]*/ jobject* result);

    NS_IMETHOD SetObjectArrayElement(/*[in]*/  jobjectArray array,
                                     /*[in]*/  jsize index,
                                     /*[in]*/  jobject val);

    NS_IMETHOD NewArray(/*[in]*/ jni_type element_type,
                        /*[in]*/  jsize len,
                        /*[out]*/ jarray* result);

    NS_IMETHOD GetArrayElements(/*[in]*/  jni_type type,
                                /*[in]*/  jarray array,
                                /*[in]*/  jboolean *isCopy,
                                /*[out]*/ void* result);

    NS_IMETHOD ReleaseArrayElements(/*[in]*/ jni_type type,
                                    /*[in]*/ jarray array,
                                    /*[in]*/ void *elems,
                                    /*[in]*/ jint mode);

    NS_IMETHOD GetArrayRegion(/*[in]*/  jni_type type,
                              /*[in]*/  jarray array,
                              /*[in]*/  jsize start,
                              /*[in]*/  jsize len,
                              /*[out]*/ void* buf);

    NS_IMETHOD SetArrayRegion(/*[in]*/  jni_type type,
                              /*[in]*/  jarray array,
                              /*[in]*/  jsize start,
                              /*[in]*/  jsize len,
                              /*[in]*/  void* buf);

    NS_IMETHOD RegisterNatives(/*[in]*/  jclass clazz,
                               /*[in]*/  const JNINativeMethod *methods,
                               /*[in]*/  jint nMethods,
                               /*[out]*/ jint* result);

    NS_IMETHOD UnregisterNatives(/*[in]*/  jclass clazz,
                                 /*[out]*/ jint* result);

    NS_IMETHOD MonitorEnter(/*[in]*/  jobject obj,
                            /*[out]*/ jint* result);

    NS_IMETHOD MonitorExit(/*[in]*/  jobject obj,
                           /*[out]*/ jint* result);

    NS_IMETHOD GetJavaVM(/*[in]*/  JavaVM **vm,
                         /*[out]*/ jint* result);
                         
    ////////////////////////////////////////////////////////////////////////////
    // from nsIRunnable:
    
    NS_IMETHOD Run();

    CSecureEnv(MRJPlugin* plugin, JNIEnv* proxyEnv, JNIEnv* javaEnv = NULL);
    virtual ~CSecureEnv(void);

    /**
     * Called by the native run method, to connect the
     * thread and the secure env.
     */
    void initialize(JNIEnv* javaEnv, jboolean* isRunning, JavaMessageQueue* javaQueue, JavaMessageQueue* nativeQueue);
    
    jboolean isInitialized() { return mJavaQueue != NULL; }

    void setProxyEnv(JNIEnv* proxyEnv) { mProxyEnv = proxyEnv; }
    JNIEnv* getProxyEnv() { return mProxyEnv; }
    
    void setJavaEnv(JNIEnv* javaEnv) { mJavaEnv = javaEnv; }
    JNIEnv* getJavaEnv() { return mJavaEnv; }
    
    MRJSession* getSession() { return mSession; }
    nsIThreadManager* getThreadManager() { return mThreadManager; }
    
    void getMessageQueues(JavaMessageQueue*& javaQueue, JavaMessageQueue*& nativeQueue)
    {
        javaQueue = mJavaQueue;
        nativeQueue = mNativeQueue;
    }
    
    void sendMessageToJava(JavaMessage* msg);
    void sendMessageFromJava(JNIEnv* javaEnv, JavaMessage* msg, Boolean busyWaiting = false);
    void messageLoop(JNIEnv* env, JavaMessage* msgToSend, JavaMessageQueue* sendQueue, JavaMessageQueue* receiveQueue, Boolean busyWaiting = false);

    void savePendingException(JNIEnv* env);
    jthrowable getPendingException(JNIEnv* env);
    void clearPendingException(JNIEnv* env);
    
protected:

    MRJPlugin*              mPlugin;
    JNIEnv*                 mProxyEnv;
    MRJSession*             mSession;
    nsIThreadManager*       mThreadManager;
    
    JNIEnv*                 mJavaEnv;
    jboolean*               mIsRunning;
    JavaMessageQueue*       mJavaQueue;
    JavaMessageQueue*       mNativeQueue;

    jthrowable              mPendingException;

private:
    // support for SupportsMixin.
    static const InterfaceInfo sInterfaces[];
    static const UInt32 kInterfaceCount;
};

/**
 * Returns the secure JNI associated with the current thread (if any).
 */
CSecureEnv* GetSecureJNI(JNIEnv* env, jobject thread);

#endif // CSecureJNI2_h___
