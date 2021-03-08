HTTP Cache
==========

This document describes the **HTTP cache implementation**.

The code resides in `/netwerk/cache2 (searchfox)
<https://searchfox.org/mozilla-central/source/netwerk/cache2>`_

API
---

Here is a detailed description of the HTTP cache v2 API, examples
included.  This document only contains what cannot be found or may not
be clear directly from the `IDL files <https://searchfox.org/mozilla-central/search?q=&path=cache2%2FnsICache&case=false&regexp=false>`_ comments.

-  The cache API is **completely thread-safe** and **non-blocking**.
-  There is **no IPC support**.  It's only accessible on the default
   chrome process.
-  When there is no profile the new HTTP cache works, but everything is
   stored only in memory not obeying any particular limits.

nsICacheStorageService
----------------------

-  The HTTP cache entry-point. Accessible as a service only, fully
   thread-safe, scriptable.

-  `nsICacheStorageService.idl (searchfox) <https://searchfox.org/mozilla-central/source/netwerk/cache2/nsICacheStorageService.idl>`_

-   \ ``"@mozilla.org/netwerk/cache-storage-service;1"``

-  Provides methods accessing "storage" objects – see `nsICacheStorage` below – giving further access to cache entries – see :ref:`nsICacheEntry` more below – per specific URL.

-  Currently we have 3 types of storages, all the access methods return
   an :ref:`nsICacheStorage` object:

   -  **memory-only** (``memoryCacheStorage``): stores data only in a
      memory cache, data in this storage are never put to disk

   -  **disk** (``diskCacheStorage``): stores data on disk, but for
      existing entries also looks into the memory-only storage; when
      instructed via a special argument also primarily looks into
      application caches

   .. note::

      **application cache** (``appCacheStorage``): when a consumer has a
      specific ``nsIApplicationCache`` (i.e. a particular app cache
      version in a group) in hands, this storage will provide read and
      write access to entries in that application cache; when the app
      cache is not specified, this storage will operate over all
      existing app caches. **This kind of storage is deprecated and will be removed** in `bug 1694662 <https://bugzilla.mozilla.org/show_bug.cgi?id=1694662>`_ 

-  The service also provides methods to clear the whole disk and memory
   cache content or purge any intermediate memory structures:

   -  ``clear``– after it returns, all entries are no longer accessible
      through the cache APIs; the method is fast to execute and
      non-blocking in any way; the actual erase happens in background

   -  ``purgeFromMemory``– removes (schedules to remove) any
      intermediate cache data held in memory for faster access (more
      about the :ref:`Intermediate_Memory_Caching` below)

nsILoadContextInfo
------------------

-  Distinguishes the scope of the storage demanded to open.

-  Mandatory argument to ``*Storage`` methods of :ref:`nsICacheStorageService`.

-  `nsILoadContextInfo.idl (searchfox) <https://searchfox.org/mozilla-central/source/netwerk/base/nsILoadContextInfo.idl>`_


-  It is a helper interface wrapping following four arguments into a single one:

   -  **private-browsing** boolean flag
   -  **anonymous load** boolean flag
   -  **origin attributes** js value

   .. note::

      Helper functions to create nsILoadContextInfo objects:

      -  C++ consumers: functions at ``LoadContextInfo.h`` exported
         header

      -  JS consumers: ``Services.loadContextInfo`` which is an instance of ``nsILoadContextInfoFactory``.

-  Two storage objects created with the same set of
   ``nsILoadContextInfo``\ arguments are identical, containing the same
   cache entries.

-  Two storage objects created with in any way different
   ``nsILoadContextInfo``\ arguments are strictly and completely
   distinct and cache entries in them do not overlap even when having
   the same URIs.


nsICacheStorage
---------------

-  `nsICacheStorage.idl (searchfox) <https://searchfox.org/mozilla-central/source/netwerk/cache2/nsICacheStorage.idl>`_

-  Obtained from call to one of the ``*Storage`` methods on
   :ref:`nsICacheStorageService`.

-  Represents a distinct storage area (or scope) to put and get cache
   entries mapped by URLs into and from it.

-  *Similarity with the old cache*\ : this interface may be with some
   limitations considered as a mirror to ``nsICacheSession``, but less
   generic and not inclining to abuse.

nsICacheEntryOpenCallback
-------------------------

-  `nsICacheEntryOpenCallback.idl (searchfox) <https://searchfox.org/mozilla-central/source/netwerk/cache2/nsICacheEntryOpenCallback.idl>`_

-  The result of ``nsICacheStorage.asyncOpenURI`` is always and only
   sent to callbacks on this interface.

-  These callbacks are ensured to be invoked when ``asyncOpenURI``
   returns ``NS_OK``.

-  

   .. note::

      When the
      cache entry object is already present in memory or open as
      "force-new" (a.k.a "open-truncate") this callback is invoked
      sooner then the ``asyncOpenURI``\ method returns (i.e.
      immediately); there is currently no way to opt out of this feature
      (see `bug
      938186 <https://bugzilla.mozilla.org/show_bug.cgi?id=938186>`__).

nsICacheEntry
-------------

-  `nsICacheEntry.idl (searchfox) <https://searchfox.org/mozilla-central/source/netwerk/cache2/nsICacheEntry.idl>`_

-  Obtained asynchronously or pseudo-asynchronously by a call to
   ``nsICacheStorage.asyncOpenURI``.

-  Provides access to a cached entry data and meta data for reading or
   writing or in some cases both, see below.

Lifetime of a new entry
-----------------------

-  Such entry is initially empty (no data or meta data is stored in it).

-  The ``aNew``\ argument in ``onCacheEntryAvailable`` is ``true`` for
   and only for new entries.

-  Only one consumer (the so called "*writer*") may have such an entry
   available (obtained via ``onCacheEntryAvailable``).

-  Other parallel openers of the same cache entry are blocked (wait) for
   invocation of their ``onCacheEntryAvailable`` until one of the
   following occurs:

   -  The *writer* simply throws the entry away: other waiting opener in
      line gets the entry again as "*new*", the cycle repeats.

      .. note::

         This applies in general, writers throwing away the cache entry
         means a failure to write the cache entry and a new writer is
         being looked for again, the cache entry remains empty (a.k.a.
         "new").

   -  The *writer* stored all necessary meta data in the cache entry and
      called ``metaDataReady`` on it: other consumers now get the entry
      and may examine and potentially modify the meta data and read the
      data (if any) of the cache entry.
   -  When the *writer* has data (i.e. the response payload) to write to
      the cache entry, it **must** open the output stream on it
      **before** it calls ``metaDataReady``.

-  When the *writer* still keeps the cache entry and has open and keeps
   open the output stream on it, other consumers may open input streams
   on the entry. The data will be available as the *writer* writes data
   to the cache entry's output stream immediately, even before the
   output stream is closed. This is called :ref:`concurrent
   read/write <Concurrent_read_and_write>`.

Concurrent read and write
-------------------------

The cache supports reading a cache entry data while it is still being
written by the first consumer - the *writer*.
This can only be engaged for resumable responses that (`bug
960902 <https://bugzilla.mozilla.org/show_bug.cgi?id=960902#c17>`__)
don't need revalidation. Reason is that when the writer is interrupted
(by e.g. external canceling of the loading channel) concurrent readers
would not be able to reach the remaining unread content.

.. note::

   This could be improved by keeping the network load running and being
   stored to the cache entry even after the writing channel has been
   canceled.

When the *writer* is interrupted, the first concurrent *reader* in line
does a range request for the rest of the data - and becomes that way a
new *writer*. The rest of the *readers* are still concurrently reading
the content since output stream for the cache entry is again open and
kept by the current *writer*.

Lifetime of an existing entry with only a partial content
---------------------------------------------------------

-  Such a cache entry is first examined in the
   ``nsICacheEntryOpenCallback.onCacheEntryCheck`` callback, where it
   has to be checked for completeness.
-  In this case, the ``Content-Length`` (or different indicator) header
   doesn't equal to the data size reported by the cache entry.
-  The consumer then indicates the cache entry needs to be revalidated
   by returning ``ENTRY_NEEDS_REVALIDATION``\ from
   ``onCacheEntryCheck``.
-  This consumer, from the point of view the cache, takes a role of the
   *writer*.
-  Other parallel consumers, if any, are blocked until the *writer*
   calls ``setValid`` on the cache entry.
-  The consumer is then responsible to validate the partial content
   cache entry with the network server and attempt to load the rest of
   the data.
-  When the server responds positively (in case of an HTTP server with a
   206 response code) the *writer* (in this order) opens the output
   stream on the cache entry and calls ``setValid`` to unblock other
   pending openers.
-  Concurrent read/write is engaged.

Lifetime of an existing entry that doesn't pass server revalidation
-------------------------------------------------------------------

-  Such a cache entry is first examined in the
   ``nsICacheEntryOpenCallback.onCacheEntryCheck`` callback, where the
   consumer finds out it must be revalidated with the server before use.
-  The consumer then indicates the cache entry needs to be revalidated
   by returning ``ENTRY_NEEDS_REVALIDATION``\ from
   ``onCacheEntryCheck``.
-  This consumer, from the point of view the cache, takes a role of the
   *writer*.
-  Other parallel consumers, if any, are blocked until the *writer*
   calls ``setValid`` on the cache entry.
-  The consumer is then responsible to validate the partial content
   cache entry with the network server.
-  The server responses with a 200 response which means the cached
   content is no longer valid and a new version must be loaded from the
   network.
-  The *writer* then calls ``recreate``\ on the cache entry. This
   returns a new empty entry to write the meta data and data to, the
   *writer* exchanges its cache entry by this new one and handles it as
   a new one.
-  The *writer* then (in this order) fills the necessary meta data of
   the cache entry, opens the output stream on it and calls
   ``metaDataReady`` on it.
-  Any other pending openers, if any, are now given this new entry to
   examine and read as an existing entry.

Adding a new storage
--------------------

Should there be a need to add a new distinct storage for which the
current scoping model would not be sufficient - use one of the two
following ways:

#. *[preferred]* Add a new ``<Your>Storage`` method on
   :ref:`nsICacheStorageService` and if needed give it any arguments to
   specify the storage scope even more.  Implementation only should need
   to enhance the context key generation and parsing code and enhance
   current - or create new when needed - :ref:`nsICacheStorage`
   implementations to carry any additional information down to the cache
   service.
#. *[*\ **not**\ *preferred]* Add a new argument to
   :ref:`nsILoadContextInfo`; **be careful
   here**, since some arguments on the context may not be known during
   the load time, what may lead to inter-context data leaking or
   implementation problems. Adding more distinction to
   :ref:`nsILoadContextInfo` also affects all existing storages which may
   not be always desirable.

See context keying details for more information.

Threading
---------

The cache API is fully thread-safe.

The cache is using a single background thread where any IO operations
like opening, reading, writing and erasing happen.  Also memory pool
management, eviction, visiting loops happen on this thread.

The thread supports several priority levels. Dispatching to a level with
a lower number is executed sooner then dispatching to higher number
layers; also any loop on lower levels yields to higher levels so that
scheduled deletion of 1000 files will not block opening cache entries.

#. **OPEN_PRIORITY:** except opening priority cache files also file
   dooming happens here to prevent races
#. **READ_PRIORITY:** top level documents and head blocking script cache
   files are open and read as the first
#. **OPEN**
#. **READ:** any normal priority content, such as images are open and
   read here
#. **WRITE:** writes are processed as last, we cache data in memory in
   the mean time
#. **MANAGEMENT:** level for the memory pool and CacheEntry background
   operations
#. **CLOSE:** file closing level
#. **INDEX:** index is being rebuild here
#. **EVICT:** files overreaching the disk space consumption limit are
   being evicted here

NOTE: Special case for eviction - when an eviction is scheduled on the
IO thread, all operations pending on the OPEN level are first merged to
the OPEN_PRIORITY level. The eviction preparation operation - i.e.
clearing of the internal IO state - is then put to the end of the
OPEN_PRIORITY level.  All this happens atomically.

Storage and entries scopes
--------------------------

A *scope key* string used to map the storage scope is based on the
arguments of :ref:`nsILoadContextInfo`. The form is following (currently
pending in `bug
968593 <https://bugzilla.mozilla.org/show_bug.cgi?id=968593>`__):

.. code:: bz_comment_text

   a,b,i1009,p,

-  Regular expression: ``(.([-,]+)?,)*``
-  The first letter is an identifier, identifiers are to be
   alphabetically sorted and always terminate with ','
-  a - when present the scope is belonging to an **anonymous** load
-  b - when present the scope is **in browser element** load
-  i - when present must have a decimal integer value that represents an
   app ID the scope belongs to, otherwise there is no app (app ID is
   considered ``0``)
-  p - when present the scope is of a **private browsing** load, this
   never persists

``CacheStorageService``\ keeps a global hashtable mapped by the *scope
key*. Elements in this global hashtable are hashtables of cache entries.
The cache entries are mapped by concantation of Enhance ID and URI
passed to ``nsICacheStorage.asyncOpenURI``.  So that when an entry is
being looked up, first the global hashtable is searched using the
*scope key*. An entries hashtable is found. Then this entries hashtable
is searched using <enhance-id:><uri> string. The elements in this
hashtable are CacheEntry classes, see below.

The hash tables keep a strong reference to ``CacheEntry`` objects. The
only way to remove ``CacheEntry`` objects from memory is by exhausting a
memory limit for :ref:`Intermediate_Memory_Caching`, what triggers a background
process of purging expired and then least used entries from memory.
Another way is to directly call the
``nsICacheStorageService.purge``\ method. That method is also called
automatically on the ``"memory-pressure"`` indication.

Access to the hashtables is protected by a global lock. We also - in a
thread-safe manner - count the number of consumers keeping a reference
on each entry. The open callback actually doesn't give the consumer
directly the ``CacheEntry`` object but a small wrapper class that
manages the 'consumer reference counter' on its cache entry. This both
mechanisms ensure thread-safe access and also inability to have more
then a single instance of a ``CacheEntry`` for a single
<scope+enhanceID+URL> key.

``CacheStorage``, implementing the :ref:`nsICacheStorage` interface, is
forwarding all calls to internal methods of ``CacheStorageService``
passing itself as an argument.  ``CacheStorageService`` then generates
the *scope key* using the ``nsILoadContextInfo`` of the storage. Note:
CacheStorage keeps a thread-safe copy of ``nsILoadContextInfo`` passed
to a ``*Storage`` method on ``nsICacheStorageService``.

Invoking open callbacks
-----------------------

``CacheEntry``, implementing the ``nsICacheEntry`` interface, is
responsible for managing the cache entry internal state and to properly
invoke ``onCacheEntryCheck`` and ``onCacheEntryAvaiable`` callbacks to
all callers of ``nsICacheStorage.asyncOpenURI``.

-  Keeps a FIFO of all openers.
-  Keeps its internal state like NOTLOADED, LOADING, EMPTY, WRITING,
   READY, REVALIDATING.
-  Keeps the number of consumers keeping a reference to it.
-  Refers a ``CacheFile`` object that holds actual data and meta data
   and, when told to, persists it to the disk.

The openers FIFO is an array of ``CacheEntry::Callback`` objects.
``CacheEntry::Callback`` keeps a strong reference to the opener plus the
opening flags.  ``nsICacheStorage.asyncOpenURI`` forwards to
``CacheEntry::AsyncOpen`` and triggers the following pseudo-code:

**CacheStorage::AsyncOpenURI** - the API entry point:

-  globally atomic:

   -  look a given ``CacheEntry`` in ``CacheStorageService`` hash tables
      up
   -  if not found: create a new one, add it to the proper hash table
      and set its state to NOTLOADED
   -  consumer reference ++

-  call to `CacheEntry::AsyncOpen`
-  consumer reference --

**CacheEntry::AsyncOpen** (entry atomic):

-  the opener is added to FIFO, consumer reference ++ (dropped back
   after an opener is removed from the FIFO)
-  state == NOTLOADED:

   -  state = LOADING
   -  when OPEN_TRUNCATE flag was used:

      -  ``CacheFile`` is created as 'new', state = EMPTY

   -  otherwise:

      -  ``CacheFile`` is created and load on it started
      -  ``CacheEntry::OnFileReady`` notification is now expected

-  state == LOADING: just do nothing and exit
-  call to `CacheEntry::InvokeCallbacks`

**CacheEntry::InvokeCallbacks** (entry atomic):

-  called on:

   -  a new opener has been added to the FIFO via an ``AsyncOpen`` call
   -  asynchronous result of CacheFile open ``CacheEntry::OnFileReady>``
   -  the writer throws the entry away - ``CacheEntry::OnHandleClosed``
   -  the **output stream** of the entry has been **opened** or
      **closed**
   -  ``metaDataReady``\ or ``setValid``\ on the entry has been called
   -  the entry has been **doomed**

-  state == EMPTY:

   -  on OPER_READONLY flag use: onCacheEntryAvailable with
      ``null``\ for the cache entry
   -  otherwise:

      -  state = WRITING
      -  opener is removed from the FIFO and remembered as the current
         '*writer*'
      -  onCacheEntryAvailable with ``aNew = true``\ and this entry is
         invoked (on the caller thread) for the *writer*

-  state == READY:

   -  onCacheEntryCheck with the entry is invoked on the first opener in
      FIFO - on the caller thread if demanded
   -  result == RECHECK_AFTER_WRITE_FINISHED:

      -  opener is left in the FIFO with a flag ``RecheckAfterWrite``
      -  such openers are skipped until the output stream on the entry
         is closed, then ``onCacheEntryCheck`` is re-invoked on them
      -  Note: here is a potential for endless looping when
         RECHECK_AFTER_WRITE_FINISHED is abused

   -  result == ENTRY_NEEDS_REVALIDATION:

      -  state = REVALIDATING, this prevents invocation of any callback
         until ``CacheEntry::SetValid`` is called
      -  continue as in state ENTRY_WANTED (just below)

   -  result == ENTRY_WANTED:

      -  consumer reference ++ (dropped back when the consumer releases
         the entry)
      -  onCacheEntryAvailable is invoked on the opener with
         ``aNew = false``\ and the entry
      -  opener is removed from the FIFO

   -  result == ENTRY_NOT_WANTED:

      -  ``onCacheEntryAvailable`` is invoked on the opener with
         ``null``\ for the entry
      -  opener is removed from the FIFO

-  state == WRITING or REVALIDATING:

   -  do nothing and exit

-  any other value of state is unexpected here (assertion failure)
-  loop this process while there are openers in the FIFO

**CacheEntry::OnFileReady** (entry atomic):

-  load result == failure or the file has not been found on disk (is
   new): state = EMPTY
-  otherwise: state = READY since the cache file has been found and is
   usable containing meta data and data of the entry
-  call to ``CacheEntry::InvokeCallbacks``

**CacheEntry::OnHandleClosed** (entry atomic):

-  Called when any consumer throws the cache entry away
-  If the handle is not the handle given to the current *writer*, then
   exit
-  state == WRITING: the writer failed to call ``metaDataReady`` on the
   entry - state = EMPTY
-  state == REVALIDATING: the writer failed the re-validation process
   and failed to call ``setValid`` on the entry - state = READY
-  call to ``CacheEntry::InvokeCallbacks``

**All consumers release the reference:**

-  the entry may now be purged (removed) from memory when found expired
   or least used on overrun of the :ref:`memory
   pool <Intermediate_Memory_Caching>` limit
-  when this is a disk cache entry, its cached data chunks are released
   from memory and only meta data is kept

Intermediate memory caching
---------------------------

Intermediate memory caching of frequently used metadata (a.k.a. disk cache memory pool).

For the disk cache entries we keep some of the most recent and most used
cache entries' meta data in memory for immediate zero-thread-loop
opening. The default size of this meta data memory pool is only 250kB
and is controlled by a new ``browser.cache.disk.metadata_memory_limit``
preference. When the limit is exceeded, we purge (throw away) first
**expired** and then **least used** entries to free up memory again. 

Only ``CacheEntry`` objects that are already loaded and filled with data
and having the 'consumer reference == 0' (`bug
942835 <https://bugzilla.mozilla.org/show_bug.cgi?id=942835#c3>`__) can
be purged.

The 'least used' entries are recognized by the lowest value of
`frecency <https://wiki.mozilla.org/User:Jesse/NewFrecency?title=User:Jesse/NewFrecency>`__
we re-compute for each entry on its every access. The decay time is
controlled by the ``browser.cache.frecency_half_life_hours`` preference
and defaults to 6 hours. The best decay time will be based on results of
`an experiment <https://bugzilla.mozilla.org/show_bug.cgi?id=986728>`__.

The memory pool is represented by two lists (strong referring ordered
arrays) of ``CacheEntry`` objects:

#. Sorted by expiration time (that default to 0xFFFFFFFF)
#. Sorted by frecency (defaults to 0)

We have two such pools, one for memory-only entries actually
representing the memory-only cache and one for disk cache entries for
which we only keep the meta data.  Each pool has a different limit
checking - the memory cache pool is controlled by
``browser.cache.memory.capacity``, the disk entries pool is already
described above. The pool can be accessed and modified only on the cache
background thread.
