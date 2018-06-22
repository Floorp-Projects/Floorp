/// Quasi-quotation macro that accepts input like the [`quote!`] macro but uses
/// type inference to figure out a return type for those tokens.
///
/// [`quote!`]: https://docs.rs/quote/0.4/quote/index.html
///
/// The return type can be any syntax tree node that implements the [`Synom`]
/// trait.
///
/// [`Synom`]: synom/trait.Synom.html
///
/// ```
/// #[macro_use]
/// extern crate syn;
///
/// #[macro_use]
/// extern crate quote;
///
/// use syn::Stmt;
///
/// fn main() {
///     let name = quote!(v);
///     let ty = quote!(u8);
///
///     let stmt: Stmt = parse_quote! {
///         let #name: #ty = Default::default();
///     };
///
///     println!("{:#?}", stmt);
/// }
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature,
/// although interpolation of syntax tree nodes into the quoted tokens is only
/// supported if Syn is built with the `"printing"` feature as well.*
///
/// # Example
///
/// The following helper function adds a bound `T: HeapSize` to every type
/// parameter `T` in the input generics.
///
/// ```
/// # #[macro_use]
/// # extern crate syn;
/// #
/// # #[macro_use]
/// # extern crate quote;
/// #
/// # use syn::{Generics, GenericParam};
/// #
/// // Add a bound `T: HeapSize` to every type parameter T.
/// fn add_trait_bounds(mut generics: Generics) -> Generics {
///     for param in &mut generics.params {
///         if let GenericParam::Type(ref mut type_param) = *param {
///             type_param.bounds.push(parse_quote!(HeapSize));
///         }
///     }
///     generics
/// }
/// #
/// # fn main() {}
/// ```
///
/// # Special cases
///
/// This macro can parse the following additional types as a special case even
/// though they do not implement the `Synom` trait.
///
/// - [`Attribute`] — parses one attribute, allowing either outer like `#[...]`
///   or inner like `#![...]`
/// - [`Punctuated<T, P>`] — parses zero or more `T` separated by punctuation
///   `P` with optional trailing punctuation
///
/// [`Attribute`]: struct.Attribute.html
/// [`Punctuated<T, P>`]: punctuated/struct.Punctuated.html
///
/// # Panics
///
/// Panics if the tokens fail to parse as the expected syntax tree type. The
/// caller is responsible for ensuring that the input tokens are syntactically
/// valid.
#[macro_export]
macro_rules! parse_quote {
    ($($tt:tt)*) => {
        $crate::parse_quote::parse($crate::parse_quote::From::from(quote!($($tt)*)))
    };
}

////////////////////////////////////////////////////////////////////////////////
// Can parse any type that implements Synom.

use buffer::Cursor;
use proc_macro2::TokenStream;
use synom::{PResult, Parser, Synom};

// Not public API.
#[doc(hidden)]
pub use std::convert::From;

// Not public API.
#[doc(hidden)]
pub fn parse<T: ParseQuote>(token_stream: TokenStream) -> T {
    let parser = T::parse;
    match parser.parse2(token_stream) {
        Ok(t) => t,
        Err(err) => match T::description() {
            Some(s) => panic!("failed to parse {}: {}", s, err),
            None => panic!("{}", err),
        },
    }
}

// Not public API.
#[doc(hidden)]
pub trait ParseQuote: Sized {
    fn parse(input: Cursor) -> PResult<Self>;
    fn description() -> Option<&'static str>;
}

impl<T> ParseQuote for T
where
    T: Synom,
{
    fn parse(input: Cursor) -> PResult<Self> {
        <T as Synom>::parse(input)
    }

    fn description() -> Option<&'static str> {
        <T as Synom>::description()
    }
}

////////////////////////////////////////////////////////////////////////////////
// Any other types that we want `parse_quote!` to be able to parse.

use punctuated::Punctuated;

#[cfg(any(feature = "full", feature = "derive"))]
use Attribute;

impl<T, P> ParseQuote for Punctuated<T, P>
where
    T: Synom,
    P: Synom,
{
    named!(parse -> Self, call!(Punctuated::parse_terminated));

    fn description() -> Option<&'static str> {
        Some("punctuated sequence")
    }
}

#[cfg(any(feature = "full", feature = "derive"))]
impl ParseQuote for Attribute {
    named!(parse -> Self, alt!(
        call!(Attribute::parse_outer)
        |
        call!(Attribute::parse_inner)
    ));

    fn description() -> Option<&'static str> {
        Some("attribute")
    }
}
