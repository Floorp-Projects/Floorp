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
