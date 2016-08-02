.. taskcluster_caches:

=============
Common Caches
=============

There are various caches used by the in-tree tasks. This page attempts to
document them and their appropriate use.

Version Control Caches
======================

level-{{level}}-hg-shared
   This cache holds Mercurial *stores*: this is the historical data of a
   Mercurial repository (as opposed to a checkout or working directory).

   This cache should ideally be written to by the ``hg robustcheckout``
   command, which is the preferred mechanism to clone a Mercurial repository
   in automation.

   When using ``hg robustcheckout``, data for the same logical repository
   will automatically be stored in the same directory in this cache. e.g.
   data for the Firefox repository will be in the directory
   ``8ba995b74e18334ab3707f27e9eb8f4e37ba3d29``, which is the SHA-1 of the
   first revision in the Firefox repository. This helps ensure that any
   given changeset is only stored once in the cache. This is why there is
   no ``{{project}}`` component in the cache name.

level-{{level}}-checkouts
   This cache holds version control checkouts or working directories. This
   is distinct from the cache that holds the version control *store* data.

   Ideally, checkouts should be read-only. If a task needs to create new
   files from content of a checkout, this content should be written in
   a separate directory/cache (like a workspace).

Workspace Caches
================

level-{{level}}-*-workspace
   These caches (of various names typically ending with ``workspace``)
   contain state to be shared between task invocations. Use cases are
   dependent on the task.
