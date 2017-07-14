.. taskcluster_caches:

=============
Common Caches
=============

There are various caches used by the in-tree tasks. This page attempts to
document them and their appropriate use.

Version Control Caches
======================

``level-{{level}}-checkouts-{{version}}``
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

``level-{{level}}-{{project}}-tc-vcs`` (deprecated)
    This cache is used internally by ``tc-vcs``.  This tool is deprecated and
    should be replaced with ``hg robustcheckout``.


Workspace Caches
================

``level-{{level}}-*-workspace``
   These caches (of various names typically ending with ``workspace``)
   contain state to be shared between task invocations. Use cases are
   dependent on the task.

Other
=====

``tooltool-cache``
    Tooltool invocations should use this cache.  Tooltool will store files here
    indexed by their hash, and will verify hashes before copying files from
    this directory, so there is no concern with sharing the cache between jobs
    of different levels.
