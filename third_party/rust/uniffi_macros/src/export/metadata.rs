/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::Span;

use super::ExportItem;

pub(super) mod convert;
mod function;
mod impl_;

use self::{function::gen_fn_metadata, impl_::gen_impl_metadata};

pub fn gen_metadata(item: syn::Item, mod_path: &[String]) -> syn::Result<ExportItem> {
    match item {
        syn::Item::Fn(item) => gen_fn_metadata(item.sig, mod_path),
        syn::Item::Impl(item) => gen_impl_metadata(item, mod_path),
        // FIXME: Support const / static?
        _ => Err(syn::Error::new(
            Span::call_site(),
            "unsupported item: only functions and impl \
             blocks may be annotated with this attribute",
        )),
    }
}
