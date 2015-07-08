the contents of this dir (testing/mozharness) represent two parts

1) the old way: mozharness.json is a manifest file that locks or "pins" mozharness to a repository and a revision.

2) the new way: an in-gecko-tree copy of mozharness.
    * hgmo/build/mozharness is still live and the defacto read/write repository
    * continuous integration jobs are based on this copy
    * As we transition to dropping support for hg.m.o/build/mozharness, this copy will continue to be synced
    * this copy is currently based on: http://hg.mozilla.org/build/mozharness/rev/4d855a6835ed


