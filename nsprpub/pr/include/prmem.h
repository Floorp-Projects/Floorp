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
#define PR_DELETE(_ptr) { PR_Free((void*)_ptr); (_ptr) = NULL; }

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

/***********************************************************************
** Typedef ENUM: PRSegmentAccess
** DESCRIPTION:
**   Defines a number of segment accessor types for PR_Seg* functions
**
***********************************************************************/
typedef struct PRSegment PRSegment;
typedef enum {
    PR_SEGMENT_NONE,
    PR_SEGMENT_RDONLY,
    PR_SEGMENT_RDWR
} PRSegmentAccess;

/***********************************************************************
** FUNCTION:	PR_NewSegment()
** DESCRIPTION:
**   Allocate a memory segment. The "size" value is rounded up to the
**   native system page size and a page aligned portion of memory is
**   returned.  This memory is not part of the malloc heap. If "vaddr" is
**   not NULL then PR tries to allocate the segment at the desired virtual
**   address. Segments are mapped PR_SEGMENT_RDWR when created.
** INPUTS:	size:  size of the desired memory segment
**          vaddr:  address at which the newly aquired segment is to be
**                  mapped into memory.
** OUTPUTS:	a memory segment is allocated, a PRSegment is allocated
** RETURN:	pointer to PRSegment
***********************************************************************/
PR_EXTERN(PRSegment*) PR_NewSegment(PRUint32 size, void *vaddr);

/***********************************************************************
** FUNCTION:	PR_DestroySegment()
** DESCRIPTION:
**   The memory segment and the PRSegment are freed
** INPUTS:	seg:  pointer to PRSegment to be freed
** OUTPUTS:	the the PRSegment and its associated memory segment are freed
** RETURN:	void
***********************************************************************/
PR_EXTERN(void) PR_DestroySegment(PRSegment *seg);

/***********************************************************************
** FUNCTION:	PR_GrowSegment()
** DESCRIPTION:
**   Attempt to grow/shrink a memory segment. If deltaBytes is positive,
**   the segment is grown. If deltaBytes is negative, the segment is
**   shrunk. This returns the number of bytes added to the segment if
**   successful, zero otherwise.
** INPUTS:	seg:  pointer to a PRSegment
** OUTPUTS:	
** RETURN:	PRUint32:   number of bytes added to the memory segment or zero
***********************************************************************/
PR_EXTERN(PRUint32) PR_GrowSegment(PRSegment *seg, PRInt32 deltaBytes);

/***********************************************************************
** FUNCTION:	PR_GetSegmentVaddr()
** DESCRIPTION:
**   PR_Segment member accessor function.
**   Return the virtual address of the memory segment
**
** INPUTS:	seg:  pointer to a PRSegment
** OUTPUTS:	none
** RETURN:	void*: Address where the memory segment is mapped.
***********************************************************************/
PR_EXTERN(void*) PR_GetSegmentVaddr(PRSegment *seg);

/***********************************************************************
** FUNCTION:	PR_GetSegmentSize()
** DESCRIPTION:
**   PR_Segment member accessor function.
**   Return the size of the associated memory segment
** INPUTS:	seg:  pointer to a PRSegment
** OUTPUTS:	none
** RETURN:	size_t:  size of the associated memory segment
***********************************************************************/
PR_EXTERN(size_t) PR_GetSegmentSize(PRSegment *seg);

/***********************************************************************
** FUNCTION:	PR_MapSegment()
** DESCRIPTION:
**  Change the mapping on a segment.
** 	  "how" == PR_SEGMENT_NONE: the segment becomes unmapped
** 	  "how" == PR_SEGMENT_RDONLY: the segment becomes mapped and readable
** 	  "how" == PR_SEGMENT_RDWR: the segment becomes mapped read/write
**
**   Note: If a segment can be read then it is also possible to execute 
**     code in it.
** INPUTS:	seg:  pointer to a PRSegment
**          how:  one of PRSegmentAccess enumerated values
** OUTPUTS:	the access for the associated memory segment is changed
** RETURN:	void
***********************************************************************/
PR_EXTERN(void) PR_MapSegment(PRSegment *seg, PRSegmentAccess how);

/***********************************************************************
** FUNCTION:	PR_GetSegmentAccess()
** DESCRIPTION:
**   PR_Segment member accessor function.
**   Return a memory segment's current access rights
** INPUTS:	seg:  pointer to a PRSegment
** OUTPUTS:	
** RETURN:	PRSegmentAccess: current access rights
***********************************************************************/
PR_EXTERN(PRSegmentAccess) PR_GetSegmentAccess(PRSegment *seg);

PR_END_EXTERN_C

#endif /* prmem_h___ */
