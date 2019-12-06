memchr
======
The `memchr` crate provides heavily optimized routines for searching bytes.

[![Build status](https://api.travis-ci.org/BurntSushi/rust-memchr.png)](https://travis-ci.org/BurntSushi/rust-memchr)
[![Build status](https://ci.appveyor.com/api/projects/status/8i9484t8l4w7uql0/branch/master?svg=true)](https://ci.appveyor.com/project/BurntSushi/rust-memchr/branch/master)
[![](http://meritbadge.herokuapp.com/memchr)](https://crates.io/crates/memchr)

Dual-licensed under MIT or the [UNLICENSE](http://unlicense.org).


### Documentation

[https://docs.rs/memchr](https://docs.rs/memchr)


### Overview

The `memchr` function is traditionally provided by libc, however, the
performance of `memchr` can vary significantly depending on the specific
implementation of libc that is used. They can range from manually tuned
Assembly implementations (like that found in GNU's libc) all the way to
non-vectorized C implementations (like that found in MUSL).

To smooth out the differences between implementations of libc, at least
on `x86_64` for Rust 1.27+, this crate provides its own implementation of
`memchr` that should perform competitively with the one found in GNU's libc.
The implementation is in pure Rust and has no dependency on a C compiler or an
Assembler.

Additionally, GNU libc also provides an extension, `memrchr`. This crate
provides its own implementation of `memrchr` as well, on top of `memchr2`,
`memchr3`, `memrchr2` and `memrchr3`. The difference between `memchr` and
`memchr2` is that that `memchr2` permits finding all occurrences of two bytes
instead of one. Similarly for `memchr3`.

### Compiling without the standard library

memchr links to the standard library by default, but you can disable the
`use_std` feature if you want to use it in a `#![no_std]` crate:

```toml
[dependencies]
memchr = { version = "2", default-features = false }
```

On x86 platforms, when the `use_std` feature is disabled, the SSE2
implementation of memchr will be used in compilers that support it. When
`use_std` is enabled, the AVX implementation of memchr will be used if the CPU
is determined to support it at runtime.
