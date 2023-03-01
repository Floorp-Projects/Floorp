/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub(crate) fn validate(arguments: &syn::AttributeArgs) -> syn::Result<()> {
    // For now we validate that there are no arguments.
    // if we want to improve the macro to support attributes, for example,
    // if we'd like to pass in the return type of the body we can modify this
    // logic to parse and return the type.
    if !arguments.is_empty() {
        return Err(syn::Error::new(
            proc_macro2::Span::call_site(),
            "Expected #[handle_error] with no arguments",
        ));
    }
    Ok(())
}
