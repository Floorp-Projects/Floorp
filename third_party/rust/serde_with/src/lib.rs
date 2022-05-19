#![deny(
    missing_copy_implementations,
    missing_debug_implementations,
    missing_docs,
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    variant_size_differences
)]
#![warn(rust_2018_idioms)]
#![doc(test(attr(deny(
    missing_copy_implementations,
    missing_debug_implementations,
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    variant_size_differences,
))))]
#![doc(test(attr(warn(rust_2018_idioms))))]
// Not needed for 2018 edition and conflicts with `rust_2018_idioms`
#![doc(test(no_crate_inject))]
#![doc(html_root_url = "https://docs.rs/serde_with/1.6.4")]
// Necessary to silence the warning about clippy::unknown_clippy_lints on nightly
#![allow(renamed_and_removed_lints)]
// Rust 1.45: introduction of `strip_prefix` used by clippy::manual_strip
#![allow(clippy::unknown_clippy_lints)]

//! [![docs.rs badge](https://docs.rs/serde_with/badge.svg)](https://docs.rs/serde_with/)
//! [![crates.io badge](https://img.shields.io/crates/v/serde_with.svg)](https://crates.io/crates/serde_with/)
//! [![Build Status](https://github.com/jonasbb/serde_with/workflows/Rust%20CI/badge.svg)](https://github.com/jonasbb/serde_with)
//! [![codecov](https://codecov.io/gh/jonasbb/serde_with/branch/master/graph/badge.svg)](https://codecov.io/gh/jonasbb/serde_with)
//!
//! ---
//!
//! This crate provides custom de/serialization helpers to use in combination with [serde's with-annotation][with-annotation] and with the improved [`serde_as`][user guide]-annotation.
//! Some common use cases are:
//!
//! * De/Serializing a type using the `Display` and `FromStr` traits, e.g., for `u8`, `url::Url`, or `mime::Mime`.
//!      Check [`DisplayFromStr`][] or [`serde_with::rust::display_fromstr`][display_fromstr] for details.
//! * Skip serializing all empty `Option` types with [`#[skip_serializing_none]`][skip_serializing_none].
//! * Apply a prefix to each field name of a struct, without changing the de/serialize implementations of the struct using [`with_prefix!`][].
//! * Deserialize a comma separated list like `#hash,#tags,#are,#great` into a `Vec<String>`.
//!      Check the documentation for [`serde_with::rust::StringWithSeparator::<CommaSeparator>`][StringWithSeparator].
//!
//! ## Getting Help
//!
//! **Check out the [user guide][user guide] to find out more tips and tricks about this crate.**
//!
//! For further help using this crate you can [open a new discussion](https://github.com/jonasbb/serde_with/discussions/new) or ask on [users.rust-lang.org](https://users.rust-lang.org/).
//! For bugs please open a [new issue](https://github.com/jonasbb/serde_with/issues/new) on Github.
//!
//! # Use `serde_with` in your Project
//!
//! Add this to your `Cargo.toml`:
//!
//! ```toml
//! [dependencies.serde_with]
//! version = "1.6.4"
//! features = [ "..." ]
//! ```
//!
//! The crate contains different features for integration with other common crates.
//! Check the [feature flags][] section for information about all available features.
//!
//! # Examples
//!
//! Annotate your struct or enum to enable the custom de/serializer.
//!
//! ## `DisplayFromStr`
//!
//! ```rust
//! # #[cfg(feature = "macros")]
//! # use serde_derive::{Deserialize, Serialize};
//! # #[cfg(feature = "macros")]
//! # use serde_with::{serde_as, DisplayFromStr};
//! # #[cfg(feature = "macros")]
//! #[serde_as]
//! # #[derive(Debug, Eq, PartialEq)]
//! #[derive(Deserialize, Serialize)]
//! struct Foo {
//!     // Serialize with Display, deserialize with FromStr
//!     #[serde_as(as = "DisplayFromStr")]
//!     bar: u8,
//! }
//!
//! # #[cfg(all(feature = "macros", feature = "json"))] {
//! // This will serialize
//! # let foo =
//! Foo {bar: 12}
//! # ;
//!
//! // into this JSON
//! # let json = r#"
//! {"bar": "12"}
//! # "#;
//! # assert_eq!(json.replace(" ", "").replace("\n", ""), serde_json::to_string(&foo).unwrap());
//! # assert_eq!(foo, serde_json::from_str(&json).unwrap());
//! # }
//! ```
//!
//! ## `skip_serializing_none`
//!
//! This situation often occurs with JSON, but other formats also support optional fields.
//! If many fields are optional, putting the annotations on the structs can become tedious.
//!
//! ```rust
//! # #[cfg(feature = "macros")]
//! # use serde_derive::{Deserialize, Serialize};
//! # #[cfg(feature = "macros")]
//! # use serde_with::skip_serializing_none;
//! # #[cfg(feature = "macros")]
//! #[skip_serializing_none]
//! # #[derive(Debug, Eq, PartialEq)]
//! #[derive(Deserialize, Serialize)]
//! struct Foo {
//!     a: Option<usize>,
//!     b: Option<usize>,
//!     c: Option<usize>,
//!     d: Option<usize>,
//!     e: Option<usize>,
//!     f: Option<usize>,
//!     g: Option<usize>,
//! }
//!
//! # #[cfg(all(feature = "macros", feature = "json"))] {
//! // This will serialize
//! # let foo =
//! Foo {a: None, b: None, c: None, d: Some(4), e: None, f: None, g: Some(7)}
//! # ;
//!
//! // into this JSON
//! # let json = r#"
//! {"d": 4, "g": 7}
//! # "#;
//! # assert_eq!(json.replace(" ", "").replace("\n", ""), serde_json::to_string(&foo).unwrap());
//! # assert_eq!(foo, serde_json::from_str(&json).unwrap());
//! # }
//! ```
//!
//! ## Advanced `serde_as` usage
//!
//! This example is mainly supposed to highlight the flexibility of the `serde_as`-annotation compared to [serde's with-annotation][with-annotation].
//! More details about `serde_as` can be found in the [user guide][].
//!
//! ```rust
//! # #[cfg(all(feature = "macros", feature = "hex"))]
//! # use {
//! #     serde_derive::{Deserialize, Serialize},
//! #     serde_with::{serde_as, DisplayFromStr, DurationSeconds, hex::Hex},
//! #     std::time::Duration,
//! #     std::collections::BTreeMap,
//! # };
//! # #[cfg(all(feature = "macros", feature = "hex"))]
//! #[serde_as]
//! # #[derive(Debug, Eq, PartialEq)]
//! #[derive(Deserialize, Serialize)]
//! struct Foo {
//!      // Serialize them into a list of number as seconds
//!      #[serde_as(as = "Vec<DurationSeconds>")]
//!      durations: Vec<Duration>,
//!      // We can treat a Vec like a map with duplicates.
//!      // JSON only allows string keys, so convert i32 to strings
//!      // The bytes will be hex encoded
//!      #[serde_as(as = "BTreeMap<DisplayFromStr, Hex>")]
//!      bytes: Vec<(i32, Vec<u8>)>,
//! }
//!
//! # #[cfg(all(feature = "macros", feature = "json", feature = "hex"))] {
//! // This will serialize
//! # let foo =
//! Foo {
//!     durations: vec![Duration::new(5, 0), Duration::new(3600, 0), Duration::new(0, 0)],
//!     bytes: vec![
//!         (1, vec![0, 1, 2]),
//!         (-100, vec![100, 200, 255]),
//!         (1, vec![0, 111, 222]),
//!     ],
//! }
//! # ;
//!
//! // into this JSON
//! # let json = r#"
//! {
//!     "durations": [5, 3600, 0],
//!     "bytes": {
//!         "1": "000102",
//!         "-100": "64c8ff",
//!         "1": "006fde"
//!     }
//! }
//! # "#;
//! # assert_eq!(json.replace(" ", "").replace("\n", ""), serde_json::to_string(&foo).unwrap());
//! # assert_eq!(foo, serde_json::from_str(&json).unwrap());
//! # }
//! ```
//!
//! [`DisplayFromStr`]: https://docs.rs/serde_with/1.6.4/serde_with/struct.DisplayFromStr.html
//! [`with_prefix!`]: https://docs.rs/serde_with/1.6.4/serde_with/macro.with_prefix.html
//! [display_fromstr]: https://docs.rs/serde_with/1.6.4/serde_with/rust/display_fromstr/index.html
//! [feature flags]: https://docs.rs/serde_with/1.6.4/serde_with/guide/feature_flags/index.html
//! [skip_serializing_none]: https://docs.rs/serde_with/1.6.4/serde_with/attr.skip_serializing_none.html
//! [StringWithSeparator]: https://docs.rs/serde_with/1.6.4/serde_with/rust/struct.StringWithSeparator.html
//! [user guide]: https://docs.rs/serde_with/1.6.4/serde_with/guide/index.html
//! [with-annotation]: https://serde.rs/field-attrs.html#with

#[doc(hidden)]
pub extern crate serde;

#[cfg(feature = "chrono")]
pub mod chrono;
pub mod de;
mod duplicate_key_impls;
mod flatten_maybe;
pub mod formats;
#[cfg(feature = "hex")]
pub mod hex;
#[cfg(feature = "json")]
pub mod json;
pub mod rust;
pub mod ser;
mod utils;
#[doc(hidden)]
pub mod with_prefix;

// Taken from shepmaster/snafu
// Originally licensed as MIT+Apache 2
// https://github.com/shepmaster/snafu/blob/fd37d79d4531ed1d3eebffad0d658928eb860cfe/src/lib.rs#L121-L165
#[cfg(feature = "guide")]
macro_rules! generate_guide {
    (pub mod $name:ident; $($rest:tt)*) => {
        generate_guide!(@gen ".", pub mod $name { } $($rest)*);
    };
    (pub mod $name:ident { $($children:tt)* } $($rest:tt)*) => {
        generate_guide!(@gen ".", pub mod $name { $($children)* } $($rest)*);
    };
    (@gen $prefix:expr, ) => {};
    (@gen $prefix:expr, pub mod $name:ident; $($rest:tt)*) => {
        generate_guide!(@gen $prefix, pub mod $name { } $($rest)*);
    };
    (@gen $prefix:expr, @code pub mod $name:ident; $($rest:tt)*) => {
        pub mod $name;
        generate_guide!(@gen $prefix, $($rest)*);
    };
    (@gen $prefix:expr, pub mod $name:ident { $($children:tt)* } $($rest:tt)*) => {
        doc_comment::doc_comment! {
            include_str!(concat!($prefix, "/", stringify!($name), ".md")),
            pub mod $name {
                generate_guide!(@gen concat!($prefix, "/", stringify!($name)), $($children)*);
            }
        }
        generate_guide!(@gen $prefix, $($rest)*);
    };
}

#[cfg(feature = "guide")]
generate_guide! {
    pub mod guide {
        pub mod feature_flags;
        pub mod serde_as;
    }
}

#[doc(inline)]
pub use crate::{de::DeserializeAs, rust::StringWithSeparator, ser::SerializeAs};
use serde::{Deserialize, Deserializer, Serialize, Serializer};
// Re-Export all proc_macros, as these should be seen as part of the serde_with crate
#[cfg(feature = "macros")]
#[doc(inline)]
pub use serde_with_macros::*;
use std::marker::PhantomData;

/// Separator for string-based collection de/serialization
pub trait Separator {
    /// Return the string delimiting two elements in the string-based collection
    fn separator() -> &'static str;
}

/// Predefined separator using a single space
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Default)]
pub struct SpaceSeparator;

impl Separator for SpaceSeparator {
    #[inline]
    fn separator() -> &'static str {
        " "
    }
}

/// Predefined separator using a single comma
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Default)]
pub struct CommaSeparator;

impl Separator for CommaSeparator {
    #[inline]
    fn separator() -> &'static str {
        ","
    }
}

/// Adapter to convert from `serde_as` to the serde traits.
///
/// The `As` type adapter allows to use types implementing [`DeserializeAs`][] or [`SerializeAs`][] in place of serde's with-annotation.
/// The with-annotation allows to run custom code when de/serializing, however it is quite inflexible.
/// The traits [`DeserializeAs`][]/[`SerializeAs`][] are more flexible, as they allow composition and nesting of types to create more complex de/serialization behavior.
/// However, they are not directly compatible with serde, as they are not provided by serde.
/// The `As` type adapter makes them compatible, by forwarding the function calls to `serialize`/`deserialize` to the corresponding functions `serialize_as` and `deserialize_as`.
///
/// It is not required to use this type directly.
/// Instead, it is highly encouraged to use the [`#[serde_as]`][serde_as] attribute since it includes further usability improvements.
/// If the use of the use of the proc-macro is not acceptable, then `As` can be used directly with serde.
///
/// ```rust
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_with::{As, DisplayFromStr};
/// #
/// #[derive(Deserialize, Serialize)]
/// # struct S {
/// // Serialize numbers as sequence of strings, using Display and FromStr
/// #[serde(with = "As::<Vec<DisplayFromStr>>")]
/// field: Vec<u8>,
/// # }
/// ```
/// If the normal `Deserialize`/`Serialize` traits should be used, the placeholder type [`Same`] can be used.
/// It implements [`DeserializeAs`][]/[`SerializeAs`][], when the underlying type implements `Deserialize`/`Serialize`.
///
/// ```rust
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_with::{As, DisplayFromStr, Same};
/// # use std::collections::BTreeMap;
/// #
/// #[derive(Deserialize, Serialize)]
/// # struct S {
/// // Serialize map, turn keys into strings but keep type of value
/// #[serde(with = "As::<BTreeMap<DisplayFromStr, Same>>")]
/// field: BTreeMap<u8, i32>,
/// # }
/// ```
///
/// [serde_as]: https://docs.rs/serde_with/1.6.4/serde_with/attr.serde_as.html
#[derive(Copy, Clone, Debug, Default)]
pub struct As<T: ?Sized>(PhantomData<T>);

impl<T: ?Sized> As<T> {
    /// Serialize type `T` using [`SerializeAs`][]
    ///
    /// The function signature is compatible with [serde's with-annotation][with-annotation].
    ///
    /// [with-annotation]: https://serde.rs/field-attrs.html#with
    pub fn serialize<S, I>(value: &I, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        T: SerializeAs<I>,
        I: ?Sized,
    {
        T::serialize_as(value, serializer)
    }

    /// Deserialize type `T` using [`DeserializeAs`][]
    ///
    /// The function signature is compatible with [serde's with-annotation][with-annotation].
    ///
    /// [with-annotation]: https://serde.rs/field-attrs.html#with
    pub fn deserialize<'de, D, I>(deserializer: D) -> Result<I, D::Error>
    where
        T: DeserializeAs<'de, I>,
        D: Deserializer<'de>,
    {
        T::deserialize_as(deserializer)
    }
}

/// Adapter to convert from `serde_as` to the serde traits.
///
/// This is the counter-type to [`As`][].
/// It can be used whenever a type implementing [`DeserializeAs`][]/[`SerializeAs`][] is required but the normal `Deserialize`/`Serialize` traits should be used.
/// Check [`As`] for an example.
#[derive(Copy, Clone, Debug, Default)]
pub struct Same;

/// De/Serialize using [`Display`] and [`FromStr`] implementation
///
/// This allows to deserialize a string as a number.
/// It can be very useful for serialization formats like JSON, which do not support integer
/// numbers and have to resort to strings to represent them.
///
/// Another use case is types with [`Display`] and [`FromStr`] implementations, but without serde
/// support, which can be found in some crates.
///
/// If you control the type you want to de/serialize, you can instead use the two derive macros, [`SerializeDisplay`] and [`DeserializeFromStr`].
/// They properly implement the traits [`Serialize`] and [`Deserialize`] such that user of the type no longer have to use the `serde_as` system.
///
/// The same functionality is also available as [`serde_with::rust::display_fromstr`][crate::rust::display_fromstr] compatible with serde's with-annotation.
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "DisplayFromStr")]
///     mime: mime::Mime,
///     #[serde_as(as = "DisplayFromStr")]
///     number: u32,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "mime": "text/plain",
///     "number": "159",
/// })).unwrap();
/// assert_eq!(mime::TEXT_PLAIN, v.mime);
/// assert_eq!(159, v.number);
///
/// let x = A {
///     mime: mime::STAR_STAR,
///     number: 777,
/// };
/// assert_eq!(json!({ "mime": "*/*", "number": "777" }), serde_json::to_value(&x).unwrap());
/// # }
/// ```
///
/// [`Display`]: std::fmt::Display
/// [`FromStr`]: std::str::FromStr
#[derive(Copy, Clone, Debug, Default)]
pub struct DisplayFromStr;

/// De/Serialize a [`Option`]`<`[`String`]`>` type while transforming the empty string to [`None`]
///
/// Convert an [`Option`]`<T>` from/to string using [`FromStr`] and [`AsRef`]`<`[`str`]`>` implementations.
/// An empty string is deserialized as [`None`] and a [`None`] vice versa.
///
/// The same functionality is also available as [`serde_with::rust::string_empty_as_none`][crate::rust::string_empty_as_none] compatible with serde's with-annotation.
///
/// # Examples
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, NoneAsEmptyString};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "NoneAsEmptyString")]
///     tags: Option<String>,
/// }
///
/// let v: A = serde_json::from_value(json!({ "tags": "" })).unwrap();
/// assert_eq!(None, v.tags);
///
/// let v: A = serde_json::from_value(json!({ "tags": "Hi" })).unwrap();
/// assert_eq!(Some("Hi".to_string()), v.tags);
///
/// let x = A {
///     tags: Some("This is text".to_string()),
/// };
/// assert_eq!(json!({ "tags": "This is text" }), serde_json::to_value(&x).unwrap());
///
/// let x = A {
///     tags: None,
/// };
/// assert_eq!(json!({ "tags": "" }), serde_json::to_value(&x).unwrap());
/// # }
/// ```
///
/// [`FromStr`]: std::str::FromStr
#[derive(Copy, Clone, Debug, Default)]
pub struct NoneAsEmptyString;

/// Deserialize value and return [`Default`] on error
///
/// The main use case is ignoring error while deserializing.
/// Instead of erroring, it simply deserializes the [`Default`] variant of the type.
/// It is not possible to find the error location, i.e., which field had a deserialization error, with this method.
/// During serialization this wrapper does nothing.
/// The serialization behavior of the underlying type is preserved.
/// The type must implement [`Default`] for this conversion to work.
///
/// The same functionality is also available as [`serde_with::rust::default_on_error`][crate::rust::default_on_error] compatible with serde's with-annotation.
///
/// # Examples
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::Deserialize;
/// # use serde_with::{serde_as, DefaultOnError};
/// #
/// #[serde_as]
/// #[derive(Deserialize,  Debug)]
/// struct A {
///     #[serde_as(deserialize_as = "DefaultOnError")]
///     value: u32,
/// }
///
/// let a: A = serde_json::from_str(r#"{"value": 123}"#).unwrap();
/// assert_eq!(123, a.value);
///
/// // null is of invalid type
/// let a: A = serde_json::from_str(r#"{"value": null}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // String is of invalid type
/// let a: A = serde_json::from_str(r#"{"value": "123"}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // Map is of invalid type
/// let a: A = dbg!(serde_json::from_str(r#"{"value": {}}"#)).unwrap();
/// assert_eq!(0, a.value);
///
/// // Missing entries still cause errors
/// assert!(serde_json::from_str::<A>(r#"{  }"#).is_err());
/// # }
/// ```
///
/// Deserializing missing values can be supported by adding the `default` field attribute:
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::Deserialize;
/// # use serde_with::{serde_as, DefaultOnError};
/// #
/// #[serde_as]
/// #[derive(Deserialize)]
/// struct B {
///     #[serde_as(deserialize_as = "DefaultOnError")]
///     #[serde(default)]
///     value: u32,
/// }
///
///
/// let b: B = serde_json::from_str(r#"{  }"#).unwrap();
/// assert_eq!(0, b.value);
/// # }
/// ```
///
/// `DefaultOnError` can be combined with other conversion methods.
/// In this example we deserialize a `Vec`, each element is deserialized from a string.
/// If the string does not parse as a number, then we get the default value of 0.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DefaultOnError, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Serialize, Deserialize)]
/// struct C {
///     #[serde_as(as = "Vec<DefaultOnError<DisplayFromStr>>")]
///     value: Vec<u32>,
/// };
///
/// let c: C = serde_json::from_value(json!({
///     "value": ["1", "2", "a3", "", {}, "6"]
/// })).unwrap();
/// assert_eq!(vec![1, 2, 0, 0, 0, 6], c.value);
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct DefaultOnError<T = Same>(PhantomData<T>);

/// Deserialize [`Default`] from `null` values
///
/// Instead of erroring on `null` values, it simply deserializes the [`Default`] variant of the type.
/// During serialization this wrapper does nothing.
/// The serialization behavior of the underlying type is preserved.
/// The type must implement [`Default`] for this conversion to work.
///
/// The same functionality is also available as [`serde_with::rust::default_on_null`][crate::rust::default_on_null] compatible with serde's with-annotation.
///
/// # Examples
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::Deserialize;
/// # use serde_with::{serde_as, DefaultOnNull};
/// #
/// #[serde_as]
/// #[derive(Deserialize,  Debug)]
/// struct A {
///     #[serde_as(deserialize_as = "DefaultOnNull")]
///     value: u32,
/// }
///
/// let a: A = serde_json::from_str(r#"{"value": 123}"#).unwrap();
/// assert_eq!(123, a.value);
///
/// // null values are deserialized into the default, here 0
/// let a: A = serde_json::from_str(r#"{"value": null}"#).unwrap();
/// assert_eq!(0, a.value);
/// # }
/// ```
///
/// `DefaultOnNull` can be combined with other conversion methods.
/// In this example we deserialize a `Vec`, each element is deserialized from a string.
/// If we encounter null, then we get the default value of 0.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DefaultOnNull, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Serialize, Deserialize)]
/// struct C {
///     #[serde_as(as = "Vec<DefaultOnNull<DisplayFromStr>>")]
///     value: Vec<u32>,
/// };
///
/// let c: C = serde_json::from_value(json!({
///     "value": ["1", "2", null, null, "5"]
/// })).unwrap();
/// assert_eq!(vec![1, 2, 0, 0, 5], c.value);
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct DefaultOnNull<T = Same>(PhantomData<T>);

/// Deserialize from bytes or string
///
/// Any Rust [`String`] can be converted into bytes, i.e., `Vec<u8>`.
/// Accepting both as formats while deserializing can be helpful while interacting with language
/// which have a looser definition of string than Rust.
///
/// # Example
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, BytesOrString};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "BytesOrString")]
///     bytes_or_string: Vec<u8>,
/// }
///
/// // Here we deserialize from a byte array ...
/// let j = json!({
///   "bytes_or_string": [
///     0,
///     1,
///     2,
///     3
///   ]
/// });
///
/// let a: A = serde_json::from_value(j.clone()).unwrap();
/// assert_eq!(vec![0, 1, 2, 3], a.bytes_or_string);
///
/// // and serialization works too.
/// assert_eq!(j, serde_json::to_value(&a).unwrap());
///
/// // But we also support deserializing from a String
/// let j = json!({
///   "bytes_or_string": "✨Works!"
/// });
///
/// let a: A = serde_json::from_value(j).unwrap();
/// assert_eq!("✨Works!".as_bytes(), &*a.bytes_or_string);
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct BytesOrString;

/// De/Serialize Durations as number of seconds.
///
/// De/serialize durations as number of seconds with subsecond precision.
/// Subsecond precision is *only* supported for [`DurationSecondsWithFrac`], but not for [`DurationSeconds`].
/// You can configure the serialization format between integers, floats, and stringified numbers with the `FORMAT` specifier and configure the deserialization with the `STRICTNESS` specifier.
///
/// The `STRICTNESS` specifier can either be [`formats::Strict`] or [`formats::Flexible`] and defaults to [`formats::Strict`].
/// [`formats::Strict`] means that deserialization only supports the type given in `FORMAT`, e.g., if `FORMAT` is `u64` deserialization from a `f64` will error.
/// [`formats::Flexible`] means that deserialization will perform a best effort to extract the correct duration and allows deserialization from any type.
/// For example, deserializing `DurationSeconds<f64, Flexible>` will discard any subsecond precision during deserialization from `f64` and will parse a `String` as an integer number.
///
/// This type also supports [`chrono::Duration`] with the `chrono`-[feature flag].
///
/// | Duration Type         | Converter                 | Available `FORMAT`s    |
/// | --------------------- | ------------------------- | ---------------------- |
/// | `std::time::Duration` | `DurationSeconds`         | `u64`, `f64`, `String` |
/// | `std::time::Duration` | `DurationSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::Duration`    | `DurationSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::Duration`    | `DurationSecondsWithFrac` | `f64`, `String`        |
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DurationSeconds};
/// use std::time::Duration;
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Durations {
///     #[serde_as(as = "DurationSeconds<u64>")]
///     d_u64: Duration,
///     #[serde_as(as = "DurationSeconds<f64>")]
///     d_f64: Duration,
///     #[serde_as(as = "DurationSeconds<String>")]
///     d_string: Duration,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let d = Durations {
///     d_u64: Duration::new(12345, 0), // Create from seconds and nanoseconds
///     d_f64: Duration::new(12345, 500_000_000),
///     d_string: Duration::new(12345, 999_999_999),
/// };
/// // Observe the different datatypes
/// let expected = json!({
///     "d_u64": 12345,
///     "d_f64": 12346.0,
///     "d_string": "12346",
/// });
/// assert_eq!(expected, serde_json::to_value(&d).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "d_u64": 12345,
///     "d_f64": 12345.5,
///     "d_string": "12346",
/// });
/// let expected = Durations {
///     d_u64: Duration::new(12345, 0), // Create from seconds and nanoseconds
///     d_f64: Duration::new(12346, 0),
///     d_string: Duration::new(12346, 0),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::Duration`] is also supported when using the `chrono` feature.
/// It is a signed duration, thus can be de/serialized as an `i64` instead of a `u64`.
///
/// ```rust
/// # #[cfg(all(feature = "macros", feature = "chrono"))] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DurationSeconds};
/// # use chrono_crate::Duration;
/// # /* Ugliness to make the docs look nicer since I want to hide the rename of the chrono crate
/// use chrono::Duration;
/// # */
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Durations {
///     #[serde_as(as = "DurationSeconds<i64>")]
///     d_i64: Duration,
///     #[serde_as(as = "DurationSeconds<f64>")]
///     d_f64: Duration,
///     #[serde_as(as = "DurationSeconds<String>")]
///     d_string: Duration,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let d = Durations {
///     d_i64: Duration::seconds(-12345),
///     d_f64: Duration::seconds(-12345) + Duration::milliseconds(500),
///     d_string: Duration::seconds(12345) + Duration::nanoseconds(999_999_999),
/// };
/// // Observe the different datatypes
/// let expected = json!({
///     "d_i64": -12345,
///     "d_f64": -12345.0,
///     "d_string": "12346",
/// });
/// assert_eq!(expected, serde_json::to_value(&d).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "d_i64": -12345,
///     "d_f64": -12345.5,
///     "d_string": "12346",
/// });
/// let expected = Durations {
///     d_i64: Duration::seconds(-12345),
///     d_f64: Duration::seconds(-12346),
///     d_string: Duration::seconds(12346),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::Duration`]: chrono_crate::Duration
/// [feature flag]: https://docs.rs/serde_with/1.6.4/serde_with/guide/feature_flags/index.html
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationSeconds<
    FORMAT: formats::Format = u64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// De/Serialize Durations as number of seconds.
///
/// De/serialize durations as number of seconds with subsecond precision.
/// Subsecond precision is *only* supported for [`DurationSecondsWithFrac`], but not for [`DurationSeconds`].
/// You can configure the serialization format between integers, floats, and stringified numbers with the `FORMAT` specifier and configure the deserialization with the `STRICTNESS` specifier.
///
/// The `STRICTNESS` specifier can either be [`formats::Strict`] or [`formats::Flexible`] and defaults to [`formats::Strict`].
/// [`formats::Strict`] means that deserialization only supports the type given in `FORMAT`, e.g., if `FORMAT` is `u64` deserialization from a `f64` will error.
/// [`formats::Flexible`] means that deserialization will perform a best effort to extract the correct duration and allows deserialization from any type.
/// For example, deserializing `DurationSeconds<f64, Flexible>` will discard any subsecond precision during deserialization from `f64` and will parse a `String` as an integer number.
///
/// This type also supports [`chrono::Duration`] with the `chrono`-[feature flag].
///
/// | Duration Type         | Converter                 | Available `FORMAT`s    |
/// | --------------------- | ------------------------- | ---------------------- |
/// | `std::time::Duration` | `DurationSeconds`         | `u64`, `f64`, `String` |
/// | `std::time::Duration` | `DurationSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::Duration`    | `DurationSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::Duration`    | `DurationSecondsWithFrac` | `f64`, `String`        |
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DurationSecondsWithFrac};
/// use std::time::Duration;
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Durations {
///     #[serde_as(as = "DurationSecondsWithFrac<f64>")]
///     d_f64: Duration,
///     #[serde_as(as = "DurationSecondsWithFrac<String>")]
///     d_string: Duration,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let d = Durations {
///     d_f64: Duration::new(12345, 500_000_000), // Create from seconds and nanoseconds
///     d_string: Duration::new(12345, 999_999_000),
/// };
/// // Observe the different datatypes
/// let expected = json!({
///     "d_f64": 12345.5,
///     "d_string": "12345.999999",
/// });
/// assert_eq!(expected, serde_json::to_value(&d).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "d_f64": 12345.5,
///     "d_string": "12345.987654",
/// });
/// let expected = Durations {
///     d_f64: Duration::new(12345, 500_000_000), // Create from seconds and nanoseconds
///     d_string: Duration::new(12345, 987_654_000),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::Duration`] is also supported when using the `chrono` feature.
/// It is a signed duration, thus can be de/serialized as an `i64` instead of a `u64`.
///
/// ```rust
/// # #[cfg(all(feature = "macros", feature = "chrono"))] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DurationSecondsWithFrac};
/// # use chrono_crate::Duration;
/// # /* Ugliness to make the docs look nicer since I want to hide the rename of the chrono crate
/// use chrono::Duration;
/// # */
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Durations {
///     #[serde_as(as = "DurationSecondsWithFrac<f64>")]
///     d_f64: Duration,
///     #[serde_as(as = "DurationSecondsWithFrac<String>")]
///     d_string: Duration,
/// };
///
/// // Serialization
///
/// let d = Durations {
///     d_f64: Duration::seconds(-12345) + Duration::milliseconds(500),
///     d_string: Duration::seconds(12345) + Duration::nanoseconds(999_999_000),
/// };
/// // Observe the different datatypes
/// let expected = json!({
///     "d_f64": -12344.5,
///     "d_string": "12345.999999",
/// });
/// assert_eq!(expected, serde_json::to_value(&d).unwrap());
///
/// // Deserialization works too
///
/// let json = json!({
///     "d_f64": -12344.5,
///     "d_string": "12345.987",
/// });
/// let expected = Durations {
///     d_f64: Duration::seconds(-12345) + Duration::milliseconds(500),
///     d_string: Duration::seconds(12345) + Duration::milliseconds(987),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::Duration`]: chrono_crate::Duration
/// [feature flag]: https://docs.rs/serde_with/1.6.4/serde_with/guide/feature_flags/index.html
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSeconds`] with milli-seconds as base unit.
///
/// This type is equivalent to [`DurationSeconds`] except that the each unit represents 1 milli-second instead of 1 second for [`DurationSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationMilliSeconds<
    FORMAT: formats::Format = u64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSecondsWithFrac`] with milli-seconds as base unit.
///
/// This type is equivalent to [`DurationSecondsWithFrac`] except that the each unit represents 1 milli-second instead of 1 second for [`DurationSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationMilliSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSeconds`] with micro-seconds as base unit.
///
/// This type is equivalent to [`DurationSeconds`] except that the each unit represents 1 micro-second instead of 1 second for [`DurationSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationMicroSeconds<
    FORMAT: formats::Format = u64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSecondsWithFrac`] with micro-seconds as base unit.
///
/// This type is equivalent to [`DurationSecondsWithFrac`] except that the each unit represents 1 micro-second instead of 1 second for [`DurationSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationMicroSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSeconds`] with nano-seconds as base unit.
///
/// This type is equivalent to [`DurationSeconds`] except that the each unit represents 1 nano-second instead of 1 second for [`DurationSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationNanoSeconds<
    FORMAT: formats::Format = u64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSecondsWithFrac`] with nano-seconds as base unit.
///
/// This type is equivalent to [`DurationSecondsWithFrac`] except that the each unit represents 1 nano-second instead of 1 second for [`DurationSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationNanoSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// De/Serialize timestamps as seconds since the UNIX epoch
///
/// De/serialize timestamps as seconds since the UNIX epoch.
/// Subsecond precision is *only* supported for [`TimestampSecondsWithFrac`], but not for [`TimestampSeconds`].
/// You can configure the serialization format between integers, floats, and stringified numbers with the `FORMAT` specifier and configure the deserialization with the `STRICTNESS` specifier.
///
/// The `STRICTNESS` specifier can either be [`formats::Strict`] or [`formats::Flexible`] and defaults to [`formats::Strict`].
/// [`formats::Strict`] means that deserialization only supports the type given in `FORMAT`, e.g., if `FORMAT` is `i64` deserialization from a `f64` will error.
/// [`formats::Flexible`] means that deserialization will perform a best effort to extract the correct timestamp and allows deserialization from any type.
/// For example, deserializing `TimestampSeconds<f64, Flexible>` will discard any subsecond precision during deserialization from `f64` and will parse a `String` as an integer number.
///
/// This type also supports [`chrono::DateTime`][DateTime] with the `chrono`-[feature flag].
///
/// | Timestamp Type            | Converter                  | Available `FORMAT`s    |
/// | ------------------------- | -------------------------- | ---------------------- |
/// | `std::time::SystemTime`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `std::time::SystemTime`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::DateTime<Utc>`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::DateTime<Utc>`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::DateTime<Local>` | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::DateTime<Local>` | `TimestampSecondsWithFrac` | `f64`, `String`        |
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TimestampSeconds};
/// use std::time::{Duration, SystemTime};
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Timestamps {
///     #[serde_as(as = "TimestampSeconds<i64>")]
///     st_i64: SystemTime,
///     #[serde_as(as = "TimestampSeconds<f64>")]
///     st_f64: SystemTime,
///     #[serde_as(as = "TimestampSeconds<String>")]
///     st_string: SystemTime,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let ts = Timestamps {
///     st_i64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 0)).unwrap(),
///     st_f64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 500_000_000)).unwrap(),
///     st_string: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 999_999_999)).unwrap(),
/// };
/// // Observe the different datatypes
/// let expected = json!({
///     "st_i64": 12345,
///     "st_f64": 12346.0,
///     "st_string": "12346",
/// });
/// assert_eq!(expected, serde_json::to_value(&ts).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "st_i64": 12345,
///     "st_f64": 12345.5,
///     "st_string": "12346",
/// });
/// let expected  = Timestamps {
///     st_i64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 0)).unwrap(),
///     st_f64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12346, 0)).unwrap(),
///     st_string: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12346, 0)).unwrap(),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::DateTime<Utc>`][DateTime] and [`chrono::DateTime<Local>`][DateTime] are also supported when using the `chrono` feature.
/// Like [`SystemTime`], it is a signed timestamp, thus can be de/serialized as an `i64`.
///
/// ```rust
/// # #[cfg(all(feature = "macros", feature = "chrono"))] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TimestampSeconds};
/// # use chrono_crate::{DateTime, Local, TimeZone, Utc};
/// # /* Ugliness to make the docs look nicer since I want to hide the rename of the chrono crate
/// use chrono::{DateTime, Local, TimeZone, Utc};
/// # */
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Timestamps {
///     #[serde_as(as = "TimestampSeconds<i64>")]
///     dt_i64: DateTime<Utc>,
///     #[serde_as(as = "TimestampSeconds<f64>")]
///     dt_f64: DateTime<Local>,
///     #[serde_as(as = "TimestampSeconds<String>")]
///     dt_string: DateTime<Utc>,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let ts = Timestamps {
///     dt_i64: Utc.timestamp(-12345, 0),
///     dt_f64: Local.timestamp(-12345, 500_000_000),
///     dt_string: Utc.timestamp(12345, 999_999_999),
/// };
/// // Observe the different datatypes
/// let expected = json!({
///     "dt_i64": -12345,
///     "dt_f64": -12345.0,
///     "dt_string": "12346",
/// });
/// assert_eq!(expected, serde_json::to_value(&ts).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "dt_i64": -12345,
///     "dt_f64": -12345.5,
///     "dt_string": "12346",
/// });
/// let expected = Timestamps {
///     dt_i64: Utc.timestamp(-12345, 0),
///     dt_f64: Local.timestamp(-12346, 0),
///     dt_string: Utc.timestamp(12346, 0),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`SystemTime`]: std::time::SystemTime
/// [DateTime]: chrono_crate::DateTime
/// [feature flag]: https://docs.rs/serde_with/1.6.4/serde_with/guide/feature_flags/index.html
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampSeconds<
    FORMAT: formats::Format = i64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// De/Serialize timestamps as seconds since the UNIX epoch
///
/// De/serialize timestamps as seconds since the UNIX epoch.
/// Subsecond precision is *only* supported for [`TimestampSecondsWithFrac`], but not for [`TimestampSeconds`].
/// You can configure the serialization format between integers, floats, and stringified numbers with the `FORMAT` specifier and configure the deserialization with the `STRICTNESS` specifier.
///
/// The `STRICTNESS` specifier can either be [`formats::Strict`] or [`formats::Flexible`] and defaults to [`formats::Strict`].
/// [`formats::Strict`] means that deserialization only supports the type given in `FORMAT`, e.g., if `FORMAT` is `i64` deserialization from a `f64` will error.
/// [`formats::Flexible`] means that deserialization will perform a best effort to extract the correct timestamp and allows deserialization from any type.
/// For example, deserializing `TimestampSeconds<f64, Flexible>` will discard any subsecond precision during deserialization from `f64` and will parse a `String` as an integer number.
///
/// This type also supports [`chrono::DateTime`][DateTime] with the `chrono`-[feature flag].
///
/// | Timestamp Type            | Converter                  | Available `FORMAT`s    |
/// | ------------------------- | -------------------------- | ---------------------- |
/// | `std::time::SystemTime`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `std::time::SystemTime`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::DateTime<Utc>`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::DateTime<Utc>`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::DateTime<Local>` | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::DateTime<Local>` | `TimestampSecondsWithFrac` | `f64`, `String`        |
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TimestampSecondsWithFrac};
/// use std::time::{Duration, SystemTime};
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Timestamps {
///     #[serde_as(as = "TimestampSecondsWithFrac<f64>")]
///     st_f64: SystemTime,
///     #[serde_as(as = "TimestampSecondsWithFrac<String>")]
///     st_string: SystemTime,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let ts = Timestamps {
///     st_f64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 500_000_000)).unwrap(),
///     st_string: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 999_999_000)).unwrap(),
/// };
/// // Observe the different datatypes
/// let expected = json!({
///     "st_f64": 12345.5,
///     "st_string": "12345.999999",
/// });
/// assert_eq!(expected, serde_json::to_value(&ts).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "st_f64": 12345.5,
///     "st_string": "12345.987654",
/// });
/// let expected = Timestamps {
///     st_f64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 500_000_000)).unwrap(),
///     st_string: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 987_654_000)).unwrap(),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::DateTime<Utc>`][DateTime] and [`chrono::DateTime<Local>`][DateTime] are also supported when using the `chrono` feature.
/// Like [`SystemTime`], it is a signed timestamp, thus can be de/serialized as an `i64`.
///
/// ```rust
/// # #[cfg(all(feature = "macros", feature = "chrono"))] {
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TimestampSecondsWithFrac};
/// # use chrono_crate::{DateTime, Local, TimeZone, Utc};
/// # /* Ugliness to make the docs look nicer since I want to hide the rename of the chrono crate
/// use chrono::{DateTime, Local, TimeZone, Utc};
/// # */
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Timestamps {
///     #[serde_as(as = "TimestampSecondsWithFrac<f64>")]
///     dt_f64: DateTime<Utc>,
///     #[serde_as(as = "TimestampSecondsWithFrac<String>")]
///     dt_string: DateTime<Local>,
/// };
///
/// // Serialization
///
/// let ts = Timestamps {
///     dt_f64: Utc.timestamp(-12345, 500_000_000),
///     dt_string: Local.timestamp(12345, 999_999_000),
/// };
/// // Observe the different datatypes
/// let expected = json!({
///     "dt_f64": -12344.5,
///     "dt_string": "12345.999999",
/// });
/// assert_eq!(expected, serde_json::to_value(&ts).unwrap());
///
/// // Deserialization works too
///
/// let json = json!({
///     "dt_f64": -12344.5,
///     "dt_string": "12345.987",
/// });
/// let expected = Timestamps {
///     dt_f64: Utc.timestamp(-12345, 500_000_000),
///     dt_string: Local.timestamp(12345, 987_000_000),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`SystemTime`]: std::time::SystemTime
/// [DateTime]: chrono_crate::DateTime
/// [feature flag]: https://docs.rs/serde_with/1.6.4/serde_with/guide/feature_flags/index.html
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSeconds`] with milli-seconds as base unit.
///
/// This type is equivalent to [`TimestampSeconds`] except that the each unit represents 1 milli-second instead of 1 second for [`TimestampSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampMilliSeconds<
    FORMAT: formats::Format = i64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSecondsWithFrac`] with milli-seconds as base unit.
///
/// This type is equivalent to [`TimestampSecondsWithFrac`] except that the each unit represents 1 milli-second instead of 1 second for [`TimestampSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampMilliSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSeconds`] with micro-seconds as base unit.
///
/// This type is equivalent to [`TimestampSeconds`] except that the each unit represents 1 micro-second instead of 1 second for [`TimestampSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampMicroSeconds<
    FORMAT: formats::Format = i64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSecondsWithFrac`] with micro-seconds as base unit.
///
/// This type is equivalent to [`TimestampSecondsWithFrac`] except that the each unit represents 1 micro-second instead of 1 second for [`TimestampSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampMicroSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSeconds`] with nano-seconds as base unit.
///
/// This type is equivalent to [`TimestampSeconds`] except that the each unit represents 1 nano-second instead of 1 second for [`TimestampSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampNanoSeconds<
    FORMAT: formats::Format = i64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSecondsWithFrac`] with nano-seconds as base unit.
///
/// This type is equivalent to [`TimestampSecondsWithFrac`] except that the each unit represents 1 nano-second instead of 1 second for [`TimestampSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampNanoSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);
