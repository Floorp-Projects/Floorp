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
#ifndef _OOBJ_H_
#define _OOBJ_H_

#include <stddef.h>

#include "typedefs.h"
#include "bool.h"
#include "jriext.h"

typedef void Hjava_lang_Class;
typedef Hjava_lang_Class ClassClass;

typedef void Hjava_lang_Object;
typedef Hjava_lang_Object HObject;
typedef Hjava_lang_Object JHandle;

struct methodblock;

JRI_PUBLIC_API(void)
MakeClassSticky(ClassClass *cb);

#endif /* !_OOBJ_H_ */
