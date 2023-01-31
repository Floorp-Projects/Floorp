/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::interface::{ComponentInterface, Enum, Error, Record, Type};
use anyhow::anyhow;
use uniffi_meta::Metadata;

/// Add Metadata items to the ComponentInterface
///
/// This function exists to support the transition period where the `uniffi::export` macro can only
/// handle some components.  This means that crates need to continue using UDL files to define the
/// parts of the components that aren't supported yet.
///
/// To make things work, we generate a `ComponentInterface` from the UDL file, then combine it with
/// the `Metadata` items that the macro creates.
pub fn add_to_ci(
    iface: &mut ComponentInterface,
    metadata_items: Vec<Metadata>,
) -> anyhow::Result<()> {
    for item in metadata_items {
        let (item_desc, crate_name) = match &item {
            Metadata::Func(meta) => (
                format!("function `{}`", meta.name),
                meta.module_path.first().unwrap(),
            ),
            Metadata::Method(meta) => (
                format!("method `{}.{}`", meta.self_name, meta.name),
                meta.module_path.first().unwrap(),
            ),
            Metadata::Record(meta) => (
                format!("record `{}`", meta.name),
                meta.module_path.first().unwrap(),
            ),
            Metadata::Enum(meta) => (
                format!("enum `{}`", meta.name),
                meta.module_path.first().unwrap(),
            ),
            Metadata::Object(meta) => (
                format!("object `{}`", meta.name),
                meta.module_path.first().unwrap(),
            ),
            Metadata::Error(meta) => (
                format!("error `{}`", meta.name),
                meta.module_path.first().unwrap(),
            ),
        };

        let ns = iface.namespace();
        if crate_name != ns {
            return Err(anyhow!("Found {item_desc} from crate `{crate_name}`.")
                .context(format!(
                    "Main crate is expected to be named `{ns}` based on the UDL namespace."
                ))
                .context("Mixing symbols from multiple crates is not supported yet."));
        }

        match item {
            Metadata::Func(meta) => {
                iface.add_fn_meta(meta)?;
            }
            Metadata::Method(meta) => {
                iface.add_method_meta(meta);
            }
            Metadata::Record(meta) => {
                let ty = Type::Record(meta.name.clone());
                iface.types.add_known_type(&ty)?;
                iface.types.add_type_definition(&meta.name, ty)?;

                let record: Record = meta.into();
                iface.add_record_definition(record)?;
            }
            Metadata::Enum(meta) => {
                let ty = Type::Enum(meta.name.clone());
                iface.types.add_known_type(&ty)?;
                iface.types.add_type_definition(&meta.name, ty)?;

                let enum_: Enum = meta.into();
                iface.add_enum_definition(enum_)?;
            }
            Metadata::Object(meta) => {
                iface.add_object_free_fn(meta);
            }
            Metadata::Error(meta) => {
                let ty = Type::Error(meta.name.clone());
                iface.types.add_known_type(&ty)?;
                iface.types.add_type_definition(&meta.name, ty)?;

                let error: Error = meta.into();
                iface.add_error_definition(error)?;
            }
        }
    }

    iface.resolve_types()?;
    iface.derive_ffi_funcs()?;
    iface.check_consistency()?;

    Ok(())
}
