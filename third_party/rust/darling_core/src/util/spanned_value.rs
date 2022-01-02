use proc_macro2::Span;
use std::ops::{Deref, DerefMut};
use syn;
use syn::spanned::Spanned;
use {
    FromDeriveInput, FromField, FromGenericParam, FromGenerics, FromMeta, FromTypeParam,
    FromVariant, Result,
};

/// A value and an associated position in source code. The main use case for this is
/// to preserve position information to emit warnings from proc macros. You can use
/// a `SpannedValue<T>` as a field in any struct that implements or derives any of
/// `darling`'s core traits.
///
/// To access the underlying value, use the struct's `Deref` implementation.
///
/// # Defaulting
/// This type is meant to be used in conjunction with attribute-extracted options,
/// but the user may not always explicitly set those options in their source code.
/// In this case, using `Default::default()` will create an instance which points
/// to `Span::call_site()`.
#[derive(Debug, Clone)]
pub struct SpannedValue<T> {
    value: T,
    span: Span,
}

impl<T> SpannedValue<T> {
    pub fn new(value: T, span: Span) -> Self {
        SpannedValue { value, span }
    }

    /// Get the source code location referenced by this struct.
    pub fn span(&self) -> Span {
        self.span
    }
}

impl<T: Default> Default for SpannedValue<T> {
    fn default() -> Self {
        SpannedValue::new(Default::default(), Span::call_site())
    }
}

impl<T> Deref for SpannedValue<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.value
    }
}

impl<T> DerefMut for SpannedValue<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.value
    }
}

impl<T> AsRef<T> for SpannedValue<T> {
    fn as_ref(&self) -> &T {
        &self.value
    }
}

macro_rules! spanned {
    ($trayt:ident, $method:ident, $syn:path) => {
        impl<T: $trayt> $trayt for SpannedValue<T> {
            fn $method(value: &$syn) -> Result<Self> {
                Ok(SpannedValue::new(
                    $trayt::$method(value).map_err(|e| e.with_span(value))?,
                    value.span(),
                ))
            }
        }
    };
}

spanned!(FromGenericParam, from_generic_param, syn::GenericParam);
spanned!(FromGenerics, from_generics, syn::Generics);
spanned!(FromTypeParam, from_type_param, syn::TypeParam);
spanned!(FromMeta, from_meta, syn::Meta);
spanned!(FromDeriveInput, from_derive_input, syn::DeriveInput);
spanned!(FromField, from_field, syn::Field);
spanned!(FromVariant, from_variant, syn::Variant);

impl<T: Spanned> From<T> for SpannedValue<T> {
    fn from(value: T) -> Self {
        let span = value.span();
        SpannedValue::new(value, span)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use proc_macro2::Span;

    /// Make sure that `SpannedValue` can be seamlessly used as its underlying type.
    #[test]
    fn deref() {
        let test = SpannedValue::new("hello", Span::call_site());
        assert_eq!("hello", test.trim());
    }
}
