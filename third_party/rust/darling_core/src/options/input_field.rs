use std::borrow::Cow;

use syn;

use codegen;
use options::{Core, DefaultExpression, ParseAttribute};
use {Error, FromMeta, Result};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InputField {
    pub ident: syn::Ident,
    pub attr_name: Option<String>,
    pub ty: syn::Type,
    pub default: Option<DefaultExpression>,
    pub with: Option<syn::Path>,

    /// If `true`, generated code will not look for this field in the input meta item,
    /// instead always falling back to either `InputField::default` or `Default::default`.
    pub skip: bool,
    pub map: Option<syn::Path>,
    pub multiple: bool,
}

impl InputField {
    /// Generate a view into this field that can be used for code generation.
    pub fn as_codegen_field<'a>(&'a self) -> codegen::Field<'a> {
        codegen::Field {
            ident: &self.ident,
            name_in_attr: self
                .attr_name
                .as_ref()
                .map_or_else(|| Cow::Owned(self.ident.to_string()), Cow::Borrowed),
            ty: &self.ty,
            default_expression: self.as_codegen_default(),
            with_path: self.with.as_ref().map_or_else(
                || Cow::Owned(parse_quote!(::darling::FromMeta::from_meta)),
                Cow::Borrowed,
            ),
            skip: self.skip,
            map: self.map.as_ref(),
            multiple: self.multiple,
        }
    }

    /// Generate a codegen::DefaultExpression for this field. This requires the field name
    /// in the `Inherit` case.
    fn as_codegen_default<'a>(&'a self) -> Option<codegen::DefaultExpression<'a>> {
        self.default.as_ref().map(|expr| match *expr {
            DefaultExpression::Explicit(ref path) => codegen::DefaultExpression::Explicit(path),
            DefaultExpression::Inherit => codegen::DefaultExpression::Inherit(&self.ident),
            DefaultExpression::Trait => codegen::DefaultExpression::Trait,
        })
    }

    fn new(ident: syn::Ident, ty: syn::Type) -> Self {
        InputField {
            ident,
            ty,
            attr_name: None,
            default: None,
            with: None,
            skip: false,
            map: Default::default(),
            multiple: false,
        }
    }

    pub fn from_field(f: &syn::Field, parent: Option<&Core>) -> Result<Self> {
        let ident = f
            .ident
            .clone()
            .unwrap_or_else(|| syn::Ident::new("__unnamed", ::proc_macro2::Span::call_site()));
        let ty = f.ty.clone();
        let base = Self::new(ident, ty).parse_attributes(&f.attrs)?;

        if let Some(container) = parent {
            base.with_inherited(container)
        } else {
            Ok(base)
        }
    }

    /// Apply inherited settings from the container. This is done _after_ parsing
    /// to ensure deference to explicit field-level settings.
    fn with_inherited(mut self, parent: &Core) -> Result<Self> {
        // explicit renamings take precedence over rename rules on the container,
        // but in the absence of an explicit name we apply the rule.
        if self.attr_name.is_none() {
            self.attr_name = Some(parent.rename_rule.apply_to_field(self.ident.to_string()));
        }

        // Determine the default expression for this field, based on three pieces of information:
        // 1. Will we look for this field in the attribute?
        // 1. Is there a locally-defined default?
        // 1. Did the parent define a default?
        self.default = match (self.skip, self.default.is_some(), parent.default.is_some()) {
            // If we have a default, use it.
            (_, true, _) => self.default,

            // If there isn't an explicit default but the struct sets a default, we'll
            // inherit from that.
            (_, false, true) => Some(DefaultExpression::Inherit),

            // If we're skipping the field and no defaults have been expressed then we should
            // use the ::darling::export::Default trait.
            (true, false, false) => Some(DefaultExpression::Trait),

            // If we don't have or need a default, then leave it blank.
            (false, false, false) => None,
        };

        Ok(self)
    }
}

impl ParseAttribute for InputField {
    fn parse_nested(&mut self, mi: &syn::Meta) -> Result<()> {
        let path = mi.path();

        if path.is_ident("rename") {
            self.attr_name = FromMeta::from_meta(mi)?;
        } else if path.is_ident("default") {
            self.default = FromMeta::from_meta(mi)?;
        } else if path.is_ident("with") {
            self.with = Some(FromMeta::from_meta(mi)?);
        } else if path.is_ident("skip") {
            self.skip = FromMeta::from_meta(mi)?;
        } else if path.is_ident("map") {
            self.map = Some(FromMeta::from_meta(mi)?);
        } else if path.is_ident("multiple") {
            self.multiple = FromMeta::from_meta(mi)?;
        } else {
            return Err(Error::unknown_field_path(path).with_span(mi))
        }

        Ok(())
    }
}
