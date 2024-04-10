/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::APIConverter;
use crate::{attributes::EnumAttributes, converters::convert_docstring, InterfaceCollector};
use anyhow::{bail, Result};

use uniffi_meta::{EnumMetadata, VariantMetadata};

// Note that we have 2 `APIConverter` impls here - one for the `enum` case
// (including an enum with `[Error]`), and one for the `[Error] interface` cas
// (which is still an enum, but with different "flatness" characteristics.)
impl APIConverter<EnumMetadata> for weedle::EnumDefinition<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<EnumMetadata> {
        let attributes = EnumAttributes::try_from(self.attributes.as_ref())?;
        Ok(EnumMetadata {
            module_path: ci.module_path(),
            name: self.identifier.0.to_string(),
            forced_flatness: None,
            discr_type: None,
            variants: self
                .values
                .body
                .list
                .iter()
                .map::<Result<_>, _>(|v| {
                    Ok(VariantMetadata {
                        name: v.value.0.to_string(),
                        discr: None,
                        fields: vec![],
                        docstring: v.docstring.as_ref().map(|v| convert_docstring(&v.0)),
                    })
                })
                .collect::<Result<Vec<_>>>()?,
            non_exhaustive: attributes.contains_non_exhaustive_attr(),
            docstring: self.docstring.as_ref().map(|v| convert_docstring(&v.0)),
        })
    }
}

impl APIConverter<EnumMetadata> for weedle::InterfaceDefinition<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<EnumMetadata> {
        if self.inheritance.is_some() {
            bail!("interface inheritance is not supported for enum interfaces");
        }
        let attributes = EnumAttributes::try_from(self.attributes.as_ref())?;
        Ok(EnumMetadata {
            module_path: ci.module_path(),
            name: self.identifier.0.to_string(),
            forced_flatness: Some(false),
            variants: self
                .members
                .body
                .iter()
                .map::<Result<VariantMetadata>, _>(|member| match member {
                    weedle::interface::InterfaceMember::Operation(t) => Ok(t.convert(ci)?),
                    _ => bail!(
                        "interface member type {:?} not supported in enum interface",
                        member
                    ),
                })
                .collect::<Result<Vec<_>>>()?,
            discr_type: None,
            non_exhaustive: attributes.contains_non_exhaustive_attr(),
            docstring: self.docstring.as_ref().map(|v| convert_docstring(&v.0)),
            // Enums declared using the `[Enum] interface` syntax might have variants with fields.
            //flat: false,
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use uniffi_meta::Metadata;

    #[test]
    fn test_duplicate_variants() {
        const UDL: &str = r#"
            namespace test{};
            // Weird, but currently allowed!
            // We should probably disallow this...
            enum Testing { "one", "two", "one" };
        "#;
        let mut ci = InterfaceCollector::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.types.namespace, "test");
        assert_eq!(ci.types.module_path(), "crate_name");
        assert_eq!(ci.items.len(), 1);
        let e = &ci.items.pop_first().unwrap();
        match e {
            Metadata::Enum(e) => assert_eq!(e.variants.len(), 3),
            _ => unreachable!(),
        }
    }
}
