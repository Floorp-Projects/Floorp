/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef jvmmgr_h___
#define jvmmgr_h___

#include "prtypes.h"
#include "jni.h"
#include "jsdbgapi.h"
#include "nsError.h"

#include "nsISecurityContext.h"

struct nsJVMManager;

typedef enum nsJVMStatus {
    nsJVMStatus_Enabled,  /* but not Running */
    nsJVMStatus_Disabled, /* explicitly disabled */
    nsJVMStatus_Running,  /* enabled and started */
    nsJVMStatus_Failed    /* enabled but failed to start */
} nsJVMStatus;


/*******************************************************************************
 * Interface for C Clients
 ******************************************************************************/

PR_BEGIN_EXTERN_C

PR_EXTERN(void)
JVM_ReleaseJVMMgr(struct nsJVMManager* mgr);

PR_EXTERN(nsJVMStatus)
JVM_StartupJVM(void);

PR_EXTERN(nsJVMStatus)
JVM_ShutdownJVM(void);

PR_EXTERN(nsJVMStatus)
JVM_GetJVMStatus(void);

PR_EXTERN(PRBool)
JVM_AddToClassPath(const char* dirPath);

PR_EXTERN(void)
JVM_ShowConsole(void);

PR_EXTERN(void)
JVM_HideConsole(void);

PR_EXTERN(PRBool)
JVM_IsConsoleVisible(void);

PR_EXTERN(void)
JVM_PrintToConsole(const char* msg);

PR_EXTERN(void)
JVM_ShowPrefsWindow(void);

PR_EXTERN(void)
JVM_HidePrefsWindow(void);

PR_EXTERN(PRBool)
JVM_IsPrefsWindowVisible(void);

PR_EXTERN(void)
JVM_StartDebugger(void);

PR_EXTERN(JNIEnv*)
JVM_GetJNIEnv(void);

PR_IMPLEMENT(void)
JVM_ReleaseJNIEnv(JNIEnv *pJNIEnv);

PR_EXTERN(nsresult)
JVM_SpendTime(PRUint32 timeMillis);

PR_EXTERN(PRBool)
JVM_MaybeStartupLiveConnect(void);

PR_EXTERN(PRBool)
JVM_MaybeShutdownLiveConnect(void);

PR_EXTERN(PRBool)
JVM_IsLiveConnectEnabled(void);

PR_EXTERN(nsJVMStatus)
JVM_ShutdownJVM(void);

PR_EXTERN(PRBool)
JVM_NSISecurityContextImplies(JSStackFrame  *pCurrentFrame, const char* target, const char* action);

PR_EXTERN(JSPrincipals*)
JVM_GetJavaPrincipalsFromStack(JSStackFrame  *pCurrentFrame);

PR_EXTERN(void *)
JVM_GetJavaPrincipalsFromStackAsNSVector(JSStackFrame  *pCurrentFrame);

PR_EXTERN(JSStackFrame**)
JVM_GetStartJSFrameFromParallelStack(void);

PR_EXTERN(JSStackFrame*)
JVM_GetEndJSFrameFromParallelStack(JSStackFrame  *pCurrentFrame);

PR_EXTERN(nsISecurityContext*) 
JVM_GetJSSecurityContext();

PR_END_EXTERN_C

#endif /* jvmmgr_h___ */
