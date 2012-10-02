/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: arena.c,v $ $Revision: 1.15 $ $Date: 2012/07/06 18:19:32 $";
#endif /* DEBUG */

/*
 * arena.c
 *
 * This contains the implementation of NSS's thread-safe arenas.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#ifdef ARENA_THREADMARK
#include "prthread.h"
#endif /* ARENA_THREADMARK */

#include "prlock.h"
#include "plarena.h"

#include <string.h>

/*
 * NSSArena
 *
 * This is based on NSPR's arena code, but it is threadsafe.
 *
 * The public methods relating to this type are:
 *
 *  NSSArena_Create  -- constructor
 *  NSSArena_Destroy
 *  NSS_ZAlloc
 *  NSS_ZRealloc
 *  NSS_ZFreeIf
 *
 * The nonpublic methods relating to this type are:
 *
 *  nssArena_Create  -- constructor
 *  nssArena_Destroy
 *  nssArena_Mark
 *  nssArena_Release
 *  nssArena_Unmark
 * 
 *  nss_ZAlloc
 *  nss_ZFreeIf
 *  nss_ZRealloc
 *
 * In debug builds, the following calls are available:
 *
 *  nssArena_verifyPointer
 *  nssArena_registerDestructor
 *  nssArena_deregisterDestructor
 */

struct NSSArenaStr {
  PLArenaPool pool;
  PRLock *lock;
#ifdef ARENA_THREADMARK
  PRThread *marking_thread;
  nssArenaMark *first_mark;
  nssArenaMark *last_mark;
#endif /* ARENA_THREADMARK */
#ifdef ARENA_DESTRUCTOR_LIST
  struct arena_destructor_node *first_destructor;
  struct arena_destructor_node *last_destructor;
#endif /* ARENA_DESTRUCTOR_LIST */
};

/*
 * nssArenaMark
 *
 * This type is used to mark the current state of an NSSArena.
 */

struct nssArenaMarkStr {
  PRUint32 magic;
  void *mark;
#ifdef ARENA_THREADMARK
  nssArenaMark *next;
#endif /* ARENA_THREADMARK */
#ifdef ARENA_DESTRUCTOR_LIST
  struct arena_destructor_node *next_destructor;
  struct arena_destructor_node *prev_destructor;
#endif /* ARENA_DESTRUCTOR_LIST */
};

#define MARK_MAGIC 0x4d41524b /* "MARK" how original */

/*
 * But first, the pointer-tracking code
 */
#ifdef DEBUG
extern const NSSError NSS_ERROR_INTERNAL_ERROR;

static nssPointerTracker arena_pointer_tracker;

static PRStatus
arena_add_pointer
(
  const NSSArena *arena
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&arena_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return rv;
  }

  rv = nssPointerTracker_add(&arena_pointer_tracker, arena);
  if( PR_SUCCESS != rv ) {
    NSSError e = NSS_GetError();
    if( NSS_ERROR_NO_MEMORY != e ) {
      nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    }

    return rv;
  }

  return PR_SUCCESS;
}

static PRStatus
arena_remove_pointer
(
  const NSSArena *arena
)
{
  PRStatus rv;

  rv = nssPointerTracker_remove(&arena_pointer_tracker, arena);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  }

  return rv;
}

/*
 * nssArena_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSArena object,
 * this routine will return PR_SUCCESS.  Otherwise, it will put an
 * error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_IMPLEMENT PRStatus
nssArena_verifyPointer
(
  const NSSArena *arena
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&arena_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    /*
     * This is a little disingenious.  We have to initialize the
     * tracker, because someone could "legitimately" try to verify
     * an arena pointer before one is ever created.  And this step
     * might fail, due to lack of memory.  But the only way that
     * this step can fail is if it's doing the call_once stuff,
     * (later calls just no-op).  And if it didn't no-op, there
     * aren't any valid arenas.. so the argument certainly isn't one.
     */
    nss_SetError(NSS_ERROR_INVALID_ARENA);
    return PR_FAILURE;
  }

  rv = nssPointerTracker_verify(&arena_pointer_tracker, arena);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INVALID_ARENA);
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}
#endif /* DEBUG */

#ifdef ARENA_DESTRUCTOR_LIST

struct arena_destructor_node {
  struct arena_destructor_node *next;
  struct arena_destructor_node *prev;
  void (*destructor)(void *argument);
  void *arg;
};

/*
 * nssArena_registerDestructor
 *
 * This routine stores a pointer to a callback and an arbitrary
 * pointer-sized argument in the arena, at the current point in
 * the mark stack.  If the arena is destroyed, or an "earlier"
 * mark is released, then this destructor will be called at that
 * time.  Note that the destructor will be called with the arena
 * locked, which means the destructor may free memory in that
 * arena, but it may not allocate or cause to be allocated any
 * memory.  This callback facility was included to support our
 * debug-version pointer-tracker feature; overuse runs counter to
 * the the original intent of arenas.  This routine returns a 
 * PRStatus value; if successful, it will return PR_SUCCESS.  If 
 * unsuccessful, it will set an error on the error stack and 
 * return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nssArena_registerDestructor
(
  NSSArena *arena,
  void (*destructor)(void *argument),
  void *arg
)
{
  struct arena_destructor_node *it;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */
  
  it = nss_ZNEW(arena, struct arena_destructor_node);
  if( (struct arena_destructor_node *)NULL == it ) {
    return PR_FAILURE;
  }

  it->prev = arena->last_destructor;
  arena->last_destructor->next = it;
  arena->last_destructor = it;
  it->destructor = destructor;
  it->arg = arg;

  if( (nssArenaMark *)NULL != arena->last_mark ) {
    arena->last_mark->prev_destructor = it->prev;
    arena->last_mark->next_destructor = it->next;
  }

  return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssArena_deregisterDestructor
(
  NSSArena *arena,
  void (*destructor)(void *argument),
  void *arg
)
{
  struct arena_destructor_node *it;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  for( it = arena->first_destructor; it; it = it->next ) {
    if( (it->destructor == destructor) && (it->arg == arg) ) {
      break;
    }
  }

  if( (struct arena_destructor_node *)NULL == it ) {
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
  }

  if( it == arena->first_destructor ) {
    arena->first_destructor = it->next;
  }

  if( it == arena->last_destructor ) {
    arena->last_destructor = it->prev;
  }

  if( (struct arena_destructor_node *)NULL != it->prev ) {
    it->prev->next = it->next;
  }

  if( (struct arena_destructor_node *)NULL != it->next ) {
    it->next->prev = it->prev;
  }

  {
    nssArenaMark *m;
    for( m = arena->first_mark; m; m = m->next ) {
      if( m->next_destructor == it ) {
        m->next_destructor = it->next;
      }
      if( m->prev_destructor == it ) {
        m->prev_destructor = it->prev;
      }
    }
  }

  nss_ZFreeIf(it);
  return PR_SUCCESS;
}

static void
nss_arena_call_destructor_chain
(
  struct arena_destructor_node *it
)
{
  for( ; it ; it = it->next ) {
    (*(it->destructor))(it->arg);
  }
}

#endif /* ARENA_DESTRUCTOR_LIST */

/*
 * NSSArena_Create
 *
 * This routine creates a new memory arena.  This routine may return
 * NULL upon error, in which case it will have created an error stack.
 *
 * The top-level error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSArena upon success
 */

NSS_IMPLEMENT NSSArena *
NSSArena_Create
(
  void
)
{
  nss_ClearErrorStack();
  return nssArena_Create();
}

/*
 * nssArena_Create
 *
 * This routine creates a new memory arena.  This routine may return
 * NULL upon error, in which case it will have set an error on the 
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSArena upon success
 */

NSS_IMPLEMENT NSSArena *
nssArena_Create
(
  void
)
{
  NSSArena *rv = (NSSArena *)NULL;

  rv = nss_ZNEW((NSSArena *)NULL, NSSArena);
  if( (NSSArena *)NULL == rv ) {
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (NSSArena *)NULL;
  }

  rv->lock = PR_NewLock();
  if( (PRLock *)NULL == rv->lock ) {
    (void)nss_ZFreeIf(rv);
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (NSSArena *)NULL;
  }

  /*
   * Arena sizes.  The current security code has 229 occurrences of
   * PORT_NewArena.  The default chunksizes specified break down as
   *
   *  Size    Mult.   Specified as
   *   512       1    512
   *  1024       7    1024
   *  2048       5    2048
   *  2048       5    CRMF_DEFAULT_ARENA_SIZE
   *  2048     190    DER_DEFAULT_CHUNKSIZE
   *  2048      20    SEC_ASN1_DEFAULT_ARENA_SIZE
   *  4096       1    4096
   *
   * Obviously this "default chunksize" flexibility isn't very 
   * useful to us, so I'll just pick 2048.
   */

  PL_InitArenaPool(&rv->pool, "NSS", 2048, sizeof(double));

#ifdef DEBUG
  {
    PRStatus st;
    st = arena_add_pointer(rv);
    if( PR_SUCCESS != st ) {
      PL_FinishArenaPool(&rv->pool);
      PR_DestroyLock(rv->lock);
      (void)nss_ZFreeIf(rv);
      return (NSSArena *)NULL;
    }
  }
#endif /* DEBUG */

  return rv;
}

/*
 * NSSArena_Destroy
 *
 * This routine will destroy the specified arena, freeing all memory
 * allocated from it.  This routine returns a PRStatus value; if 
 * successful, it will return PR_SUCCESS.  If unsuccessful, it will
 * create an error stack and return PR_FAILURE.
 *
 * The top-level error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_IMPLEMENT PRStatus
NSSArena_Destroy
(
  NSSArena *arena
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  return nssArena_Destroy(arena);
}

/*
 * nssArena_Destroy
 *
 * This routine will destroy the specified arena, freeing all memory
 * allocated from it.  This routine returns a PRStatus value; if 
 * successful, it will return PR_SUCCESS.  If unsuccessful, it will
 * set an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nssArena_Destroy
(
  NSSArena *arena
)
{
  PRLock *lock;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  if( (PRLock *)NULL == arena->lock ) {
    /* Just got destroyed */
    nss_SetError(NSS_ERROR_INVALID_ARENA);
    return PR_FAILURE;
  }
  PR_Lock(arena->lock);
  
#ifdef DEBUG
  if( PR_SUCCESS != arena_remove_pointer(arena) ) {
    PR_Unlock(arena->lock);
    return PR_FAILURE;
  }
#endif /* DEBUG */

#ifdef ARENA_DESTRUCTOR_LIST
  /* Note that the arena is locked at this time */
  nss_arena_call_destructor_chain(arena->first_destructor);
#endif /* ARENA_DESTRUCTOR_LIST */

  PL_FinishArenaPool(&arena->pool);
  lock = arena->lock;
  arena->lock = (PRLock *)NULL;
  PR_Unlock(lock);
  PR_DestroyLock(lock);
  (void)nss_ZFreeIf(arena);
  return PR_SUCCESS;
}

static void *nss_zalloc_arena_locked(NSSArena *arena, PRUint32 size);

/*
 * nssArena_Mark
 *
 * This routine "marks" the current state of an arena.  Space
 * allocated after the arena has been marked can be freed by
 * releasing the arena back to the mark with nssArena_Release,
 * or committed by calling nssArena_Unmark.  When successful, 
 * this routine returns a valid nssArenaMark pointer.  This 
 * routine may return NULL upon error, in which case it will 
 * have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *
 * Return value:
 *  NULL upon failure
 *  An nssArenaMark pointer upon success
 */

NSS_IMPLEMENT nssArenaMark *
nssArena_Mark
(
  NSSArena *arena
)
{
  nssArenaMark *rv;
  void *p;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
    return (nssArenaMark *)NULL;
  }
#endif /* NSSDEBUG */

  if( (PRLock *)NULL == arena->lock ) {
    /* Just got destroyed */
    nss_SetError(NSS_ERROR_INVALID_ARENA);
    return (nssArenaMark *)NULL;
  }
  PR_Lock(arena->lock);

#ifdef ARENA_THREADMARK
  if( (PRThread *)NULL == arena->marking_thread ) {
    /* Unmarked.  Store our thread ID */
    arena->marking_thread = PR_GetCurrentThread();
    /* This call never fails. */
  } else {
    /* Marked.  Verify it's the current thread */
    if( PR_GetCurrentThread() != arena->marking_thread ) {
      PR_Unlock(arena->lock);
      nss_SetError(NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD);
      return (nssArenaMark *)NULL;
    }
  }
#endif /* ARENA_THREADMARK */

  p = PL_ARENA_MARK(&arena->pool);
  /* No error possible */

  /* Do this after the mark */
  rv = (nssArenaMark *)nss_zalloc_arena_locked(arena, sizeof(nssArenaMark));
  if( (nssArenaMark *)NULL == rv ) {
    PR_Unlock(arena->lock);
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (nssArenaMark *)NULL;
  }

#ifdef ARENA_THREADMARK
  if ( (nssArenaMark *)NULL == arena->first_mark) {
    arena->first_mark = rv;
    arena->last_mark = rv;
  } else {
    arena->last_mark->next = rv;
    arena->last_mark = rv;
  }
#endif /* ARENA_THREADMARK */

  rv->mark = p;
  rv->magic = MARK_MAGIC;

#ifdef ARENA_DESTRUCTOR_LIST
  rv->prev_destructor = arena->last_destructor;
#endif /* ARENA_DESTRUCTOR_LIST */

  PR_Unlock(arena->lock);

  return rv;
}

/*
 * nss_arena_unmark_release
 *
 * This static routine implements the routines nssArena_Release
 * ans nssArena_Unmark, which are almost identical.
 */

static PRStatus
nss_arena_unmark_release
(
  NSSArena *arena,
  nssArenaMark *arenaMark,
  PRBool release
)
{
  void *inner_mark;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  if( MARK_MAGIC != arenaMark->magic ) {
    nss_SetError(NSS_ERROR_INVALID_ARENA_MARK);
    return PR_FAILURE;
  }

  if( (PRLock *)NULL == arena->lock ) {
    /* Just got destroyed */
    nss_SetError(NSS_ERROR_INVALID_ARENA);
    return PR_FAILURE;
  }
  PR_Lock(arena->lock);

#ifdef ARENA_THREADMARK
  if( (PRThread *)NULL != arena->marking_thread ) {
    if( PR_GetCurrentThread() != arena->marking_thread ) {
      PR_Unlock(arena->lock);
      nss_SetError(NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD);
      return PR_FAILURE;
    }
  }
#endif /* ARENA_THREADMARK */

  if( MARK_MAGIC != arenaMark->magic ) {
    /* Just got released */
    PR_Unlock(arena->lock);
    nss_SetError(NSS_ERROR_INVALID_ARENA_MARK);
    return PR_FAILURE;
  }

  arenaMark->magic = 0;
  inner_mark = arenaMark->mark;

#ifdef ARENA_THREADMARK
  {
    nssArenaMark **pMark = &arena->first_mark;
    nssArenaMark *rest;
    nssArenaMark *last = (nssArenaMark *)NULL;

    /* Find this mark */
    while( *pMark != arenaMark ) {
      last = *pMark;
      pMark = &(*pMark)->next;
    }

    /* Remember the pointer, then zero it */
    rest = (*pMark)->next;
    *pMark = (nssArenaMark *)NULL;

    arena->last_mark = last;

    /* Invalidate any later marks being implicitly released */
    for( ; (nssArenaMark *)NULL != rest; rest = rest->next ) {
      rest->magic = 0;
    }

    /* If we just got rid of the first mark, clear the thread ID */
    if( (nssArenaMark *)NULL == arena->first_mark ) {
      arena->marking_thread = (PRThread *)NULL;
    }
  }
#endif /* ARENA_THREADMARK */

  if( release ) {
#ifdef ARENA_DESTRUCTOR_LIST
    if( (struct arena_destructor_node *)NULL != arenaMark->prev_destructor ) {
      arenaMark->prev_destructor->next = (struct arena_destructor_node *)NULL;
    }
    arena->last_destructor = arenaMark->prev_destructor;

    /* Note that the arena is locked at this time */
    nss_arena_call_destructor_chain(arenaMark->next_destructor);
#endif /* ARENA_DESTRUCTOR_LIST */

    PR_ARENA_RELEASE(&arena->pool, inner_mark);
    /* No error return */
  }

  PR_Unlock(arena->lock);
  return PR_SUCCESS;
}

/*
 * nssArena_Release
 *
 * This routine invalidates and releases all memory allocated from
 * the specified arena after the point at which the specified mark
 * was obtained.  This routine returns a PRStatus value; if successful,
 * it will return PR_SUCCESS.  If unsuccessful, it will set an error
 * on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_ARENA_MARK
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nssArena_Release
(
  NSSArena *arena,
  nssArenaMark *arenaMark
)
{
  return nss_arena_unmark_release(arena, arenaMark, PR_TRUE);
}

/*
 * nssArena_Unmark
 *
 * This routine "commits" the indicated mark and any marks after
 * it, making them unreleasable.  Note that any earlier marks can
 * still be released, and such a release will invalidate these
 * later unmarked regions.  If an arena is to be safely shared by
 * more than one thread, all marks must be either released or
 * unmarked.  This routine returns a PRStatus value; if successful,
 * it will return PR_SUCCESS.  If unsuccessful, it will set an error
 * on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_ARENA_MARK
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nssArena_Unmark
(
  NSSArena *arena,
  nssArenaMark *arenaMark
)
{
  return nss_arena_unmark_release(arena, arenaMark, PR_FALSE);
}

/*
 * We prefix this header to all allocated blocks.  It is a multiple
 * of the alignment size.  Note that this usage of a header may make
 * purify spew bogus warnings about "potentially leaked blocks" of
 * memory; if that gets too annoying we can add in a pointer to the
 * header in the header itself.  There's not a lot of safety here;
 * maybe we should add a magic value?
 */
struct pointer_header {
  NSSArena *arena;
  PRUint32 size;
};

static void *
nss_zalloc_arena_locked
(
  NSSArena *arena,
  PRUint32 size
)
{
  void *p;
  void *rv;
  struct pointer_header *h;
  PRUint32 my_size = size + sizeof(struct pointer_header);
  PR_ARENA_ALLOCATE(p, &arena->pool, my_size);
  if( (void *)NULL == p ) {
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (void *)NULL;
  }
  /* 
   * Do this before we unlock.  This way if the user is using
   * an arena in one thread while destroying it in another, he'll
   * fault/FMR in his code, not ours.
   */
  h = (struct pointer_header *)p;
  h->arena = arena;
  h->size = size;
  rv = (void *)((char *)h + sizeof(struct pointer_header));
  (void)nsslibc_memset(rv, 0, size);
  return rv;
}

/*
 * NSS_ZAlloc
 *
 * This routine allocates and zeroes a section of memory of the 
 * size, and returns to the caller a pointer to that memory.  If
 * the optional arena argument is non-null, the memory will be
 * obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return NULL upon error, in
 * which case it will have set an error upon the error stack.  The
 * value specified for size may be zero; in which case a valid 
 * zero-length block of memory will be allocated.  This block may
 * be expanded by calling NSS_ZRealloc.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *
 * Return value:
 *  NULL upon error
 *  A pointer to the new segment of zeroed memory
 */

NSS_IMPLEMENT void *
NSS_ZAlloc
(
  NSSArena *arenaOpt,
  PRUint32 size
)
{
  return nss_ZAlloc(arenaOpt, size);
}

/*
 * nss_ZAlloc
 *
 * This routine allocates and zeroes a section of memory of the 
 * size, and returns to the caller a pointer to that memory.  If
 * the optional arena argument is non-null, the memory will be
 * obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return NULL upon error, in
 * which case it will have set an error upon the error stack.  The
 * value specified for size may be zero; in which case a valid 
 * zero-length block of memory will be allocated.  This block may
 * be expanded by calling nss_ZRealloc.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *
 * Return value:
 *  NULL upon error
 *  A pointer to the new segment of zeroed memory
 */

NSS_IMPLEMENT void *
nss_ZAlloc
(
  NSSArena *arenaOpt,
  PRUint32 size
)
{
  struct pointer_header *h;
  PRUint32 my_size = size + sizeof(struct pointer_header);

  if( my_size < sizeof(struct pointer_header) ) {
    /* Wrapped */
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (void *)NULL;
  }

  if( (NSSArena *)NULL == arenaOpt ) {
    /* Heap allocation, no locking required. */
    h = (struct pointer_header *)PR_Calloc(1, my_size);
    if( (struct pointer_header *)NULL == h ) {
      nss_SetError(NSS_ERROR_NO_MEMORY);
      return (void *)NULL;
    }

    h->arena = (NSSArena *)NULL;
    h->size = size;
    /* We used calloc: it's already zeroed */

    return (void *)((char *)h + sizeof(struct pointer_header));
  } else {
    void *rv;
    /* Arena allocation */
#ifdef NSSDEBUG
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (void *)NULL;
    }
#endif /* NSSDEBUG */

    if( (PRLock *)NULL == arenaOpt->lock ) {
      /* Just got destroyed */
      nss_SetError(NSS_ERROR_INVALID_ARENA);
      return (void *)NULL;
    }
    PR_Lock(arenaOpt->lock);

#ifdef ARENA_THREADMARK
    if( (PRThread *)NULL != arenaOpt->marking_thread ) {
      if( PR_GetCurrentThread() != arenaOpt->marking_thread ) {
        nss_SetError(NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD);
        PR_Unlock(arenaOpt->lock);
        return (void *)NULL;
      }
    }
#endif /* ARENA_THREADMARK */

    rv = nss_zalloc_arena_locked(arenaOpt, size);

    PR_Unlock(arenaOpt->lock);
    return rv;
  }
  /*NOTREACHED*/
}

/*
 * NSS_ZFreeIf
 *
 * If the specified pointer is non-null, then the region of memory 
 * to which it points -- which must have been allocated with 
 * NSS_ZAlloc -- will be zeroed and released.  This routine 
 * returns a PRStatus value; if successful, it will return PR_SUCCESS.
 * If unsuccessful, it will set an error on the error stack and return 
 * PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */
NSS_IMPLEMENT PRStatus
NSS_ZFreeIf
(
  void *pointer
)
{
   return nss_ZFreeIf(pointer);
}

/*
 * nss_ZFreeIf
 *
 * If the specified pointer is non-null, then the region of memory 
 * to which it points -- which must have been allocated with 
 * nss_ZAlloc -- will be zeroed and released.  This routine 
 * returns a PRStatus value; if successful, it will return PR_SUCCESS.
 * If unsuccessful, it will set an error on the error stack and return 
 * PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nss_ZFreeIf
(
  void *pointer
)
{
  struct pointer_header *h;

  if( (void *)NULL == pointer ) {
    return PR_SUCCESS;
  }

  h = (struct pointer_header *)((char *)pointer
    - sizeof(struct pointer_header));

  /* Check any magic here */

  if( (NSSArena *)NULL == h->arena ) {
    /* Heap */
    (void)nsslibc_memset(pointer, 0, h->size);
    PR_Free(h);
    return PR_SUCCESS;
  } else {
    /* Arena */
#ifdef NSSDEBUG
    if( PR_SUCCESS != nssArena_verifyPointer(h->arena) ) {
      return PR_FAILURE;
    }
#endif /* NSSDEBUG */

    if( (PRLock *)NULL == h->arena->lock ) {
      /* Just got destroyed.. so this pointer is invalid */
      nss_SetError(NSS_ERROR_INVALID_POINTER);
      return PR_FAILURE;
    }
    PR_Lock(h->arena->lock);

    (void)nsslibc_memset(pointer, 0, h->size);

    /* No way to "free" it within an NSPR arena. */

    PR_Unlock(h->arena->lock);
    return PR_SUCCESS;
  }
  /*NOTREACHED*/
}

/*
 * NSS_ZRealloc
 *
 * This routine reallocates a block of memory obtained by calling
 * nss_ZAlloc or nss_ZRealloc.  The portion of memory 
 * between the new and old sizes -- which is either being newly
 * obtained or released -- is in either case zeroed.  This routine 
 * may return NULL upon failure, in which case it will have placed 
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *
 * Return value:
 *  NULL upon error
 *  A pointer to the replacement segment of memory
 */

NSS_EXTERN void *
NSS_ZRealloc
(
  void *pointer,
  PRUint32 newSize
)
{
    return nss_ZRealloc(pointer, newSize);
}

/*
 * nss_ZRealloc
 *
 * This routine reallocates a block of memory obtained by calling
 * nss_ZAlloc or nss_ZRealloc.  The portion of memory 
 * between the new and old sizes -- which is either being newly
 * obtained or released -- is in either case zeroed.  This routine 
 * may return NULL upon failure, in which case it will have placed 
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *
 * Return value:
 *  NULL upon error
 *  A pointer to the replacement segment of memory
 */

NSS_EXTERN void *
nss_ZRealloc
(
  void *pointer,
  PRUint32 newSize
)
{
  NSSArena *arena;
  struct pointer_header *h, *new_h;
  PRUint32 my_newSize = newSize + sizeof(struct pointer_header);
  void *rv;

  if( my_newSize < sizeof(struct pointer_header) ) {
    /* Wrapped */
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (void *)NULL;
  }

  if( (void *)NULL == pointer ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (void *)NULL;
  }

  h = (struct pointer_header *)((char *)pointer
    - sizeof(struct pointer_header));

  /* Check any magic here */

  if( newSize == h->size ) {
    /* saves thrashing */
    return pointer;
  }

  arena = h->arena;
  if (!arena) {
    /* Heap */
    new_h = (struct pointer_header *)PR_Calloc(1, my_newSize);
    if( (struct pointer_header *)NULL == new_h ) {
      nss_SetError(NSS_ERROR_NO_MEMORY);
      return (void *)NULL;
    }

    new_h->arena = (NSSArena *)NULL;
    new_h->size = newSize;
    rv = (void *)((char *)new_h + sizeof(struct pointer_header));

    if( newSize > h->size ) {
      (void)nsslibc_memcpy(rv, pointer, h->size);
      (void)nsslibc_memset(&((char *)rv)[ h->size ], 
                           0, (newSize - h->size));
    } else {
      (void)nsslibc_memcpy(rv, pointer, newSize);
    }

    (void)nsslibc_memset(pointer, 0, h->size);
    h->size = 0;
    PR_Free(h);

    return rv;
  } else {
    void *p;
    /* Arena */
#ifdef NSSDEBUG
    if (PR_SUCCESS != nssArena_verifyPointer(arena)) {
      return (void *)NULL;
    }
#endif /* NSSDEBUG */

    if (!arena->lock) {
      /* Just got destroyed.. so this pointer is invalid */
      nss_SetError(NSS_ERROR_INVALID_POINTER);
      return (void *)NULL;
    }
    PR_Lock(arena->lock);

#ifdef ARENA_THREADMARK
    if (arena->marking_thread) {
      if (PR_GetCurrentThread() != arena->marking_thread) {
        PR_Unlock(arena->lock);
        nss_SetError(NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD);
        return (void *)NULL;
      }
    }
#endif /* ARENA_THREADMARK */

    if( newSize < h->size ) {
      /*
       * We have no general way of returning memory to the arena
       * (mark/release doesn't work because things may have been
       * allocated after this object), so the memory is gone
       * anyway.  We might as well just return the same pointer to
       * the user, saying "yeah, uh-hunh, you can only use less of
       * it now."  We'll zero the leftover part, of course.  And
       * in fact we might as well *not* adjust h->size-- this way,
       * if the user reallocs back up to something not greater than
       * the original size, then voila, there's the memory!  This
       * way a thrash big/small/big/small doesn't burn up the arena.
       */
      char *extra = &((char *)pointer)[ newSize ];
      (void)nsslibc_memset(extra, 0, (h->size - newSize));
      PR_Unlock(arena->lock);
      return pointer;
    }

    PR_ARENA_ALLOCATE(p, &arena->pool, my_newSize);
    if( (void *)NULL == p ) {
      PR_Unlock(arena->lock);
      nss_SetError(NSS_ERROR_NO_MEMORY);
      return (void *)NULL;
    }

    new_h = (struct pointer_header *)p;
    new_h->arena = arena;
    new_h->size = newSize;
    rv = (void *)((char *)new_h + sizeof(struct pointer_header));
    if (rv != pointer) {
	(void)nsslibc_memcpy(rv, pointer, h->size);
	(void)nsslibc_memset(pointer, 0, h->size);
    }
    (void)nsslibc_memset(&((char *)rv)[ h->size ], 0, (newSize - h->size));
    h->arena = (NSSArena *)NULL;
    h->size = 0;
    PR_Unlock(arena->lock);
    return rv;
  }
  /*NOTREACHED*/
}

PRStatus 
nssArena_Shutdown(void)
{
  PRStatus rv = PR_SUCCESS;
#ifdef DEBUG
  rv = nssPointerTracker_finalize(&arena_pointer_tracker);
#endif
  return rv;
}
