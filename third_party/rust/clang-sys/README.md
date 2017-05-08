# clang-sys

[![crates.io](https://img.shields.io/crates/v/clang-sys.svg)](https://crates.io/crates/clang-sys)
[![Travis CI](https://travis-ci.org/KyleMayes/clang-sys.svg?branch=master)](https://travis-ci.org/KyleMayes/clang-sys)
[![AppVeyor](https://ci.appveyor.com/api/projects/status/7tv5mjyg55rof356/branch/master?svg=true)](https://ci.appveyor.com/project/KyleMayes/clang-sys-vtvy5/branch/master)

Rust bindings for `libclang`.

If you are interested in a Rust wrapper for these bindings, see
[clang-rs](https://github.com/KyleMayes/clang-rs).

Supported on the stable, beta, and nightly Rust channels.

Released under the Apache License 2.0.

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on contributing to this repository.

## Supported Versions

To target a version of `libclang`, enable one of the following Cargo features:

* `clang_3_5` - requires `libclang` 3.5 or later
  ([Documentation](https://kylemayes.github.io/clang-sys/3_5/clang_sys))
* `clang_3_6` - requires `libclang` 3.6 or later
  ([Documentation](https://kylemayes.github.io/clang-sys/3_6/clang_sys))
* `clang_3_7` - requires `libclang` 3.7 or later
  ([Documentation](https://kylemayes.github.io/clang-sys/3_7/clang_sys))
* `clang_3_8` - requires `libclang` 3.8 or later
  ([Documentation](https://kylemayes.github.io/clang-sys/3_8/clang_sys))
* `clang_3_9` - requires `libclang` 3.9 or later
  ([Documentation](https://kylemayes.github.io/clang-sys/3_9/clang_sys))
* `clang_4_0` - requires `libclang` 4.0 or later
  ([Documentation](https://kylemayes.github.io/clang-sys/4_0/clang_sys))

If you do not enable one of these features, the API provided by `libclang` 3.5 will be available by
default.

## Dependencies

By default, this crate will attempt to link to `libclang` dynamically. In this case, this crate
depends on the `libclang` shared library (`libclang.so` on Linux, `libclang.dylib` on OS X,
`libclang.dll` on Windows). If you want to link to `libclang` statically instead, enable the
`static` Cargo feature. In this case, this crate depends on the LLVM and Clang static libraries. If
you don't want to link to `libclang` at compiletime but instead want to load it at runtime, enable
the `runtime` Cargo feature.

These libraries can be either be installed as a part of Clang or downloaded
[here](http://llvm.org/releases/download.html).

**Note:** Installing `libclang` through a package manager might install the `libclang` shared
library as something like `libclang.so.1` instead of `libclang.so`. In this case, you need to make a
symbolic link from the versioned shared library to `libclang.so`.

**Note:** The downloads for LLVM and Clang 3.8 and later do not include the `libclang.a` static
library. This means you cannot link to any of these versions of `libclang` statically unless you
build it from source.

## Environment Variables

The following environment variables, if set, are used by this crate to find the required libraries
and executables:

* `LLVM_CONFIG_PATH` **(compile time)** - provides a path to an `llvm-config` executable
* `LIBCLANG_PATH` **(compile time)** - provides a path to a directory containing a `libclang` shared
  library
* `LIBCLANG_STATIC_PATH` **(compile time)** - provides a path to a directory containing LLVM and
  Clang static libraries
* `CLANG_PATH` **(run time)** - provides a path to a `clang` executable

## Linking

### Dynamic

First, the `libclang` shared library will be searched for in the directory provided by the
`LIBCLANG_PATH` environment variable if it was set. If this fails, the directory returned by
`llvm-config --libdir` will be searched. If neither of these approaches is successful, a list of
likely directories will be searched (e.g., `/usr/local/lib` on Linux).

On Linux, running an executable that has been dynamically linked to `libclang` may require you to
add a path to `libclang.so` to the `LD_LIBRARY_PATH` environment variable. The same is true on OS
X, except the `DYLD_LIBRARY_PATH` environment variable is used instead.

On Windows, running an executable that has been dynamically linked to `libclang` requires that
`libclang.dll` can be found by the executable at runtime. See
[here](https://msdn.microsoft.com/en-us/library/7d83bc18.aspx) for more information.

### Static

The availability of `llvm-config` is not optional for static linking. Ensure that an instance of
this executable can be found on your system's path or set the `LLVM_CONFIG_PATH` environment
variable. The required LLVM and Clang static libraries will be searched for in the same way as the
shared library is searched for, except the `LIBCLANG_STATIC_PATH` environment variable is used in
place of the `LIBCLANG_PATH` environment variable.

### Runtime

The `clang_sys::load` function is used to load a `libclang` shared library for use in the thread in
which it is called. The `clang_sys::unload` function will unload the `libclang` shared library.
`clang_sys::load` searches for a `libclang` shared library in the same way one is searched for when
linking to `libclang` dynamically at compiletime.
