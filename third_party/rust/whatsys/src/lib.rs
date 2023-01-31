//! What kernel version is running?
//!
//! # Example
//!
//! ```
//! let kernel = whatsys::kernel_version(); // E.g. Some("20.3.0")
//! ```
//!
//! # Supported operating systems
//!
//! We support the following operating systems:
//!
//! * Windows
//! * macOS
//! * Linux
//! * Android
//!
//! # License
//!
//! MIT. Copyright (c) 2021-2022 Jan-Erik Rediger
//!
//! Based on:
//!
//! * [sys-info](https://crates.io/crates/sys-info), [Repository](https://github.com/FillZpp/sys-info-rs), [MIT LICENSE][sys-info-mit]
//! * [sysinfo](https://crates.io/crates/sysinfo), [Repository](https://github.com/GuillaumeGomez/sysinfo), [MIT LICENSE][sysinfo-mit]
//!
//! [sys-info-mit]: https://github.com/FillZpp/sys-info-rs/blob/master/LICENSE
//! [sysinfo-mit]: https://github.com/GuillaumeGomez/sysinfo/blob/master/LICENSE

#![deny(missing_docs)]
#![deny(rustdoc::broken_intra_doc_links)]

cfg_if::cfg_if! {
    if #[cfg(target_os = "macos")] {
        mod apple;
        use apple as system;

    } else if #[cfg(any(target_os = "linux", target_os = "android"))] {
        mod linux;
        use linux as system;
    } else if #[cfg(windows)] {
        mod windows;
        use windows as system;
    } else {
        mod fallback;
        use fallback as system;
    }
}

pub use system::kernel_version;

#[cfg(target_os = "windows")]
pub use system::windows_build_number;

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn gets_a_version() {
        assert!(kernel_version().is_some());
    }

    #[cfg(target_os = "windows")]
    #[test]
    fn test_windows_build_number() {
        let build_number = windows::windows_build_number();
        assert!(build_number.is_some());
    }
}
