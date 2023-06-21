//! A public API for more fine-grained customization of bindgen behavior.

pub use crate::ir::analysis::DeriveTrait;
pub use crate::ir::derive::CanDerive as ImplementsTrait;
pub use crate::ir::enum_ty::{EnumVariantCustomBehavior, EnumVariantValue};
pub use crate::ir::int::IntKind;
use std::fmt;

/// An enum to allow ignoring parsing of macros.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum MacroParsingBehavior {
    /// Ignore the macro, generating no code for it, or anything that depends on
    /// it.
    Ignore,
    /// The default behavior bindgen would have otherwise.
    Default,
}

impl Default for MacroParsingBehavior {
    fn default() -> Self {
        MacroParsingBehavior::Default
    }
}

/// A trait to allow configuring different kinds of types in different
/// situations.
pub trait ParseCallbacks: fmt::Debug {
    #[cfg(feature = "__cli")]
    #[doc(hidden)]
    fn cli_args(&self) -> Vec<String> {
        vec![]
    }

    /// This function will be run on every macro that is identified.
    fn will_parse_macro(&self, _name: &str) -> MacroParsingBehavior {
        MacroParsingBehavior::Default
    }

    /// This function will run for every extern variable and function. The returned value determines
    /// the name visible in the bindings.
    fn generated_name_override(
        &self,
        _item_info: ItemInfo<'_>,
    ) -> Option<String> {
        None
    }

    /// This function will run for every extern variable and function. The returned value determines
    /// the link name in the bindings.
    fn generated_link_name_override(
        &self,
        _item_info: ItemInfo<'_>,
    ) -> Option<String> {
        None
    }

    /// The integer kind an integer macro should have, given a name and the
    /// value of that macro, or `None` if you want the default to be chosen.
    fn int_macro(&self, _name: &str, _value: i64) -> Option<IntKind> {
        None
    }

    /// This will be run on every string macro. The callback cannot influence the further
    /// treatment of the macro, but may use the value to generate additional code or configuration.
    fn str_macro(&self, _name: &str, _value: &[u8]) {}

    /// This will be run on every function-like macro. The callback cannot
    /// influence the further treatment of the macro, but may use the value to
    /// generate additional code or configuration.
    ///
    /// The first parameter represents the name and argument list (including the
    /// parentheses) of the function-like macro. The second parameter represents
    /// the expansion of the macro as a sequence of tokens.
    fn func_macro(&self, _name: &str, _value: &[&[u8]]) {}

    /// This function should return whether, given an enum variant
    /// name, and value, this enum variant will forcibly be a constant.
    fn enum_variant_behavior(
        &self,
        _enum_name: Option<&str>,
        _original_variant_name: &str,
        _variant_value: EnumVariantValue,
    ) -> Option<EnumVariantCustomBehavior> {
        None
    }

    /// Allows to rename an enum variant, replacing `_original_variant_name`.
    fn enum_variant_name(
        &self,
        _enum_name: Option<&str>,
        _original_variant_name: &str,
        _variant_value: EnumVariantValue,
    ) -> Option<String> {
        None
    }

    /// Allows to rename an item, replacing `_original_item_name`.
    fn item_name(&self, _original_item_name: &str) -> Option<String> {
        None
    }

    /// This will be called on every file inclusion, with the full path of the included file.
    fn include_file(&self, _filename: &str) {}

    /// This will be called every time `bindgen` reads an environment variable whether it has any
    /// content or not.
    fn read_env_var(&self, _key: &str) {}

    /// This will be called to determine whether a particular blocklisted type
    /// implements a trait or not. This will be used to implement traits on
    /// other types containing the blocklisted type.
    ///
    /// * `None`: use the default behavior
    /// * `Some(ImplementsTrait::Yes)`: `_name` implements `_derive_trait`
    /// * `Some(ImplementsTrait::Manually)`: any type including `_name` can't
    ///   derive `_derive_trait` but can implemented it manually
    /// * `Some(ImplementsTrait::No)`: `_name` doesn't implement `_derive_trait`
    fn blocklisted_type_implements_trait(
        &self,
        _name: &str,
        _derive_trait: DeriveTrait,
    ) -> Option<ImplementsTrait> {
        None
    }

    /// Provide a list of custom derive attributes.
    ///
    /// If no additional attributes are wanted, this function should return an
    /// empty `Vec`.
    fn add_derives(&self, _info: &DeriveInfo<'_>) -> Vec<String> {
        vec![]
    }

    /// Process a source code comment.
    fn process_comment(&self, _comment: &str) -> Option<String> {
        None
    }

    /// Potentially override the visibility of a composite type field.
    ///
    /// Caution: This allows overriding standard C++ visibility inferred by
    /// `respect_cxx_access_specs`.
    fn field_visibility(
        &self,
        _info: FieldInfo<'_>,
    ) -> Option<crate::FieldVisibilityKind> {
        None
    }

    /// Process a function name that as exactly one `va_list` argument
    /// to be wrapped as a variadic function with the wrapped static function
    /// feature.
    ///
    /// The returned string is new function name.
    #[cfg(feature = "experimental")]
    fn wrap_as_variadic_fn(&self, _name: &str) -> Option<String> {
        None
    }
}

/// Relevant information about a type to which new derive attributes will be added using
/// [`ParseCallbacks::add_derives`].
#[derive(Debug)]
#[non_exhaustive]
pub struct DeriveInfo<'a> {
    /// The name of the type.
    pub name: &'a str,
    /// The kind of the type.
    pub kind: TypeKind,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// The kind of the current type.
pub enum TypeKind {
    /// The type is a Rust `struct`.
    Struct,
    /// The type is a Rust `enum`.
    Enum,
    /// The type is a Rust `union`.
    Union,
}

/// A struct providing information about the item being passed to [`ParseCallbacks::generated_name_override`].
#[non_exhaustive]
pub struct ItemInfo<'a> {
    /// The name of the item
    pub name: &'a str,
    /// The kind of item
    pub kind: ItemKind,
}

/// An enum indicating the kind of item for an ItemInfo.
#[non_exhaustive]
pub enum ItemKind {
    /// A Function
    Function,
    /// A Variable
    Var,
}

/// Relevant information about a field for which visibility can be determined using
/// [`ParseCallbacks::field_visibility`].
#[derive(Debug)]
#[non_exhaustive]
pub struct FieldInfo<'a> {
    /// The name of the type.
    pub type_name: &'a str,
    /// The name of the field.
    pub field_name: &'a str,
}
