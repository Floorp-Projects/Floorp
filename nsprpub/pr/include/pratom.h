/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* GLOBAL FUNCTIONS:
** DESCRIPTION:
**     PR Atomic operations
*/

#ifndef pratom_h___
#define pratom_h___

#include "prtypes.h"
#include "prlock.h"

PR_BEGIN_EXTERN_C

/*
** FUNCTION: PR_AtomicIncrement
** DESCRIPTION:
**    Atomically increment a 32 bit value.
** INPUTS:
**    val:  a pointer to the value to increment
** RETURN:
**    the returned value is the result of the increment
*/
NSPR_API(PRInt32)	PR_AtomicIncrement(PRInt32 *val);

/*
** FUNCTION: PR_AtomicDecrement
** DESCRIPTION:
**    Atomically decrement a 32 bit value.
** INPUTS:
**    val:  a pointer to the value to decrement
** RETURN:
**    the returned value is the result of the decrement
*/
NSPR_API(PRInt32)	PR_AtomicDecrement(PRInt32 *val);

/*
** FUNCTION: PR_AtomicSet
** DESCRIPTION:
**    Atomically set a 32 bit value.
** INPUTS:
**    val: A pointer to a 32 bit value to be set
**    newval: The newvalue to assign to val
** RETURN:
**    Returns the prior value
*/
NSPR_API(PRInt32) PR_AtomicSet(PRInt32 *val, PRInt32 newval);

/*
** FUNCTION: PR_AtomicAdd
** DESCRIPTION:
**    Atomically add a 32 bit value.
** INPUTS:
**    ptr:  a pointer to the value to increment
**	  val:  value to be added
** RETURN:
**    the returned value is the result of the addition
*/
NSPR_API(PRInt32)	PR_AtomicAdd(PRInt32 *ptr, PRInt32 val);

/*
** LIFO linked-list (stack)
*/
typedef struct PRStackElemStr PRStackElem;

struct PRStackElemStr {
    PRStackElem	*prstk_elem_next;	/* next pointer MUST be at offset 0;
									  assembly language code relies on this */
};

typedef struct PRStackStr PRStack;

/*
** FUNCTION: PR_CreateStack
** DESCRIPTION:
**    Create a stack, a LIFO linked list
** INPUTS:
**    stack_name:  a pointer to string containing the name of the stack
** RETURN:
**    A pointer to the created stack, if successful, else NULL.
*/
NSPR_API(PRStack *)	PR_CreateStack(const char *stack_name);

/*
** FUNCTION: PR_StackPush
** DESCRIPTION:
**    Push an element on the top of the stack
** INPUTS:
**    stack:		pointer to the stack
**    stack_elem:	pointer to the stack element
** RETURN:
**    None
*/
NSPR_API(void)			PR_StackPush(PRStack *stack, PRStackElem *stack_elem);

/*
** FUNCTION: PR_StackPop
** DESCRIPTION:
**    Remove the element on the top of the stack
** INPUTS:
**    stack:		pointer to the stack
** RETURN:
**    A pointer to the stack element removed from the top of the stack,
**	  if non-empty,
**    else NULL
*/
NSPR_API(PRStackElem *)	PR_StackPop(PRStack *stack);

/*
** FUNCTION: PR_DestroyStack
** DESCRIPTION:
**    Destroy the stack
** INPUTS:
**    stack:		pointer to the stack
** RETURN:
**    PR_SUCCESS - if successfully deleted
**	  PR_FAILURE - if the stack is not empty
**					PR_GetError will return
**						PR_INVALID_STATE_ERROR - stack is not empty
*/
NSPR_API(PRStatus)		PR_DestroyStack(PRStack *stack);

PR_END_EXTERN_C

#endif /* pratom_h___ */
