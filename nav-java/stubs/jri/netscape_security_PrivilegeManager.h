/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#include "jri.h"

#ifndef _netscape_security_PrivilegeManager_H_
#define _netscape_security_PrivilegeManager_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct netscape_security_Principal;
struct netscape_security_Target;
struct netscape_security_PrivilegeManager;
struct java_lang_Class;

/*******************************************************************************
 * Class netscape/security/PrivilegeManager
 ******************************************************************************/

typedef struct netscape_security_PrivilegeManager netscape_security_PrivilegeManager;

struct netscape_security_PrivilegeManager {
  void *ptr;
};

/*******************************************************************************
 * Public Methods
 ******************************************************************************/

JRI_PUBLIC_API(jref)
native_netscape_security_PrivilegeManager_getClassPrincipalsFromStackUnsafe(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jint a);

JRI_PUBLIC_API(jbool)
netscape_security_PrivilegeManager_canExtendTrust(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jobjectArray a, jobjectArray b) ;

JRI_PUBLIC_API(jint)
netscape_security_PrivilegeManager_comparePrincipalArray(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jobjectArray a, jobjectArray b);

JRI_PUBLIC_API(jref)
netscape_security_PrivilegeManager_getClassPrincipalsFromStack(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jint a);

JRI_PUBLIC_API(struct netscape_security_PrivilegeManager *)
netscape_security_PrivilegeManager_getPrivilegeManager(JRIEnv* env, struct java_lang_Class* clazz);

JRI_PUBLIC_API(jref)
netscape_security_PrivilegeManager_intersectPrincipalArray(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jobjectArray a, jobjectArray b);

JRI_PUBLIC_API(jbool)
netscape_security_PrivilegeManager_isPrivilegeEnabled(JRIEnv* env, struct netscape_security_PrivilegeManager* self, struct netscape_security_Target *a, jint b);

JRI_PUBLIC_API(void)
netscape_security_PrivilegeManager_registerPrincipal(JRIEnv* env, struct netscape_security_PrivilegeManager* self, struct netscape_security_Principal *a);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


#endif /* Class netscape/security/PrivilegeManager */
