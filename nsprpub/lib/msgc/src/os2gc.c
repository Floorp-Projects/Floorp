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
 * GC related routines
 *
 */
#include "prlog.h"

#include <stdlib.h>

/* Leave a bit of room for any malloc header bytes... */
#define MAX_SEGMENT_SIZE    (65536L - 4096L)

/************************************************************************/
/*
** Machine dependent GC Heap management routines:
**    _MD_GrowGCHeap
*/
/************************************************************************/
void _MD_InitGC() {}

void *_MD_GrowGCHeap(PRUint32 *sizep)
{
    void *addr;

    if ( *sizep > MAX_SEGMENT_SIZE )
    {
        *sizep = MAX_SEGMENT_SIZE;
    }

    addr = malloc((size_t)*sizep);
    return addr;
}


PRBool _MD_ExtendGCHeap(char *base, PRInt32 oldSize, PRInt32 newSize) {
  /* Not sure about this.  Todd?  */
  return PR_FALSE;
}


void _MD_FreeGCSegment(void *base, PRInt32 len)
{
   if (base)
   {
       free(base);
   }
}
