use proc_macro2::{Delimiter, Span};

use error::Result;
use parse::{ParseBuffer, ParseStream};
use private;
use token;

// Not public API.
#[doc(hidden)]
pub struct Parens<'a> {
    pub token: token::Paren,
    pub content: ParseBuffer<'a>,
}

// Not public API.
#[doc(hidden)]
pub struct Braces<'a> {
    pub token: token::Brace,
    pub content: ParseBuffer<'a>,
}

// Not public API.
#[doc(hidden)]
pub struct Brackets<'a> {
    pub token: token::Bracket,
    pub content: ParseBuffer<'a>,
}

// Not public API.
#[cfg(any(feature = "full", feature = "derive"))]
#[doc(hidden)]
pub struct Group<'a> {
    pub token: token::Group,
    pub content: ParseBuffer<'a>,
}

// Not public API.
#[doc(hidden)]
pub fn parse_parens(input: ParseStream) -> Result<Parens> {
    parse_delimited(input, Delimiter::Parenthesis).map(|(span, content)| Parens {
        token: token::Paren(span),
        content: content,
    })
}

// Not public API.
#[doc(hidden)]
pub fn parse_braces(input: ParseStream) -> Result<Braces> {
    parse_delimited(input, Delimiter::Brace).map(|(span, content)| Braces {
        token: token::Brace(span),
        content: content,
    })
}

// Not public API.
#[doc(hidden)]
pub fn parse_brackets(input: ParseStream) -> Result<Brackets> {
    parse_delimited(input, Delimiter::Bracket).map(|(span, content)| Brackets {
        token: token::Bracket(span),
        content: content,
    })
}

#[cfg(any(feature = "full", feature = "derive"))]
impl private {
    pub fn parse_group(input: ParseStream) -> Result<Group> {
        parse_delimited(input, Delimiter::None).map(|(span, content)| Group {
            token: token::Group(span),
            content: content,
        })
    }
}

fn parse_delimited(input: ParseStream, delimiter: Delimiter) -> Result<(Span, ParseBuffer)> {
    input.step(|cursor| {
        if let Some((content, span, rest)) = cursor.group(delimiter) {
            let unexpected = private::get_unexpected(input);
            let nested = private::advance_step_cursor(cursor, content);
            let content = private::new_parse_buffer(span, nested, unexpected);
            Ok(((span, content), rest))
        } else {
            let message = match delimiter {
                Delimiter::Parenthesis => "expected parentheses",
                Delimiter::Brace => "expected curly braces",
                Delimiter::Bracket => "expected square brackets",
                Delimiter::None => "expected invisible group",
            };
            Err(cursor.error(message))
        }
    })
}

/// Parse a set of parentheses and expose their content to subsequent parsers.
///
/// # Example
///
/// ```rust
/// # #[macro_use]
/// # extern crate quote;
/// #
/// #[macro_use]
/// extern crate syn;
///
/// use syn::{token, Ident, Type};
/// use syn::parse::{Parse, ParseStream, Result};
/// use syn::punctuated::Punctuated;
///
/// // Parse a simplified tuple struct syntax like:
/// //
/// //     struct S(A, B);
/// struct TupleStruct {
///     struct_token: Token![struct],
///     ident: Ident,
///     paren_token: token::Paren,
///     fields: Punctuated<Type, Token![,]>,
///     semi_token: Token![;],
/// }
///
/// impl Parse for TupleStruct {
///     fn parse(input: ParseStream) -> Result<Self> {
///         let content;
///         Ok(TupleStruct {
///             struct_token: input.parse()?,
///             ident: input.parse()?,
///             paren_token: parenthesized!(content in input),
///             fields: content.parse_terminated(Type::parse)?,
///             semi_token: input.parse()?,
///         })
///     }
/// }
/// #
/// # fn main() {
/// #     let input = quote! {
/// #         struct S(A, B);
/// #     };
/// #     syn::parse2::<TupleStruct>(input).unwrap();
/// # }
/// ```
#[macro_export]
macro_rules! parenthesized {
    ($content:ident in $cursor:expr) => {
        match $crate::group::parse_parens(&$cursor) {
            $crate::export::Ok(parens) => {
                $content = parens.content;
                parens.token
            }
            $crate::export::Err(error) => {
                return $crate::export::Err(error);
            }
        }
    };
}

/// Parse a set of curly braces and expose their content to subsequent parsers.
///
/// # Example
///
/// ```rust
/// # #[macro_use]
/// # extern crate quote;
/// #
/// #[macro_use]
/// extern crate syn;
/// use syn::{token, Ident, Type};
/// use syn::parse::{Parse, ParseStream, Result};
/// use syn::punctuated::Punctuated;
///
/// // Parse a simplified struct syntax like:
/// //
/// //     struct S {
/// //         a: A,
/// //         b: B,
/// //     }
/// struct Struct {
///     struct_token: Token![struct],
///     ident: Ident,
///     brace_token: token::Brace,
///     fields: Punctuated<Field, Token![,]>,
/// }
///
/// struct Field {
///     name: Ident,
///     colon_token: Token![:],
///     ty: Type,
/// }
///
/// impl Parse for Struct {
///     fn parse(input: ParseStream) -> Result<Self> {
///         let content;
///         Ok(Struct {
///             struct_token: input.parse()?,
///             ident: input.parse()?,
///             brace_token: braced!(content in input),
///             fields: content.parse_terminated(Field::parse)?,
///         })
///     }
/// }
///
/// impl Parse for Field {
///     fn parse(input: ParseStream) -> Result<Self> {
///         Ok(Field {
///             name: input.parse()?,
///             colon_token: input.parse()?,
///             ty: input.parse()?,
///         })
///     }
/// }
/// #
/// # fn main() {
/// #     let input = quote! {
/// #         struct S {
/// #             a: A,
/// #             b: B,
/// #         }
/// #     };
/// #     syn::parse2::<Struct>(input).unwrap();
/// # }
/// ```
#[macro_export]
macro_rules! braced {
    ($content:ident in $cursor:expr) => {
        match $crate::group::parse_braces(&$cursor) {
            $crate::export::Ok(braces) => {
                $content = braces.content;
                braces.token
            }
            $crate::export::Err(error) => {
                return $crate::export::Err(error);
            }
        }
    };
}

/// Parse a set of square brackets and expose their content to subsequent
/// parsers.
///
/// # Example
///
/// ```rust
/// # #[macro_use]
/// # extern crate quote;
/// #
/// #[macro_use]
/// extern crate syn;
///
/// extern crate proc_macro2;
///
/// use proc_macro2::TokenStream;
/// use syn::token;
/// use syn::parse::{Parse, ParseStream, Result};
///
/// // Parse an outer attribute like:
/// //
/// //     #[repr(C, packed)]
/// struct OuterAttribute {
///     pound_token: Token![#],
///     bracket_token: token::Bracket,
///     content: TokenStream,
/// }
///
/// impl Parse for OuterAttribute {
///     fn parse(input: ParseStream) -> Result<Self> {
///         let content;
///         Ok(OuterAttribute {
///             pound_token: input.parse()?,
///             bracket_token: bracketed!(content in input),
///             content: content.parse()?,
///         })
///     }
/// }
/// #
/// # fn main() {
/// #     let input = quote! {
/// #         #[repr(C, packed)]
/// #     };
/// #     syn::parse2::<OuterAttribute>(input).unwrap();
/// # }
/// ```
#[macro_export]
macro_rules! bracketed {
    ($content:ident in $cursor:expr) => {
        match $crate::group::parse_brackets(&$cursor) {
            $crate::export::Ok(brackets) => {
                $content = brackets.content;
                brackets.token
            }
            $crate::export::Err(error) => {
                return $crate::export::Err(error);
            }
        }
    };
}
