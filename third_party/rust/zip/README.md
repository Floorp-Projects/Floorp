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

* ZIP64
* Encryption
* Multi-disk

Usage
-----

With all default features:

```toml
[dependencies]
zip = "0.1"
```

Without the default features:

```toml
[dependencies]
zip = { version = "0.1", default-features = false }
```
