Build tools for Rust on Fuchsia
===============================

This directory contains a wrapper so that clang can be invoked using a
target-specific command line (as is typically done using Gnu tools for
cross-compiling).

To compile standalone (not part of a build system):

```
clang++ -O --std=c++11 clang_wrapper.cc -o clang_wrapper
ln -s clang_wrapper x86-64-unknown-fuchsia-ar
ln -s clang_wrapper x86-64-unknown-fuchsia-cc
ln -s clang_wrapper aarch64-unknown-fuchsia-ar
ln -s clang_wrapper aarch64-unknown-fuchsia-cc
```

The resulting binaries (`x86-64-unknown-fuchsia-cc` and the like) must be
placed somewhere under the root of the fuchsia tree.

The wrapper sets the target triple appropriately, and also finds the
appropriate sysroot for the given target (necessary for linking).

Note: this wrapper is provisional, hopefully to be supplanted by a more
general config mechanism in LLVM
(see [relevant LLVM patch](https://reviews.llvm.org/D24933)).
