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

#ifndef _netscape_security_Principal_H_
#define _netscape_security_Principal_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct java_lang_String;
struct netscape_security_Zig;
struct java_lang_Class;
struct netscape_security_Principal;

/*******************************************************************************
 * Class netscape/security/Principal
 ******************************************************************************/

typedef struct netscape_security_Principal netscape_security_Principal;

struct netscape_security_Principal {
  void *ptr;
};

/*******************************************************************************
 * Public Methods
 ******************************************************************************/

JRI_PUBLIC_API(struct java_lang_String *)
netscape_security_Principal_getVendor(JRIEnv* env, struct netscape_security_Principal* self);

JRI_PUBLIC_API(jbool)
netscape_security_Principal_isCodebaseExact(JRIEnv* env, struct netscape_security_Principal* self);

JRI_PUBLIC_API(struct netscape_security_Principal*)
netscape_security_Principal_new_3(JRIEnv* env, struct java_lang_Class* clazz, jint a, jbyteArray b);

JRI_PUBLIC_API(struct netscape_security_Principal*)
netscape_security_Principal_new_5(JRIEnv* env, struct java_lang_Class* clazz, jint a, jbyteArray b, struct netscape_security_Zig *c);

JRI_PUBLIC_API(struct java_lang_String *)
netscape_security_Principal_toString(JRIEnv* env, struct netscape_security_Principal* self);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* Class netscape/security/Principal */
