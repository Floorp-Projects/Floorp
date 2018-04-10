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
    /// Data structure sent to a `proc_macro_derive` macro.
    ///
    /// *This type is available if Syn is built with the `"derive"` feature.*
    pub struct DeriveInput {
        /// Attributes tagged on the whole struct or enum.
        pub attrs: Vec<Attribute>,

        /// Visibility of the struct or enum.
        pub vis: Visibility,

        /// Name of the struct or enum.
        pub ident: Ident,

        /// Generics required to complete the definition.
        pub generics: Generics,

        /// Data within the struct or enum.
        pub data: Data,
    }
}

ast_enum_of_structs! {
    /// The storage of a struct, enum or union data structure.
    ///
    /// *This type is available if Syn is built with the `"derive"` feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum Data {
        /// A struct input to a `proc_macro_derive` macro.
        ///
        /// *This type is available if Syn is built with the `"derive"`
        /// feature.*
        pub Struct(DataStruct {
            pub struct_token: Token![struct],
            pub fields: Fields,
            pub semi_token: Option<Token![;]>,
        }),

        /// An enum input to a `proc_macro_derive` macro.
        ///
        /// *This type is available if Syn is built with the `"derive"`
        /// feature.*
        pub Enum(DataEnum {
            pub enum_token: Token![enum],
            pub brace_token: token::Brace,
            pub variants: Punctuated<Variant, Token![,]>,
        }),

        /// A tagged union input to a `proc_macro_derive` macro.
        ///
        /// *This type is available if Syn is built with the `"derive"`
        /// feature.*
        pub Union(DataUnion {
            pub union_token: Token![union],
            pub fields: FieldsNamed,
        }),
    }

    do_not_generate_to_tokens
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;

    use synom::Synom;

    impl Synom for DeriveInput {
        named!(parse -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            vis: syn!(Visibility) >>
            which: alt!(
                keyword!(struct) => { Ok }
                |
                keyword!(enum) => { Err }
            ) >>
            id: syn!(Ident) >>
            generics: syn!(Generics) >>
            item: switch!(value!(which),
                Ok(s) => map!(data_struct, move |(wh, fields, semi)| DeriveInput {
                    ident: id,
                    vis: vis,
                    attrs: attrs,
                    generics: Generics {
                        where_clause: wh,
                        .. generics
                    },
                    data: Data::Struct(DataStruct {
                        struct_token: s,
                        fields: fields,
                        semi_token: semi,
                    }),
                })
                |
                Err(e) => map!(data_enum, move |(wh, brace, variants)| DeriveInput {
                    ident: id,
                    vis: vis,
                    attrs: attrs,
                    generics: Generics {
                        where_clause: wh,
                        .. generics
                    },
                    data: Data::Enum(DataEnum {
                        variants: variants,
                        brace_token: brace,
                        enum_token: e,
                    }),
                })
            ) >>
            (item)
        ));

        fn description() -> Option<&'static str> {
            Some("derive input")
        }
    }

    named!(data_struct -> (Option<WhereClause>, Fields, Option<Token![;]>), alt!(
        do_parse!(
            wh: option!(syn!(WhereClause)) >>
            fields: syn!(FieldsNamed) >>
            (wh, Fields::Named(fields), None)
        )
        |
        do_parse!(
            fields: syn!(FieldsUnnamed) >>
            wh: option!(syn!(WhereClause)) >>
            semi: punct!(;) >>
            (wh, Fields::Unnamed(fields), Some(semi))
        )
        |
        do_parse!(
            wh: option!(syn!(WhereClause)) >>
            semi: punct!(;) >>
            (wh, Fields::Unit, Some(semi))
        )
    ));

    named!(data_enum -> (Option<WhereClause>, token::Brace, Punctuated<Variant, Token![,]>), do_parse!(
        wh: option!(syn!(WhereClause)) >>
        data: braces!(Punctuated::parse_terminated) >>
        (wh, data.0, data.1)
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use attr::FilterAttrs;
    use quote::{ToTokens, Tokens};

    impl ToTokens for DeriveInput {
        fn to_tokens(&self, tokens: &mut Tokens) {
            for attr in self.attrs.outer() {
                attr.to_tokens(tokens);
            }
            self.vis.to_tokens(tokens);
            match self.data {
                Data::Struct(ref d) => d.struct_token.to_tokens(tokens),
                Data::Enum(ref d) => d.enum_token.to_tokens(tokens),
                Data::Union(ref d) => d.union_token.to_tokens(tokens),
            }
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            match self.data {
                Data::Struct(ref data) => match data.fields {
                    Fields::Named(ref fields) => {
                        self.generics.where_clause.to_tokens(tokens);
                        fields.to_tokens(tokens);
                    }
                    Fields::Unnamed(ref fields) => {
                        fields.to_tokens(tokens);
                        self.generics.where_clause.to_tokens(tokens);
                        TokensOrDefault(&data.semi_token).to_tokens(tokens);
                    }
                    Fields::Unit => {
                        self.generics.where_clause.to_tokens(tokens);
                        TokensOrDefault(&data.semi_token).to_tokens(tokens);
                    }
                },
                Data::Enum(ref data) => {
                    self.generics.where_clause.to_tokens(tokens);
                    data.brace_token.surround(tokens, |tokens| {
                        data.variants.to_tokens(tokens);
                    });
                }
                Data::Union(ref data) => {
                    self.generics.where_clause.to_tokens(tokens);
                    data.fields.to_tokens(tokens);
                }
            }
        }
    }
}
