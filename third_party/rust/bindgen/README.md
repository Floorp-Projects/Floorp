# `bindgen`

Automatically generates Rust FFI bindings to C and C++ libraries.

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->


- [Usage](#usage)
  - [Requirements](#requirements)
    - [Installing Clang 3.9](#installing-clang-39)
      - [Windows](#windows)
      - [OSX](#osx)
      - [Debian-based Linuxes](#debian-based-linuxes)
      - [Arch](#arch)
      - [From source](#from-source)
  - [Library usage with `build.rs`](#library-usage-with-buildrs)
    - [`build.rs` Tutorial](#buildrs-tutorial)
    - [Simple Example: `./bindgen-integration`](#simple-example-bindgen-integration)
    - [Real World Example: Stylo](#real-world-example-stylo)
  - [Command Line Usage](#command-line-usage)
  - [C++](#c)
  - [Annotations](#annotations)
    - [`opaque`](#opaque)
    - [`hide`](#hide)
    - [`replaces`](#replaces)
    - [`nocopy`](#nocopy)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## Usage

### Requirements

It is recommended to use Clang 3.9 or greater, however `bindgen` can run with
older Clangs with some features disabled.

#### Installing Clang 3.9

##### Windows

Download and install the official pre-built binary from
[LLVM download page](http://releases.llvm.org/download.html).

##### OSX

If you use Homebrew:

```
$ brew install llvm
```

If you use MacPorts:

```
$ port install clang-3.9
```

##### Debian-based Linuxes

```
# apt-get install llvm-3.9-dev libclang-3.9-dev
```

Ubuntu 16.10 provides the necessary packages directly. If you are using older
version of Ubuntu or other Debian-based distros, you may need to add the LLVM
repos to get version 3.9. See http://apt.llvm.org/.

##### Arch

```
# pacman -S clang
```

##### From source

If your package manager doesn't yet offer Clang 3.9, you'll need to build from
source. For that, follow the instructions
[here](http://clang.llvm.org/get_started.html).

Those instructions list optional steps. For bindgen:

* Checkout and build clang
* Checkout and build the extra-clang-tools
* Checkout and build the compiler-rt
* You do not need to checkout or build libcxx

### Library usage with `build.rs`

💡 This is the recommended way to use `bindgen`. 💡

#### `build.rs` Tutorial

[Here is a step-by-step tutorial for generating FFI bindings to the `bzip2` C library.][tutorial]

[tutorial]: http://fitzgeraldnick.com/2016/12/14/using-libbindgen-in-build-rs.html

#### Simple Example: `./bindgen-integration`

The [`./bindgen-integration`][integration] directory has an example crate that
generates FFI bindings in `build.rs` and can be used a template for new
projects.

[integration]: ./bindgen-integration

#### Real World Example: Stylo

A real world example is [the Stylo build script][stylo-script] used for
integrating Servo's layout system into Gecko.

[stylo-script]: https://github.com/servo/servo/blob/master/components/style/build_gecko.rs

In `Cargo.toml`:

```toml
[package]
# ...
build = "build.rs"

[build-dependencies]
bindgen = "0.20"
```

In `build.rs`:

```rust
extern crate bindgen;

use std::env;
use std::path::Path;

fn main() {
  let out_dir = env::var("OUT_DIR").unwrap();
  let _ = bindgen::builder()
    .header("example.h")
    .use_core()
    .generate().unwrap()
    .write_to_file(Path::new(&out_dir).join("example.rs"));
}
```

In `src/main.rs`:

```rust
include!(concat!(env!("OUT_DIR"), "/example.rs"));
```

### Command Line Usage

```
$ cargo install bindgen
```

There are a few options documented when running `bindgen --help`. Bindgen is installed to `~/.cargo/bin`. You have to add that directory to your path to use `bindgen`.

### C++

`bindgen` can handle most C++ features, but not all of them (C++ is hard!)

Notable C++ features that are unsupported or only partially supported:

* Partial template specialization
* Traits templates
* SFINAE
* Instantiating new template specializations

When passing in header files, the file will automatically be treated as C++ if
it ends in ``.hpp``. If it doesn't, ``-x c++`` can be used to force C++ mode.

You must use whitelisting when working with C++ to avoid pulling in all of the
`std::*` types, some of which `bindgen` cannot handle. Additionally, you may
want to blacklist other types that `bindgen` stumbles on, or make `bindgen`
treat certain types as opaque.

### Annotations

The translation of classes, structs, enums, and typedefs can be adjusted using
annotations. Annotations are specifically formatted html tags inside doxygen
style comments.

#### `opaque`

The `opaque` annotation instructs bindgen to ignore all fields defined in
a struct/class.

```cpp
/// <div rustbindgen opaque></div>
```

#### `hide`

The `hide` annotation instructs bindgen to ignore the struct/class/field/enum
completely.

```cpp
/// <div rustbindgen hide></div>
```

#### `replaces`

The `replaces` annotation can be used to use a type as a replacement for other
(presumably more complex) type. This is used in Stylo to generate bindings for
structures that for multiple reasons are too complex for bindgen to understand.

For example, in a C++ header:

```cpp
/**
 * <div rustbindgen replaces="nsTArray"></div>
 */
template<typename T>
class nsTArray_Simple {
  T* mBuffer;
public:
  // The existence of a destructor here prevents bindgen from deriving the Clone
  // trait via a simple memory copy.
  ~nsTArray_Simple() {};
};
```

That way, after code generation, the bindings for the `nsTArray` type are
the ones that would be generated for `nsTArray_Simple`.

#### `nocopy`

The `nocopy` annotation is used to prevent bindgen to autoderive the `Copy`
and `Clone` traits for a type.
