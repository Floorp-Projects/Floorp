use syn;

use {FromMetaItem, FromDeriveInput, FromField, FromVariant, Result};

/// An efficient way of discarding data from an attribute.
///
/// All meta-items, fields, and variants will be successfully read into
/// the `Ignored` struct, with all properties discarded.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct Ignored;

impl FromMetaItem for Ignored {
    fn from_meta_item(_: &syn::Meta) -> Result<Self> {
        Ok(Ignored)
    }

    fn from_nested_meta_item(_: &syn::NestedMeta) -> Result<Self> {
        Ok(Ignored)
    }
}

impl FromDeriveInput for Ignored {
    fn from_derive_input(_: &syn::DeriveInput) -> Result<Self> {
        Ok(Ignored)
    }
}

impl FromField for Ignored {
    fn from_field(_: &syn::Field) -> Result<Self> {
        Ok(Ignored)
    }
}

impl FromVariant for Ignored {
    fn from_variant(_: &syn::Variant) -> Result<Self> {
        Ok(Ignored)
    }
}