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

/*
** File:   ptmisc.c
** Descritpion:  Implemenation of miscellaneous methods for pthreads
*/

#if defined(_PR_PTHREADS)

#include "primpl.h"

#include <stdio.h>

#define PT_LOG(f)

void _PR_InitCPUs(void) {PT_LOG("_PR_InitCPUs")}
void _PR_InitStacks(void) {PT_LOG("_PR_InitStacks")}

PR_IMPLEMENT(void) PR_SetConcurrency(PRUintn numCPUs) 
    {PT_LOG("PR_SetConcurrency")}

PR_IMPLEMENT(void) PR_SetThreadRecycleMode(PRUint32 flag)
    {PT_LOG("PR_SetThreadRecycleMode")}

#endif /* defined(_PR_PTHREADS) */

/* ptmisc.c */
