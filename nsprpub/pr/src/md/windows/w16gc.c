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

#include "primpl.h"
#include <sys/timeb.h>

PRWord *
_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np) 
{
    if (isCurrent) 
    {
        _MD_SAVE_CONTEXT(t);
    }
    /*
    ** In Win16 because the premption is "cooperative" it can never be the 
    ** case that a register holds the sole reference to an object.  It
    ** will always have been pushed onto the stack before the thread
    ** switch...  So don't bother to scan the registers...
    */
    *np = 0;

    return (PRWord *) CONTEXT(t);
}

#if 0
#ifndef SPORT_MODEL

#define MAX_SEGMENT_SIZE (65536l - 4096l)

/************************************************************************/
/*
** Machine dependent GC Heap management routines:
**    _MD_GrowGCHeap
*/
/************************************************************************/

extern void *
_MD_GrowGCHeap(uint32 *sizep)
{
    void *addr;

    if( *sizep > MAX_SEGMENT_SIZE ) {
        *sizep = MAX_SEGMENT_SIZE;
    }

    addr = malloc((size_t)*sizep);
    return addr;
}

#endif /* SPORT_MODEL */
#endif /* 0 */

