/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Uniffi support for webidl syntax, typically from a .udl file, as described by weedle.
//!
//! This library is dedicated to parsing a string in a webidl syntax, as described by
//! weedle and with our own custom take on the attributes etc, pushing the boundaries
//! of that syntax to describe a uniffi `MetatadataGroup`.
//!
//! The output of this module is consumed by uniffi_bindgen to generate stuff.

mod attributes;
mod collectors;
mod converters;
mod finder;
mod literal;
mod resolver;

use anyhow::Result;
use collectors::{InterfaceCollector, TypeCollector};
use uniffi_meta::Type;

/// The single entry-point to this module.
pub fn parse_udl(udl: &str, crate_name: &str) -> Result<uniffi_meta::MetadataGroup> {
    Ok(InterfaceCollector::from_webidl(udl, crate_name)?.into())
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_group() {
        const UDL: &str = r#"
            namespace test{};
            dictionary Empty {};
        "#;
        let group = parse_udl(UDL, "crate_name").unwrap();
        assert_eq!(group.namespace.name, "test");
        assert_eq!(group.items.len(), 1);
        assert!(matches!(
            group.items.into_iter().next().unwrap(),
            uniffi_meta::Metadata::Record(r) if r.module_path == "crate_name" && r.name == "Empty" && r.fields.is_empty()
        ));
    }
}
