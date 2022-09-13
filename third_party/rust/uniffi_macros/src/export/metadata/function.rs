/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use uniffi_meta::{checksum, FnMetadata, FnParamMetadata};

use super::{convert_type, create_metadata_static_var};
use crate::export::ExportItem;

pub(super) fn gen_fn_metadata(item: syn::ItemFn, mod_path: &[String]) -> syn::Result<ExportItem> {
    let meta = fn_metadata(&item, mod_path)?;

    Ok(ExportItem::Function {
        checksum: checksum(&meta),
        meta_static_var: create_metadata_static_var(&item.sig.ident, meta.into()),
        item,
    })
}

fn fn_metadata(f: &syn::ItemFn, mod_path: &[String]) -> syn::Result<FnMetadata> {
    let return_type = match &f.sig.output {
        syn::ReturnType::Default => None,
        syn::ReturnType::Type(_, ty) => Some(convert_type(ty)?),
    };

    Ok(FnMetadata {
        module_path: mod_path.to_owned(),
        name: f.sig.ident.to_string(),
        inputs: f
            .sig
            .inputs
            .iter()
            .map(|a| fn_param_metadata(a, false))
            .collect::<syn::Result<_>>()?,
        return_type,
    })
}

fn fn_param_metadata(a: &syn::FnArg, _is_method: bool) -> syn::Result<FnParamMetadata> {
    let (name, ty) = match a {
        syn::FnArg::Receiver(_) => unimplemented!(),
        syn::FnArg::Typed(pat_ty) => {
            let name = match &*pat_ty.pat {
                syn::Pat::Ident(pat_id) => pat_id.ident.to_string(),
                _ => unimplemented!(),
            };
            (name, &pat_ty.ty)
        }
    };

    Ok(FnParamMetadata {
        name,
        ty: convert_type(ty)?,
    })
}
