use std::ops::Deref;
use std::string::ToString;

use syn::{Ident, Meta, NestedMeta};

use {Error, FromMeta, Result};

/// A list of `syn::Ident` instances. This type is used to extract a list of words from an
/// attribute.
///
/// # Usage
/// An `IdentList` field on a struct implementing `FromMeta` will turn `#[builder(derive(Debug, Clone))]` into:
///
/// ```rust,ignore
/// StructOptions {
///     derive: IdentList(vec![syn::Ident::new("Debug"), syn::Ident::new("Clone")])
/// }
/// ```
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct IdentList(Vec<Ident>);

impl IdentList {
    /// Create a new list.
    pub fn new<T: Into<Ident>>(vals: Vec<T>) -> Self {
        IdentList(vals.into_iter().map(T::into).collect())
    }

    /// Create a new `Vec` containing the string representation of each ident.
    pub fn to_strings(&self) -> Vec<String> {
        self.0.iter().map(ToString::to_string).collect()
    }
}

impl Deref for IdentList {
    type Target = Vec<Ident>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<Ident>> for IdentList {
    fn from(v: Vec<Ident>) -> Self {
        IdentList(v)
    }
}

impl FromMeta for IdentList {
    fn from_list(v: &[NestedMeta]) -> Result<Self> {
        let mut idents = Vec::with_capacity(v.len());
        for nmi in v {
            if let NestedMeta::Meta(Meta::Word(ref ident)) = *nmi {
                idents.push(ident.clone());
            } else {
                return Err(Error::unexpected_type("non-word"));
            }
        }

        Ok(IdentList(idents))
    }
}
