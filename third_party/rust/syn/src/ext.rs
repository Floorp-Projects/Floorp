//! Extension traits to provide parsing methods on foreign types.
//!
//! *This module is available if Syn is built with the `"parsing"` feature.*

use proc_macro2::Ident;

use parse::{ParseStream, Result};

/// Additional parsing methods for `Ident`.
///
/// This trait is sealed and cannot be implemented for types outside of Syn.
///
/// *This trait is available if Syn is built with the `"parsing"` feature.*
pub trait IdentExt: Sized + private::Sealed {
    /// Parses any identifier including keywords.
    ///
    /// This is useful when parsing a DSL which allows Rust keywords as
    /// identifiers.
    ///
    /// ```rust
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::Ident;
    /// use syn::ext::IdentExt;
    /// use syn::parse::{Error, ParseStream, Result};
    ///
    /// // Parses input that looks like `name = NAME` where `NAME` can be
    /// // any identifier.
    /// //
    /// // Examples:
    /// //
    /// //     name = anything
    /// //     name = impl
    /// fn parse_dsl(input: ParseStream) -> Result<Ident> {
    ///     let name_token: Ident = input.parse()?;
    ///     if name_token != "name" {
    ///         return Err(Error::new(name_token.span(), "expected `name`"));
    ///     }
    ///     input.parse::<Token![=]>()?;
    ///     let name = input.call(Ident::parse_any)?;
    ///     Ok(name)
    /// }
    /// #
    /// # fn main() {}
    /// ```
    fn parse_any(input: ParseStream) -> Result<Self>;
}

impl IdentExt for Ident {
    fn parse_any(input: ParseStream) -> Result<Self> {
        input.step(|cursor| match cursor.ident() {
            Some((ident, rest)) => Ok((ident, rest)),
            None => Err(cursor.error("expected ident")),
        })
    }
}

mod private {
    use proc_macro2::Ident;

    pub trait Sealed {}

    impl Sealed for Ident {}
}
