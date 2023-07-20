/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use uniffi_meta::FnMetadata;

use super::convert::{convert_return_type, fn_param_metadata};
use crate::export::{ExportItem, Signature};

pub(super) fn gen_fn_metadata(sig: syn::Signature, mod_path: &[String]) -> syn::Result<ExportItem> {
    let sig = Signature::new(sig)?;
    let metadata = fn_metadata(&sig, mod_path)?;
    Ok(ExportItem::Function { sig, metadata })
}

fn fn_metadata(sig: &Signature, mod_path: &[String]) -> syn::Result<FnMetadata> {
    let (return_type, throws) = match &sig.output {
        Some(ret) => (
            convert_return_type(&ret.ty)?,
            ret.throws.as_ref().map(ToString::to_string),
        ),
        None => (None, None),
    };

    Ok(FnMetadata {
        module_path: mod_path.to_owned(),
        name: sig.ident.to_string(),
        inputs: fn_param_metadata(&sig.inputs)?,
        return_type,
        throws,
    })
}
