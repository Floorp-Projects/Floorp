/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    export::ImplItem,
    fnsig::{FnKind, FnSignature},
    util::{create_metadata_items, ident_to_string},
};
use proc_macro2::{Span, TokenStream};
use quote::quote;
use std::iter;
use syn::Ident;

pub(super) fn trait_impl(
    ident: &Ident,
    trait_ident: &Ident,
    internals_ident: &Ident,
    items: &[ImplItem],
) -> syn::Result<TokenStream> {
    let trait_name = ident_to_string(trait_ident);
    let trait_impl_methods = items
        .iter()
        .map(|item| match item {
            ImplItem::Method(sig) => gen_method_impl(sig, internals_ident),
            _ => unreachable!("traits have no constructors"),
        })
        .collect::<syn::Result<TokenStream>>()?;

    Ok(quote! {
        #[doc(hidden)]
        #[derive(Debug)]
        struct #ident {
            handle: u64,
        }

        impl #ident {
            fn new(handle: u64) -> Self {
                Self { handle }
            }
        }

        impl ::std::ops::Drop for #ident {
            fn drop(&mut self) {
                #internals_ident.invoke_callback::<(), crate::UniFfiTag>(
                    self.handle, uniffi::IDX_CALLBACK_FREE, Default::default()
                )
            }
        }

        ::uniffi::deps::static_assertions::assert_impl_all!(#ident: Send);

        impl #trait_ident for #ident {
            #trait_impl_methods
        }

        ::uniffi::ffi_converter_callback_interface!(#trait_ident, #ident, #trait_name, crate::UniFfiTag);
    })
}

fn gen_method_impl(sig: &FnSignature, internals_ident: &Ident) -> syn::Result<TokenStream> {
    let FnSignature {
        ident,
        return_ty,
        kind,
        receiver,
        ..
    } = sig;
    let index = match kind {
        // Note: the callback index is 1-based, since 0 is reserved for the free function
        FnKind::TraitMethod { index, .. } => index + 1,
        k => {
            return Err(syn::Error::new(
                sig.span,
                format!(
                    "Internal UniFFI error: Unexpected function kind for callback interface {k:?}"
                ),
            ));
        }
    };

    if receiver.is_none() {
        return Err(syn::Error::new(
            sig.span,
            "callback interface methods must take &self as their first argument",
        ));
    }
    let params = sig.params();
    let buf_ident = Ident::new("uniffi_args_buf", Span::call_site());
    let write_exprs = sig.write_exprs(&buf_ident);

    Ok(quote! {
        fn #ident(&self, #(#params),*) -> #return_ty {
            #[allow(unused_mut)]
            let mut #buf_ident = ::std::vec::Vec::new();
            #(#write_exprs;)*
            let uniffi_args_rbuf = uniffi::RustBuffer::from_vec(#buf_ident);

            #internals_ident.invoke_callback::<#return_ty, crate::UniFfiTag>(self.handle, #index, uniffi_args_rbuf)
        }
    })
}

pub(super) fn metadata_items(
    self_ident: &Ident,
    items: &[ImplItem],
    module_path: &str,
) -> syn::Result<Vec<TokenStream>> {
    let trait_name = ident_to_string(self_ident);
    let callback_interface_items = create_metadata_items(
        "callback_interface",
        &trait_name,
        quote! {
            ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::CALLBACK_INTERFACE)
                .concat_str(#module_path)
                .concat_str(#trait_name)
        },
        None,
    );

    iter::once(Ok(callback_interface_items))
        .chain(items.iter().map(|item| match item {
            ImplItem::Method(sig) => sig.metadata_items(),
            _ => unreachable!("traits have no constructors"),
        }))
        .collect()
}
