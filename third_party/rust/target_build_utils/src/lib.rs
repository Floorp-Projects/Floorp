//! Utility crate to handle the `TARGET` environment variable passed into build.rs scripts.
//!
//! Unlike rust’s `#[cfg(target…)]` attributes, `build.rs`-scripts do not expose a convenient way
//! to detect the system the code will be built for in a way which would properly support
//! cross-compilation.
//!
//! This crate exposes `target_arch`, `target_vendor`, `target_os` and `target_abi` very much in
//! the same manner as the corresponding `cfg` attributes in Rust do, thus allowing `build.rs`
//! script to adjust the output depending on the target the crate is being built for..
//!
//! Custom target json files are also supported.
//!
//! # Usage
//!
//! This crate is only useful if you’re using a build script (`build.rs`). Add dependency to this
//! crate to your `Cargo.toml` via:
//!
//! ```toml
//! [package]
//! # ...
//! build = "build.rs"
//!
//! [build-dependencies]
//! target_build_utils = "0.1"
//! ```
//!
//! Then write your `build.rs` like this:
//!
//! ```rust,no_run
//! extern crate target_build_utils;
//! use target_build_utils::TargetInfo;
//!
//! fn main() {
//!     let target = TargetInfo::new().expect("could not get target info");
//!     if target.target_os() == "windows" {
//!         // conditional stuff for windows
//!     }
//! }
//! ```
//!
//! Now, when running `cargo build`, your `build.rs` should be aware of the properties of the
//! target system when your crate is being cross-compiled.
#[cfg(feature = "serde_json")]
extern crate serde_json;
extern crate phf;

use std::env;
use std::path::{Path, PathBuf};
use std::ffi::OsString;
use std::borrow::Cow;
use std::borrow::Cow::Borrowed as B;

pub mod changelog;

#[derive(Debug)]
pub enum Error {
    /// The `TARGET` environment variable does not exist or is not valid utf-8
    TargetUnset,
    /// Target was not found
    TargetNotFound,
    /// Custom target JSON was found, but was invalid
    InvalidSpec,
    /// IO error occured during search of JSON target files
    Io(::std::io::Error),
    /// Crate was built without support for custom targets JSON file
    CustomTargetsUnsupported,
}

impl ::std::fmt::Display for Error {
    fn fmt(&self, fmt: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        match *self {
            Error::Io(ref e) => <::std::io::Error as ::std::fmt::Display>::fmt(e, fmt),
            ref e => fmt.write_str(std::error::Error::description(e)),
        }
    }
}

impl ::std::error::Error for Error {
    fn description(&self) -> &str {
        match *self {
            Error::TargetUnset => "TARGET environment variable is not set or is not valid utf-8",
            Error::TargetNotFound => "The requested target was not found",
            Error::InvalidSpec => "Custom target JSON file was not valid",
            Error::Io(ref e) => e.description(),
            Error::CustomTargetsUnsupported => "Support for custom target JSON file was disabled at compilation",
        }
    }

    fn cause(&self) -> Option<&::std::error::Error> {
        match *self {
            Error::Io(ref e) => Some(e),
            _ => None
        }
    }
}


include!(concat!(env!("OUT_DIR"), "/builtins.rs"));

#[derive(Clone, Debug)]
pub struct TargetInfo {
    arch: Cow<'static, str>,
    os: Cow<'static, str>,
    env: Cow<'static, str>,
    endian: Cow<'static, str>,
    pointer_width: Cow<'static, str>,
    // Switches such as `cfg(unix)`
    switches: Cow<'static, [Cow<'static, str>]>,
    // Other keys such as `target_vendor` or `target_has_atomic`
    other_keys: Cow<'static, [(Cow<'static, str>, Cow<'static, str>)]>,
}

impl TargetInfo {
    /// Parse the target info from `TARGET` environment variable
    ///
    /// `TARGET` environment variable is usually set for you in build.rs scripts, therefore this
    /// function is all that’s necessary in majority of cases.
    ///
    /// # Example
    ///
    /// ```rust,no_run
    /// use target_build_utils::TargetInfo;
    /// let target = TargetInfo::new().expect("could not get target");
    /// ```
    pub fn new() -> Result<TargetInfo, Error> {
        env::var("TARGET").map_err(|_| Error::TargetUnset).and_then(|s| TargetInfo::from_str(&s))
    }

    /// Calculate the target info from the provided target value
    ///
    /// String may contain a triple or path to the json file.
    ///
    /// # Example
    ///
    /// ```rust,no_run
    /// use target_build_utils::TargetInfo;
    /// let target = TargetInfo::from_str("x86_64-unknown-linux-gnu")
    ///     .expect("could not get target");
    /// ```
    pub fn from_str(s: &str) -> Result<TargetInfo, Error> {
        #[cfg(feature = "serde_json")]
        fn load_json(path: &Path) -> Result<TargetInfo, Error> {
            use std::fs::File;
            use serde_json as s;
            let f = try!(File::open(path).map_err(|e| Error::Io(e)));
            let json: s::Value = try!(s::from_reader(f).map_err(|_| Error::InvalidSpec));
            let req = |name: &str|
                json.get(name).and_then(|a| a.as_str()).ok_or(Error::InvalidSpec);

            let vendor = json.get("vendor").and_then(|s| s.as_str()).unwrap_or("unknown").into();
            Ok(TargetInfo {
                arch: Cow::Owned(try!(req("arch")).into()),
                os: Cow::Owned(try!(req("os")).into()),
                env: Cow::Owned(json.get("env").and_then(|s| s.as_str()).unwrap_or("").into()),
                endian: Cow::Owned(try!(req("target-endian")).into()),
                pointer_width: Cow::Owned(try!(req("target-pointer-width")).into()),
                switches: B(&[]),
                other_keys: Cow::Owned(vec![(B("target_vendor"), Cow::Owned(vendor))]),
            })
        }

        #[cfg(not(feature = "serde_json"))]
        fn load_json(_: &Path) -> Result<TargetInfo, Error> {
            Err(Error::CustomTargetsUnsupported)
        }

        if let Some(t) = TargetInfo::load_specific(s) {
            return Ok(t);
        }
        let path = Path::new(s);
        if path.is_file() {
            return load_json(&path);
        }
        let path = {
            let mut target = String::from(s);
            target.push_str(".json");
            PathBuf::from(target)
        };
        let target_path = env::var_os("RUST_TARGET_PATH")
                              .unwrap_or(OsString::new());
        for dir in env::split_paths(&target_path) {
            let p =  dir.join(&path);
            if p.is_file() {
                return load_json(&p);
            }
        }
        Err(Error::TargetNotFound)
    }

    fn load_specific(s: &str) -> Option<TargetInfo> {
        BUILTINS.get(s).cloned()
    }
}

impl TargetInfo {
    /// Architecture of the targeted machine
    ///
    /// Corresponds to the `#[cfg(target_arch = {})]` in Rust code.
    pub fn target_arch(&self) -> &str {
        &*self.arch
    }
    /// OS of the targeted machine
    ///
    /// Corresponds to the `#[cfg(target_os = {})]` in Rust code.
    pub fn target_os(&self) -> &str {
        &*self.os
    }
    /// Environment (ABI) of the targeted machine
    ///
    /// Corresponds to the `#[cfg(target_env = {})]` in Rust code.
    pub fn target_env(&self) -> &str {
        &*self.env
    }
    /// Endianess of the targeted machine
    ///
    /// Valid values are: `little` and `big`.
    ///
    /// Corresponds to the `#[cfg(target_endian = {})]` in Rust code.
    pub fn target_endian(&self) -> &str {
        &*self.endian
    }
    /// Pointer width of the targeted machine
    ///
    /// Corresponds to the `#[cfg(target_pointer_width = {})]` in Rust code.
    pub fn target_pointer_width(&self) -> &str {
        &*self.pointer_width
    }

    /// Vendor of the targeted machine
    ///
    /// Corresponds to the `#[cfg(target_vendor = {})]` in Rust code.
    ///
    /// This currently returns `Some` only when when targetting nightly rustc version as well as
    /// for custom JSON targets.
    pub fn target_vendor(&self) -> Option<&str> {
        self.target_cfg_value("target_vendor")
    }

    /// Check if the configuration switch is set for target
    ///
    /// Corresponds to the `#[cfg({key} = {})]` in Rust code.
    ///
    /// This function behaves specially in regard to custom JSON targets and will always return
    /// `false` for them currently.
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use target_build_utils::TargetInfo;
    /// let info = TargetInfo::new().expect("target info");
    /// let is_unix = info.target_cfg("unix");
    /// ```
    pub fn target_cfg(&self, key: &str) -> bool {
        self.switches.iter().any(|x| x == key)
    }

    /// Return the value of an arbitrary configuration key
    ///
    /// This function behaves specially in regard to custom JSON targets and will rarely return
    /// any extra target information.
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use target_build_utils::TargetInfo;
    /// let info = TargetInfo::new().expect("target info");
    /// assert_eq!(info.target_cfg_value("target_os"), Some(info.target_os()));
    /// assert_eq!(info.target_cfg_value("target_banana"), None);
    /// ```
    pub fn target_cfg_value<'a>(&'a self, key: &str) -> Option<&'a str> {
        match key {
            "target_arch" => Some(self.target_arch()),
            "target_os" => Some(self.target_os()),
            "target_env" => Some(self.target_env()),
            "target_endian" => Some(self.target_endian()),
            "target_pointer_width" => Some(self.target_pointer_width()),
            key => self.other_keys.iter().find(|t| t.0 == key).map(|t| &*t.1)
        }
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn correct_archs() {
        macro_rules! check_arch {
            ($expected: expr, $bit: expr, $end: expr, $($str: expr),+) => {
                $(
                    if let Ok(ti) = super::TargetInfo::from_str($str) {
                        assert_eq!(ti.target_arch(), $expected);
                        assert_eq!(ti.target_endian(), $end);
                        assert_eq!(ti.target_pointer_width(), $bit);
                    }
                )+
            }
        }
        check_arch!("x86_64", "64", "little"
                   , "x86_64-unknown-linux-gnu"
                   , "x86_64-unknown-linux-musl"
                   , "x86_64-unknown-freebsd"
                   , "x86_64-unknown-dragonfly"
                   , "x86_64-unknown-bitrig"
                   , "x86_64-unknown-openbsd"
                   , "x86_64-unknown-netbsd"
                   , "x86_64-rumprun-netbsd"
                   , "x86_64-apple-darwin"
                   , "x86_64-apple-ios"
                   , "x86_64-sun-solaris"
                   , "x86_64-pc-windows-gnu"
                   , "x86_64-pc-windows-msvc");

        check_arch!("x86", "32", "little"
                   , "i586-unknown-linux-gnu"
                   , "i686-unknown-linux-musl"
                   , "i686-linux-android"
                   , "i686-unknown-freebsd"
                   , "i686-unknown-dragonfly"
                   , "i686-apple-darwin"
                   , "i686-pc-windows-gnu"
                   , "i686-pc-windows-msvc"
                   , "i586-pc-windows-msvc"
                   , "i386-apple-ios");
        check_arch!("mips", "32", "big"
                   , "mips-unknown-linux-musl"
                   , "mips-unknown-linux-gnu");
        check_arch!("mips", "32", "little"
                   , "mipsel-unknown-linux-musl"
                   , "mipsel-unknown-linux-gnu");
        check_arch!("aarch64", "64", "little"
                   , "aarch64-unknown-linux-gnu"
                   , "aarch64-linux-android"
                   , "aarch64-apple-ios");
        check_arch!("arm", "32", "little"
                   , "arm-unknown-linux-gnueabi"
                   , "arm-unknown-linux-gnueabihf"
                   , "arm-linux-androideabi"
                   , "armv7-linux-androideabi"
                   , "armv7-apple-ios");
        check_arch!("powerpc", "32", "big", "powerpc-unknown-linux-gnu");
        check_arch!("powerpc64", "64", "big"
                   , "powerpc64-unknown-linux-gnu");
        check_arch!("powerpc64", "64", "little"
                   , "powerpc64le-unknown-linux-gnu");
    }

    #[test]
    fn correct_vendors() {
        macro_rules! check_vnd {
            ($expected: expr, $($str: expr),+) => {
                $(
                    if let Ok(ti) = super::TargetInfo::from_str($str) {
                        if let Some(vnd) = ti.target_vendor() {
                            assert_eq!(vnd, $expected);
                        }
                    }
                )+
            }
        }
        check_vnd!("unknown", "x86_64-unknown-linux-gnu"
                            , "x86_64-unknown-linux-musl"
                            , "x86_64-unknown-freebsd"
                            , "x86_64-unknown-dragonfly"
                            , "x86_64-unknown-bitrig"
                            , "x86_64-unknown-openbsd"
                            , "x86_64-unknown-netbsd"
                            , "i686-unknown-linux-gnu"
                            , "i586-unknown-linux-gnu"
                            , "i686-unknown-linux-musl"
                            , "i686-unknown-freebsd"
                            , "i686-unknown-dragonfly"
                            , "mips-unknown-linux-musl"
                            , "mips-unknown-linux-gnu"
                            , "mipsel-unknown-linux-musl"
                            , "mipsel-unknown-linux-gnu"
                            , "aarch64-unknown-linux-gnu"
                            , "arm-unknown-linux-gnueabi"
                            , "arm-unknown-linux-gnueabihf"
                            , "armv7-unknown-linux-gnueabihf"
                            , "powerpc-unknown-linux-gnu"
                            , "powerpc64-unknown-linux-gnu"
                            , "powerpc64le-unknown-linux-gnu"
                            , "i686-linux-android"
                            , "aarch64-linux-android"
                            , "arm-linux-androideabi"
                            , "armv7-linux-androideabi");
        check_vnd!("apple", "x86_64-apple-darwin"
                          , "x86_64-apple-ios"
                          , "i686-apple-darwin"
                          , "i386-apple-ios"
                          , "aarch64-apple-ios"
                          , "armv7-apple-ios"
                          , "armv7s-apple-ios");
        check_vnd!("pc", "x86_64-pc-windows-gnu"
                       , "x86_64-pc-windows-msvc"
                       , "i686-pc-windows-gnu"
                       , "i686-pc-windows-msvc"
                       , "i586-pc-windows-msvc");
        check_vnd!("rumprun", "x86_64-rumprun-netbsd");
        check_vnd!("sun", "x86_64-sun-solaris");
    }

    #[test]
    fn correct_os() {
        macro_rules! check_os {
            ($expected: expr, $($str: expr),+) => {
                $(
                    if let Ok(ti) = super::TargetInfo::from_str($str) {
                        assert_eq!(ti.target_os(), $expected);
                    }
                )+
            }
        }
        check_os!("linux", "x86_64-unknown-linux-gnu"
                         , "x86_64-unknown-linux-musl"
                         , "i686-unknown-linux-gnu"
                         , "i586-unknown-linux-gnu"
                         , "i686-unknown-linux-musl"
                         , "mips-unknown-linux-musl"
                         , "mips-unknown-linux-gnu"
                         , "mipsel-unknown-linux-musl"
                         , "mipsel-unknown-linux-gnu"
                         , "aarch64-unknown-linux-gnu"
                         , "arm-unknown-linux-gnueabi"
                         , "arm-unknown-linux-gnueabihf"
                         , "armv7-unknown-linux-gnueabihf"
                         , "powerpc-unknown-linux-gnu"
                         , "powerpc64-unknown-linux-gnu"
                         , "powerpc64le-unknown-linux-gnu");
        check_os!("android", "i686-linux-android"
                           , "aarch64-linux-android"
                           , "arm-linux-androideabi"
                           , "armv7-linux-androideabi");
        check_os!("windows", "x86_64-pc-windows-gnu"
                           , "x86_64-pc-windows-msvc"
                           , "i686-pc-windows-gnu"
                           , "i686-pc-windows-msvc"
                           , "i586-pc-windows-msvc");
        check_os!("freebsd", "x86_64-unknown-freebsd"
                           , "i686-unknown-freebsd");
        check_os!("dragonfly", "x86_64-unknown-dragonfly"
                             , "i686-unknown-dragonfly");
        check_os!("bitrig", "x86_64-unknown-bitrig");
        check_os!("openbsd", "x86_64-unknown-openbsd");
        check_os!("netbsd", "x86_64-unknown-netbsd"
                          , "x86_64-rumprun-netbsd");
        check_os!("solaris", "x86_64-sun-solaris");
        check_os!("macos", "x86_64-apple-darwin"
                         , "i686-apple-darwin");
        check_os!("ios", "x86_64-apple-ios"
                       , "i386-apple-ios"
                       , "aarch64-apple-ios"
                       , "armv7-apple-ios"
                       , "armv7s-apple-ios");
    }

    #[test]
    fn correct_env() {
        macro_rules! check_env {
            ($expected: expr, $($str: expr),+) => {
                $(
                    if let Ok(ti) = super::TargetInfo::from_str($str) {
                        assert_eq!(ti.target_env(), $expected);
                    }
                )+
            }
        }
        check_env!("gnu", "x86_64-unknown-linux-gnu"
                        , "i686-unknown-linux-gnu"
                        , "i586-unknown-linux-gnu"
                        , "mips-unknown-linux-gnu"
                        , "mipsel-unknown-linux-gnu"
                        , "aarch64-unknown-linux-gnu"
                        , "arm-unknown-linux-gnueabi"
                        , "arm-unknown-linux-gnueabihf"
                        , "armv7-unknown-linux-gnueabihf"
                        , "powerpc-unknown-linux-gnu"
                        , "powerpc64-unknown-linux-gnu"
                        , "powerpc64le-unknown-linux-gnu"
                        , "x86_64-pc-windows-gnu"
                        , "i686-pc-windows-gnu");
        check_env!("musl", "x86_64-unknown-linux-musl"
                         , "i686-unknown-linux-musl"
                         , "mips-unknown-linux-musl"
                         , "mipsel-unknown-linux-musl");
        check_env!("msvc", "x86_64-pc-windows-msvc"
                         , "i686-pc-windows-msvc"
                         , "i586-pc-windows-msvc");
        check_env!("", "i686-linux-android"
                     , "aarch64-linux-android"
                     , "arm-linux-androideabi"
                     , "armv7-linux-androideabi"
                     , "x86_64-unknown-freebsd"
                     , "i686-unknown-freebsd"
                     , "x86_64-unknown-dragonfly"
                     , "i686-unknown-dragonfly"
                     , "x86_64-unknown-bitrig"
                     , "x86_64-unknown-openbsd"
                     , "x86_64-unknown-netbsd"
                     , "x86_64-rumprun-netbsd"
                     , "x86_64-sun-solaris"
                     , "x86_64-apple-darwin"
                     , "i686-apple-darwin"
                     , "x86_64-apple-ios"
                     , "i386-apple-ios"
                     , "aarch64-apple-ios"
                     , "armv7-apple-ios"
                     , "armv7s-apple-ios"
                     );
    }

    #[test]
    #[cfg(feature = "serde_json")]
    fn external_work() {
        use std::env;
        env::set_var("TARGET", "src/my-great-target.json");
        let target = super::TargetInfo::new().unwrap();
        external_is_correct(&target);
    }

    #[test]
    #[cfg(not(feature = "serde_json"))]
    fn external_work() {
        use std::env;
        env::set_var("TARGET", "src/my-great-target.json");
        super::TargetInfo::new().err().unwrap();
    }

    #[test]
    #[cfg(feature = "serde_json")]
    fn external_search_work() {
        use std::env;
        env::set_var("RUST_TARGET_PATH", "");
        super::TargetInfo::from_str("my-great-target").err().unwrap();
        env::set_var("RUST_TARGET_PATH", env::join_paths(&["/usr/"]).unwrap());
        super::TargetInfo::from_str("my-great-target").err().unwrap();
        env::set_var("RUST_TARGET_PATH", env::join_paths(&["/usr/","src/"]).unwrap());
        let target = super::TargetInfo::from_str("my-great-target").unwrap();
        external_is_correct(&target);
    }

    #[test]
    #[cfg(not(feature = "serde_json"))]
    fn external_search_work() {
        use std::env;
        env::set_var("RUST_TARGET_PATH", "");
        super::TargetInfo::from_str("my-great-target").err().unwrap();
        env::set_var("RUST_TARGET_PATH", env::join_paths(&["/usr/"]).unwrap());
        super::TargetInfo::from_str("my-great-target").err().unwrap();
        env::set_var("RUST_TARGET_PATH", env::join_paths(&["/usr/","src/"]).unwrap());
        super::TargetInfo::from_str("my-great-target").err().unwrap();
    }

    #[cfg(feature = "serde_json")]
    fn external_is_correct(ti: &super::TargetInfo) {
        assert_eq!(ti.target_arch(), "x86_64");
        assert_eq!(ti.target_endian(), "little");
        assert_eq!(ti.target_pointer_width(), "42");
        assert_eq!(ti.target_os(), "nux");
        assert_eq!(ti.target_vendor(), Some("unknown"));
    }
}
