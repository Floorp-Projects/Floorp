//! `bitflags` crate Serde shims
//!
//! To enable to `bitflags` shims, add it as a dependency:
//!
//! ```toml
//! [dependencies]
//! bitflags_serde_shim = "0.2"
//! ```
//!
//! Full example:
//!
//! ```
//! #[macro_use]
//! extern crate serde_derive;
//! extern crate serde_json;
//!
//! #[macro_use]
//! extern crate bitflags;
//! #[macro_use] // required for impl_serde_for_bitflags
//! extern crate bitflags_serde_shim;
//!
//! bitflags! {
//!     // Note that `impl_serde_for_bitflags` requires the flag type to
//!     // implement `Serialize` and `Deserialize`.
//!     //
//!     // All primitive integer types satisfy this requirement.
//!     pub struct Permission: u32 {
//!         const SEND_MESSAGE = 0x00000001;
//!         const EDIT_MESSAGE = 0x00000002;
//!         const KICK_MEMBER  = 0x00000004;
//!         const BAN_MEMBER   = 0x00000008;
//!     }
//! }
//!
//! impl_serde_for_bitflags!(Permission);
//!
//! fn main() {
//!     let test = Permission::SEND_MESSAGE | Permission::EDIT_MESSAGE;
//!
//!     assert_eq!(serde_json::to_string(&test).unwrap(), "3");
//!
//!     assert_eq!(serde_json::from_str::<Permission>("3").unwrap(), test);
//!
//!     assert!(serde_json::from_str::<Permission>("51").is_err());
//! }
//! ```
#![cfg_attr(not(feature = "std"), no_std)]

#[doc(hidden)]
pub extern crate serde;

#[doc(hidden)]
#[cfg(not(feature = "std"))]
pub use core::result::Result;

#[doc(hidden)]
#[cfg(feature = "std")]
pub use std::result::Result;

/// Implements `Serialize` and `Deserialize` for a `bitflags!` generated structure.
///
/// Note that `impl_serde_for_bitflags` requires the flag type to
/// implement `Serialize` and `Deserialize`.
///
/// All primitive integer types satisfy these requirements.
///
/// See the [`bitflags`](../bitflags_serde_shim/index.html) shim for a full example.
#[macro_export]
macro_rules! impl_serde_for_bitflags {
    ($name:ident) => {
        impl $crate::serde::Serialize for $name {
            fn serialize<S>(&self, serializer: S) -> $crate::Result<S::Ok, S::Error>
            where
                S: $crate::serde::Serializer,
            {
                self.bits().serialize(serializer)
            }
        }

        #[cfg(feature = "std")]
        impl<'de> $crate::serde::Deserialize<'de> for $name {
            fn deserialize<D>(deserializer: D) -> $crate::Result<$name, D::Error>
            where
                D: $crate::serde::Deserializer<'de>,
            {
                let value = <_ as $crate::serde::Deserialize<'de>>::deserialize(deserializer)?;

                $name::from_bits(value)
                    .ok_or_else(|| $crate::serde::de::Error::custom(format!("Invalid bits {:#X} for {}", value, stringify!($name))))
            }
        }

        #[cfg(not(feature = "std"))]
        impl<'de> $crate::serde::Deserialize<'de> for $name {
            fn deserialize<D>(deserializer: D) -> $crate::Result<$name, D::Error>
            where
                D: $crate::serde::Deserializer<'de>,
            {
                let value = <_ as $crate::serde::Deserialize<'de>>::deserialize(deserializer)?;

                // Use a 'static str for the no_std version
                $name::from_bits(value)
                    .ok_or_else(|| $crate::serde::de::Error::custom(stringify!(Invalid bits for $name)))
            }
        }
    };
}
