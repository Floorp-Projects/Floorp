/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

#ifndef prenv_h___
#define prenv_h___

#include "prtypes.h"

/*******************************************************************************/
/*******************************************************************************/
/****************** THESE FUNCTIONS MAY NOT BE THREAD SAFE *********************/
/*******************************************************************************/
/*******************************************************************************/

PR_BEGIN_EXTERN_C

/*
** Lookup a variable in the environment. Return NULL if it's not found,
** otherwise return it's value.
*/
PR_EXTERN(char*) PR_GetEnv(const char *var);

PR_END_EXTERN_C

#endif /* prenv_h___ */
