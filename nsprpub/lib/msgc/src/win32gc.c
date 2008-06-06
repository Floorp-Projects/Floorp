/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
