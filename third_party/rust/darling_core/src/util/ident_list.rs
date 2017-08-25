use std::ops::Deref;

use syn::{Ident, NestedMetaItem, MetaItem};

use {FromMetaItem, Result, Error};

/// A list of `syn::Ident` instances. This type is used to extract a list of words from an 
/// attribute.
///
/// # Usage
/// An `IdentList` field on a struct implementing `FromMetaItem` will turn `#[builder(derive(Debug, Clone))]` into:
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
        IdentList(vals.into_iter().map(Ident::new).collect())
    }

    /// Creates a view of the contained identifiers as `&str`s.
    pub fn as_strs<'a>(&'a self) -> Vec<&'a str> {
        self.iter().map(|i| i.as_ref()).collect()
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

impl FromMetaItem for IdentList {
    fn from_list(v: &[NestedMetaItem]) -> Result<Self> {
        let mut idents = Vec::with_capacity(v.len());
        for nmi in v {
            if let NestedMetaItem::MetaItem(MetaItem::Word(ref ident)) = *nmi {
                idents.push(ident.clone());
            } else {
                return Err(Error::unexpected_type("non-word"))
            }
        }

        Ok(IdentList(idents))
    }
}