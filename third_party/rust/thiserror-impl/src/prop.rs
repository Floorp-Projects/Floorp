use crate::ast::{Enum, Field, Struct, Variant};
use syn::{Member, Type};

impl Struct<'_> {
    pub(crate) fn from_field(&self) -> Option<&Field> {
        from_field(&self.fields)
    }

    pub(crate) fn source_field(&self) -> Option<&Field> {
        source_field(&self.fields)
    }

    pub(crate) fn backtrace_field(&self) -> Option<&Field> {
        backtrace_field(&self.fields)
    }
}

impl Enum<'_> {
    pub(crate) fn has_source(&self) -> bool {
        self.variants
            .iter()
            .any(|variant| variant.source_field().is_some() || variant.attrs.transparent.is_some())
    }

    pub(crate) fn has_backtrace(&self) -> bool {
        self.variants
            .iter()
            .any(|variant| variant.backtrace_field().is_some())
    }

    pub(crate) fn has_display(&self) -> bool {
        self.attrs.display.is_some()
            || self.attrs.transparent.is_some()
            || self
                .variants
                .iter()
                .any(|variant| variant.attrs.display.is_some())
            || self
                .variants
                .iter()
                .all(|variant| variant.attrs.transparent.is_some())
    }
}

impl Variant<'_> {
    pub(crate) fn from_field(&self) -> Option<&Field> {
        from_field(&self.fields)
    }

    pub(crate) fn source_field(&self) -> Option<&Field> {
        source_field(&self.fields)
    }

    pub(crate) fn backtrace_field(&self) -> Option<&Field> {
        backtrace_field(&self.fields)
    }
}

impl Field<'_> {
    pub(crate) fn is_backtrace(&self) -> bool {
        type_is_backtrace(self.ty)
    }
}

fn from_field<'a, 'b>(fields: &'a [Field<'b>]) -> Option<&'a Field<'b>> {
    for field in fields {
        if field.attrs.from.is_some() {
            return Some(&field);
        }
    }
    None
}

fn source_field<'a, 'b>(fields: &'a [Field<'b>]) -> Option<&'a Field<'b>> {
    for field in fields {
        if field.attrs.from.is_some() || field.attrs.source.is_some() {
            return Some(&field);
        }
    }
    for field in fields {
        match &field.member {
            Member::Named(ident) if ident == "source" => return Some(&field),
            _ => {}
        }
    }
    None
}

fn backtrace_field<'a, 'b>(fields: &'a [Field<'b>]) -> Option<&'a Field<'b>> {
    for field in fields {
        if field.attrs.backtrace.is_some() {
            return Some(&field);
        }
    }
    for field in fields {
        if field.is_backtrace() {
            return Some(&field);
        }
    }
    None
}

fn type_is_backtrace(ty: &Type) -> bool {
    let path = match ty {
        Type::Path(ty) => &ty.path,
        _ => return false,
    };

    let last = path.segments.last().unwrap();
    last.ident == "Backtrace" && last.arguments.is_empty()
}
