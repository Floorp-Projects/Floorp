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

