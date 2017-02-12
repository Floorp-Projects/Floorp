//! Project Changelog

/// Release 0.3.0 (2017-02-10)
///
/// # Breaking changes
///
/// * The `Error` enumeration gained a new variant `CustomTargetsUnsupported` to signify the fact
///   this particular build of the crate does not support custom JSON targets.
///
/// # Other changes
///
/// * `serde_json` is now an enabled-by-default optional dependency. If custom target support is
/// not necessary in your project, it can be disabled with [`default-features =
/// false`](http://doc.crates.io/specifying-dependencies.html#choosing-features).
pub mod r0_3_0 {}

/// Release 0.2.1 (2017-02-03)
///
/// * Upgrade serde to 0.9
pub mod r0_2_1 {}

/// Release 0.2.0 (2017-01-18)
///
/// # Breaking changes
///
/// * `TargetInfo::target_vendor` changed signature to return `Option<&str>` instead of `&str`.
/// Non-nightly rustc doesn’t give the information about target vendor, so it is not available when
/// compiling with stable/beta rustc.
///
/// # Other changes
///
/// * Added `TargetInfo::target_cfg`. Can be used to emulate e.g. `#[cfg(unix)]` or
/// `#[cfg(windows)]`.
/// * Added `TargetInfo::target_cfg_value`. Can be used to extract more obscure target properties
/// such as `#[cfg(target_has_atomic = "64")]`. Note that many of these depend on rustc channel,
/// just like `target_vendor`.
pub mod r0_2_0 {}

/// Release 0.1.2 (2016-10-17)
///
/// * Now figures out target info from the rustc that is used to compile the library. This results
/// in less divergence between versions of rustc (i.e. when targets are added), but is not able to
/// provide target info for some targets on some hosts anymore. For example all `*-apple-ios`
/// targets are not available anymore on the linux host.
/// * `Error` implements `std::error::Error` trait now.
pub mod r0_1_2 {}
