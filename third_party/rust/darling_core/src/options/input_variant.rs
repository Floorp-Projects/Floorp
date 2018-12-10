use syn;

use ast::{Fields, Style};
use codegen;
use options::{Core, InputField, ParseAttribute};
use {Error, FromMeta, Result};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InputVariant {
    ident: syn::Ident,
    attr_name: Option<String>,
    data: Fields<InputField>,
    skip: bool,
}

impl InputVariant {
    pub fn as_codegen_variant<'a>(&'a self, ty_ident: &'a syn::Ident) -> codegen::Variant<'a> {
        codegen::Variant {
            ty_ident,
            variant_ident: &self.ident,
            name_in_attr: self.attr_name
                .clone()
                .unwrap_or(self.ident.to_string()),
            data: self.data.as_ref().map(InputField::as_codegen_field),
            skip: self.skip,
        }
    }

    pub fn from_variant(v: &syn::Variant, parent: Option<&Core>) -> Result<Self> {
        let mut starter = (InputVariant {
            ident: v.ident.clone(),
            attr_name: Default::default(),
            data: Fields::empty_from(&v.fields),
            skip: Default::default(),
        }).parse_attributes(&v.attrs)?;

        starter.data = match v.fields {
            syn::Fields::Unit => Style::Unit.into(),
            syn::Fields::Unnamed(ref fields) => {
                let mut items = Vec::with_capacity(fields.unnamed.len());
                for item in &fields.unnamed {
                    items.push(InputField::from_field(item, parent)?);
                }

                Fields {
                    style: v.fields.clone().into(),
                    fields: items,
                }
            }
            syn::Fields::Named(ref fields) => {
                let mut items = Vec::with_capacity(fields.named.len());
                for item in &fields.named {
                    items.push(InputField::from_field(item, parent)?);
                }

                Fields {
                    style: v.fields.clone().into(),
                    fields: items,
                }
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

        self
    }
}

impl ParseAttribute for InputVariant {
    fn parse_nested(&mut self, mi: &syn::Meta) -> Result<()> {
        let name = mi.name().to_string();
        match name.as_str() {
            "rename" => {
                self.attr_name = FromMeta::from_meta(mi)?;
                Ok(())
            }
            "skip" => {
                self.skip = FromMeta::from_meta(mi)?;
                Ok(())
            }
            n => Err(Error::unknown_field(n)),
        }
    }
}
