# Libudev Rust Bindings

The `libudev-sys` crate provides declarations and linkage for the `libudev` C library. Following the
`*-sys` package conventions, the `libudev-sys` crate does not define higher-level abstractions over
the native `libudev` library functions.

## Dependencies
In order to use the `libudev-sys` crate, you must have a Linux system with the `libudev` library
installed where it can be found by `pkg-config`. To install `libudev` on Debian-based Linux
distributions, execute the following command:

```
sudo apt-get install libudev-dev
```

`libudev` is a Linux-specific package. It is not available for Windows, OSX, or other operating
systems.

### Cross-Compiling
To link to a cross-compiled version of the native `libudev` library, it's necessary to set several
environment variables to configure `pkg-config` to work with a cross-compiler's sysroot. [Autotools
Mythbuster](https://autotools.io/) has a good explanation of [supporting
cross-compilation](https://autotools.io/pkgconfig/cross-compiling.html) with `pkg-config`.

However, Rust's [`pkg-config` build helper](https://github.com/alexcrichton/pkg-config-rs) doesn't
support calling a `$CHOST`-prefixed `pkg-config`. It will always call `pkg-config` without a prefix.
To cross-compile `libudev-sys` with the `pkg-config` build helper, one must define the environment
variables `PKG_CONFIG_DIR`, `PKG_CONFIG_LIBDIR`, and `PKG_CONFIG_SYSROOT_DIR` for the *default*
`pkg-config`. It's also necessary to set `PKG_CONFIG_ALLOW_CROSS` to tell Rust's `pkg-config` helper
that it's okay to proceed with a cross-compile.

To adapt the `pkg-config` wrapper in the Autotools Mythbuster guide so that it works with Rust, one
will end up with a script similar to the following:

```sh
#!/bin/sh

SYSROOT=/build/root

export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}
export PKG_CONFIG_ALLOW_CROSS=1

cargo build
```

## Usage
Add `libudev-sys` as a dependency in `Cargo.toml`:

```toml
[dependencies]
libudev-sys = "0.1.1"
```

Import the `libudev_sys` crate and use the functions as they're defined in the native `libudev`
library. See the [`libudev` API documention](http://www.freedesktop.org/software/systemd/libudev/)
for more usage information.

```rust
extern crate libudev_sys as ffi;
```

The `libudev-sys` build script detects the presence of native `libudev` functions and exports the
functions found to exist. `libudev-sys` exports this information from its build script, which Cargo
will provide to the build scripts of dependent packages in the form of environment variables:

* `DEP_LIBUDEV_HWDB={true,false}`: The native `libudev` library has `udev_hwdb_*` functions. They will be
  exported by `libudev-sys`.

### Finding Help
Since `libudev-sys` does nothing more than export symbols from the native `libudev` library, the
best source for help is the information already available for the native `libudev`:

* [API Documentation](http://www.freedesktop.org/software/systemd/libudev/)

## License
Copyright © 2015 David Cuddeback

Distributed under the [MIT License](LICENSE).
