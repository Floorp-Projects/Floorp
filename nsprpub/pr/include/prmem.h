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
** File: prmem.h
** Description: API to NSPR 2.0 memory management functions
**
*/
#ifndef prmem_h___
#define prmem_h___

#include "prtypes.h"
#include <stddef.h>
#include <stdlib.h>

PR_BEGIN_EXTERN_C

/*
** Thread safe memory allocation.
**
** NOTE: pr wraps up malloc, free, calloc, realloc so they are already
** thread safe (and are not declared here - look in stdlib.h).
*/

/*
** PR_Malloc, PR_Calloc, PR_Realloc, and PR_Free have the same signatures
** as their libc equivalent malloc, calloc, realloc, and free, and have
** the same semantics.  (Note that the argument type size_t is replaced
** by PRUint32.)  Memory allocated by PR_Malloc, PR_Calloc, or PR_Realloc
** must be freed by PR_Free.
*/

PR_EXTERN(void *) PR_Malloc(PRUint32 size);

PR_EXTERN(void *) PR_Calloc(PRUint32 nelem, PRUint32 elsize);

PR_EXTERN(void *) PR_Realloc(void *ptr, PRUint32 size);

PR_EXTERN(void) PR_Free(void *ptr);

/*
** The following are some convenience macros defined in terms of
** PR_Malloc, PR_Calloc, PR_Realloc, and PR_Free.
*/

/***********************************************************************
** FUNCTION:	PR_MALLOC()
** DESCRIPTION:
**   PR_NEW() allocates an untyped item of size _size from the heap.
** INPUTS:  _size: size in bytes of item to be allocated
** OUTPUTS:	untyped pointer to the node allocated
** RETURN:	pointer to node or error returned from malloc().
***********************************************************************/
#define PR_MALLOC(_bytes) (PR_Malloc((_bytes)))

/***********************************************************************
** FUNCTION:	PR_NEW()
** DESCRIPTION:
**   PR_NEW() allocates an item of type _struct from the heap.
** INPUTS:  _struct: a data type
** OUTPUTS:	pointer to _struct
** RETURN:	pointer to _struct or error returns from malloc().
***********************************************************************/
#define PR_NEW(_struct) ((_struct *) PR_MALLOC(sizeof(_struct)))

/***********************************************************************
** FUNCTION:	PR_REALLOC()
** DESCRIPTION:
**   PR_REALLOC() re-allocates _ptr bytes from the heap as a _size
**   untyped item.
** INPUTS:	_ptr: pointer to node to reallocate
**          _size: size of node to allocate
** OUTPUTS:	pointer to node allocated
** RETURN:	pointer to node allocated
***********************************************************************/
#define PR_REALLOC(_ptr, _size) (PR_Realloc((_ptr), (_size)))

/***********************************************************************
** FUNCTION:	PR_CALLOC()
** DESCRIPTION:
**   PR_CALLOC() allocates a _size bytes untyped item from the heap
**   and sets the allocated memory to all 0x00.
** INPUTS:	_size: size of node to allocate
** OUTPUTS:	pointer to node allocated
** RETURN:	pointer to node allocated
***********************************************************************/
#define PR_CALLOC(_size) (PR_Calloc(1, (_size)))

/***********************************************************************
** FUNCTION:	PR_NEWZAP()
** DESCRIPTION:
**   PR_NEWZAP() allocates an item of type _struct from the heap
**   and sets the allocated memory to all 0x00.
** INPUTS:	_struct: a data type
** OUTPUTS:	pointer to _struct
** RETURN:	pointer to _struct
***********************************************************************/
#define PR_NEWZAP(_struct) ((_struct*)PR_Calloc(1, sizeof(_struct)))

/***********************************************************************
** FUNCTION:	PR_DELETE()
** DESCRIPTION:
**   PR_DELETE() unallocates an object previosly allocated via PR_NEW()
**   or PR_NEWZAP() to the heap.
** INPUTS:	pointer to previously allocated object
** OUTPUTS:	the referenced object is returned to the heap
** RETURN:	void
***********************************************************************/
#define PR_DELETE(_ptr) { PR_Free(_ptr); (_ptr) = NULL; }

/***********************************************************************
** FUNCTION:	PR_FREEIF()
** DESCRIPTION:
**   PR_FREEIF() conditionally unallocates an object previously allocated
**   vial PR_NEW() or PR_NEWZAP(). If the pointer to the object is
**   equal to zero (0), the object is not released.
** INPUTS:	pointer to previously allocated object
** OUTPUTS:	the referenced object is conditionally returned to the heap
** RETURN:	void
***********************************************************************/
#define PR_FREEIF(_ptr)	if (_ptr) PR_DELETE(_ptr)

PR_END_EXTERN_C

#endif /* prmem_h___ */
