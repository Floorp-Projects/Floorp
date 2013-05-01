dumbmake
========

*dumbmake* is a simple dependency tracker for make.

It turns lists of make targets into longer lists of make targets that
include dependencies.  For example:

    netwerk, package

might be turned into

    netwerk, netwerk/build, toolkit/library, package

The dependency list is read from the plain text file
`topsrcdir/build/dumbmake-dependencies`.  The format best described by
example:

    build_this
        when_this_changes

Interpret this to mean that `build_this` is a dependency of
`when_this_changes`.  More formally, a line (CHILD) indented more than
the preceding line (PARENT) means that CHILD should trigger building
PARENT.  That is, building CHILD will trigger building first CHILD and
then PARENT.

This structure is recursive:

    build_this_when_either_change
        build_this_only_when
            this_changes

This means that `build_this_when_either_change` is a dependency of
`build_this_only_when` and `this_changes`, and `build_this_only_when`
is a dependency of `this_changes`.  Building `this_changes` will build
first `this_changes`, then `build_this_only_when`, and finally
`build_this_when_either_change`.
