This crate provides a safe interface `libc`'s `memchr` and `memrchr`.
This crate also provides fallback implementations when either function is
unavailable.

[![Build status](https://api.travis-ci.org/BurntSushi/rust-memchr.png)](https://travis-ci.org/BurntSushi/rust-memchr)
[![Build status](https://ci.appveyor.com/api/projects/status/8i9484t8l4w7uql0/branch/master?svg=true)](https://ci.appveyor.com/project/BurntSushi/rust-memchr/branch/master)
[![](http://meritbadge.herokuapp.com/memchr)](https://crates.io/crates/memchr)

Dual-licensed under MIT or the [UNLICENSE](http://unlicense.org).


### Documentation

[https://docs.rs/memchr](https://docs.rs/memchr)

### no_std

memchr links to the standard library by default, but you can disable the
`use_std` feature if you want to use it in a `#![no_std]` crate:

```toml
[dependencies]
memchr = { version = "1.0", default-features = false }
```

### Performance

On my system (Linux/amd64), `memchr` is about an order of magnitude faster than
the more idiomatic `haystack.iter().position(|&b| b == needle)`:

```
test iterator          ... bench:       5,280 ns/iter (+/- 13) = 1893 MB/s
test iterator_reversed ... bench:       5,271 ns/iter (+/- 7) = 1897 MB/s
test libc_memchr       ... bench:         202 ns/iter (+/- 0) = 49504 MB/s
test libc_memrchr      ... bench:         197 ns/iter (+/- 1) = 50761 MB/s
```
