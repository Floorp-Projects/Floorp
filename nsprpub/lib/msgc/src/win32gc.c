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
#include <windows.h>
#include "prlog.h"

extern PRLogModuleInfo* _pr_msgc_lm;

#define GC_VMBASE               0x40000000
#define GC_VMLIMIT              0x00FFFFFF

/************************************************************************/
/*
** Machine dependent GC Heap management routines:
**    _MD_GrowGCHeap
*/
/************************************************************************/

void *baseaddr = (void*) GC_VMBASE;
void *lastaddr = (void*) GC_VMBASE;

void _MD_InitGC() {}

void *_MD_GrowGCHeap(PRUint32 *sizep)
{
    void *addr;
    size_t size;

    /* Reserve a block of memory for the GC */
    if( lastaddr == baseaddr ) {
        addr = VirtualAlloc( (void *)GC_VMBASE, GC_VMLIMIT, MEM_RESERVE, PAGE_READWRITE );

        /* 
        ** If the GC_VMBASE address is already mapped, then let the OS choose a 
        ** base address that is available...
        */
        if (addr == NULL) {
            addr = VirtualAlloc( NULL, GC_VMLIMIT, MEM_RESERVE, PAGE_READWRITE );

            baseaddr = lastaddr = addr;
            if (addr == NULL) {
                PR_LOG(_pr_msgc_lm, PR_LOG_ALWAYS, ("GC: unable to allocate heap: LastError=%ld",
                       GetLastError()));
                return 0;
            }
        }
    }
    size = *sizep;

    /* Extend the mapping */
    addr = VirtualAlloc( lastaddr, size, MEM_COMMIT, PAGE_READWRITE );
    if (addr == NULL) {
        return 0;
    }

    lastaddr = ((char*)addr + size);
    PR_LOG(_pr_msgc_lm, PR_LOG_ALWAYS,
	   ("GC: heap extends from %08x to %08x",
	    baseaddr, (long)baseaddr + (char*)lastaddr - (char*)baseaddr));

    return addr;
}

PRBool _MD_ExtendGCHeap(char *base, PRInt32 oldSize, PRInt32 newSize) {
  void* addr;

  addr = VirtualAlloc( base + oldSize, newSize - oldSize,
		       MEM_COMMIT, PAGE_READWRITE );
  if (NULL == addr) {
    PR_LOG(_pr_msgc_lm, PR_LOG_ALWAYS, ("GC: unable to extend heap: LastError=%ld",
		     GetLastError()));
    return PR_FALSE;
  }
  if (base + oldSize != (char*)addr) {
    PR_LOG(_pr_msgc_lm, PR_LOG_ALWAYS, ("GC: segment extension returned %x instead of %x",
		     addr, base + oldSize));
    VirtualFree(addr, newSize - oldSize, MEM_DECOMMIT);
    return PR_FALSE;
  }
  lastaddr = base + newSize;
  PR_LOG(_pr_msgc_lm, PR_LOG_ALWAYS,
	 ("GC: heap now extends from %p to %p",
	  base, base + newSize));
  return PR_TRUE;
}


void _MD_FreeGCSegment(void *base, PRInt32 len)
{
     (void)VirtualFree(base, 0, MEM_RELEASE);
}
