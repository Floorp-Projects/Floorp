.. _mozilla_projects_nss_memory_allocation:

NSS Memory allocation
=====================

.. container::

   NSS makes extensive use of NSPR's PLArenaPools for memory allocation.

   Each block of memory allocated in a PLArenaPool is called a PLArena. When a PLArenaPool is freed,
   all the arenas in that pool are put on an arena free list. When NSS attempts to allocate more
   memory for an arena pool, the PLArenaPool code attempts to use an arena from its free list, and
   only gets a new arena from the heap if there are no arenas in the free list that are large enough
   to satisfy the request.

   There are two consequences of the use of PLArenaPools that affect leak analysis. They are:

   1. At the end of execution of a program, all the arenas in the free list will appear to have been
   leaked. This makes it difficult to tell arenas that are truly leaked from those that are merely
   in the free list.

   There is a function named PL_ArenaFinish that really frees all the arenas on the free list. See
   the prototype at
   `http://mxr.mozilla.org/nspr/source/n.../ds/plarenas.h <http://mxr.mozilla.org/nspr/source/nsprpub/lib/ds/plarenas.h>`__

   A program should call that function at the very end, after having shutdown NSS and NSPR, to
   really free the contents of the free list. After that function returns, any arenas that still
   appear to be leaked have truly been leaked, and are not merely on the free list.

   2. Leak analysis tools will frequently report the wrong call stack for the allocation of leaked
   arenas.

   When the arena free list is in use, the first user of an arena will allocate it from the heap,
   but will then free it to the free list. The second user will allocated it from the free list and
   return it to the free list. If and when an arena is leaked, the developer wants to see the call
   stack of the most recent allocation of the arena, not the stack of the oldest allocation of that
   arena. But leak analysis tools only record the allocation of memory from the heap, not memory
   from the arena free list, so they will always show the first allocation (from the heap) and not
   the most recent allocation (from the arena free list).

   Consequently, when the arena free list is in use, the allocation call stacks shown will typically
   NOT be the stack of the code that most recently allocated that arena, but rather will be the
   stack of the code that FIRST allocated that arena from the heap, and then placed it on the free
   list.

   To solve that problem, it is generally necessary to disable the arena free list, so that arenas
   are actually freed back to the heap each time they are freed, and are allocated afresh from the
   heap each time they are allocated. This makes NSS slower, but produces accurate leak allocation
   stacks. To accomplish that, set an environment variable prior to the initialization of NSS and
   NSPR. This can be done outside the program entirely, or can be done by the program itself, in the
   main() function. Set the environment variable NSS_DISABLE_ARENA_FREE_LIST to have any non-empty
   value, e.g. NSS_DISABLE_ARENA_FREE_LIST=1.