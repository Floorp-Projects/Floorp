// Copyright (c) 2017 The Rust Project Developers
// Copyright (c) 2018-2020 The Rust Secure Code Working Group
// Licensed under the MIT License.
// Original license:
// https://github.com/rustsec/rustsec/blob/2a080f173ad9d8ac7fa260f0a3a6aebf0000de06/platforms/LICENSE-MIT
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
pub const ARCH: &str = "Unknown";

#[cfg(any(target_os = "macos", target_os = "windows"))]
/// Returns Darwin kernel version for MacOS, or NT Kernel version for Windows
pub fn get_os_version() -> String {
    whatsys::kernel_version().unwrap_or_else(|| "Unknown".to_owned())
}

#[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
/// Returns "Unknown" for platforms other than Linux, MacOS or Windows
pub fn get_os_version() -> String {
    "Unknown".to_owned()
}

#[cfg(target_os = "linux")]
/// Returns Linux kernel version, in the format of <Major>.<Minor> e.g. 5.8
pub fn get_os_version() -> String {
    parse_linux_os_string(whatsys::kernel_version().unwrap_or_else(|| "Unknown".to_owned()))
}

#[cfg(target_os = "linux")]
fn parse_linux_os_string(os_str: String) -> String {
    os_str.split('.').take(2).collect::<Vec<&str>>().join(".")
}

#[test]
#[cfg(target_os = "linux")]
fn parse_fixed_linux_os_string() {
    let alpine_os_string = "4.12.0-rc6-g48ec1f0-dirty".to_owned();
    assert_eq!(parse_linux_os_string(alpine_os_string), "4.12");
    let centos_os_string = "3.10.0-514.16.1.el7.x86_64".to_owned();
    assert_eq!(parse_linux_os_string(centos_os_string), "3.10");
    let ubuntu_os_string = "5.8.0-44-generic".to_owned();
    assert_eq!(parse_linux_os_string(ubuntu_os_string), "5.8");
}
