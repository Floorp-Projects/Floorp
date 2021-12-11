use std::error::Error;

/// Derives `std::fmt::Display` for structs/enums.
/// Derived implementation outputs empty string.
/// Useful, as a way to formally satisfy `Display` trait bound.
///
/// ## Syntax:
///
/// For regular structs/enums:
///
/// ```
/// enum MyEnum {
///     ...
/// }
///
/// derive_display!(MyEnum);
/// ```
///
/// For generic structs/enums:
///
/// ```
/// struct MyGenericStruct<T, U> {
///     ...
/// }
///
/// derive_display!(MyGenericStruct, T, U);
/// ```
macro_rules! derive_display {
    (@fmt) => {
        fn fmt(&self, f: &mut ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
            write!(f, "")
        }
    };
    ($type:ident) => {
        impl ::std::fmt::Display for $type {
            derive_display!(@fmt);
        }
    };
    ($type:ident, $($type_parameters:ident),*) => {
        impl<$($type_parameters),*> ::std::fmt::Display for $type<$($type_parameters),*> {
            derive_display!(@fmt);
        }
    };
}

mod derives_for_enums_with_source;
mod derives_for_generic_enums_with_source;
mod derives_for_generic_structs_with_source;
mod derives_for_structs_with_source;

#[cfg(feature = "nightly")]
mod nightly;

derive_display!(SimpleErr);
#[derive(Default, Debug, Error)]
struct SimpleErr;
