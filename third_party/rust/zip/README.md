zip-rs
======

[![Build Status](https://travis-ci.org/mvdnes/zip-rs.svg?branch=master)](https://travis-ci.org/mvdnes/zip-rs)
[![Build status](https://ci.appveyor.com/api/projects/status/gsnpqcodg19iu253/branch/master?svg=true)](https://ci.appveyor.com/project/mvdnes/zip-rs/branch/master)
[![Crates.io version](https://img.shields.io/crates/v/zip.svg)](https://crates.io/crates/zip)

[Documentation](http://mvdnes.github.io/rust-docs/zip-rs/zip/index.html)

Info
----

A zip library for rust which supports reading and writing of simple ZIP files.

Supported compression formats:

* stored (i.e. none)
* deflate
* bzip2 (optional, enabled by default)

Currently unsupported zip extensions:

* Encryption
* Multi-disk

We aim to support rust versions 1.20+.

Usage
-----

With all default features:

```toml
[dependencies]
zip = "0.4"
```

Without the default features:

```toml
[dependencies]
zip = { version = "0.4", default-features = false }
```

You can further control the backend of `deflate` compression method with these features:
* `deflate` (enabled by default) uses [miniz_oxide](https://github.com/Frommi/miniz_oxide)
* `deflate-miniz` uses [miniz](https://github.com/richgel999/miniz)
* `deflate-zlib` uses zlib

For example:

```toml
[dependencies]
zip = { version = "0.4", features = ["deflate-zlib"], default-features = false }
```

Examples
--------

See the [examples directory](examples) for:
   * How to write a file to a zip.
   * how to write a directory of files to a zip (using [walkdir](https://github.com/BurntSushi/walkdir)).
   * How to extract a zip file.
   * How to extract a single file from a zip.
   * How to read a zip from the standard input.
