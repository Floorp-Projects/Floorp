/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::interface::{CallbackInterface, ComponentInterface, Enum, Record, Type};
use anyhow::{bail, Context};
use uniffi_meta::{
    create_metadata_groups, group_metadata, EnumMetadata, ErrorMetadata, Metadata, MetadataGroup,
};

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
    let mut group_map = create_metadata_groups(&metadata_items);
    group_metadata(&mut group_map, metadata_items)?;
    for group in group_map.into_values() {
        if group.items.is_empty() {
            continue;
        }
        if group.namespace.name != iface.namespace() {
            let crate_name = group.namespace.crate_name;
            bail!("Found metadata items from crate `{crate_name}`.  Use the `--library` to generate bindings for multiple crates")
        }
        add_group_to_ci(iface, group)?;
    }

    Ok(())
}

/// Add items from a MetadataGroup to a component interface
pub fn add_group_to_ci(iface: &mut ComponentInterface, group: MetadataGroup) -> anyhow::Result<()> {
    if group.namespace.name != iface.namespace() {
        bail!(
            "Namespace mismatch: {} - {}",
            group.namespace.name,
            iface.namespace()
        );
    }

    for item in group.items {
        add_item_to_ci(iface, item)?
    }

    iface
        .derive_ffi_funcs()
        .context("Failed to derive FFI functions")?;
    iface
        .check_consistency()
        .context("ComponentInterface consistency error")?;
    Ok(())
}

fn add_enum_to_ci(
    iface: &mut ComponentInterface,
    meta: EnumMetadata,
    is_flat: bool,
) -> anyhow::Result<()> {
    let ty = Type::Enum {
        name: meta.name.clone(),
        module_path: meta.module_path.clone(),
    };
    iface.types.add_known_type(&ty)?;

    let enum_ = Enum::try_from_meta(meta, is_flat)?;
    iface.add_enum_definition(enum_)?;
    Ok(())
}

fn add_item_to_ci(iface: &mut ComponentInterface, item: Metadata) -> anyhow::Result<()> {
    match item {
        Metadata::Namespace(_) => unreachable!(),
        Metadata::UdlFile(_) => (),
        Metadata::Func(meta) => {
            iface.add_function_definition(meta.into())?;
        }
        Metadata::Constructor(meta) => {
            iface.add_constructor_meta(meta)?;
        }
        Metadata::Method(meta) => {
            iface.add_method_meta(meta)?;
        }
        Metadata::Record(meta) => {
            let ty = Type::Record {
                name: meta.name.clone(),
                module_path: meta.module_path.clone(),
            };
            iface.types.add_known_type(&ty)?;
            let record: Record = meta.try_into()?;
            iface.add_record_definition(record)?;
        }
        Metadata::Enum(meta) => {
            let flat = meta.variants.iter().all(|v| v.fields.is_empty());
            add_enum_to_ci(iface, meta, flat)?;
        }
        Metadata::Object(meta) => {
            iface.types.add_known_type(&Type::Object {
                module_path: meta.module_path.clone(),
                name: meta.name.clone(),
                imp: meta.imp,
            })?;
            iface.add_object_meta(meta)?;
        }
        Metadata::UniffiTrait(meta) => {
            iface.add_uniffitrait_meta(meta)?;
        }
        Metadata::CallbackInterface(meta) => {
            iface.types.add_known_type(&Type::CallbackInterface {
                module_path: meta.module_path.clone(),
                name: meta.name.clone(),
            })?;
            iface.add_callback_interface_definition(CallbackInterface::new(
                meta.name,
                meta.module_path,
            ));
        }
        Metadata::TraitMethod(meta) => {
            iface.add_trait_method_meta(meta)?;
        }
        Metadata::Error(meta) => {
            iface.note_name_used_as_error(meta.name());
            match meta {
                ErrorMetadata::Enum { enum_, is_flat } => {
                    add_enum_to_ci(iface, enum_, is_flat)?;
                }
            };
        }
        Metadata::CustomType(meta) => {
            iface.types.add_known_type(&Type::Custom {
                module_path: meta.module_path.clone(),
                name: meta.name,
                builtin: Box::new(meta.builtin),
            })?;
        }
    }
    Ok(())
}
