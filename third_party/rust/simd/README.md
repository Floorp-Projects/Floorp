# `simd`

[![Build Status](https://travis-ci.org/hsivonen/simd.svg?branch=master)](https://travis-ci.org/hsivonen/simd)
[![crates.io](https://meritbadge.herokuapp.com/simd)](https://crates.io/crates/simd)
[![docs.rs](https://docs.rs/simd/badge.svg)](https://docs.rs/simd/)

`simd` offers a basic interface to the SIMD functionality of CPUs. (Note: This crate fails to build unless the target is aarch64, x86_64, i686 (i.e. SSE2 enabled; not i586) or an ARMv7 target (thumb or not) with NEON enabled.)

This crate is expected to become _obsolete_ once the implementation of [RFC 2366](https://github.com/rust-lang/rfcs/pull/2366) lands in the standard library.

[Documentation](https://docs.rs/simd)
