/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <nsISecureEnv.h>

class nsDummySecureEnv : public nsISecureEnv {
public:
    nsDummySecureEnv() {}

	//nsISupports interface

    NS_METHOD QueryInterface(const nsIID & uuid, void * *result) { return NS_OK; }

    NS_METHOD_(nsrefcnt) AddRef(void) { return NS_OK; }

    NS_METHOD_(nsrefcnt) Release(void) { return NS_OK; }


	//nsISecureEnv interface

    NS_METHOD NewObject(/*[in]*/  jclass clazz, 
                         /*[in]*/  jmethodID methodID,
                         /*[in]*/  jvalue *args, 
                         /*[out]*/ jobject* result,
                         /*[in]*/  nsISecurityContext* ctx = NULL) { return NS_OK; }
   
    NS_METHOD CallMethod(/*[in]*/  jni_type type,
                          /*[in]*/  jobject obj, 
                          /*[in]*/  jmethodID methodID,
                          /*[in]*/  jvalue *args, 
                          /*[out]*/ jvalue* result,
                          /*[in]*/  nsISecurityContext* ctx = NULL) { return NS_OK; }

    NS_METHOD CallNonvirtualMethod(/*[in]*/  jni_type type,
                                    /*[in]*/  jobject obj, 
                                    /*[in]*/  jclass clazz,
                                    /*[in]*/  jmethodID methodID,
                                    /*[in]*/  jvalue *args, 
                                    /*[out]*/ jvalue* result,
                                    /*[in]*/  nsISecurityContext* ctx = NULL) { return NS_OK; }

    NS_METHOD GetField(/*[in]*/  jni_type type,
                        /*[in]*/  jobject obj, 
                        /*[in]*/  jfieldID fieldID,
                        /*[out]*/ jvalue* result,
                        /*[in]*/  nsISecurityContext* ctx = NULL) { return NS_OK; }

    NS_METHOD SetField(/*[in]*/ jni_type type,
                        /*[in]*/ jobject obj, 
                        /*[in]*/ jfieldID fieldID,
                        /*[in]*/ jvalue val,
                        /*[in]*/ nsISecurityContext* ctx = NULL) { return NS_OK; }

    NS_METHOD CallStaticMethod(/*[in]*/  jni_type type,
                                /*[in]*/  jclass clazz,
                                /*[in]*/  jmethodID methodID,
                                /*[in]*/  jvalue *args, 
                                /*[out]*/ jvalue* result,
                                /*[in]*/  nsISecurityContext* ctx = NULL) { return NS_OK; }

    NS_METHOD GetStaticField(/*[in]*/  jni_type type,
                              /*[in]*/  jclass clazz, 
                              /*[in]*/  jfieldID fieldID, 
                              /*[out]*/ jvalue* result,
                              /*[in]*/  nsISecurityContext* ctx = NULL) { return NS_OK; }


    NS_METHOD SetStaticField(/*[in]*/ jni_type type,
                              /*[in]*/ jclass clazz, 
                              /*[in]*/ jfieldID fieldID,
                              /*[in]*/ jvalue val,
                              /*[in]*/ nsISecurityContext* ctx = NULL) { return NS_OK; }


    NS_METHOD GetVersion(/*[out]*/ jint* version) { return NS_OK; }

    NS_METHOD DefineClass(/*[in]*/  const char* name,
                           /*[in]*/  jobject loader,
                           /*[in]*/  const jbyte *buf,
                           /*[in]*/  jsize len,
                           /*[out]*/ jclass* clazz) { return NS_OK; }

    NS_METHOD FindClass(/*[in]*/  const char* name, 
                         /*[out]*/ jclass* clazz) { return NS_OK; }

    NS_METHOD GetSuperclass(/*[in]*/  jclass sub,
                             /*[out]*/ jclass* super) { return NS_OK; }

    NS_METHOD IsAssignableFrom(/*[in]*/  jclass sub,
                                /*[in]*/  jclass super,
                                /*[out]*/ jboolean* result) { return NS_OK; }

    NS_METHOD Throw(/*[in]*/  jthrowable obj,
                     /*[out]*/ jint* result) { return NS_OK; }

    NS_METHOD ThrowNew(/*[in]*/  jclass clazz,
                        /*[in]*/  const char *msg,
                        /*[out]*/ jint* result) { return NS_OK; }

    NS_METHOD ExceptionOccurred(/*[out]*/ jthrowable* result) { return NS_OK; }

    NS_METHOD ExceptionDescribe(void) { return NS_OK; }

    NS_METHOD ExceptionClear(void) { return NS_OK; }

    NS_METHOD FatalError(/*[in]*/ const char* msg) { return NS_OK; }

    NS_METHOD NewGlobalRef(/*[in]*/  jobject lobj, 
                            /*[out]*/ jobject* result) { return NS_OK; }

    NS_METHOD DeleteGlobalRef(/*[in]*/ jobject gref) { return NS_OK; }

    NS_METHOD DeleteLocalRef(/*[in]*/ jobject obj) { return NS_OK; }

    NS_METHOD IsSameObject(/*[in]*/  jobject obj1,
                            /*[in]*/  jobject obj2,
                            /*[out]*/ jboolean* result) { return NS_OK; }

    NS_METHOD AllocObject(/*[in]*/  jclass clazz,
                           /*[out]*/ jobject* result) { return NS_OK; }

    NS_METHOD GetObjectClass(/*[in]*/  jobject obj,
                              /*[out]*/ jclass* result) { return NS_OK; }

    NS_METHOD IsInstanceOf(/*[in]*/  jobject obj,
                            /*[in]*/  jclass clazz,
                            /*[out]*/ jboolean* result) { return NS_OK; }

    NS_METHOD GetMethodID(/*[in]*/  jclass clazz, 
                           /*[in]*/  const char* name,
                           /*[in]*/  const char* sig,
                           /*[out]*/ jmethodID* id) { return NS_OK; }

    NS_METHOD GetFieldID(/*[in]*/  jclass clazz, 
                          /*[in]*/  const char* name,
                          /*[in]*/  const char* sig,
                          /*[out]*/ jfieldID* id) { return NS_OK; }

    NS_METHOD GetStaticMethodID(/*[in]*/  jclass clazz, 
                                 /*[in]*/  const char* name,
                                 /*[in]*/  const char* sig,
                                 /*[out]*/ jmethodID* id) { return NS_OK; }

    NS_METHOD GetStaticFieldID(/*[in]*/  jclass clazz, 
                                /*[in]*/  const char* name,
                                /*[in]*/  const char* sig,
                                /*[out]*/ jfieldID* id) { return NS_OK; }

    NS_METHOD NewString(/*[in]*/  const jchar* unicode,
                         /*[in]*/  jsize len,
                         /*[out]*/ jstring* result) { return NS_OK; }

    NS_METHOD GetStringLength(/*[in]*/  jstring str,
                               /*[out]*/ jsize* result) { return NS_OK; }
    
    NS_METHOD GetStringChars(/*[in]*/  jstring str,
                              /*[in]*/  jboolean *isCopy,
                              /*[out]*/ const jchar** result) { return NS_OK; }

    NS_METHOD ReleaseStringChars(/*[in]*/  jstring str,
                                  /*[in]*/  const jchar *chars) { return NS_OK; }

    NS_METHOD NewStringUTF(/*[in]*/  const char *utf,
                            /*[out]*/ jstring* result) { return NS_OK; }

    NS_METHOD GetStringUTFLength(/*[in]*/  jstring str,
                                  /*[out]*/ jsize* result) { return NS_OK; }
    
    NS_METHOD GetStringUTFChars(/*[in]*/  jstring str,
                                 /*[in]*/  jboolean *isCopy,
                                 /*[out]*/ const char** result) { return NS_OK; }

    NS_METHOD ReleaseStringUTFChars(/*[in]*/  jstring str,
                                     /*[in]*/  const char *chars) { return NS_OK; }

    NS_METHOD GetArrayLength(/*[in]*/  jarray array,
                              /*[out]*/ jsize* result) { return NS_OK; }

    NS_METHOD NewObjectArray(/*[in]*/  jsize len,
    					/*[in]*/  jclass clazz,
                        /*[in]*/  jobject init,
                        /*[out]*/ jobjectArray* result) { return NS_OK; }

    NS_METHOD GetObjectArrayElement(/*[in]*/  jobjectArray array,
                                     /*[in]*/  jsize index,
                                     /*[out]*/ jobject* result) { return NS_OK; }

    NS_METHOD SetObjectArrayElement(/*[in]*/  jobjectArray array,
                                     /*[in]*/  jsize index,
                                     /*[in]*/  jobject val) { return NS_OK; }

    NS_METHOD NewArray(/*[in]*/ jni_type element_type,
                        /*[in]*/  jsize len,
                        /*[out]*/ jarray* result) { return NS_OK; }

    NS_METHOD GetArrayElements(/*[in]*/  jni_type type,
                                /*[in]*/  jarray array,
                                /*[in]*/  jboolean *isCopy,
                                /*[out]*/ void* result) { return NS_OK; }

    NS_METHOD ReleaseArrayElements(/*[in]*/ jni_type type,
                                    /*[in]*/ jarray array,
                                    /*[in]*/ void *elems,
                                    /*[in]*/ jint mode) { return NS_OK; }


    NS_METHOD GetArrayRegion(/*[in]*/  jni_type type,
                              /*[in]*/  jarray array,
                              /*[in]*/  jsize start,
                              /*[in]*/  jsize len,
                              /*[out]*/ void* buf) { return NS_OK; }

    NS_METHOD SetArrayRegion(/*[in]*/  jni_type type,
                              /*[in]*/  jarray array,
                              /*[in]*/  jsize start,
                              /*[in]*/  jsize len,
                              /*[in]*/  void* buf) { return NS_OK; }

    NS_METHOD RegisterNatives(/*[in]*/  jclass clazz,
                               /*[in]*/  const JNINativeMethod *methods,
                               /*[in]*/  jint nMethods,
                               /*[out]*/ jint* result) { return NS_OK; }

    NS_METHOD UnregisterNatives(/*[in]*/  jclass clazz,
                                 /*[out]*/ jint* result) { return NS_OK; }

    NS_METHOD MonitorEnter(/*[in]*/  jobject obj,
                            /*[out]*/ jint* result) { return NS_OK; }

    NS_METHOD MonitorExit(/*[in]*/  jobject obj,
                           /*[out]*/ jint* result) { return NS_OK; }

    NS_METHOD GetJavaVM(/*[in]*/  JavaVM **vm,
                         /*[out]*/ jint* result) { return NS_OK; }

};

