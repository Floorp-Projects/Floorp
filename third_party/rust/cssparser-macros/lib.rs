/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate proc_macro;

use proc_macro::TokenStream;

#[proc_macro]
pub fn _cssparser_internal_max_len(input: TokenStream) -> TokenStream {
    struct Input {
        max_length: usize,
    }

    impl syn::parse::Parse for Input {
        fn parse(input: syn::parse::ParseStream) -> syn::parse::Result<Self> {
            let mut max_length = 0;
            while !input.is_empty() {
                if input.peek(syn::Token![_]) {
                    input.parse::<syn::Token![_]>().unwrap();
                    continue;
                }
                let lit: syn::LitStr = input.parse()?;
                let value = lit.value();
                if value.to_ascii_lowercase() != value {
                    return Err(syn::Error::new(lit.span(), "must be ASCII-lowercase"));
                }
                max_length = max_length.max(value.len());
            }
            Ok(Input { max_length })
        }
    }

    let Input { max_length } = syn::parse_macro_input!(input);
    quote::quote!(
        pub(super) const MAX_LENGTH: usize = #max_length;
    )
    .into()
}
