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

#include "prlock.h"
#include "prlog.h"
#include "prmem.h"
#include "gcint.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define _PR_GC_VMBASE 0x40000000

#if defined(SOLARIS)
#define _MD_MMAP_FLAGS MAP_SHARED
#elif defined(RELIANTUNIX)
#define _MD_MMAP_FLAGS MAP_PRIVATE|MAP_FIXED
#else
#define _MD_MMAP_FLAGS MAP_PRIVATE
#endif

static PRInt32 zero_fd = -1;
static PRLock *zero_fd_lock = NULL;

void _MD_InitGC(void)
{
#ifdef DEBUG
    /*
     * Disable using mmap(2) if NSPR_NO_MMAP is set
     */
    if (getenv("NSPR_NO_MMAP")) {
        zero_fd = -2;
        return;
    }
#endif
    zero_fd = open("/dev/zero",O_RDWR , 0);
    zero_fd_lock = PR_NewLock();
}

/* This static variable is used by _MD_GrowGCHeap and _MD_ExtendGCHeap */
static void *lastaddr = (void*) _PR_GC_VMBASE;

void *_MD_GrowGCHeap(PRUint32 *sizep)
{
	void *addr;
	PRUint32 size;

	size = *sizep;

	PR_Lock(zero_fd_lock);
	if (zero_fd < 0) {
		goto mmap_loses;
	}

	/* Extend the mapping */
	addr = mmap(lastaddr, size, PROT_READ|PROT_WRITE|PROT_EXEC,
	    _MD_MMAP_FLAGS,
	    zero_fd, 0);
	if (addr == (void*)-1) {
		zero_fd = -1;
		goto mmap_loses;
	}
	lastaddr = ((char*)addr + size);
#ifdef DEBUG
	PR_LOG(_pr_msgc_lm, PR_LOG_WARNING,
	    ("GC: heap extends from %08x to %08x\n",
	    _PR_GC_VMBASE,
	    _PR_GC_VMBASE + (char*)lastaddr - (char*)_PR_GC_VMBASE));
#endif
	PR_Unlock(zero_fd_lock);
	return addr;

mmap_loses:
	PR_Unlock(zero_fd_lock);
	return PR_MALLOC(size);
}

/* XXX - This is disabled.  MAP_FIXED just does not work. */
#if 0
PRBool _MD_ExtendGCHeap(char *base, PRInt32 oldSize, PRInt32 newSize) {
	PRBool rv = PR_FALSE;
	void* addr;
	PRInt32 allocSize = newSize - oldSize;

	PR_Lock(zero_fd_lock);
	addr = mmap(base + oldSize, allocSize, PROT_READ|PROT_WRITE|PROT_EXEC,
	    _MD_MMAP_FLAGS | MAP_FIXED, zero_fd, 0);
	if (addr == (void*)-1) {
		goto loser;
	}
	if (addr != (void*) (base + oldSize)) {
		munmap(base + oldSize, allocSize);
		goto loser;
	}
	lastaddr = ((char*)base + newSize);
	PR_LOG(_pr_msgc_lm, PR_LOG_ALWAYS,
	    ("GC: heap now extends from %p to %p",
	    base, base + newSize));
	rv = PR_TRUE;

loser:
	PR_Unlock(zero_fd_lock);
	return rv;
}
#else
PRBool _MD_ExtendGCHeap(char *base, PRInt32 oldSize, PRInt32 newSize) {
	return PR_FALSE;
}
#endif

void _MD_FreeGCSegment(void *base, PRInt32 len)
{
	if (zero_fd < 0) {
		PR_DELETE(base);
	} else {
		(void) munmap(base, len);
	}
}
