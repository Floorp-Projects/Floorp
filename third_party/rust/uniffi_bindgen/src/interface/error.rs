/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Error definitions for a `ComponentInterface`.
//!
//! This module converts error definition from UDL into structures that can be
//! added to a `ComponentInterface`. A declaration in the UDL like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! [Error]
//! enum Example {
//!   "one",
//!   "two"
//! };
//! # "##)?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in an [`Error`] member with fieldless variants being added to the resulting [`ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # [Error]
//! # enum Example {
//! #   "one",
//! #   "two"
//! # };
//! # "##)?;
//! let err = ci.get_error_definition("Example").unwrap();
//! assert_eq!(err.name(), "Example");
//! assert_eq!(err.variants().len(), 2);
//! assert_eq!(err.variants()[0].name(), "one");
//! assert_eq!(err.variants()[1].name(), "two");
//! assert_eq!(err.is_flat(), true);
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
//! # "##)?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in an [`Error`] member with variants that have fields being added to the resulting [`ComponentInterface`]:
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
//! # "##)?;
//! let err = ci.get_error_definition("Example").unwrap();
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
//! # Ok::<(), anyhow::Error>(())
//! ```

use anyhow::Result;

use super::enum_::{Enum, Variant};
use super::types::{Type, TypeIterator};
use super::{APIConverter, ComponentInterface};

/// Represents an Error that might be thrown by functions/methods in the component interface.
///
/// Errors are represented in the UDL as enums with the special `[Error]` attribute, but
/// they're handled in the FFI very differently. We create them in `uniffi::call_with_result()` if
/// the wrapped function returns an `Err` value
/// struct and assign an integer error code to each variant.
#[derive(Debug, Clone, Hash)]
pub struct Error {
    pub name: String,
    enum_: Enum,
}

impl Error {
    pub fn from_enum(enum_: Enum) -> Self {
        Self {
            name: enum_.name.clone(),
            enum_,
        }
    }

    pub fn type_(&self) -> Type {
        // *sigh* at the clone here, the relationship between a ComponentInterace
        // and its contained types could use a bit of a cleanup.
        Type::Error(self.name.clone())
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn wrapped_enum(&self) -> &Enum {
        &self.enum_
    }

    pub fn variants(&self) -> Vec<&Variant> {
        self.enum_.variants()
    }

    pub fn is_flat(&self) -> bool {
        self.enum_.is_flat()
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        self.wrapped_enum().iter_types()
    }
}

impl APIConverter<Error> for weedle::EnumDefinition<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Error> {
        Ok(Error::from_enum(APIConverter::<Enum>::convert(self, ci)?))
    }
}

impl APIConverter<Error> for weedle::InterfaceDefinition<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Error> {
        Ok(Error::from_enum(APIConverter::<Enum>::convert(self, ci)?))
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variants() {
        const UDL: &str = r#"
            namespace test{};
            [Error]
            enum Testing { "one", "two", "three" };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.error_definitions().len(), 1);
        let error = ci.get_error_definition("Testing").unwrap();
        assert_eq!(
            error
                .variants()
                .iter()
                .map(|v| v.name())
                .collect::<Vec<&str>>(),
            vec!("one", "two", "three")
        );
        assert!(error.is_flat());
    }

    #[test]
    fn test_duplicate_variants() {
        const UDL: &str = r#"
            namespace test{};
            // Weird, but currently allowed!
            // We should probably disallow this...
            [Error]
            enum Testing { "one", "two", "one" };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.error_definitions().len(), 1);
        assert_eq!(
            ci.get_error_definition("Testing").unwrap().variants().len(),
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
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.error_definitions().len(), 1);
        let error: &Error = ci.get_error_definition("Testing").unwrap();
        assert_eq!(
            error
                .variants()
                .iter()
                .map(|v| v.name())
                .collect::<Vec<&str>>(),
            vec!("One", "Two")
        );
        assert!(!error.is_flat());
    }
}
