/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef _INTERPRETER_H_
#define _INTERPRETER_H_

#include "prtypes.h"

PR_BEGIN_EXTERN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bool.h"
#include "jni.h"

typedef struct execenv ExecEnv;

typedef void JavaStack;

typedef void JavaFrame; 

struct Hjava_lang_ClassLoader;

JRI_PUBLIC_API(JHandle *)
ArrayAlloc(int32_t, int32_t);

JRI_PUBLIC_API(JavaFrame *)
CompiledFramePrev(JavaFrame *, JavaFrame *);

JRI_PUBLIC_API(JavaStack *)
CreateNewJavaStack(ExecEnv *, JavaStack *);

JRI_PUBLIC_API(bool_t)
ExecuteJava(unsigned char *, ExecEnv *);

JRI_PUBLIC_API(ClassClass *)
FindClassFromClass(struct execenv *, char *, bool_t, ClassClass *);

JRI_PUBLIC_API(ClassClass *)
FindLoadedClass(char *, struct Hjava_lang_ClassLoader *);

JRI_PUBLIC_API(void)
PrintToConsole(const char *);

JRI_PUBLIC_API(bool_t)
VerifyClassAccess(ClassClass *, ClassClass *, bool_t);

JRI_PUBLIC_API(bool_t)
VerifyFieldAccess(ClassClass *, ClassClass *, int, bool_t);

JRI_PUBLIC_API(long)
do_execute_java_method(ExecEnv *, void *, char *, char *,
                       struct methodblock *, bool_t, ...);

JRI_PUBLIC_API(long)
do_execute_java_method_vararg(ExecEnv *, void *, char *, char *,
                              struct methodblock *, bool_t, va_list,
                              long *, bool_t);

JRI_PUBLIC_API(HObject *)
execute_java_constructor_vararg(struct execenv *, char *, ClassClass *,
                                char *, va_list);

JRI_PUBLIC_API(HObject *)
execute_java_constructor(ExecEnv *, char *, ClassClass *, char *, ...);

JRI_PUBLIC_API(bool_t)
is_subclass_of(ClassClass *, ClassClass *, ExecEnv *);

JRI_PUBLIC_API(HObject*)
newobject(ClassClass *, unsigned char *, struct execenv *);

JRI_PUBLIC_API(int32_t)
sizearray(int32_t, int32_t);

PR_END_EXTERN_C

#endif /* ! _INTERPRETER_H_ */
