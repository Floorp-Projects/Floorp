use syn::NestedMetaItem;

use {FromMetaItem, Result};
use util::IdentList;

/// A rule about which attributes to forward to the generated struct.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ForwardAttrs {
    All,
    Only(IdentList),
}

impl ForwardAttrs {
    /// Returns `true` if this will not forward any attributes.
    pub fn is_empty(&self) -> bool {
        match *self {
            ForwardAttrs::All => false,
            ForwardAttrs::Only(ref list) => list.is_empty(),
        }
    }
}

impl FromMetaItem for ForwardAttrs {
    fn from_word() -> Result<Self> {
        Ok(ForwardAttrs::All)
    }

    fn from_list(nested: &[NestedMetaItem]) -> Result<Self> {
        Ok(ForwardAttrs::Only(IdentList::from_list(nested)?))
    }
}