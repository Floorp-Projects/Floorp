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

#include "seccomon.h"
#include "prmem.h"
#include "prerror.h"
#include "plarena.h"
#include "prlog.h"

void *
PORT_Alloc(size_t bytes)
{
    void *rv;

    /* Always allocate a non-zero amount of bytes */
    rv = (void *)PR_Malloc(bytes ? bytes : 1);
    return rv;
}

void *
PORT_Realloc(void *oldptr, size_t bytes)
{
    void *rv;

    rv = (void *)PR_Realloc(oldptr, bytes);
    return rv;
}

void *
PORT_ZAlloc(size_t bytes)
{
    void *rv;

    /* Always allocate a non-zero amount of bytes */
    rv = (void *)PR_Calloc(1, bytes ? bytes : 1);
    return rv;
}

void
PORT_Free(void *ptr)
{
    if (ptr) {
	PR_Free(ptr);
    }
}

void
PORT_ZFree(void *ptr, size_t len)
{
    if (ptr) {
	memset(ptr, 0, len);
	PR_Free(ptr);
    }
}

PRArenaPool *
PORT_NewArena(unsigned long chunksize)
{
    PRArenaPool *arena;
    
    arena = PORT_ZAlloc(sizeof(PRArenaPool));
    if ( arena != NULL ) {
	PR_InitArenaPool(arena, "security", chunksize, sizeof(double));
    }

    return(arena);
}

void *
PORT_ArenaAlloc(PRArenaPool *arena, size_t size)
{
    void *p;

    PR_ARENA_ALLOCATE(p, arena, size);

    return(p);
}

void *
PORT_ArenaZAlloc(PRArenaPool *arena, size_t size)
{
    void *p;

    PR_ARENA_ALLOCATE(p, arena, size);
    if (p == NULL) {
    } else {
	memset(p, 0, size);
    }

    return(p);
}

/* XXX - need to zeroize!! - jsw */
void
PORT_FreeArena(PRArenaPool *arena, PRBool zero)
{
    PR_FinishArenaPool(arena);
    PORT_Free(arena);
}

void *
PORT_ArenaGrow(PRArenaPool *arena, void *ptr, size_t oldsize, size_t newsize)
{
    PR_ASSERT(newsize >= oldsize);
    
    PR_ARENA_GROW(ptr, arena, oldsize, ( newsize - oldsize ) );
    
    return(ptr);
}

void *
PORT_ArenaMark(PRArenaPool *arena)
{
    return PR_ARENA_MARK(arena);
}

void
PORT_ArenaRelease(PRArenaPool *arena, void *mark)
{
    PR_ARENA_RELEASE(arena, mark);
}

