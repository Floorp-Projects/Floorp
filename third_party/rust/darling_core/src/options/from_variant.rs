use syn::{DeriveInput, Field, Ident, Meta};

use {FromMetaItem, Result};
use codegen::FromVariantImpl;
use options::{OuterFrom, ParseAttribute, ParseData, DataShape};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FromVariantOptions {
    pub base: OuterFrom,
    pub fields: Option<Ident>,
    pub supports: Option<DataShape>,
}

impl FromVariantOptions {
    pub fn new(di: &DeriveInput) -> Result<Self> {
        (FromVariantOptions {
            base: OuterFrom::start(di),
            fields: Default::default(),
            supports: Default::default(),
        }).parse_attributes(&di.attrs)?.parse_body(&di.data)
    }
}

impl<'a> From<&'a FromVariantOptions> for FromVariantImpl<'a> {
    fn from(v: &'a FromVariantOptions) -> Self {
        FromVariantImpl {
            base: (&v.base.container).into(),
            ident: v.base.ident.as_ref(),
            fields: v.fields.as_ref(),
            attrs: v.base.attrs.as_ref(),
            attr_names: v.base.attr_names.as_strs(),
            forward_attrs: v.base.forward_attrs.as_ref(),
            from_ident: Some(v.base.from_ident),
            supports: v.supports.as_ref(),
        }
    }
}

impl ParseAttribute for FromVariantOptions {
    fn parse_nested(&mut self, mi: &Meta) -> Result<()> {
        match mi.name().as_ref() {
            "supports" => { self.supports = FromMetaItem::from_meta_item(mi)?; Ok(()) }
            _ => self.base.parse_nested(mi)
        }
    }
}

impl ParseData for FromVariantOptions {
    fn parse_field(&mut self, field: &Field) -> Result<()> {
        match field.ident.as_ref().map(|i| i.as_ref()) {
            Some("fields") => { self.fields = field.ident.clone(); Ok(()) }
            _ => self.base.parse_field(field)
        }
    }
}
