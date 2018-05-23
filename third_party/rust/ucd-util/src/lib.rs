/*!
The `ucd-util` crate contains a smattering of utility functions that implement
various algorithms specified by Unicode. There is no specific goal for
exhaustiveness. Instead, implementations should be added on an as-needed basis.

A *current* design constraint of this crate is that it should not bring in any
large Unicode tables. For example, to use the various property name and value
canonicalization functions, you'll need to supply your own table, which can
be generated using `ucd-generate`.
*/

#![deny(missing_docs)]

mod hangul;
mod ideograph;
mod name;
mod property;
mod unicode_tables;

pub use hangul::{
    RANGE_HANGUL_SYLLABLE, hangul_name, hangul_full_canonical_decomposition,
};
pub use ideograph::{RANGE_IDEOGRAPH, ideograph_name};
pub use name::{character_name_normalize, symbolic_name_normalize};
pub use property::{
    PropertyTable, PropertyValueTable, PropertyValues,
    canonical_property_name, property_values, canonical_property_value,
};
