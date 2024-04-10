/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Enum definitions for a `ComponentInterface`.
//!
//! This module converts enum definition from UDL into structures that can be
//! added to a `ComponentInterface`. A declaration in the UDL like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! enum Example {
//!   "one",
//!   "two"
//! };
//! # "##, "crate_name")?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in a [`Enum`] member being added to the resulting [`crate::ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # enum Example {
//! #   "one",
//! #   "two"
//! # };
//! # "##, "crate_name")?;
//! let e = ci.get_enum_definition("Example").unwrap();
//! assert_eq!(e.name(), "Example");
//! assert_eq!(e.variants().len(), 2);
//! assert_eq!(e.variants()[0].name(), "one");
//! assert_eq!(e.variants()[1].name(), "two");
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Like in Rust, UniFFI enums can contain associated data, but this needs to be
//! declared with a different syntax in order to work within the restrictions of
//! WebIDL. A declaration like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! [Enum]
//! interface Example {
//!   Zero();
//!   One(u32 first);
//!   Two(u32 first, string second);
//! };
//! # "##, "crate_name")?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in an [`Enum`] member whose variants have associated fields:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # [Enum]
//! # interface ExampleWithData {
//! #   Zero();
//! #   One(u32 first);
//! #   Two(u32 first, string second);
//! # };
//! # "##, "crate_name")?;
//! let e = ci.get_enum_definition("ExampleWithData").unwrap();
//! assert_eq!(e.name(), "ExampleWithData");
//! assert_eq!(e.variants().len(), 3);
//! assert_eq!(e.variants()[0].name(), "Zero");
//! assert_eq!(e.variants()[0].fields().len(), 0);
//! assert_eq!(e.variants()[1].name(), "One");
//! assert_eq!(e.variants()[1].fields().len(), 1);
//! assert_eq!(e.variants()[1].fields()[0].name(), "first");
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! # Enums are also used to represent error definitions for a `ComponentInterface`.
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! [Error]
//! enum Example {
//!   "one",
//!   "two"
//! };
//! # "##, "crate_name")?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in an [`Enum`] member with fieldless variants being added to the resulting [`crate::ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # [Error]
//! # enum Example {
//! #   "one",
//! #   "two"
//! # };
//! # "##, "crate_name")?;
//! let err = ci.get_enum_definition("Example").unwrap();
//! assert_eq!(err.name(), "Example");
//! assert_eq!(err.variants().len(), 2);
//! assert_eq!(err.variants()[0].name(), "one");
//! assert_eq!(err.variants()[1].name(), "two");
//! assert_eq!(err.is_flat(), true);
//! assert!(ci.is_name_used_as_error(&err.name()));
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! A declaration in the UDL like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! [Error]
//! interface Example {
//!   one(i16 code);
//!   two(string reason);
//!   three(i32 x, i32 y);
//! };
//! # "##, "crate_name")?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in an [`Enum`] member with variants that have fields being added to the resulting [`crate::ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # [Error]
//! # interface Example {
//! #   one();
//! #   two(string reason);
//! #   three(i32 x, i32 y);
//! # };
//! # "##, "crate_name")?;
//! let err = ci.get_enum_definition("Example").unwrap();
//! assert_eq!(err.name(), "Example");
//! assert_eq!(err.variants().len(), 3);
//! assert_eq!(err.variants()[0].name(), "one");
//! assert_eq!(err.variants()[1].name(), "two");
//! assert_eq!(err.variants()[2].name(), "three");
//! assert_eq!(err.variants()[0].fields().len(), 0);
//! assert_eq!(err.variants()[1].fields().len(), 1);
//! assert_eq!(err.variants()[1].fields()[0].name(), "reason");
//! assert_eq!(err.variants()[2].fields().len(), 2);
//! assert_eq!(err.variants()[2].fields()[0].name(), "x");
//! assert_eq!(err.variants()[2].fields()[1].name(), "y");
//! assert_eq!(err.is_flat(), false);
//! assert!(ci.is_name_used_as_error(err.name()));
//! # Ok::<(), anyhow::Error>(())
//! ```

use anyhow::Result;
use uniffi_meta::Checksum;

use super::record::Field;
use super::{AsType, Type, TypeIterator};

/// Represents an enum with named variants, each of which may have named
/// and typed fields.
///
/// Enums are passed across the FFI by serializing to a bytebuffer, with a
/// i32 indicating the variant followed by the serialization of each field.
#[derive(Debug, Clone, PartialEq, Eq, Checksum)]
pub struct Enum {
    pub(super) name: String,
    pub(super) module_path: String,
    pub(super) variants: Vec<Variant>,
    // NOTE: `flat` is a misleading name and to make matters worse, has 2 different
    // meanings depending on the context :(
    // * When used as part of Rust scaffolding generation, it means "is this enum
    //   used with an Error, and that error should we lowered to foreign bindings
    //   by converting each variant to a string and lowering the variant with that
    //   string?". In that context, it should probably be called `lowered_as_string` or
    //   similar.
    // * When used as part of bindings generation, it means "does this enum have only
    //   variants with no associated data"? The foreign binding generators are likely
    //   to generate significantly different versions of the enum based on that value.
    //
    // The reason it is described as "has 2 different meanings" by way of example:
    // * For an Enum described as being a flat error, but the enum itself has variants with data,
    //   `flat` will be `true` for the Enum when generating scaffolding and `false` when
    //   generating bindings.
    // * For an Enum not used as an error but which has no variants with data, `flat` will be
    //   false when generating the scaffolding but `true` when generating bindings.
    pub(super) flat: bool,
}

impl Enum {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn variants(&self) -> &[Variant] {
        &self.variants
    }

    pub fn is_flat(&self) -> bool {
        self.flat
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.variants.iter().flat_map(Variant::iter_types))
    }

    // Sadly can't use TryFrom due to the 'is_flat' complication.
    pub fn try_from_meta(meta: uniffi_meta::EnumMetadata, flat: bool) -> Result<Self> {
        // This is messy - error enums are considered "flat" if the user
        // opted in via a special attribute, regardless of whether the enum
        // is actually flat.
        // Real enums are considered flat iff they are actually flat.
        // We don't have that context here, so this is handled by our caller.
        Ok(Self {
            name: meta.name,
            module_path: meta.module_path,
            variants: meta
                .variants
                .into_iter()
                .map(TryInto::try_into)
                .collect::<Result<_>>()?,
            flat,
        })
    }
}

impl AsType for Enum {
    fn as_type(&self) -> Type {
        Type::Enum {
            name: self.name.clone(),
            module_path: self.module_path.clone(),
        }
    }
}

/// Represents an individual variant in an Enum.
///
/// Each variant has a name and zero or more fields.
#[derive(Debug, Clone, Default, PartialEq, Eq, Checksum)]
pub struct Variant {
    pub(super) name: String,
    pub(super) fields: Vec<Field>,
}

impl Variant {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn fields(&self) -> &[Field] {
        &self.fields
    }

    pub fn has_fields(&self) -> bool {
        !self.fields.is_empty()
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.fields.iter().flat_map(Field::iter_types))
    }
}

impl TryFrom<uniffi_meta::VariantMetadata> for Variant {
    type Error = anyhow::Error;

    fn try_from(meta: uniffi_meta::VariantMetadata) -> Result<Self> {
        Ok(Self {
            name: meta.name,
            fields: meta
                .fields
                .into_iter()
                .map(TryInto::try_into)
                .collect::<Result<_>>()?,
        })
    }
}

#[cfg(test)]
mod test {
    use super::super::{ComponentInterface, FfiType};
    use super::*;

    #[test]
    fn test_duplicate_variants() {
        const UDL: &str = r#"
            namespace test{};
            // Weird, but currently allowed!
            // We should probably disallow this...
            enum Testing { "one", "two", "one" };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.enum_definitions().count(), 1);
        assert_eq!(
            ci.get_enum_definition("Testing").unwrap().variants().len(),
            3
        );
    }

    #[test]
    fn test_associated_data() {
        const UDL: &str = r#"
            namespace test {
                void takes_an_enum(TestEnum e);
                void takes_an_enum_with_data(TestEnumWithData ed);
                TestEnum returns_an_enum();
                TestEnumWithData returns_an_enum_with_data();
            };

            enum TestEnum { "one", "two" };

            [Enum]
            interface TestEnumWithData {
                Zero();
                One(u32 first);
                Two(u32 first, string second);
            };

            [Enum]
            interface TestEnumWithoutData {
                One();
                Two();
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.enum_definitions().count(), 3);
        assert_eq!(ci.function_definitions().len(), 4);

        // The "flat" enum with no associated data.
        let e = ci.get_enum_definition("TestEnum").unwrap();
        assert!(e.is_flat());
        assert_eq!(e.variants().len(), 2);
        assert_eq!(
            e.variants().iter().map(|v| v.name()).collect::<Vec<_>>(),
            vec!["one", "two"]
        );
        assert_eq!(e.variants()[0].fields().len(), 0);
        assert_eq!(e.variants()[1].fields().len(), 0);

        // The enum with associated data.
        let ed = ci.get_enum_definition("TestEnumWithData").unwrap();
        assert!(!ed.is_flat());
        assert_eq!(ed.variants().len(), 3);
        assert_eq!(
            ed.variants().iter().map(|v| v.name()).collect::<Vec<_>>(),
            vec!["Zero", "One", "Two"]
        );
        assert_eq!(ed.variants()[0].fields().len(), 0);
        assert_eq!(
            ed.variants()[1]
                .fields()
                .iter()
                .map(|f| f.name())
                .collect::<Vec<_>>(),
            vec!["first"]
        );
        assert_eq!(
            ed.variants()[1]
                .fields()
                .iter()
                .map(|f| f.as_type())
                .collect::<Vec<_>>(),
            vec![Type::UInt32]
        );
        assert_eq!(
            ed.variants()[2]
                .fields()
                .iter()
                .map(|f| f.name())
                .collect::<Vec<_>>(),
            vec!["first", "second"]
        );
        assert_eq!(
            ed.variants()[2]
                .fields()
                .iter()
                .map(|f| f.as_type())
                .collect::<Vec<_>>(),
            vec![Type::UInt32, Type::String]
        );

        // The enum declared via interface, but with no associated data.
        let ewd = ci.get_enum_definition("TestEnumWithoutData").unwrap();
        assert_eq!(ewd.variants().len(), 2);
        assert_eq!(
            ewd.variants().iter().map(|v| v.name()).collect::<Vec<_>>(),
            vec!["One", "Two"]
        );
        assert_eq!(ewd.variants()[0].fields().len(), 0);
        assert_eq!(ewd.variants()[1].fields().len(), 0);

        // Flat enums pass over the FFI as bytebuffers.
        // (It might be nice to optimize these to pass as plain integers, but that's
        // difficult atop the current factoring of `ComponentInterface` and friends).
        let farg = ci.get_function_definition("takes_an_enum").unwrap();
        assert_eq!(
            farg.arguments()[0].as_type(),
            Type::Enum {
                name: "TestEnum".into(),
                module_path: "crate_name".into()
            }
        );
        assert_eq!(
            farg.ffi_func().arguments()[0].type_(),
            FfiType::RustBuffer(None)
        );
        let fret = ci.get_function_definition("returns_an_enum").unwrap();
        assert!(
            matches!(fret.return_type(), Some(Type::Enum { name, .. }) if name == "TestEnum" && !ci.is_name_used_as_error(name))
        );
        assert!(matches!(
            fret.ffi_func().return_type(),
            Some(FfiType::RustBuffer(None))
        ));

        // Enums with associated data pass over the FFI as bytebuffers.
        let farg = ci
            .get_function_definition("takes_an_enum_with_data")
            .unwrap();
        assert_eq!(
            farg.arguments()[0].as_type(),
            Type::Enum {
                name: "TestEnumWithData".into(),
                module_path: "crate_name".into()
            }
        );
        assert_eq!(
            farg.ffi_func().arguments()[0].type_(),
            FfiType::RustBuffer(None)
        );
        let fret = ci
            .get_function_definition("returns_an_enum_with_data")
            .unwrap();
        assert!(
            matches!(fret.return_type(), Some(Type::Enum { name, .. }) if name == "TestEnumWithData" && !ci.is_name_used_as_error(name))
        );
        assert!(matches!(
            fret.ffi_func().return_type(),
            Some(FfiType::RustBuffer(None))
        ));
    }

    // Tests for [Error], which are represented as `Enum`
    #[test]
    fn test_variants() {
        const UDL: &str = r#"
            namespace test{};
            [Error]
            enum Testing { "one", "two", "three" };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.enum_definitions().count(), 1);
        let error = ci.get_enum_definition("Testing").unwrap();
        assert_eq!(
            error
                .variants()
                .iter()
                .map(|v| v.name())
                .collect::<Vec<&str>>(),
            vec!("one", "two", "three")
        );
        assert!(error.is_flat());
        assert!(ci.is_name_used_as_error(&error.name));
    }

    #[test]
    fn test_duplicate_error_variants() {
        const UDL: &str = r#"
            namespace test{};
            // Weird, but currently allowed!
            // We should probably disallow this...
            [Error]
            enum Testing { "one", "two", "one" };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.enum_definitions().count(), 1);
        assert_eq!(
            ci.get_enum_definition("Testing").unwrap().variants().len(),
            3
        );
    }

    #[test]
    fn test_variant_data() {
        const UDL: &str = r#"
            namespace test{};

            [Error]
            interface Testing {
                One(string reason);
                Two(u8 code);
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.enum_definitions().count(), 1);
        let error: &Enum = ci.get_enum_definition("Testing").unwrap();
        assert_eq!(
            error
                .variants()
                .iter()
                .map(|v| v.name())
                .collect::<Vec<&str>>(),
            vec!("One", "Two")
        );
        assert!(!error.is_flat());
        assert!(ci.is_name_used_as_error(&error.name));
    }

    #[test]
    fn test_enum_variant_named_error() {
        const UDL: &str = r#"
            namespace test{};

            [Enum]
            interface Testing {
                Normal(string first);
                Error(string first);
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.enum_definitions().count(), 1);
        let testing: &Enum = ci.get_enum_definition("Testing").unwrap();
        assert_eq!(
            testing.variants()[0]
                .fields()
                .iter()
                .map(|f| f.name())
                .collect::<Vec<_>>(),
            vec!["first"]
        );
        assert_eq!(
            testing.variants()[0]
                .fields()
                .iter()
                .map(|f| f.as_type())
                .collect::<Vec<_>>(),
            vec![Type::String]
        );
        assert_eq!(
            testing.variants()[1]
                .fields()
                .iter()
                .map(|f| f.name())
                .collect::<Vec<_>>(),
            vec!["first"]
        );
        assert_eq!(
            testing.variants()[1]
                .fields()
                .iter()
                .map(|f| f.as_type())
                .collect::<Vec<_>>(),
            vec![Type::String]
        );
        assert_eq!(
            testing
                .variants()
                .iter()
                .map(|v| v.name())
                .collect::<Vec<_>>(),
            vec!["Normal", "Error"]
        );
    }
}
