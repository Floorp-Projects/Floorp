use std::borrow::Cow;

use syn;

use ast::Fields;
use codegen;
use options::{Core, InputField, ParseAttribute};
use {Error, FromMeta, Result};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InputVariant {
    ident: syn::Ident,
    attr_name: Option<String>,
    data: Fields<InputField>,
    skip: bool,
    /// Whether or not unknown fields are acceptable in this
    allow_unknown_fields: Option<bool>,
}

impl InputVariant {
    pub fn as_codegen_variant<'a>(&'a self, ty_ident: &'a syn::Ident) -> codegen::Variant<'a> {
        codegen::Variant {
            ty_ident,
            variant_ident: &self.ident,
            name_in_attr: self
                .attr_name
                .as_ref()
                .map_or_else(|| Cow::Owned(self.ident.to_string()), Cow::Borrowed),
            data: self.data.as_ref().map(InputField::as_codegen_field),
            skip: self.skip,
            allow_unknown_fields: self.allow_unknown_fields.unwrap_or_default(),
        }
    }

    pub fn from_variant(v: &syn::Variant, parent: Option<&Core>) -> Result<Self> {
        let mut starter = (InputVariant {
            ident: v.ident.clone(),
            attr_name: Default::default(),
            data: Fields::empty_from(&v.fields),
            skip: Default::default(),
            allow_unknown_fields: None,
        })
        .parse_attributes(&v.attrs)?;

        starter.data.fields = match v.fields {
            syn::Fields::Unit => vec![],
            syn::Fields::Unnamed(ref fields) => {
                let mut items = Vec::with_capacity(fields.unnamed.len());
                for item in &fields.unnamed {
                    items.push(InputField::from_field(item, parent)?);
                }

                items
            }
            syn::Fields::Named(ref fields) => {
                let mut items = Vec::with_capacity(fields.named.len());
                for item in &fields.named {
                    items.push(InputField::from_field(item, parent)?);
                }

                items
            }
        };

        Ok(if let Some(p) = parent {
            starter.with_inherited(p)
        } else {
            starter
        })
    }

    fn with_inherited(mut self, parent: &Core) -> Self {
        if self.attr_name.is_none() {
            self.attr_name = Some(parent.rename_rule.apply_to_variant(self.ident.to_string()));
        }

        if self.allow_unknown_fields.is_none() {
            self.allow_unknown_fields = Some(parent.allow_unknown_fields.is_some());
        }

        self
    }
}

impl ParseAttribute for InputVariant {
    fn parse_nested(&mut self, mi: &syn::Meta) -> Result<()> {
        let path = mi.path();
        if path.is_ident("rename") {
            self.attr_name = FromMeta::from_meta(mi)?;
            Ok(())
        } else if path.is_ident("skip") {
            self.skip = FromMeta::from_meta(mi)?;
            Ok(())
        } else {
            Err(Error::unknown_field_path(&path).with_span(mi))
        }
    }
}
