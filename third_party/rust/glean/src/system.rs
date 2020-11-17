// Copyright (c) 2017 The Rust Project Developers
// Licensed under the MIT License.
// Original license:
// https://github.com/RustSec/platforms-crate/blob/ebbd3403243067ba3096f31684557285e352b639/LICENSE-MIT
//
// Permission is hereby granted, free of charge, to any
// person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the
// Software without restriction, including without
// limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice
// shall be included in all copies or substantial portions
// of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
// IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

//! Detect and expose `target_arch` as a constant

#[cfg(target_arch = "aarch64")]
/// `target_arch` when building this crate: `aarch64`
pub const ARCH: &str = "aarch64";

#[cfg(target_arch = "arm")]
/// `target_arch` when building this crate: `arm`
pub const ARCH: &str = "arm";

#[cfg(target_arch = "x86")]
/// `target_arch` when building this crate: `x86`
pub const ARCH: &str = "x86";

#[cfg(target_arch = "x86_64")]
/// `target_arch` when building this crate: `x86_64`
pub const ARCH: &str = "x86_64";

#[cfg(not(any(
    target_arch = "aarch64",
    target_arch = "arm",
    target_arch = "x86",
    target_arch = "x86_64"
)))]
/// `target_arch` when building this crate: unknown!
pub const ARCH: &str = "unknown";
