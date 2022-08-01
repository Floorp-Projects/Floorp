/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Namespace definition for a `ComponentInterface`.
//!
//! This module converts a namespace definition from UDL into structures that
//! can be added to a `ComponentInterface`.
//!
//! In WebIDL proper, each `namespace` declares a set of functions and attributes that
//! are exposed as a global object of that name, and there can be any number of such
//! namespace definitions.
//!
//! For our purposes with UDL, we expect just a single `namespace` declaration, which
//! defines properties of the component as a whole (currently just the name). It also
//! contains the functions that will be exposed as individual plain functions exported by
//! the component, if any. So something like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! namespace example {
//!   string hello();
//! };
//! # "##)?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Declares a component named "example" with a single exported function named "hello":
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {
//! #   string hello();
//! # };
//! # "##)?;
//! assert_eq!(ci.namespace(), "example");
//! assert_eq!(ci.get_function_definition("hello").unwrap().name(), "hello");
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! While this awkward-looking syntax:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! namespace example {};
//! # "##)?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Declares a component named "example" with no exported functions:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # "##)?;
//! assert_eq!(ci.namespace(), "example");
//! assert_eq!(ci.function_definitions().len(), 0);
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Yeah, it's a bit of an awkward fit syntactically, but it's enough
//! to get us up and running for a first version of this tool.

use anyhow::{bail, Result};

use super::{APIBuilder, APIConverter, ComponentInterface};

/// A namespace is currently just a name, but might hold more metadata about
/// the component in future.
///
#[derive(Debug, Clone, Hash)]
pub struct Namespace {
    pub(super) name: String,
}

impl APIBuilder for weedle::NamespaceDefinition<'_> {
    fn process(&self, ci: &mut ComponentInterface) -> Result<()> {
        if self.attributes.is_some() {
            bail!("namespace attributes are not supported yet");
        }
        ci.add_namespace_definition(Namespace {
            name: self.identifier.0.to_string(),
        })?;
        for func in self.members.body.convert(ci)? {
            ci.add_function_definition(func)?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_empty_namespace() {
        const UDL: &str = r#"
            namespace foobar{};
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.namespace(), "foobar");
    }

    #[test]
    fn test_namespace_with_functions() {
        const UDL: &str = r#"
            namespace foobar{
                boolean hello();
                void world();
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.namespace(), "foobar");
        assert_eq!(ci.function_definitions().len(), 2);
        assert!(ci.get_function_definition("hello").is_some());
        assert!(ci.get_function_definition("world").is_some());
        assert!(ci.get_function_definition("potato").is_none());
    }

    #[test]
    fn test_rejects_duplicate_namespaces() {
        const UDL: &str = r#"
            namespace foobar{
                boolean hello();
                void world();
            };
            namespace something_else{};
        "#;
        let err = ComponentInterface::from_webidl(UDL).unwrap_err();
        assert_eq!(err.to_string(), "duplicate namespace definition");
    }
}
