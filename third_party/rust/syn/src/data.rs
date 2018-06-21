// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::*;
use punctuated::Punctuated;

ast_struct! {
    /// An enum variant.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct Variant {
        /// Attributes tagged on the variant.
        pub attrs: Vec<Attribute>,

        /// Name of the variant.
        pub ident: Ident,

        /// Content stored in the variant.
        pub fields: Fields,

        /// Explicit discriminant: `Variant = 1`
        pub discriminant: Option<(Token![=], Expr)>,
    }
}

ast_enum_of_structs! {
    /// Data stored within an enum variant or struct.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum Fields {
        /// Named fields of a struct or struct variant such as `Point { x: f64,
        /// y: f64 }`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Named(FieldsNamed {
            pub brace_token: token::Brace,
            pub named: Punctuated<Field, Token![,]>,
        }),

        /// Unnamed fields of a tuple struct or tuple variant such as `Some(T)`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Unnamed(FieldsUnnamed {
            pub paren_token: token::Paren,
            pub unnamed: Punctuated<Field, Token![,]>,
        }),

        /// Unit struct or unit variant such as `None`.
        pub Unit,
    }
}

impl Fields {
    /// Get an iterator over the [`Field`] items in this object. This iterator
    /// can be used to iterate over a named or unnamed struct or variant's
    /// fields uniformly.
    ///
    /// [`Field`]: struct.Field.html
    pub fn iter(&self) -> punctuated::Iter<Field> {
        match *self {
            Fields::Unit => punctuated::Iter::private_empty(),
            Fields::Named(ref f) => f.named.iter(),
            Fields::Unnamed(ref f) => f.unnamed.iter(),
        }
    }
}

impl<'a> IntoIterator for &'a Fields {
    type Item = &'a Field;
    type IntoIter = punctuated::Iter<'a, Field>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

ast_struct! {
    /// A field of a struct or enum variant.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct Field {
        /// Attributes tagged on the field.
        pub attrs: Vec<Attribute>,

        /// Visibility of the field.
        pub vis: Visibility,

        /// Name of the field, if any.
        ///
        /// Fields of tuple structs have no names.
        pub ident: Option<Ident>,

        pub colon_token: Option<Token![:]>,

        /// Type of the field.
        pub ty: Type,
    }
}

ast_enum_of_structs! {
    /// The visibility level of an item: inherited or `pub` or
    /// `pub(restricted)`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum Visibility {
        /// A public visibility level: `pub`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Public(VisPublic {
            pub pub_token: Token![pub],
        }),

        /// A crate-level visibility: `crate`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Crate(VisCrate {
            pub crate_token: Token![crate],
        }),

        /// A visibility level restricted to some path: `pub(self)` or
        /// `pub(super)` or `pub(crate)` or `pub(in some::module)`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Restricted(VisRestricted {
            pub pub_token: Token![pub],
            pub paren_token: token::Paren,
            pub in_token: Option<Token![in]>,
            pub path: Box<Path>,
        }),

        /// An inherited visibility, which usually means private.
        pub Inherited,
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;

    use synom::Synom;

    impl Synom for Variant {
        named!(parse -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            id: syn!(Ident) >>
            fields: alt!(
                syn!(FieldsNamed) => { Fields::Named }
                |
                syn!(FieldsUnnamed) => { Fields::Unnamed }
                |
                epsilon!() => { |_| Fields::Unit }
            ) >>
            disr: option!(tuple!(punct!(=), syn!(Expr))) >>
            (Variant {
                ident: id,
                attrs: attrs,
                fields: fields,
                discriminant: disr,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("enum variant")
        }
    }

    impl Synom for FieldsNamed {
        named!(parse -> Self, map!(
            braces!(call!(Punctuated::parse_terminated_with, Field::parse_named)),
            |(brace, fields)| FieldsNamed {
                brace_token: brace,
                named: fields,
            }
        ));

        fn description() -> Option<&'static str> {
            Some("named fields in a struct or struct variant")
        }
    }

    impl Synom for FieldsUnnamed {
        named!(parse -> Self, map!(
            parens!(call!(Punctuated::parse_terminated_with, Field::parse_unnamed)),
            |(paren, fields)| FieldsUnnamed {
                paren_token: paren,
                unnamed: fields,
            }
        ));

        fn description() -> Option<&'static str> {
            Some("unnamed fields in a tuple struct or tuple variant")
        }
    }

    impl Field {
        named!(pub parse_named -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            vis: syn!(Visibility) >>
            id: syn!(Ident) >>
            colon: punct!(:) >>
            ty: syn!(Type) >>
            (Field {
                ident: Some(id),
                vis: vis,
                attrs: attrs,
                ty: ty,
                colon_token: Some(colon),
            })
        ));

        named!(pub parse_unnamed -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            vis: syn!(Visibility) >>
            ty: syn!(Type) >>
            (Field {
                ident: None,
                colon_token: None,
                vis: vis,
                attrs: attrs,
                ty: ty,
            })
        ));
    }

    impl Synom for Visibility {
        named!(parse -> Self, alt!(
            do_parse!(
                pub_token: keyword!(pub) >>
                other: parens!(keyword!(crate)) >>
                (Visibility::Restricted(VisRestricted {
                    pub_token: pub_token,
                    paren_token: other.0,
                    in_token: None,
                    path: Box::new(other.1.into()),
                }))
            )
            |
            keyword!(crate) => { |tok| {
                Visibility::Crate(VisCrate {
                    crate_token: tok,
                })
            } }
            |
            do_parse!(
                pub_token: keyword!(pub) >>
                other: parens!(keyword!(self)) >>
                (Visibility::Restricted(VisRestricted {
                    pub_token: pub_token,
                    paren_token: other.0,
                    in_token: None,
                    path: Box::new(other.1.into()),
                }))
            )
            |
            do_parse!(
                pub_token: keyword!(pub) >>
                other: parens!(keyword!(super)) >>
                (Visibility::Restricted(VisRestricted {
                    pub_token: pub_token,
                    paren_token: other.0,
                    in_token: None,
                    path: Box::new(other.1.into()),
                }))
            )
            |
            do_parse!(
                pub_token: keyword!(pub) >>
                other: parens!(do_parse!(
                    in_tok: keyword!(in) >>
                    restricted: call!(Path::parse_mod_style) >>
                    (in_tok, restricted)
                )) >>
                (Visibility::Restricted(VisRestricted {
                    pub_token: pub_token,
                    paren_token: other.0,
                    in_token: Some((other.1).0),
                    path: Box::new((other.1).1),
                }))
            )
            |
            keyword!(pub) => { |tok| {
                Visibility::Public(VisPublic {
                    pub_token: tok,
                })
            } }
            |
            epsilon!() => { |_| Visibility::Inherited }
        ));

        fn description() -> Option<&'static str> {
            Some("visibility qualifier such as `pub`")
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use proc_macro2::TokenStream;
    use quote::{ToTokens, TokenStreamExt};

    impl ToTokens for Variant {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(&self.attrs);
            self.ident.to_tokens(tokens);
            self.fields.to_tokens(tokens);
            if let Some((ref eq_token, ref disc)) = self.discriminant {
                eq_token.to_tokens(tokens);
                disc.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for FieldsNamed {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.brace_token.surround(tokens, |tokens| {
                self.named.to_tokens(tokens);
            });
        }
    }

    impl ToTokens for FieldsUnnamed {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.paren_token.surround(tokens, |tokens| {
                self.unnamed.to_tokens(tokens);
            });
        }
    }

    impl ToTokens for Field {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(&self.attrs);
            self.vis.to_tokens(tokens);
            if let Some(ref ident) = self.ident {
                ident.to_tokens(tokens);
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
            }
            self.ty.to_tokens(tokens);
        }
    }

    impl ToTokens for VisPublic {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.pub_token.to_tokens(tokens)
        }
    }

    impl ToTokens for VisCrate {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.crate_token.to_tokens(tokens);
        }
    }

    impl ToTokens for VisRestricted {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.pub_token.to_tokens(tokens);
            self.paren_token.surround(tokens, |tokens| {
                // XXX: If we have a path which is not "self" or "super" or
                // "crate", automatically add the "in" token.
                self.in_token.to_tokens(tokens);
                self.path.to_tokens(tokens);
            });
        }
    }
}
