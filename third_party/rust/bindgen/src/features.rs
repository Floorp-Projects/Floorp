//! Contains code for selecting features

#![deny(missing_docs)]
#![deny(warnings)]
#![deny(unused_extern_crates)]

use std::io;
use std::str::FromStr;

/// Define RustTarget struct definition, Default impl, and conversions
/// between RustTarget and String.
macro_rules! rust_target_def {
    ( $( $( #[$attr:meta] )* => $release:ident => $value:expr; )* ) => {
        /// Represents the version of the Rust language to target.
        ///
        /// To support a beta release, use the corresponding stable release.
        ///
        /// This enum will have more variants added as necessary.
        #[derive(Debug, Copy, Clone, Eq, PartialEq, PartialOrd, Hash)]
        #[allow(non_camel_case_types)]
        pub enum RustTarget {
            $(
                $(
                    #[$attr]
                )*
                $release,
            )*
        }

        impl Default for RustTarget {
            /// Gives the latest stable Rust version
            fn default() -> RustTarget {
                LATEST_STABLE_RUST
            }
        }

        impl FromStr for RustTarget {
            type Err = io::Error;

            /// Create a `RustTarget` from a string.
            ///
            /// * The stable/beta versions of Rust are of the form "1.0",
            /// "1.19", etc.
            /// * The nightly version should be specified with "nightly".
            fn from_str(s: &str) -> Result<Self, Self::Err> {
                match s.as_ref() {
                    $(
                        stringify!($value) => Ok(RustTarget::$release),
                    )*
                    _ => Err(
                        io::Error::new(
                            io::ErrorKind::InvalidInput,
                            concat!(
                                "Got an invalid rust target. Accepted values ",
                                "are of the form ",
                                "\"1.0\" or \"nightly\"."))),
                }
            }
        }

        impl From<RustTarget> for String {
            fn from(target: RustTarget) -> Self {
                match target {
                    $(
                        RustTarget::$release => stringify!($value),
                    )*
                }.into()
            }
        }
    }
}

/// Defines an array slice with all RustTarget values
macro_rules! rust_target_values_def {
    ( $( $( #[$attr:meta] )* => $release:ident => $value:expr; )* ) => {
        /// Strings of allowed `RustTarget` values
        pub static RUST_TARGET_STRINGS: &'static [&str] = &[
            $(
                stringify!($value),
            )*
        ];
    }
}

/// Defines macro which takes a macro
macro_rules! rust_target_base {
    ( $x_macro:ident ) => {
        $x_macro!(
            /// Rust stable 1.0
            => Stable_1_0 => 1.0;
            /// Rust stable 1.19
            => Stable_1_19 => 1.19;
            /// Rust stable 1.20
            => Stable_1_20 => 1.20;
            /// Rust stable 1.21
            => Stable_1_21 => 1.21;
            /// Rust stable 1.25
            => Stable_1_25 => 1.25;
            /// Nightly rust
            => Nightly => nightly;
        );
    }
}

rust_target_base!(rust_target_def);
rust_target_base!(rust_target_values_def);

/// Latest stable release of Rust
pub const LATEST_STABLE_RUST: RustTarget = RustTarget::Stable_1_21;

/// Create RustFeatures struct definition, new(), and a getter for each field
macro_rules! rust_feature_def {
    ( $( $( #[$attr:meta] )* => $feature:ident; )* ) => {
        /// Features supported by a rust target
        #[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
        pub struct RustFeatures {
            $(
                $(
                    #[$attr]
                )*
                pub $feature: bool,
            )*
        }

        impl RustFeatures {
            /// Gives a RustFeatures struct with all features disabled
            fn new() -> Self {
                RustFeatures {
                    $(
                        $feature: false,
                    )*
                }
            }
        }
    }
}

rust_feature_def!(
    /// Untagged unions ([RFC 1444](https://github.com/rust-lang/rfcs/blob/master/text/1444-union.md))
    => untagged_union;
    /// `thiscall` calling convention ([Tracking issue](https://github.com/rust-lang/rust/issues/42202))
    => thiscall_abi;
    /// builtin impls for `Clone` ([PR](https://github.com/rust-lang/rust/pull/43690))
    => builtin_clone_impls;
    /// repr(align) https://github.com/rust-lang/rust/pull/47006
    => repr_align;
    /// associated constants https://github.com/rust-lang/rust/issues/29646
    => associated_const;
);

impl From<RustTarget> for RustFeatures {
    fn from(rust_target: RustTarget) -> Self {
        let mut features = RustFeatures::new();

        if rust_target >= RustTarget::Stable_1_19 {
            features.untagged_union = true;
        }

        if rust_target >= RustTarget::Stable_1_20 {
            features.associated_const = true;
        }

        if rust_target >= RustTarget::Stable_1_21 {
            features.builtin_clone_impls = true;
        }

        if rust_target >= RustTarget::Stable_1_25 {
            features.repr_align = true;
        }

        if rust_target >= RustTarget::Nightly {
            features.thiscall_abi = true;
        }

        features
    }
}

impl Default for RustFeatures {
    fn default() -> Self {
        let default_rust_target: RustTarget = Default::default();
        Self::from(default_rust_target)
    }
}

#[cfg(test)]
mod test {
#![allow(unused_imports)]
    use super::*;

    fn test_target(target_str: &str, target: RustTarget) {
        let target_string: String = target.into();
        assert_eq!(target_str, target_string);
        assert_eq!(target, RustTarget::from_str(target_str).unwrap());
    }

    #[test]
    fn str_to_target() {
        test_target("1.0", RustTarget::Stable_1_0);
        test_target("1.19", RustTarget::Stable_1_19);
        test_target("1.21", RustTarget::Stable_1_21);
        test_target("1.25", RustTarget::Stable_1_25);
        test_target("nightly", RustTarget::Nightly);
    }
}
