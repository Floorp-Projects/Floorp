/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsISecureJNI_h___
#define nsISecureJNI_h___

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsISecurityContext.h"
#include "jni.h"

class nsISecureJNI : public nsISupports {
public:

    /**
     * Create new Java object in LiveConnect.
     *
     * @param env        -- JNIEnv pointer.
     * @param clazz      -- Java Class object.
     * @param name       -- constructor name.
     * @param sig        -- constructor signature.
     * @param args       -- arguments for invoking the constructor.
     * @param ctx        -- security context 
     * @param result     -- return new Java object.
     */
    NS_IMETHOD NewObject(/*[in]*/  JNIEnv *env, 
                         /*[in]*/  jclass clazz, 
                         /*[in]*/  const char *name, 
                         /*[in]*/  const char *sig, 
                         /*[in]*/  jvalue *args, 
                         /*[in]*/  nsISecurityContext* ctx,
                         /*[out]*/ jvalue* result) = 0;
   
    /**
     * Invoke method on Java object in LiveConnect.
     *
     * @param env        -- JNIEnv pointer.
     * @param obj        -- Java object.
     * @param name       -- method name.
     * @param sig        -- method signature.
     * @param args       -- arguments for invoking the constructor.
     * @param ctx        -- security context 
     * @param result     -- return result of invocation.
     */
    NS_IMETHOD CallMethod(/*[in]*/  JNIEnv *env, 
                          /*[in]*/  jobject obj, 
                          /*[in]*/  const char *name, 
                          /*[in]*/  const char *sig, 
                          /*[in]*/  jvalue *args, 
                          /*[in]*/  nsISecurityContext* ctx,
                          /*[out]*/ jvalue* result) = 0;

    /**
     * Invoke non-virtual method on Java object in LiveConnect.
     *
     * @param env        -- JNIEnv pointer.
     * @param obj        -- Java object.
     * @param name       -- method name.
     * @param sig        -- method signature.
     * @param args       -- arguments for invoking the constructor.
     * @param ctx        -- security context 
     * @param result     -- return result of invocation.
     */
    NS_IMETHOD CallNonvirtualMethod(/*[in]*/  JNIEnv *env, 
                                    /*[in]*/  jobject obj, 
                                    /*[in]*/  jclass clazz,
                                    /*[in]*/  const char *name, 
                                    /*[in]*/  const char *sig, 
                                    /*[in]*/  jvalue *args, 
                                    /*[in]*/  nsISecurityContext* ctx,
                                    /*[out]*/ jvalue* result) = 0;

    /**
     * Get a field on Java object in LiveConnect.
     *
     * @param env        -- JNIEnv pointer.
     * @param obj        -- Java object.
     * @param name       -- field name.
     * @param sig        -- field signature
     * @param ctx        -- security context 
     * @param result     -- return field value
     */
    NS_IMETHOD GetField(/*[in]*/  JNIEnv *env, 
                        /*[in]*/  jobject obj, 
                        /*[in]*/  const char *name, 
                        /*[in]*/  const char *sig, 
                        /*[in]*/  nsISecurityContext* ctx,
                        /*[out]*/ jvalue* result) = 0;

    /**
     * Set a field on Java object in LiveConnect.
     *
     * @param env        -- JNIEnv pointer.
     * @param obj        -- Java object.
     * @param name       -- field name.
     * @param sig        -- field signature
     * @param result     -- field value to set
     * @param ctx        -- security context 
     */
    NS_IMETHOD SetField(/*[in]*/ JNIEnv *env, 
                        /*[in]*/ jobject obj, 
                        /*[in]*/ const char *name, 
                        /*[in]*/ const char *sig, 
                        /*[in]*/ jvalue val,
                        /*[in]*/ nsISecurityContext* ctx) = 0;

    /**
     * Invoke static method on Java object in LiveConnect.
     *
     * @param env        -- JNIEnv pointer.
     * @param obj        -- Java object.
     * @param name       -- method name.
     * @param sig        -- method signature.
     * @param args       -- arguments for invoking the constructor.
     * @param ctx        -- security context 
     * @param result     -- return result of invocation.
     */
    NS_IMETHOD CallStaticMethod(/*[in]*/  JNIEnv *env, 
                                /*[in]*/  jclass clazz,
                                /*[in]*/  const char *name, 
                                /*[in]*/  const char *sig, 
                                /*[in]*/  jvalue *args, 
                                /*[in]*/  nsISecurityContext* ctx,
                                /*[out]*/ jvalue* result) = 0;

    /**
     * Get a static field on Java object in LiveConnect.
     *
     * @param env        -- JNIEnv pointer.
     * @param obj        -- Java object.
     * @param name       -- field name.
     * @param sig        -- field signature
     * @param ctx        -- security context 
     * @param result     -- return field value
     */
    NS_IMETHOD GetStaticField(/*[in]*/  JNIEnv *env, 
                              /*[in]*/  jclass clazz, 
                              /*[in]*/  const char *name, 
                              /*[in]*/  const char *sig, 
                              /*[in]*/  nsISecurityContext* ctx,
                              /*[out]*/ jvalue* result) = 0;


    /**
     * Set a static field on Java object in LiveConnect.
     *
     * @param env        -- JNIEnv pointer.
     * @param obj        -- Java object.
     * @param name       -- field name.
     * @param sig        -- field signature
     * @param result     -- field value to set
     * @param ctx        -- security context 
     */
    NS_IMETHOD SetStaticField(/*[in]*/ JNIEnv *env, 
                              /*[in]*/ jclass clazz, 
                              /*[in]*/ const char *name, 
                              /*[in]*/ const char *sig, 
                              /*[in]*/ jvalue val,
                              /*[in]*/ nsISecurityContext* ctx) = 0;
};

#define NS_ISECUREJNI_IID                         \
{ /* {7C968050-4C4B-11d2-A1CB-00805F8F694D} */         \
    0x7c968050,                                        \
    0x4c4b,                                            \
    0x11d2,                                            \
    { 0xa1, 0xcb, 0x0, 0x80, 0x5f, 0x8f, 0x69, 0x4d }  \
};

#endif // nsISecureJNI_h___
