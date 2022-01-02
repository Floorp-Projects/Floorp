//! Macro for opaque `Debug` trait implementation.
#![no_std]

/// Macro for defining opaque `Debug` implementation.
///
/// It will use the following format: "StructName { ... }". While it's
/// convinient to have it (e.g. for including into other structs), it could be
/// undesirable to leak internall state, which can happen for example through
/// uncareful logging.
#[macro_export]
macro_rules! impl_opaque_debug {
    ($struct:ty) => {
        #[cfg(feature = "std")]
        impl ::std::fmt::Debug for $struct {
            fn fmt(&self, f: &mut ::std::fmt::Formatter)
                -> Result<(), ::std::fmt::Error>
            {
                write!(f, concat!(stringify!($struct), " {{ ... }}"))
            }
        }

        #[cfg(not(feature = "std"))]
        impl ::core::fmt::Debug for $struct {
            fn fmt(&self, f: &mut ::core::fmt::Formatter)
                -> Result<(), ::core::fmt::Error>
            {
                write!(f, concat!(stringify!($struct), " {{ ... }}"))
            }
        }
    }
}
