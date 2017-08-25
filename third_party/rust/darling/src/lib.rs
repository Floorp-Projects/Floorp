//! # Darling
//! Darling is a tool for declarative attribute parsing in proc macro implementations.
//!
//!
//! ## Design
//! Darling takes considerable design inspiration from [`serde`]. A data structure that can be
//! read from any attribute implements `FromMetaItem` (or has an implementation automatically 
//! generated using `derive`). Any crate can provide `FromMetaItem` implementations, even one not
//! specifically geared towards proc-macro authors.
//!
//! Proc-macro crates should provide their own structs which implement or derive `FromDeriveInput` and
//! `FromField` to gather settings relevant to their operation.
//!
//! ## Attributes
//! There are a number of attributes that `darling` exposes to enable finer-grained control over the code
//! it generates.
//!
//! * **Field renaming**: You can use `#[darling(rename="new_name")]` on a field to change the name Darling looks for.
//!   You can also use `#[darling(rename_all="...")]` at the struct or enum level to apply a casing rule to all fields or variants.
//! * **Map function**: You can use `#[darling(map="path::to::function")]` to run code on a field before its stored in the struct.
//! * **Default values**: You can use `#[darling(default)]` at the type or field level to use that type's default value to fill
//!   in values not specified by the caller.
//! * **Skipped fields**: You can skip a variant or field using `#[darling(skip)]`. Fields marked with this will fall back to
//!   `Default::default()` for their value, but you can override that with an explicit default or a value from the type-level default.
//!
//! ## Forwarded Fields
//! The traits `FromDeriveInput` and `FromField` support forwarding fields from the input AST directly 
//! to the derived struct. These fields are matched up by identifier **before** `rename` attribute values are
//! considered. The deriving struct is responsible for making sure the types of fields it does declare match this
//! table.
//!
//! A deriving struct is free to include or exclude any of the fields below.
//!
//! ### `FromDeriveInput`
//! |Field name|Type|Meaning|
//! |---|---|---|
//! |`ident`|`syn::Ident`|The identifier of the passed-in type|
//! |`vis`|`syn::Visibility`|The visibility of the passed-in type|
//! |`generics`|`syn::Generics`|The generics of the passed-in type|
//! |`body`|`darling::ast::Body`|The body of the passed-in type|
//! |`attrs`|`Vec<syn::Attribute>`|The forwarded attributes from the passed in type. These are controlled using the `forward_attrs` attribute.|
//!
//! ### `FromField`
//! |Field name|Type|Meaning|
//! |---|---|---|
//! |`ident`|`syn::Ident`|The identifier of the passed-in field|
//! |`vis`|`syn::Visibility`|The visibility of the passed-in field|
//! |`ty`|`syn::Ty`|The type of the passed-in field|
//! |`attrs`|`Vec<syn::Attribute>`|The forwarded attributes from the passed in field. These are controlled using the `forward_attrs` attribute.|

extern crate core;
extern crate darling_core;

#[allow(unused_imports)]
#[macro_use]
extern crate darling_macro;

#[doc(hidden)]
pub use darling_macro::*;

#[doc(inline)]
pub use darling_core::{FromMetaItem, FromDeriveInput, FromField, FromVariant};

#[doc(inline)]
pub use darling_core::{Result, Error};

#[doc(inline)]
pub use darling_core::{ast, error, util};

/// Core/std trait re-exports. This should help produce generated code which doesn't
/// depend on `std` unnecessarily, and avoids problems caused by aliasing `std` or any
/// of the referenced types.
#[doc(hidden)]
pub mod export {    
    pub use ::core::convert::From;
    pub use ::core::option::Option::{self, Some, None};
    pub use ::core::result::Result::{self, Ok, Err};
    pub use ::core::default::Default;
    pub use ::std::vec::Vec;
}
