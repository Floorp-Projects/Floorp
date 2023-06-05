use proc_macro2::{Span, TokenTree};
use syn::parse::{Error, Parse, ParseStream, Result};
use syn::{token, Attribute, Data, DeriveInput, Expr, Fields, Ident, Meta, Token};

pub struct Input {
    pub ident: Ident,
    pub repr: Ident,
    pub variants: Vec<Variant>,
    pub default_variant: Option<Variant>,
}

#[derive(Clone)]
pub struct Variant {
    pub ident: Ident,
    pub attrs: VariantAttrs,
}

#[derive(Clone)]
pub struct VariantAttrs {
    pub is_default: bool,
}

fn parse_serde_attr(attrs: &mut VariantAttrs, attr: &Attribute) {
    if let Meta::List(_) = attr.meta {
        let _ = attr.parse_nested_meta(|meta| {
            if meta.input.peek(Token![=]) {
                let _value: Expr = meta.value()?.parse()?;
            } else if meta.input.peek(token::Paren) {
                let _group: TokenTree = meta.input.parse()?;
            } else if meta.path.is_ident("other") {
                attrs.is_default = true;
            }
            Ok(())
        });
    }
}

fn parse_attrs(variant: &syn::Variant) -> VariantAttrs {
    let mut attrs = VariantAttrs { is_default: false };
    for attr in &variant.attrs {
        if attr.path().is_ident("serde") {
            parse_serde_attr(&mut attrs, attr);
        }
    }
    attrs
}

impl Parse for Input {
    fn parse(input: ParseStream) -> Result<Self> {
        let call_site = Span::call_site();
        let derive_input = DeriveInput::parse(input)?;

        let data = match derive_input.data {
            Data::Enum(data) => data,
            _ => {
                return Err(Error::new(call_site, "input must be an enum"));
            }
        };

        let variants = data
            .variants
            .into_iter()
            .map(|variant| match variant.fields {
                Fields::Unit => {
                    let attrs = parse_attrs(&variant);
                    Ok(Variant {
                        ident: variant.ident,
                        attrs,
                    })
                }
                Fields::Named(_) | Fields::Unnamed(_) => {
                    Err(Error::new(variant.ident.span(), "must be a unit variant"))
                }
            })
            .collect::<Result<Vec<Variant>>>()?;

        if variants.is_empty() {
            return Err(Error::new(call_site, "there must be at least one variant"));
        }

        let generics = derive_input.generics;
        if !generics.params.is_empty() || generics.where_clause.is_some() {
            return Err(Error::new(call_site, "generic enum is not supported"));
        }

        let mut repr = None;
        for attr in derive_input.attrs {
            if attr.path().is_ident("repr") {
                if let Meta::List(_) = &attr.meta {
                    let ty: Ident = attr.parse_args()?;
                    repr = Some(ty);
                    break;
                }
            }
        }
        let repr = repr.ok_or_else(|| Error::new(call_site, "missing #[repr(...)] attribute"))?;

        let mut default_variants = variants.iter().filter(|x| x.attrs.is_default);
        let default_variant = default_variants.next().cloned();
        if default_variants.next().is_some() {
            return Err(Error::new(
                call_site,
                "only one variant can be #[serde(other)]",
            ));
        }

        Ok(Input {
            ident: derive_input.ident,
            repr,
            variants,
            default_variant,
        })
    }
}
