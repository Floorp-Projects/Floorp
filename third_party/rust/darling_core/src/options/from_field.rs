use syn::{self, Ident};

use {Result};
use codegen::FromFieldImpl;
use options::{ParseAttribute, ParseBody, OuterFrom};

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
        }).parse_attributes(&di.attrs)?.parse_body(&di.body)
    }
}

impl ParseAttribute for FromFieldOptions {
    fn parse_nested(&mut self, mi: &syn::MetaItem) -> Result<()> {
        self.base.parse_nested(mi)
    }
}

impl ParseBody for FromFieldOptions {
    fn parse_variant(&mut self, variant: &syn::Variant) -> Result<()> {
        self.base.parse_variant(variant)
    }

    fn parse_field(&mut self, field: &syn::Field) -> Result<()> {
        match field.ident.as_ref().map(|v| v.as_ref()) {
            Some("vis") => { self.vis = field.ident.clone(); Ok(()) },
            Some("ty") => { self.ty = field.ident.clone(); Ok(()) }
            _ => self.base.parse_field(field)
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
            attr_names: v.base.attr_names.as_strs(),
            forward_attrs: v.base.forward_attrs.as_ref(),
            from_ident: v.base.from_ident,
        }
    }
}