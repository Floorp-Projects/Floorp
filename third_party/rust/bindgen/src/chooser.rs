//! A public API for more fine-grained customization of bindgen behavior.

pub use ir::enum_ty::{EnumVariantCustomBehavior, EnumVariantValue};
pub use ir::int::IntKind;
use std::fmt;

/// A trait to allow configuring different kinds of types in different
/// situations.
pub trait TypeChooser: fmt::Debug {
    /// The integer kind an integer macro should have, given a name and the
    /// value of that macro, or `None` if you want the default to be chosen.
    fn int_macro(&self, _name: &str, _value: i64) -> Option<IntKind> {
        None
    }

    /// This function should return whether, given the a given enum variant
    /// name, and value, returns whether this enum variant will forcibly be a
    /// constant.
    fn enum_variant_behavior(&self,
                             _enum_name: Option<&str>,
                             _variant_name: &str,
                             _variant_value: EnumVariantValue)
                             -> Option<EnumVariantCustomBehavior> {
        None
    }
}
