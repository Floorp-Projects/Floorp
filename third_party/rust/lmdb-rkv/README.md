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

## Publishing to crates.io

To publish the lmdb-rkv-sys crate to crates.io:

```bash
git clone --recursive git@github.com:mozilla/lmdb-rs.git
cd lmdb-rs/lmdb-sys
# Update the version string in lmdb-sys/Cargo.toml and lmdb-sys/src/lib.rs.
cargo publish
git tag lmdb-rkv-sys-$VERSION # where $VERSION is the updated version string
git push git@github.com:mozilla/lmdb-rs.git --tags
```

To publish the lmdb-rkv crate to crates.io:

```bash
git clone --recursive git@github.com:mozilla/lmdb-rs.git
cd lmdb-rs
# Update the version string in Cargo.toml and src/lib.rs and temporarily change
# the lmdb-rkv-sys dependency in Cargo.toml to the latest version on crates.io.
cargo publish
git tag $VERSION # where $VERSION is the updated version string
git push git@github.com:mozilla/lmdb-rs.git --tags
# Change the lmdb-rkv-sys dependency in Cargo.toml back to a path dependency
# on the ./lmdb-sys directory.
```

## Features

* [x] lmdb-sys.
* [x] Cursors.
* [x] Zero-copy put API.
* [x] Nested transactions.
* [x] Database statistics.
