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

//! Detect and expose `target_os` as a constant.
//!
//! Code adopted from the "platforms" crate: <https://github.com/RustSec/platforms-crate>.

#[cfg(target_os = "android")]
/// `target_os` when building this crate: `android`
pub const OS: &str = "Android";

#[cfg(target_os = "ios")]
/// `target_os` when building this crate: `ios`
pub const OS: &str = "iOS";

#[cfg(target_os = "linux")]
/// `target_os` when building this crate: `linux`
pub const OS: &str = "Linux";

#[cfg(target_os = "macos")]
/// `target_os` when building this crate: `macos`
pub const OS: &str = "Darwin";

#[cfg(target_os = "windows")]
/// `target_os` when building this crate: `windows`
pub const OS: &str = "Windows";

#[cfg(target_os = "freebsd")]
/// `target_os` when building this crate: `freebsd`
pub const OS: &str = "FreeBSD";

#[cfg(target_os = "netbsd")]
/// `target_os` when building this crate: `netbsd`
pub const OS: &str = "NetBSD";

#[cfg(target_os = "openbsd")]
/// `target_os` when building this crate: `openbsd`
pub const OS: &str = "OpenBSD";

#[cfg(target_os = "solaris")]
/// `target_os` when building this crate: `solaris`
pub const OS: &str = "Solaris";

#[cfg(not(any(
    target_os = "android",
    target_os = "ios",
    target_os = "linux",
    target_os = "macos",
    target_os = "windows",
    target_os = "freebsd",
    target_os = "netbsd",
    target_os = "openbsd",
    target_os = "solaris",
)))]
pub const OS: &str = "unknown";
