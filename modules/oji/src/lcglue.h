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

#ifndef lcglue_h___
#define lcglue_h___

#include "prtypes.h"
#include "jni.h"
#include "jsdbgapi.h"
#include "nsError.h"

#include "nsIThreadManager.h"
#include "nsISecurityContext.h"

struct JVMSecurityStack {
  void        **pNSIPrincipaArray;
  int           numPrincipals;
  void         *pNSISecurityContext;
  JSStackFrame *pJavaToJSFrame;
  JSStackFrame *pJSToJavaFrame;
  JVMSecurityStack *next;
  JVMSecurityStack *prev;
};
typedef struct JVMSecurityStack JVMSecurityStack;

/**
 * JVMContext is maintained as thread local storage. The current thread's
 * context is accessed by calling GetJVMContext().
 */
struct JVMContext {
	JNIEnv					*proxyEnv;					/* thread local proxy JNI */
	JVMSecurityStack		*securityStack;				/* thread local security stack. */
	JSJavaThreadState		*jsj_env;					/* thread local JavaScript execution env. */
	JSContext				*js_context;				/* thread local JavaScript context. */
	JSStackFrame			*js_startframe;				/* thread local JavaScript stack frame. */
    void                    *java_applet_obj;
};

JVMContext* GetJVMContext();
void JVM_InitLCGlue(void);      // in lcglue.cpp
extern "C" void* 
ConvertNSIPrincipalToNSPrincipalArray(JNIEnv *pJNIEnv, JSContext *pJSContext, void  **ppNSIPrincipalArrayIN, int numPrincipals, void *pNSISecurityContext);

#endif /* lcglue_h___ */
