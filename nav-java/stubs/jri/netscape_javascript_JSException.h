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

/* Header for class netscape/javascript/JSException */

#ifndef _netscape_javascript_JSException_H_
#define _netscape_javascript_JSException_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct java_lang_String;
struct java_lang_Class;

/*******************************************************************************
 * Class netscape/javascript/JSException
 ******************************************************************************/

typedef struct netscape_javascript_JSException netscape_javascript_JSException;
struct netscape_javascript_JSException {
  void *ptr;
};

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* Class netscape/javascript/JSException */
