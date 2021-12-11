use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{self, Ident};

use codegen::FromTypeParamImpl;
use options::{OuterFrom, ParseAttribute, ParseData};
use Result;

#[derive(Debug)]
pub struct FromTypeParamOptions {
    pub base: OuterFrom,
    pub bounds: Option<Ident>,
    pub default: Option<Ident>,
}

impl FromTypeParamOptions {
    pub fn new(di: &syn::DeriveInput) -> Result<Self> {
        (FromTypeParamOptions {
            base: OuterFrom::start(di),
            bounds: None,
            default: None,
        })
        .parse_attributes(&di.attrs)?
        .parse_body(&di.data)
    }
}

impl ParseAttribute for FromTypeParamOptions {
    fn parse_nested(&mut self, mi: &syn::Meta) -> Result<()> {
        self.base.parse_nested(mi)
    }
}

impl ParseData for FromTypeParamOptions {
    fn parse_variant(&mut self, variant: &syn::Variant) -> Result<()> {
        self.base.parse_variant(variant)
    }

    fn parse_field(&mut self, field: &syn::Field) -> Result<()> {
        match field
            .ident
            .as_ref()
            .map(|v| v.to_string())
            .as_ref()
            .map(|v| v.as_str())
        {
            Some("bounds") => {
                self.bounds = field.ident.clone();
                Ok(())
            }
            Some("default") => {
                self.default = field.ident.clone();
                Ok(())
            }
            _ => self.base.parse_field(field),
        }
    }
}

impl<'a> From<&'a FromTypeParamOptions> for FromTypeParamImpl<'a> {
    fn from(v: &'a FromTypeParamOptions) -> Self {
        FromTypeParamImpl {
            base: (&v.base.container).into(),
            ident: v.base.ident.as_ref(),
            attrs: v.base.attrs.as_ref(),
            bounds: v.bounds.as_ref(),
            default: v.default.as_ref(),
            attr_names: &v.base.attr_names,
            forward_attrs: v.base.forward_attrs.as_ref(),
            from_ident: v.base.from_ident,
        }
    }
}

impl ToTokens for FromTypeParamOptions {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        FromTypeParamImpl::from(self).to_tokens(tokens)
    }
}
