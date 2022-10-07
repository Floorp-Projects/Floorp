/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::interface::ComponentInterface;
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
                iface.add_function_definition(meta.into())?;
            }
            Metadata::Method(meta) => {
                iface.add_method_definition(meta)?;
            }
        }
    }

    iface.check_consistency()?;
    iface.derive_ffi_funcs()?;

    Ok(())
}
