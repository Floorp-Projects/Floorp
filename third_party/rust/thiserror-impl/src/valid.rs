use crate::ast::{Enum, Field, Input, Struct, Variant};
use crate::attr::Attrs;
use quote::ToTokens;
use std::collections::BTreeSet as Set;
use syn::{Error, Member, Result};

pub(crate) const CHECKED: &str = "checked in validation";

impl Input<'_> {
    pub(crate) fn validate(&self) -> Result<()> {
        match self {
            Input::Struct(input) => input.validate(),
            Input::Enum(input) => input.validate(),
        }
    }
}

impl Struct<'_> {
    fn validate(&self) -> Result<()> {
        check_non_field_attrs(&self.attrs)?;
        check_field_attrs(&self.fields)?;
        for field in &self.fields {
            field.validate()?;
        }
        Ok(())
    }
}

impl Enum<'_> {
    fn validate(&self) -> Result<()> {
        check_non_field_attrs(&self.attrs)?;
        let has_display = self.has_display();
        for variant in &self.variants {
            variant.validate()?;
            if has_display && variant.attrs.display.is_none() {
                return Err(Error::new_spanned(
                    variant.original,
                    "missing #[error(\"...\")] display attribute",
                ));
            }
        }
        let mut from_types = Set::new();
        for variant in &self.variants {
            if let Some(from_field) = variant.from_field() {
                let repr = from_field.ty.to_token_stream().to_string();
                if !from_types.insert(repr) {
                    return Err(Error::new_spanned(
                        from_field.original,
                        "cannot derive From because another variant has the same source type",
                    ));
                }
            }
        }
        Ok(())
    }
}

impl Variant<'_> {
    fn validate(&self) -> Result<()> {
        check_non_field_attrs(&self.attrs)?;
        check_field_attrs(&self.fields)?;
        for field in &self.fields {
            field.validate()?;
        }
        Ok(())
    }
}

impl Field<'_> {
    fn validate(&self) -> Result<()> {
        if let Some(display) = &self.attrs.display {
            return Err(Error::new_spanned(
                display.original,
                "not expected here; the #[error(...)] attribute belongs on top of a struct or an enum variant",
            ));
        }
        Ok(())
    }
}

fn check_non_field_attrs(attrs: &Attrs) -> Result<()> {
    if let Some(from) = &attrs.from {
        return Err(Error::new_spanned(
            from,
            "not expected here; the #[from] attribute belongs on a specific field",
        ));
    }
    if let Some(source) = &attrs.source {
        return Err(Error::new_spanned(
            source,
            "not expected here; the #[source] attribute belongs on a specific field",
        ));
    }
    if let Some(backtrace) = &attrs.backtrace {
        return Err(Error::new_spanned(
            backtrace,
            "not expected here; the #[backtrace] attribute belongs on a specific field",
        ));
    }
    Ok(())
}

fn check_field_attrs(fields: &[Field]) -> Result<()> {
    let mut from_field = None;
    let mut source_field = None;
    let mut backtrace_field = None;
    let mut has_backtrace = false;
    for field in fields {
        if let Some(from) = field.attrs.from {
            if from_field.is_some() {
                return Err(Error::new_spanned(from, "duplicate #[from] attribute"));
            }
            from_field = Some(field);
        }
        if let Some(source) = field.attrs.source {
            if source_field.is_some() {
                return Err(Error::new_spanned(source, "duplicate #[source] attribute"));
            }
            source_field = Some(field);
        }
        if let Some(backtrace) = field.attrs.backtrace {
            if backtrace_field.is_some() {
                return Err(Error::new_spanned(
                    backtrace,
                    "duplicate #[backtrace] attribute",
                ));
            }
            backtrace_field = Some(field);
            has_backtrace = true;
        }
        has_backtrace |= field.is_backtrace();
    }
    if let (Some(from_field), Some(source_field)) = (from_field, source_field) {
        if !same_member(from_field, source_field) {
            return Err(Error::new_spanned(
                from_field.attrs.from,
                "#[from] is only supported on the source field, not any other field",
            ));
        }
    }
    if let Some(from_field) = from_field {
        if fields.len() > 1 + has_backtrace as usize {
            return Err(Error::new_spanned(
                from_field.attrs.from,
                "deriving From requires no fields other than source and backtrace",
            ));
        }
    }
    Ok(())
}

fn same_member(one: &Field, two: &Field) -> bool {
    match (&one.member, &two.member) {
        (Member::Named(one), Member::Named(two)) => one == two,
        (Member::Unnamed(one), Member::Unnamed(two)) => one.index == two.index,
        _ => unreachable!(),
    }
}
