/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use syn::spanned::Spanned;

const ERR_MSG: &str = "Expected #[handle_error(path::to::Error)]";

/// Returns the path to the type of the "internal" error.
pub(crate) fn validate(arguments: &syn::AttributeArgs) -> syn::Result<&syn::Path> {
    if arguments.len() != 1 {
        return Err(syn::Error::new(proc_macro2::Span::call_site(), ERR_MSG));
    }

    let nested_meta = arguments.iter().next().unwrap();
    if let syn::NestedMeta::Meta(syn::Meta::Path(path)) = nested_meta {
        Ok(path)
    } else {
        Err(syn::Error::new(nested_meta.span(), ERR_MSG))
    }
}
