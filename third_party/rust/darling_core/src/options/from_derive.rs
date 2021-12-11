use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{self, Ident};

use codegen::FromDeriveInputImpl;
use options::{OuterFrom, ParseAttribute, ParseData, Shape};
use {FromMeta, Result};

#[derive(Debug)]
pub struct FdiOptions {
    pub base: OuterFrom,

    /// The field on the target struct which should receive the type visibility, if any.
    pub vis: Option<Ident>,

    /// The field on the target struct which should receive the type generics, if any.
    pub generics: Option<Ident>,

    pub data: Option<Ident>,

    pub supports: Option<Shape>,
}

impl FdiOptions {
    pub fn new(di: &syn::DeriveInput) -> Result<Self> {
        (FdiOptions {
            base: OuterFrom::start(di),
            vis: Default::default(),
            generics: Default::default(),
            data: Default::default(),
            supports: Default::default(),
        })
        .parse_attributes(&di.attrs)?
        .parse_body(&di.data)
    }
}

impl ParseAttribute for FdiOptions {
    fn parse_nested(&mut self, mi: &syn::Meta) -> Result<()> {
        if mi.path().is_ident("supports") {
            self.supports = FromMeta::from_meta(mi)?;
            Ok(())
        } else {
            self.base.parse_nested(mi)
        }
    }
}

impl ParseData for FdiOptions {
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
            Some("vis") => {
                self.vis = field.ident.clone();
                Ok(())
            }
            Some("data") => {
                self.data = field.ident.clone();
                Ok(())
            }
            Some("generics") => {
                self.generics = field.ident.clone();
                Ok(())
            }
            _ => self.base.parse_field(field),
        }
    }
}

impl<'a> From<&'a FdiOptions> for FromDeriveInputImpl<'a> {
    fn from(v: &'a FdiOptions) -> Self {
        FromDeriveInputImpl {
            base: (&v.base.container).into(),
            attr_names: &v.base.attr_names,
            from_ident: v.base.from_ident,
            ident: v.base.ident.as_ref(),
            vis: v.vis.as_ref(),
            data: v.data.as_ref(),
            generics: v.generics.as_ref(),
            attrs: v.base.attrs.as_ref(),
            forward_attrs: v.base.forward_attrs.as_ref(),
            supports: v.supports.as_ref(),
        }
    }
}

impl ToTokens for FdiOptions {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        FromDeriveInputImpl::from(self).to_tokens(tokens)
    }
}
