/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Attribute definitions for a `ComponentInterface`.
//!
//! This module provides some conveniences for working with attribute definitions
//! from WebIDL. When encountering a weedle `ExtendedAttribute` node, use `TryFrom`
//! to convert it into an [`Attribute`] representing one of the attributes that we
//! support. You can also use the [`parse_attributes`] function to parse an
//! `ExtendedAttributeList` into a vec of same.
//!
//! We only support a small number of attributes, so it's manageable to have them
//! all handled by a single abstraction. This might need to be refactored in future
//! if we grow significantly more complicated attribute handling.

use anyhow::{bail, Result};
use uniffi_meta::Checksum;

/// Represents an attribute parsed from UDL, like `[ByRef]` or `[Throws]`.
///
/// This is a convenience enum for parsing UDL attributes and erroring out if we encounter
/// any unsupported ones. These don't convert directly into parts of a `ComponentInterface`, but
/// may influence the properties of things like functions and arguments.
#[derive(Debug, Clone, Checksum)]
pub(super) enum Attribute {
    ByRef,
    Enum,
    Error,
    Name(String),
    SelfType(SelfType),
    Threadsafe, // N.B. the `[Threadsafe]` attribute is deprecated and will be removed
    Throws(String),
    // `[External="crate_name"]` - We can `use crate_name::...` for the type.
    External(String),
    // Custom type on the scaffolding side
    Custom,
}

impl Attribute {
    pub fn is_error(&self) -> bool {
        matches!(self, Attribute::Error)
    }
    pub fn is_enum(&self) -> bool {
        matches!(self, Attribute::Enum)
    }
}

/// Convert a weedle `ExtendedAttribute` into an `Attribute` for a `ComponentInterface` member,
/// or error out if the attribute is not supported.
impl TryFrom<&weedle::attribute::ExtendedAttribute<'_>> for Attribute {
    type Error = anyhow::Error;
    fn try_from(
        weedle_attribute: &weedle::attribute::ExtendedAttribute<'_>,
    ) -> Result<Self, anyhow::Error> {
        match weedle_attribute {
            // Matches plain named attributes like "[ByRef"].
            weedle::attribute::ExtendedAttribute::NoArgs(attr) => match (attr.0).0 {
                "ByRef" => Ok(Attribute::ByRef),
                "Enum" => Ok(Attribute::Enum),
                "Error" => Ok(Attribute::Error),
                "Threadsafe" => Ok(Attribute::Threadsafe),
                "Custom" => Ok(Attribute::Custom),
                _ => anyhow::bail!("ExtendedAttributeNoArgs not supported: {:?}", (attr.0).0),
            },
            // Matches assignment-style attributes like ["Throws=Error"]
            weedle::attribute::ExtendedAttribute::Ident(identity) => {
                match identity.lhs_identifier.0 {
                    "Name" => Ok(Attribute::Name(name_from_id_or_string(&identity.rhs))),
                    "Throws" => Ok(Attribute::Throws(name_from_id_or_string(&identity.rhs))),
                    "Self" => Ok(Attribute::SelfType(SelfType::try_from(&identity.rhs)?)),
                    "External" => Ok(Attribute::External(name_from_id_or_string(&identity.rhs))),
                    _ => anyhow::bail!(
                        "Attribute identity Identifier not supported: {:?}",
                        identity.lhs_identifier.0
                    ),
                }
            }
            _ => anyhow::bail!("Attribute not supported: {:?}", weedle_attribute),
        }
    }
}

fn name_from_id_or_string(nm: &weedle::attribute::IdentifierOrString<'_>) -> String {
    match nm {
        weedle::attribute::IdentifierOrString::Identifier(identifier) => identifier.0.to_string(),
        weedle::attribute::IdentifierOrString::String(str_lit) => str_lit.0.to_string(),
    }
}

/// Parse a weedle `ExtendedAttributeList` into a list of `Attribute`s,
/// erroring out on duplicates.
fn parse_attributes<F>(
    weedle_attributes: &weedle::attribute::ExtendedAttributeList<'_>,
    validator: F,
) -> Result<Vec<Attribute>>
where
    F: Fn(&Attribute) -> Result<()>,
{
    let attrs = &weedle_attributes.body.list;

    let mut hash_set = std::collections::HashSet::new();
    for attr in attrs {
        if !hash_set.insert(attr) {
            anyhow::bail!("Duplicated ExtendedAttribute: {:?}", attr);
        }
    }

    let attrs = attrs
        .iter()
        .map(Attribute::try_from)
        .collect::<Result<Vec<_>, _>>()?;

    for attr in &attrs {
        validator(attr)?;
    }

    Ok(attrs)
}

/// Attributes that can be attached to an `enum` definition in the UDL.
/// There's only one case here: using `[Error]` to mark an enum as an error class.
#[derive(Debug, Clone, Checksum, Default)]
pub(super) struct EnumAttributes(Vec<Attribute>);

impl EnumAttributes {
    pub fn contains_error_attr(&self) -> bool {
        self.0.iter().any(|attr| attr.is_error())
    }
}

impl TryFrom<&weedle::attribute::ExtendedAttributeList<'_>> for EnumAttributes {
    type Error = anyhow::Error;
    fn try_from(
        weedle_attributes: &weedle::attribute::ExtendedAttributeList<'_>,
    ) -> Result<Self, Self::Error> {
        let attrs = parse_attributes(weedle_attributes, |attr| match attr {
            Attribute::Error => Ok(()),
            _ => bail!(format!("{attr:?} not supported for enums")),
        })?;
        Ok(Self(attrs))
    }
}

impl<T: TryInto<EnumAttributes, Error = anyhow::Error>> TryFrom<Option<T>> for EnumAttributes {
    type Error = anyhow::Error;
    fn try_from(value: Option<T>) -> Result<Self, Self::Error> {
        match value {
            None => Ok(Default::default()),
            Some(v) => v.try_into(),
        }
    }
}

/// Represents UDL attributes that might appear on a function.
///
/// This supports the `[Throws=ErrorName]` attribute for functions that
/// can produce an error.
#[derive(Debug, Clone, Checksum, Default)]
pub(super) struct FunctionAttributes(Vec<Attribute>);

impl FunctionAttributes {
    pub(super) fn get_throws_err(&self) -> Option<&str> {
        self.0.iter().find_map(|attr| match attr {
            // This will hopefully return a helpful compilation error
            // if the error is not defined.
            Attribute::Throws(inner) => Some(inner.as_ref()),
            _ => None,
        })
    }
}

impl FromIterator<Attribute> for FunctionAttributes {
    fn from_iter<T: IntoIterator<Item = Attribute>>(iter: T) -> Self {
        Self(Vec::from_iter(iter))
    }
}

impl TryFrom<&weedle::attribute::ExtendedAttributeList<'_>> for FunctionAttributes {
    type Error = anyhow::Error;
    fn try_from(
        weedle_attributes: &weedle::attribute::ExtendedAttributeList<'_>,
    ) -> Result<Self, Self::Error> {
        let attrs = parse_attributes(weedle_attributes, |attr| match attr {
            Attribute::Throws(_) => Ok(()),
            _ => bail!(format!("{attr:?} not supported for functions")),
        })?;
        Ok(Self(attrs))
    }
}

impl<T: TryInto<FunctionAttributes, Error = anyhow::Error>> TryFrom<Option<T>>
    for FunctionAttributes
{
    type Error = anyhow::Error;
    fn try_from(value: Option<T>) -> Result<Self, Self::Error> {
        match value {
            None => Ok(Default::default()),
            Some(v) => v.try_into(),
        }
    }
}

/// Represents UDL attributes that might appear on a function argument.
///
/// This supports the `[ByRef]` attribute for arguments that should be passed
/// by reference in the generated Rust scaffolding.
#[derive(Debug, Clone, Checksum, Default)]
pub(super) struct ArgumentAttributes(Vec<Attribute>);

impl ArgumentAttributes {
    pub fn by_ref(&self) -> bool {
        self.0.iter().any(|attr| matches!(attr, Attribute::ByRef))
    }
}

impl TryFrom<&weedle::attribute::ExtendedAttributeList<'_>> for ArgumentAttributes {
    type Error = anyhow::Error;
    fn try_from(
        weedle_attributes: &weedle::attribute::ExtendedAttributeList<'_>,
    ) -> Result<Self, Self::Error> {
        let attrs = parse_attributes(weedle_attributes, |attr| match attr {
            Attribute::ByRef => Ok(()),
            _ => bail!(format!("{attr:?} not supported for arguments")),
        })?;
        Ok(Self(attrs))
    }
}

impl<T: TryInto<ArgumentAttributes, Error = anyhow::Error>> TryFrom<Option<T>>
    for ArgumentAttributes
{
    type Error = anyhow::Error;
    fn try_from(value: Option<T>) -> Result<Self, Self::Error> {
        match value {
            None => Ok(Default::default()),
            Some(v) => v.try_into(),
        }
    }
}

/// Represents UDL attributes that might appear on an `interface` definition.
#[derive(Debug, Clone, Checksum, Default)]
pub(super) struct InterfaceAttributes(Vec<Attribute>);

impl InterfaceAttributes {
    pub fn contains_enum_attr(&self) -> bool {
        self.0.iter().any(|attr| attr.is_enum())
    }

    pub fn contains_error_attr(&self) -> bool {
        self.0.iter().any(|attr| attr.is_error())
    }

    pub fn threadsafe(&self) -> bool {
        self.0
            .iter()
            .any(|attr| matches!(attr, Attribute::Threadsafe))
    }
}

impl TryFrom<&weedle::attribute::ExtendedAttributeList<'_>> for InterfaceAttributes {
    type Error = anyhow::Error;
    fn try_from(
        weedle_attributes: &weedle::attribute::ExtendedAttributeList<'_>,
    ) -> Result<Self, Self::Error> {
        let attrs = parse_attributes(weedle_attributes, |attr| match attr {
            Attribute::Enum => Ok(()),
            Attribute::Error => Ok(()),
            Attribute::Threadsafe => Ok(()),
            _ => bail!(format!("{attr:?} not supported for interface definition")),
        })?;
        // Can't be both `[Threadsafe]` and an `[Enum]`.
        if attrs.len() > 1 {
            bail!("conflicting attributes on interface definition");
        }
        Ok(Self(attrs))
    }
}

impl<T: TryInto<InterfaceAttributes, Error = anyhow::Error>> TryFrom<Option<T>>
    for InterfaceAttributes
{
    type Error = anyhow::Error;
    fn try_from(value: Option<T>) -> Result<Self, Self::Error> {
        match value {
            None => Ok(Default::default()),
            Some(v) => v.try_into(),
        }
    }
}

/// Represents UDL attributes that might appear on a constructor.
///
/// This supports the `[Throws=ErrorName]` attribute for constructors that can produce
/// an error, and the `[Name=MethodName]` for non-default constructors.
#[derive(Debug, Clone, Checksum, Default)]
pub(super) struct ConstructorAttributes(Vec<Attribute>);

impl ConstructorAttributes {
    pub(super) fn get_throws_err(&self) -> Option<&str> {
        self.0.iter().find_map(|attr| match attr {
            // This will hopefully return a helpful compilation error
            // if the error is not defined.
            Attribute::Throws(inner) => Some(inner.as_ref()),
            _ => None,
        })
    }

    pub(super) fn get_name(&self) -> Option<&str> {
        self.0.iter().find_map(|attr| match attr {
            Attribute::Name(inner) => Some(inner.as_ref()),
            _ => None,
        })
    }
}

impl TryFrom<&weedle::attribute::ExtendedAttributeList<'_>> for ConstructorAttributes {
    type Error = anyhow::Error;
    fn try_from(
        weedle_attributes: &weedle::attribute::ExtendedAttributeList<'_>,
    ) -> Result<Self, Self::Error> {
        let attrs = parse_attributes(weedle_attributes, |attr| match attr {
            Attribute::Throws(_) => Ok(()),
            Attribute::Name(_) => Ok(()),
            _ => bail!(format!("{attr:?} not supported for constructors")),
        })?;
        Ok(Self(attrs))
    }
}

/// Represents UDL attributes that might appear on a method.
///
/// This supports the `[Throws=ErrorName]` attribute for methods that can produce
/// an error, and the `[Self=ByArc]` attribute for methods that take `Arc<Self>` as receiver.
#[derive(Debug, Clone, Checksum, Default)]
pub(super) struct MethodAttributes(Vec<Attribute>);

impl MethodAttributes {
    pub(super) fn get_throws_err(&self) -> Option<&str> {
        self.0.iter().find_map(|attr| match attr {
            // This will hopefully return a helpful compilation error
            // if the error is not defined.
            Attribute::Throws(inner) => Some(inner.as_ref()),
            _ => None,
        })
    }

    pub(super) fn get_self_by_arc(&self) -> bool {
        self.0
            .iter()
            .any(|attr| matches!(attr, Attribute::SelfType(SelfType::ByArc)))
    }
}

impl FromIterator<Attribute> for MethodAttributes {
    fn from_iter<T: IntoIterator<Item = Attribute>>(iter: T) -> Self {
        Self(Vec::from_iter(iter))
    }
}

impl TryFrom<&weedle::attribute::ExtendedAttributeList<'_>> for MethodAttributes {
    type Error = anyhow::Error;
    fn try_from(
        weedle_attributes: &weedle::attribute::ExtendedAttributeList<'_>,
    ) -> Result<Self, Self::Error> {
        let attrs = parse_attributes(weedle_attributes, |attr| match attr {
            Attribute::SelfType(_) => Ok(()),
            Attribute::Throws(_) => Ok(()),
            _ => bail!(format!("{attr:?} not supported for methods")),
        })?;
        Ok(Self(attrs))
    }
}

impl<T: TryInto<MethodAttributes, Error = anyhow::Error>> TryFrom<Option<T>> for MethodAttributes {
    type Error = anyhow::Error;
    fn try_from(value: Option<T>) -> Result<Self, Self::Error> {
        match value {
            None => Ok(Default::default()),
            Some(v) => v.try_into(),
        }
    }
}

/// Represents the different possible types of method call receiver.
///
/// Actually we only support one of these right now, `[Self=ByArc]`.
/// We might add more in future, e.g. a `[Self=ByRef]` if there are cases
/// where we need to force the receiver to be taken by reference.
#[derive(Debug, Clone, Checksum)]
pub(super) enum SelfType {
    ByArc, // Method receiver is `Arc<Self>`.
}

impl TryFrom<&weedle::attribute::IdentifierOrString<'_>> for SelfType {
    type Error = anyhow::Error;
    fn try_from(nm: &weedle::attribute::IdentifierOrString<'_>) -> Result<Self, Self::Error> {
        Ok(match nm {
            weedle::attribute::IdentifierOrString::Identifier(identifier) => match identifier.0 {
                "ByArc" => SelfType::ByArc,
                _ => bail!("Unsupported Self Type: {:?}", identifier.0),
            },
            weedle::attribute::IdentifierOrString::String(_) => {
                bail!("Unsupported Self Type: {:?}", nm)
            }
        })
    }
}

/// Represents UDL attributes that might appear on a typedef
///
/// This supports the `[External="crate_name"]` and `[Custom]` attributes for types.
#[derive(Debug, Clone, Checksum, Default)]
pub(super) struct TypedefAttributes(Vec<Attribute>);

impl TypedefAttributes {
    pub(super) fn get_crate_name(&self) -> String {
        self.0
            .iter()
            .find_map(|attr| match attr {
                Attribute::External(crate_name) => Some(crate_name.clone()),
                _ => None,
            })
            .expect("must have a crate name")
    }

    pub(super) fn is_custom(&self) -> bool {
        self.0
            .iter()
            .any(|attr| matches!(attr, Attribute::Custom { .. }))
    }
}

impl TryFrom<&weedle::attribute::ExtendedAttributeList<'_>> for TypedefAttributes {
    type Error = anyhow::Error;
    fn try_from(
        weedle_attributes: &weedle::attribute::ExtendedAttributeList<'_>,
    ) -> Result<Self, Self::Error> {
        let attrs = parse_attributes(weedle_attributes, |attr| match attr {
            Attribute::External { .. } | Attribute::Custom => Ok(()),
            _ => bail!(format!("{attr:?} not supported for typedefs")),
        })?;
        Ok(Self(attrs))
    }
}

impl<T: TryInto<TypedefAttributes, Error = anyhow::Error>> TryFrom<Option<T>>
    for TypedefAttributes
{
    type Error = anyhow::Error;
    fn try_from(value: Option<T>) -> Result<Self, Self::Error> {
        match value {
            None => Ok(Default::default()),
            Some(v) => v.try_into(),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use weedle::Parse;

    #[test]
    fn test_byref() -> Result<()> {
        let (_, node) = weedle::attribute::ExtendedAttribute::parse("ByRef").unwrap();
        let attr = Attribute::try_from(&node)?;
        assert!(matches!(attr, Attribute::ByRef));
        Ok(())
    }

    #[test]
    fn test_enum() -> Result<()> {
        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Enum").unwrap();
        let attr = Attribute::try_from(&node)?;
        assert!(matches!(attr, Attribute::Enum));
        assert!(attr.is_enum());
        Ok(())
    }

    #[test]
    fn test_error() -> Result<()> {
        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Error").unwrap();
        let attr = Attribute::try_from(&node)?;
        assert!(matches!(attr, Attribute::Error));
        assert!(attr.is_error());
        Ok(())
    }

    #[test]
    fn test_name() -> Result<()> {
        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Name=Value").unwrap();
        let attr = Attribute::try_from(&node)?;
        assert!(matches!(attr, Attribute::Name(nm) if nm == "Value"));

        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Name").unwrap();
        let err = Attribute::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "ExtendedAttributeNoArgs not supported: \"Name\""
        );

        Ok(())
    }

    #[test]
    fn test_selftype() -> Result<()> {
        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Self=ByArc").unwrap();
        let attr = Attribute::try_from(&node)?;
        assert!(matches!(attr, Attribute::SelfType(SelfType::ByArc)));
        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Self=ByMistake").unwrap();
        let err = Attribute::try_from(&node).unwrap_err();
        assert_eq!(err.to_string(), "Unsupported Self Type: \"ByMistake\"");
        Ok(())
    }

    #[test]
    fn test_threadsafe() -> Result<()> {
        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Threadsafe").unwrap();
        let attr = Attribute::try_from(&node)?;
        assert!(matches!(attr, Attribute::Threadsafe));
        Ok(())
    }

    #[test]
    fn test_throws() -> Result<()> {
        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Throws=Name").unwrap();
        let attr = Attribute::try_from(&node)?;
        assert!(matches!(attr, Attribute::Throws(nm) if nm == "Name"));

        let (_, node) = weedle::attribute::ExtendedAttribute::parse("Throws").unwrap();
        let err = Attribute::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "ExtendedAttributeNoArgs not supported: \"Throws\""
        );

        Ok(())
    }

    #[test]
    fn test_unsupported() {
        let (_, node) =
            weedle::attribute::ExtendedAttribute::parse("UnsupportedAttribute").unwrap();
        let err = Attribute::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "ExtendedAttributeNoArgs not supported: \"UnsupportedAttribute\""
        );

        let (_, node) =
            weedle::attribute::ExtendedAttribute::parse("Unsupported=Attribute").unwrap();
        let err = Attribute::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "Attribute identity Identifier not supported: \"Unsupported\""
        );
    }

    #[test]
    fn test_other_attributes_not_supported_for_enums() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Error, ByRef]").unwrap();
        let err = EnumAttributes::try_from(&node).unwrap_err();
        assert_eq!(err.to_string(), "ByRef not supported for enums");
    }

    #[test]
    fn test_throws_attribute() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Throws=Error]").unwrap();
        let attrs = FunctionAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.get_throws_err(), Some("Error")));

        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[]").unwrap();
        let attrs = FunctionAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.get_throws_err(), None));
    }

    #[test]
    fn test_other_attributes_not_supported_for_functions() {
        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Throws=Error, ByRef]").unwrap();
        let err = FunctionAttributes::try_from(&node).unwrap_err();
        assert_eq!(err.to_string(), "ByRef not supported for functions");

        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Throws=Error, Self=ByArc]").unwrap();
        let err = FunctionAttributes::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "SelfType(ByArc) not supported for functions"
        );
    }

    #[test]
    fn test_method_attributes() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Throws=Error]").unwrap();
        let attrs = MethodAttributes::try_from(&node).unwrap();
        assert!(!attrs.get_self_by_arc());
        assert!(matches!(attrs.get_throws_err(), Some("Error")));

        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[]").unwrap();
        let attrs = MethodAttributes::try_from(&node).unwrap();
        assert!(!attrs.get_self_by_arc());
        assert!(attrs.get_throws_err().is_none());

        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Self=ByArc, Throws=Error]").unwrap();
        let attrs = MethodAttributes::try_from(&node).unwrap();
        assert!(attrs.get_self_by_arc());
        assert!(attrs.get_throws_err().is_some());

        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Self=ByArc]").unwrap();
        let attrs = MethodAttributes::try_from(&node).unwrap();
        assert!(attrs.get_self_by_arc());
        assert!(attrs.get_throws_err().is_none());
    }

    #[test]
    fn test_constructor_attributes() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Throws=Error]").unwrap();
        let attrs = ConstructorAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.get_throws_err(), Some("Error")));
        assert!(matches!(attrs.get_name(), None));

        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Name=MyFactory]").unwrap();
        let attrs = ConstructorAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.get_throws_err(), None));
        assert!(matches!(attrs.get_name(), Some("MyFactory")));

        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Throws=Error, Name=MyFactory]")
                .unwrap();
        let attrs = ConstructorAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.get_throws_err(), Some("Error")));
        assert!(matches!(attrs.get_name(), Some("MyFactory")));
    }

    #[test]
    fn test_other_attributes_not_supported_for_constructors() {
        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Throws=Error, ByRef]").unwrap();
        let err = ConstructorAttributes::try_from(&node).unwrap_err();
        assert_eq!(err.to_string(), "ByRef not supported for constructors");

        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Throws=Error, Self=ByArc]").unwrap();
        let err = ConstructorAttributes::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "SelfType(ByArc) not supported for constructors"
        );
    }

    #[test]
    fn test_byref_attribute() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[ByRef]").unwrap();
        let attrs = ArgumentAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.by_ref(), true));

        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[]").unwrap();
        let attrs = ArgumentAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.by_ref(), false));
    }

    #[test]
    fn test_other_attributes_not_supported_for_arguments() {
        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Throws=Error, ByRef]").unwrap();
        let err = ArgumentAttributes::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "Throws(\"Error\") not supported for arguments"
        );
    }

    #[test]
    fn test_threadsafe_attribute() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Threadsafe]").unwrap();
        let attrs = InterfaceAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.threadsafe(), true));

        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[]").unwrap();
        let attrs = InterfaceAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.threadsafe(), false));
    }

    #[test]
    fn test_enum_attribute() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Enum]").unwrap();
        let attrs = InterfaceAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.contains_enum_attr(), true));

        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[]").unwrap();
        let attrs = InterfaceAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.contains_enum_attr(), false));

        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Threadsafe]").unwrap();
        let attrs = InterfaceAttributes::try_from(&node).unwrap();
        assert!(matches!(attrs.contains_enum_attr(), false));

        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Threadsafe, Enum]").unwrap();
        let err = InterfaceAttributes::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "conflicting attributes on interface definition"
        );
    }

    #[test]
    fn test_other_attributes_not_supported_for_interfaces() {
        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[Threadsafe, ByRef]").unwrap();
        let err = InterfaceAttributes::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "ByRef not supported for interface definition"
        );
    }

    #[test]
    fn test_typedef_attribute() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Custom]").unwrap();
        let attrs = TypedefAttributes::try_from(&node).unwrap();
        assert!(attrs.is_custom());

        let (_, node) =
            weedle::attribute::ExtendedAttributeList::parse("[External=crate_name]").unwrap();
        let attrs = TypedefAttributes::try_from(&node).unwrap();
        assert!(!attrs.is_custom());
        assert_eq!(attrs.get_crate_name(), "crate_name");
    }

    #[test]
    fn test_typedef_attributes_malformed() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[Custom=foo]").unwrap();
        let err = TypedefAttributes::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "Attribute identity Identifier not supported: \"Custom\""
        );

        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[External]").unwrap();
        let err = TypedefAttributes::try_from(&node).unwrap_err();
        assert_eq!(
            err.to_string(),
            "ExtendedAttributeNoArgs not supported: \"External\""
        );
    }

    #[test]
    fn test_other_attributes_not_supported_for_typedef() {
        let (_, node) = weedle::attribute::ExtendedAttributeList::parse("[ByRef]").unwrap();
        let err = TypedefAttributes::try_from(&node).unwrap_err();
        assert_eq!(err.to_string(), "ByRef not supported for typedefs");
    }
}
