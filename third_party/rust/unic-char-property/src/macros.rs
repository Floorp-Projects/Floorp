// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/// Macro for declaring a character property.
///
/// # Syntax (Enumerated Property)
///
/// ```
/// #[macro_use]
/// extern crate unic_char_property;
///
/// // First we define the type itself.
/// char_property! {
///     /// This is the enum type created for the character property.
///     pub enum MyProp {
///         abbr => "AbbrPropName";
///         long => "Long_Property_Name";
///         human => "Human-Readable Property Name";
///
///         /// Zero or more documentation or other attributes.
///         RustName {
///             abbr => AbbrName,
///             long => Long_Name,
///             human => "&'static str that is a nicer presentation of the name",
///         }
///     }
///
///     /// Module aliasing property value abbreviated names.
///     pub mod abbr_names for abbr;
///
///     /// Module aliasing property value long names.
///     pub mod long_names for long;
/// }
///
/// // We also need to impl `PartialCharProperty` or `TotalCharProperty` manually.
/// # impl unic_char_property::PartialCharProperty for MyProp {
/// #     fn of(_: char) -> Option<Self> { None }
/// # }
/// #
/// # fn main() {}
/// ```
///
/// # Syntax (Binary Property)
///
/// ```
/// #[macro_use] extern crate unic_char_property;
/// # #[macro_use] extern crate unic_char_range;
///
/// char_property! {
///     /// This is the newtype used for the character property.
///     pub struct MyProp(bool) {
///         abbr => "AbbrPropName";
///         long => "Long_Property_Name";
///         human => "Human-Readable Property Name";
///
///         // Unlike an enumerated property, a binary property will handle the table for you.
///         data_table_path => "../tests/tables/property_table.rsv";
///     }
///
///     /// A function that returns whether the given character has the property or not.
///     pub fn is_prop(char) -> bool;
/// }
///
/// // You may also want to create a trait for easy access to the properties you define.
/// # fn main() {}
/// ```
///
/// # Effect
///
/// - Implements the `CharProperty` trait and appropriate range trait
/// - Implements `FromStr` accepting either the abbr or long name, ascii case insensitive
/// - Implements `Display` using the `human` string
/// - Populates the module `abbr_names` with `pub use` bindings of variants to their abbr names
///   (Enumerated properties only)
/// - Populates the module `long_names` with `pub use` bindings of variants to their long names
///   (Enumerated properties only)
/// - Maintains all documentation comments and other `#[attributes]` as would be expected
///   (with some limitations, listed below)
///
#[macro_export]
macro_rules! char_property {

    // == Enumerated Property == //

    (
        $(#[$prop_meta:meta])*
        pub enum $prop_name:ident {
            abbr => $prop_abbr:expr;
            long => $prop_long:expr;
            human => $prop_human:expr;

            $(
                $(#[$variant_meta:meta])*
                $variant_name:ident {
                    abbr => $variant_abbr:ident,
                    long => $variant_long:ident,
                    human => $variant_human:expr,
                }
            )*
        }

        $(#[$abbr_mod_meta:meta])*
        pub mod $abbr_mod:ident for abbr;

        $(#[$long_mod_meta:meta])*
        pub mod $long_mod:ident for long;

    ) => {
        $(#[$prop_meta])*
        #[allow(bad_style)]
        #[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
        pub enum $prop_name {
            $( $(#[$variant_meta])* $variant_name, )*
        }

        $(#[$abbr_mod_meta])*
        #[allow(bad_style)]
        pub mod $abbr_mod {
            $( pub use super::$prop_name::$variant_name as $variant_abbr; )*
        }

        $(#[$long_mod_meta])*
        #[allow(bad_style)]
        pub mod $long_mod {
            $( pub use super::$prop_name::$variant_name as $variant_long; )*
        }

        char_property! {
            __impl FromStr for $prop_name;
            $(
                stringify!($variant_abbr) => $prop_name::$variant_name;
                stringify!($variant_long) => $prop_name::$variant_name;
            )*
        }

        char_property! {
            __impl CharProperty for $prop_name;
            $prop_abbr;
            $prop_long;
            $prop_human;
        }

        char_property! {
            __impl Display for $prop_name by EnumeratedCharProperty
        }

        impl $crate::EnumeratedCharProperty for $prop_name {
            fn all_values() -> &'static [$prop_name] {
                const VALUES: &[$prop_name] = &[
                    $( $prop_name::$variant_name, )*
                ];
                VALUES
            }
            fn abbr_name(&self) -> &'static str {
                match *self {
                    $( $prop_name::$variant_name => stringify!($variant_abbr), )*
                }
            }
            fn long_name(&self) -> &'static str {
                match *self {
                    $( $prop_name::$variant_name => stringify!($variant_long), )*
                }
            }
            fn human_name(&self) -> &'static str {
                match *self {
                    $( $prop_name::$variant_name => $variant_human, )*
                }
            }
        }
    };

    // == Binary Property == //

    (
        $(#[$prop_meta:meta])*
        pub struct $prop_name:ident(bool) {
            abbr => $prop_abbr:expr;
            long => $prop_long:expr;
            human => $prop_human:expr;

            data_table_path => $data_path:expr;
        }

        $(#[$is_fn_meta:meta])*
        pub fn $is_fn:ident(char) -> bool;

    ) => {
        $(#[$prop_meta])*
        #[derive(Copy, Clone, Debug, Default, Eq, PartialEq, Hash)]
        pub struct $prop_name(bool);

        $(#[$is_fn_meta])*
        pub fn $is_fn(ch: char) -> bool {
            $prop_name::of(ch).as_bool()
        }

        impl $prop_name {
            /// Get (struct) property value of the character.
            pub fn of(ch: char) -> Self {
                use $crate::tables::CharDataTable;
                const TABLE: CharDataTable<()> = include!($data_path);
                $prop_name(TABLE.contains(ch))
            }

            /// Get boolean property value of the character.
            pub fn as_bool(&self) -> bool { self.0 }
        }

        char_property! {
            __impl FromStr for $prop_name;
            // Yes
            "y" => $prop_name(true);
            "yes" => $prop_name(true);
            "t" => $prop_name(true);
            "true" => $prop_name(true);
            // No
            "n" => $prop_name(false);
            "no" => $prop_name(false);
            "f" => $prop_name(false);
            "false" => $prop_name(false);
        }

        char_property! {
            __impl CharProperty for $prop_name;
            $prop_abbr;
            $prop_long;
            $prop_human;
        }

        impl $crate::TotalCharProperty for $prop_name {
            fn of(ch: char) -> Self { Self::of(ch) }
        }

        impl $crate::BinaryCharProperty for $prop_name {
            fn as_bool(&self) -> bool { self.as_bool() }
        }

        impl From<$prop_name> for bool {
            fn from(prop: $prop_name) -> bool { prop.as_bool() }
        }

        char_property! {
            __impl Display for $prop_name by BinaryCharProperty
        }
    };

    // == Shared == //

    (
        __impl CharProperty for $prop_name:ident;
        $prop_abbr:expr;
        $prop_long:expr;
        $prop_human:expr;
    ) => {
        impl $crate::CharProperty for $prop_name {
            fn prop_abbr_name() -> &'static str { $prop_abbr }
            fn prop_long_name() -> &'static str { $prop_long }
            fn prop_human_name() -> &'static str { $prop_human }
        }
    };

    (
        __impl FromStr for $prop_name:ident;
        $( $id:expr => $value:expr; )*
    ) => {
        #[allow(unreachable_patterns)]
        impl $crate::__str::FromStr for $prop_name {
            type Err = ();
            fn from_str(s: &str) -> Result<Self, Self::Err> {
                match s {
                    $( $id => Ok($value), )*
                    $( s if s.eq_ignore_ascii_case($id) => Ok($value), )*
                    _ => Err(()),
                }
            }
        }
    };

    ( __impl Display for $prop_name:ident by $trait:ident ) => {
        impl $crate::__fmt::Display for $prop_name {
            fn fmt(&self, f: &mut $crate::__fmt::Formatter) -> $crate::__fmt::Result {
                $crate::$trait::human_name(self).fmt(f)
            }
        }
    };
}
