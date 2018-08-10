[![Build Status](https://travis-ci.org/mozilla/lmdb-rs.svg?branch=master)](https://travis-ci.org/mozilla/lmdb-rs)
[![Windows Build status](https://ci.appveyor.com/api/projects/status/id69kkymorycld55/branch/master?svg=true)](https://ci.appveyor.com/project/mykmelez/lmdb-rs-rrsb3/branch/master)

# lmdb-rs

Idiomatic and safe APIs for interacting with the
[Symas Lightning Memory-Mapped Database (LMDB)](http://symas.com/mdb/).

This repo is a fork of [danburkert/lmdb-rs](https://github.com/danburkert/lmdb-rs)
with fixes for issues encountered by [mozilla/rkv](https://github.com/mozilla/rkv).

## Building from Source

```bash
git clone --recursive git@github.com:mozilla/lmdb-rs.git
cd lmdb-rs
cargo build
```

## Features

* [x] lmdb-sys.
* [x] Cursors.
* [x] Zero-copy put API.
* [x] Nested transactions.
* [x] Database statistics.
