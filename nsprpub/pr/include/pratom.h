/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/* GLOBAL FUNCTIONS:
** DESCRIPTION:
**     PR Atomic operations
*/

#ifndef pratom_h___
#define pratom_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

/*
** FUNCTION: PR_AtomicIncrement
** DESCRIPTION:
**    Atomically increment a 32 bit value.
** INPUTS:
**    val:  a pointer to the value to increment
** RETURN:
**    the returned value has the same sign as the result
*/
PR_EXTERN(PRInt32)	PR_AtomicIncrement(PRInt32 *val);

/*
** FUNCTION: PR_AtomicDecrement
** DESCRIPTION:
**    Atomically decrement a 32 bit value.
** INPUTS:
**    val:  a pointer to the value to decrement
** RETURN:
**    the returned value has the same sign as the result
*/
PR_EXTERN(PRInt32)	PR_AtomicDecrement(PRInt32 *val);

/*
** FUNCTION: PR_AtomicSet
** DESCRIPTION:
**    Atomically set a 32 bit value.
** INPUTS:
**    val: A pointer to a 32 bit value to bet set
**    newval: The newvalue to assign to val
** RETURN:
**    Returns the prior value
*/
PR_EXTERN(PRInt32) PR_AtomicSet(PRInt32 *val, PRInt32 newval);

PR_END_EXTERN_C

#endif /* pratom_h___ */
