.. taskcluster_caches:

Caches
======

There are various caches used by the in-tree tasks. This page attempts to
document them and their appropriate use.

Caches are essentially isolated filesystems that are persisted between
tasks. For example, if 2 tasks run on a worker - one after the other -
and both tasks request the same cache, the subsequent task will be
able to see files in the cache that were created by the first task.
It's also worth noting that TaskCluster workers ensure a cache can only
be used by 1 task at a time. If a worker is simultaneously running
multiple tasks requesting the same named cache, the worker will
have multiple caches of the same name on the worker.

Caches and ``run-task``
-----------------------

``run-task`` is our generic task wrapper script. It does common activities
like ensure a version control checkout is present.

One of the roles of ``run-task`` is to verify and sanitize caches.
It does this by storing state in a cache on its first use. If the recorded
*capabilities* of an existing cache don't match expectations for the
current task, ``run-task`` bails. This ensures that caches are only
reused by tasks with similar execution *profiles*. This prevents
accidental cache use across incompatible tasks. It also allows run-task
to make assumptions about the state of caches, which can help avoid
costly operations.

In addition, the hash of ``run-task`` is used to derive the cache name.
So any time ``run-task`` changes, a new set of caches are used. This
ensures that any backwards incompatible changes or bug fixes to
``run-task`` result in fresh caches.

Some caches are reserved for use with run-task. That property will be denoted
below.

Common Caches
-------------

Version Control Caches
::::::::::::::::::::::

``level-{{level}}-checkouts-{{hash}}``
   This cache holds version control checkouts, each in a subdirectory named
   after the repo (e.g., ``gecko``).

   Checkouts should be read-only. If a task needs to create new files from
   content of a checkout, this content should be written in a separate
   directory/cache (like a workspace).

   This cache name pattern is managed by ``run-task`` and must only be
   used with ``run-task``.

``level-{{level}}-checkouts-sparse-{{hash}}``
   This is like the above but is used when the checkout is sparse (contains
   a subset of files).

``level-{{level}}-checkouts-{{version}}`` (deprecated)
   This cache holds version control checkouts, each in a subdirectory named
   after the repo (e.g., ``gecko``).

   Checkouts should be read-only. If a task needs to create new files from
   content of a checkout, this content should be written in a separate
   directory/cache (like a workspace).

   A ``version`` parameter appears in the cache name to allow
   backwards-incompatible changes to the cache's behavior.

   The ``hg-store`` contains a `shared store <https://www.mercurial-scm.org/wiki/ShareExtension>`
   that is is used by ``hg robustcheckout``. If you are using ``run-task`` you
   should set the ``HG_STORE_PATH`` environment variable to point to this
   directory. If you are using ``hg robustcheckout``, pass this directory to the
   ``--sharebase`` option.

Workspace Caches
::::::::::::::::

``level-{{level}}-*-workspace``
   These caches (of various names typically ending with ``workspace``)
   contain state to be shared between task invocations. Use cases are
   dependent on the task.

Other
:::::

``level-{{level}}-tooltool-cache-{{hash}}``
   Tooltool invocations should use this cache. Tooltool will store files here
   indexed by their hash.

   This cache name pattern is reserved for use with ``run-task`` and must only
   be used by ``run-task``

``tooltool-cache`` (deprecated)
   Legacy location for tooltool files. Use the per-level one instead.
