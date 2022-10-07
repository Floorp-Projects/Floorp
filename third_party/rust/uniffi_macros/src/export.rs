/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::TokenStream;

mod metadata;
mod scaffolding;

use self::metadata::gen_metadata;
use self::scaffolding::gen_scaffolding;

// TODO(jplatte): Ensure no generics, no async, …
// TODO(jplatte): Aggregate errors instead of short-circuiting, whereever possible

enum ExportItem {
    Function {
        item: syn::ItemFn,
        checksum: u16,
        meta_static_var: TokenStream,
    },
}

pub fn expand_export(item: syn::Item, mod_path: &[String]) -> syn::Result<TokenStream> {
    let item = gen_metadata(item, mod_path)?;
    gen_scaffolding(item, mod_path)
}
