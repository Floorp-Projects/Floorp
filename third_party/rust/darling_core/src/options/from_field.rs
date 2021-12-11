use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{self, Ident};

use codegen::FromFieldImpl;
use options::{OuterFrom, ParseAttribute, ParseData};
use Result;

#[derive(Debug)]
pub struct FromFieldOptions {
    pub base: OuterFrom,
    pub vis: Option<Ident>,
    pub ty: Option<Ident>,
}

impl FromFieldOptions {
    pub fn new(di: &syn::DeriveInput) -> Result<Self> {
        (FromFieldOptions {
            base: OuterFrom::start(di),
            vis: Default::default(),
            ty: Default::default(),
        })
        .parse_attributes(&di.attrs)?
        .parse_body(&di.data)
    }
}

impl ParseAttribute for FromFieldOptions {
    fn parse_nested(&mut self, mi: &syn::Meta) -> Result<()> {
        self.base.parse_nested(mi)
    }
}

impl ParseData for FromFieldOptions {
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
            Some("ty") => {
                self.ty = field.ident.clone();
                Ok(())
            }
            _ => self.base.parse_field(field),
        }
    }
}

impl<'a> From<&'a FromFieldOptions> for FromFieldImpl<'a> {
    fn from(v: &'a FromFieldOptions) -> Self {
        FromFieldImpl {
            ident: v.base.ident.as_ref(),
            vis: v.vis.as_ref(),
            ty: v.ty.as_ref(),
            attrs: v.base.attrs.as_ref(),
            base: (&v.base.container).into(),
            attr_names: &v.base.attr_names,
            forward_attrs: v.base.forward_attrs.as_ref(),
            from_ident: v.base.from_ident,
        }
    }
}

impl ToTokens for FromFieldOptions {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        FromFieldImpl::from(self).to_tokens(tokens)
    }
}
