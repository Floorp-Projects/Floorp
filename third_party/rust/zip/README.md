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

* Most of ZIP64, although there is some support for archives with more than 65535 files
* Encryption
* Multi-disk

We aim to support rust versions 1.20+.

Usage
-----

With all default features:

```toml
[dependencies]
zip = "0.3"
```

Without the default features:

```toml
[dependencies]
zip = { version = "0.3", default-features = false }
```

Examples
--------

See the [examples directory](examples) for:
   * How to write a file to a zip.
   * how to write a directory of files to a zip (using [walkdir](https://github.com/BurntSushi/walkdir)).
   * How to extract a zip file.
   * How to extract a single file from a zip.
