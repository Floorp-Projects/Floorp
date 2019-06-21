//! This tiny crate checks that the running or installed `rustc` meets some
//! version requirements. The version is queried by calling the Rust compiler
//! with `--version`. The path to the compiler is determined first via the
//! `RUSTC` environment variable. If it is not set, then `rustc` is used. If
//! that fails, no determination is made, and calls return `None`.
//!
//! # Example
//!
//! Check that the running compiler is a nightly release:
//!
//! ```rust
//! extern crate version_check;
//!
//! match version_check::is_nightly() {
//!     Some(true) => "running a nightly",
//!     Some(false) => "not nightly",
//!     None => "couldn't figure it out"
//! };
//! ```
//!
//! Check that the running compiler is at least version `1.13.0`:
//!
//! ```rust
//! extern crate version_check;
//!
//! match version_check::is_min_version("1.13.0") {
//!     Some((true, version)) => format!("Yes! It's: {}", version),
//!     Some((false, version)) => format!("No! {} is too old!", version),
//!     None => "couldn't figure it out".into()
//! };
//! ```
//!
//! Check that the running compiler was released on or after `2016-12-18`:
//!
//! ```rust
//! extern crate version_check;
//!
//! match version_check::is_min_date("2016-12-18") {
//!     Some((true, date)) => format!("Yes! It's: {}", date),
//!     Some((false, date)) => format!("No! {} is too long ago!", date),
//!     None => "couldn't figure it out".into()
//! };
//! ```
//!
//! # Alternatives
//!
//! This crate is dead simple with no dependencies. If you need something more
//! and don't care about panicking if the version cannot be obtained or adding
//! dependencies, see [rustc_version](https://crates.io/crates/rustc_version).

use std::env;
use std::process::Command;

// Convert a string of %Y-%m-%d to a single u32 maintaining ordering.
fn str_to_ymd(ymd: &str) -> Option<u32> {
    let ymd: Vec<u32> = ymd.split("-").filter_map(|s| s.parse::<u32>().ok()).collect();
    if ymd.len() != 3 {
        return None
    }

    let (y, m, d) = (ymd[0], ymd[1], ymd[2]);
    Some((y << 9) | (m << 5) | d)
}

// Convert a string with prefix major-minor-patch to a single u64 maintaining
// ordering. Assumes none of the components are > 1048576.
fn str_to_mmp(mmp: &str) -> Option<u64> {
    let mut mmp: Vec<u16> = mmp.split('-')
        .nth(0)
        .unwrap_or("")
        .split('.')
        .filter_map(|s| s.parse::<u16>().ok())
        .collect();

    if mmp.is_empty() {
        return None
    }

    while mmp.len() < 3 {
        mmp.push(0);
    }

    let (maj, min, patch) = (mmp[0] as u64, mmp[1] as u64, mmp[2] as u64);
    Some((maj << 32) | (min << 16) | patch)
}

/// Returns (version, date) as available.
fn version_and_date_from_rustc_version(s: &str) -> (Option<String>, Option<String>) {
    let last_line = s.lines().last().unwrap_or(s);
    let mut components = last_line.trim().split(" ");
    let version = components.nth(1);
    let date = components.nth(1).map(|s| s.trim_right().trim_right_matches(")"));
    (version.map(|s| s.to_string()), date.map(|s| s.to_string()))
}

/// Returns (version, date) as available.
fn get_version_and_date() -> Option<(Option<String>, Option<String>)> {
    env::var("RUSTC").ok()
        .and_then(|rustc| Command::new(rustc).arg("--version").output().ok())
        .or_else(|| Command::new("rustc").arg("--version").output().ok())
        .and_then(|output| String::from_utf8(output.stdout).ok())
        .map(|s| version_and_date_from_rustc_version(&s))
}

/// Checks that the running or installed `rustc` was released no earlier than
/// some date.
///
/// The format of `min_date` must be YYYY-MM-DD. For instance: `2016-12-20` or
/// `2017-01-09`.
///
/// If the date cannot be retrieved or parsed, or if `min_date` could not be
/// parsed, returns `None`. Otherwise returns a tuple where the first value is
/// `true` if the installed `rustc` is at least from `min_data` and the second
/// value is the date (in YYYY-MM-DD) of the installed `rustc`.
pub fn is_min_date(min_date: &str) -> Option<(bool, String)> {
    if let Some((_, Some(actual_date_str))) = get_version_and_date() {
        str_to_ymd(&actual_date_str)
            .and_then(|actual| str_to_ymd(min_date).map(|min| (min, actual)))
            .map(|(min, actual)| (actual >= min, actual_date_str))
    } else {
        None
    }
}

/// Checks that the running or installed `rustc` is at least some minimum
/// version.
///
/// The format of `min_version` is a semantic version: `1.3.0`, `1.15.0-beta`,
/// `1.14.0`, `1.16.0-nightly`, etc.
///
/// If the version cannot be retrieved or parsed, or if `min_version` could not
/// be parsed, returns `None`. Otherwise returns a tuple where the first value
/// is `true` if the installed `rustc` is at least `min_version` and the second
/// value is the version (semantic) of the installed `rustc`.
pub fn is_min_version(min_version: &str) -> Option<(bool, String)> {
    if let Some((Some(actual_version_str), _)) = get_version_and_date() {
        str_to_mmp(&actual_version_str)
            .and_then(|actual| str_to_mmp(min_version).map(|min| (min, actual)))
            .map(|(min, actual)| (actual >= min, actual_version_str))
    } else {
        None
    }
}

fn version_channel_is(channel: &str) -> Option<bool> {
    get_version_and_date()
        .and_then(|(version_str_opt, _)|  version_str_opt)
        .map(|version_str| version_str.contains(channel))
}

/// Determines whether the running or installed `rustc` is on the nightly
/// channel.
///
/// If the version could not be determined, returns `None`. Otherwise returns
/// `Some(true)` if the running version is a nightly release, and `Some(false)`
/// otherwise.
pub fn is_nightly() -> Option<bool> {
    version_channel_is("nightly")
}

/// Determines whether the running or installed `rustc` is on the beta channel.
///
/// If the version could not be determined, returns `None`. Otherwise returns
/// `Some(true)` if the running version is a beta release, and `Some(false)`
/// otherwise.
pub fn is_beta() -> Option<bool> {
    version_channel_is("beta")
}

/// Determines whether the running or installed `rustc` is on the dev channel.
///
/// If the version could not be determined, returns `None`. Otherwise returns
/// `Some(true)` if the running version is a dev release, and `Some(false)`
/// otherwise.
pub fn is_dev() -> Option<bool> {
    version_channel_is("dev")
}

/// Determines whether the running or installed `rustc` supports feature flags.
/// In other words, if the channel is either "nightly" or "dev".
///
/// If the version could not be determined, returns `None`. Otherwise returns
/// `Some(true)` if the running version supports features, and `Some(false)`
/// otherwise.
pub fn supports_features() -> Option<bool> {
    match is_nightly() {
        b@Some(true) => b,
        _ => is_dev()
    }
}

#[cfg(test)]
mod tests {
    use super::version_and_date_from_rustc_version;
    use super::str_to_mmp;

    macro_rules! check_mmp {
        ($string:expr => ($x:expr, $y:expr, $z:expr)) => (
            if let Some(mmp) = str_to_mmp($string) {
                let expected = $x << 32 | $y << 16 | $z;
                if mmp != expected {
                    panic!("{:?} didn't parse as {}.{}.{}.", $string, $x, $y, $z);
                }
            } else {
                panic!("{:?} didn't parse for mmp testing.", $string);
            }
        )
    }

    macro_rules! check_version {
        ($s:expr => ($x:expr, $y:expr, $z:expr)) => (
            if let (Some(version_str), _) = version_and_date_from_rustc_version($s) {
                check_mmp!(&version_str => ($x, $y, $z));
            } else {
                panic!("{:?} didn't parse for version testing.", $s);
            }
        )
    }

    #[test]
    fn test_str_to_mmp() {
        check_mmp!("1.18.0" => (1, 18, 0));
        check_mmp!("1.19.0" => (1, 19, 0));
        check_mmp!("1.19.0-nightly" => (1, 19, 0));
        check_mmp!("1.12.2349" => (1, 12, 2349));
        check_mmp!("0.12" => (0, 12, 0));
        check_mmp!("1.12.5" => (1, 12, 5));
        check_mmp!("1.12" => (1, 12, 0));
        check_mmp!("1" => (1, 0, 0));
    }

    #[test]
    fn test_version_parse() {
        check_version!("rustc 1.18.0" => (1, 18, 0));
        check_version!("rustc 1.8.0" => (1, 8, 0));
        check_version!("rustc 1.20.0-nightly" => (1, 20, 0));
        check_version!("rustc 1.20" => (1, 20, 0));
        check_version!("rustc 1.3" => (1, 3, 0));
        check_version!("rustc 1" => (1, 0, 0));
        check_version!("rustc 1.2.5.6" => (1, 2, 5));
        check_version!("rustc 1.5.1-beta" => (1, 5, 1));
        check_version!("rustc 1.20.0-nightly (d84693b93 2017-07-09)" => (1, 20, 0));
        check_version!("rustc 1.20.0 (d84693b93 2017-07-09)" => (1, 20, 0));
        check_version!("rustc 1.20.0 (2017-07-09)" => (1, 20, 0));
        check_version!("rustc 1.20.0-dev (2017-07-09)" => (1, 20, 0));

        check_version!("warning: invalid logging spec 'warning', ignoring it
                        rustc 1.30.0-nightly (3bc2ca7e4 2018-09-20)" => (1, 30, 0));
        check_version!("warning: invalid logging spec 'warning', ignoring it\n
                        rustc 1.30.0-nightly (3bc2ca7e4 2018-09-20)" => (1, 30, 0));
        check_version!("warning: invalid logging spec 'warning', ignoring it
                        warning: something else went wrong
                        rustc 1.30.0-nightly (3bc2ca7e4 2018-09-20)" => (1, 30, 0));
    }
}
