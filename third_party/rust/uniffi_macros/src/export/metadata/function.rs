/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use uniffi_meta::FnMetadata;

use super::convert::{fn_param_metadata, return_type_metadata};
use crate::export::ExportItem;

pub(super) fn gen_fn_metadata(sig: syn::Signature, mod_path: &[String]) -> syn::Result<ExportItem> {
    let metadata = fn_metadata(&sig, mod_path)?;

    Ok(ExportItem::Function {
        sig: Box::new(sig),
        metadata,
    })
}

fn fn_metadata(sig: &syn::Signature, mod_path: &[String]) -> syn::Result<FnMetadata> {
    Ok(FnMetadata {
        module_path: mod_path.to_owned(),
        name: sig.ident.to_string(),
        inputs: fn_param_metadata(&sig.inputs)?,
        return_type: return_type_metadata(&sig.output)?,
    })
}
