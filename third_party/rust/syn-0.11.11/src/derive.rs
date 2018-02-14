use super::*;

/// Struct or enum sent to a `proc_macro_derive` macro.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct DeriveInput {
    /// Name of the struct or enum.
    pub ident: Ident,

    /// Visibility of the struct or enum.
    pub vis: Visibility,

    /// Attributes tagged on the whole struct or enum.
    pub attrs: Vec<Attribute>,

    /// Generics required to complete the definition.
    pub generics: Generics,

    /// Data within the struct or enum.
    pub body: Body,
}

/// Body of a derived struct or enum.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub enum Body {
    /// It's an enum.
    Enum(Vec<Variant>),

    /// It's a struct.
    Struct(VariantData),
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use Generics;
    use attr::parsing::outer_attr;
    use data::parsing::{visibility, struct_body, enum_body};
    use generics::parsing::generics;
    use ident::parsing::ident;

    named!(pub derive_input -> DeriveInput, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        which: alt!(keyword!("struct") | keyword!("enum")) >>
        id: ident >>
        generics: generics >>
        item: switch!(value!(which),
            "struct" => map!(struct_body, move |(wh, body)| DeriveInput {
                ident: id,
                vis: vis,
                attrs: attrs,
                generics: Generics {
                    where_clause: wh,
                    .. generics
                },
                body: Body::Struct(body),
            })
            |
            "enum" => map!(enum_body, move |(wh, body)| DeriveInput {
                ident: id,
                vis: vis,
                attrs: attrs,
                generics: Generics {
                    where_clause: wh,
                    .. generics
                },
                body: Body::Enum(body),
            })
        ) >>
        (item)
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use attr::FilterAttrs;
    use data::VariantData;
    use quote::{Tokens, ToTokens};

    impl ToTokens for DeriveInput {
        fn to_tokens(&self, tokens: &mut Tokens) {
            for attr in self.attrs.outer() {
                attr.to_tokens(tokens);
            }
            self.vis.to_tokens(tokens);
            match self.body {
                Body::Enum(_) => tokens.append("enum"),
                Body::Struct(_) => tokens.append("struct"),
            }
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            match self.body {
                Body::Enum(ref variants) => {
                    self.generics.where_clause.to_tokens(tokens);
                    tokens.append("{");
                    for variant in variants {
                        variant.to_tokens(tokens);
                        tokens.append(",");
                    }
                    tokens.append("}");
                }
                Body::Struct(ref variant_data) => {
                    match *variant_data {
                        VariantData::Struct(_) => {
                            self.generics.where_clause.to_tokens(tokens);
                            variant_data.to_tokens(tokens);
                            // no semicolon
                        }
                        VariantData::Tuple(_) => {
                            variant_data.to_tokens(tokens);
                            self.generics.where_clause.to_tokens(tokens);
                            tokens.append(";");
                        }
                        VariantData::Unit => {
                            self.generics.where_clause.to_tokens(tokens);
                            tokens.append(";");
                        }
                    }
                }
            }
        }
    }
}
