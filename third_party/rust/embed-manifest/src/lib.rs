//! The `embed-manifest` crate provides a straightforward way to embed
//! a Windows manifest in an executable, whatever the build environment
//! and even when cross-compiling, without dependencies on external
//! tools from LLVM or MinGW.
//!
//! This should be called from a [build script][1], as shown below.
//!
//! [1]: https://doc.rust-lang.org/cargo/reference/build-scripts.html
//!
//! On MSVC targets, the manifest file is embedded in the executable by
//! instructing Cargo to pass `/MANIFEST` options to `LINK.EXE`. This
//! requires Cargo from Rust 1.56.
//!
//! On GNU targets, the manifest file is added as a resource in a COFF
//! object file, and Cargo is instructed to link this file into the
//! executable, also using functionality from Rust 1.56.
//!
//! # Usage
//!
//! This crate should be added to the `[build-dependencies]` section in
//! your executable’s `Cargo.toml`:
//!
//! ```toml
//! [build-dependencies]
//! embed-manifest = "1.3.1"
//! ```
//!
//! In the same directory, create a `build.rs` file to call this crate’s
//! code when building for Windows, and to only run once:
//!
//! ```
//! use embed_manifest::{embed_manifest, new_manifest};
//!
//! fn main() {
//!     # let tempdir = tempfile::tempdir().unwrap();
//!     # std::env::set_var("OUT_DIR", tempdir.path());
//!     # std::env::set_var("TARGET", "x86_64-pc-windows-gnu");
//!     # std::env::set_var("CARGO_CFG_WINDOWS", "");
//!     if std::env::var_os("CARGO_CFG_WINDOWS").is_some() {
//!         embed_manifest(new_manifest("Contoso.Sample")).expect("unable to embed manifest file");
//!     }
//!     println!("cargo:rerun-if-changed=build.rs");
//! }
//! ```
//!
//! To customise the application manifest, use the methods on it to change things like
//! enabling the segment heap:
//!
//! ```
//! use embed_manifest::{embed_manifest, new_manifest, manifest::HeapType};
//!
//! fn main() {
//!     # let tempdir = tempfile::tempdir().unwrap();
//!     # std::env::set_var("OUT_DIR", tempdir.path());
//!     # std::env::set_var("TARGET", "x86_64-pc-windows-gnu");
//!     # std::env::set_var("CARGO_CFG_WINDOWS", "");
//!     if std::env::var_os("CARGO_CFG_WINDOWS").is_some() {
//!         embed_manifest(new_manifest("Contoso.Sample").heap_type(HeapType::SegmentHeap))
//!             .expect("unable to embed manifest file");
//!     }
//!     println!("cargo:rerun-if-changed=build.rs");
//! }
//! ```
//!
//! or making it always use legacy single-byte API encoding and only declaring compatibility
//! up to Windows 8.1, without checking whether this is a Windows build:
//!
//! ```
//! use embed_manifest::{embed_manifest, new_manifest};
//! use embed_manifest::manifest::{ActiveCodePage::Legacy, SupportedOS::*};
//!
//! fn main() {
//!     # let tempdir = tempfile::tempdir().unwrap();
//!     # std::env::set_var("OUT_DIR", tempdir.path());
//!     # std::env::set_var("TARGET", "x86_64-pc-windows-gnu");
//!     let manifest = new_manifest("Contoso.Sample")
//!         .active_code_page(Legacy)
//!         .supported_os(Windows7..=Windows81);
//!     embed_manifest(manifest).expect("unable to embed manifest file");
//!     println!("cargo:rerun-if-changed=build.rs");
//! }
//! ```

#![allow(clippy::needless_doctest_main)]

pub use embed::error::Error;
pub use embed::{embed_manifest, embed_manifest_file};

use crate::manifest::ManifestBuilder;

mod embed;
pub mod manifest;

/// Creates a new [`ManifestBuilder`] with sensible defaults, allowing customisation
/// before the Windows application manifest XML is generated.
///
/// The initial values used by the manifest are:
/// - Version number from the `CARGO_PKG_VERSION_MAJOR`, `CARGO_PKG_VERSION_MINOR` and
///   `CARGO_PKG_VERSION_PATCH` environment variables. This can then be changed with
///   [`version()`][ManifestBuilder::version].
/// - A dependency on version 6 of the Common Controls so that message boxes and dialogs
///   will use the latest design, and have the best available support for high DPI displays.
///   This can be removed with
///   [`remove_dependency`][ManifestBuilder::remove_dependency].
/// - [Compatible with Windows from 7 to 11][ManifestBuilder::supported_os],
///   matching the Rust compiler [tier 1 targets][tier1].
/// - A “[maximum version tested][ManifestBuilder::max_version_tested]” of Windows 10
///   version 1903.
/// - An [active code page][ManifestBuilder::active_code_page] of UTF-8, so that
///   single-byte Windows APIs will generally interpret Rust strings correctly, starting
///   from Windows 10 version 1903.
/// - [Version 2 of per-monitor high DPI awareness][manifest::DpiAwareness::PerMonitorV2Only],
///   so that user interface elements can be scaled correctly by the application and
///   Common Controls from Windows 10 version 1703.
/// - [Long path awareness][ManifestBuilder::long_path_aware] from Windows 10 version
///   1607 [when separately enabled][longpaths].
/// - [Printer driver isolation][ManifestBuilder::printer_driver_isolation] enabled
///   to improve reliability and security.
/// - An [execution level][ManifestBuilder::requested_execution_level] of “as invoker”
///   so that the UAC elevation dialog will never be displayed, regardless of the name
///   of the program, and [UAC Virtualisation][uac] is disabled in 32-bit programs.
///
/// [tier1]: https://doc.rust-lang.org/nightly/rustc/platform-support.html
/// [longpaths]: https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=cmd#enable-long-paths-in-windows-10-version-1607-and-later
/// [uac]: https://docs.microsoft.com/en-us/windows/security/identity-protection/user-account-control/how-user-account-control-works#virtualization
pub fn new_manifest(name: &str) -> ManifestBuilder {
    ManifestBuilder::new(name)
}

/// Creates a new [`ManifestBuilder`] without any settings, allowing creation of
/// a manifest with only desired content.
pub fn empty_manifest() -> ManifestBuilder {
    ManifestBuilder::empty()
}
