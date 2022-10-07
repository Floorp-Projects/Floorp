/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, Span, TokenStream};
use quote::{format_ident, quote};
use syn::{FnArg, ItemFn, Pat, ReturnType};

use super::ExportItem;

pub(super) fn gen_scaffolding(item: ExportItem, mod_path: &[String]) -> syn::Result<TokenStream> {
    match item {
        ExportItem::Function {
            item,
            checksum,
            meta_static_var,
        } => {
            let scaffolding = gen_fn_scaffolding(&item, mod_path, checksum)?;
            Ok(quote! {
                #scaffolding
                #meta_static_var
            })
        }
    }
}

fn gen_fn_scaffolding(
    item: &ItemFn,
    mod_path: &[String],
    checksum: u16,
) -> syn::Result<TokenStream> {
    let name = &item.sig.ident;
    let name_s = name.to_string();
    let ffi_name = Ident::new(
        &uniffi_meta::fn_ffi_symbol_name(mod_path, &name_s, checksum),
        Span::call_site(),
    );

    let mut params = Vec::new();
    let mut args = Vec::new();

    for (i, arg) in item.sig.inputs.iter().enumerate() {
        match arg {
            FnArg::Receiver(receiver) => {
                return Err(syn::Error::new_spanned(
                    receiver,
                    "methods are not yet supported by uniffi::export",
                ));
            }
            FnArg::Typed(pat_ty) => {
                let ty = &pat_ty.ty;
                let name = format_ident!("arg{i}");

                params.push(quote! { #name: <#ty as ::uniffi::FfiConverter>::FfiType });

                let panic_fmt = match &*pat_ty.pat {
                    Pat::Ident(i) => {
                        format!("Failed to convert arg '{}': {{}}", i.ident)
                    }
                    _ => {
                        format!("Failed to convert arg #{i}: {{}}")
                    }
                };
                args.push(quote! {
                    <#ty as ::uniffi::FfiConverter>::try_lift(#name).unwrap_or_else(|err| {
                        ::std::panic!(#panic_fmt, err)
                    })
                });
            }
        }
    }

    let fn_call = quote! {
        #name(#(#args),*)
    };

    // FIXME(jplatte): Use an extra trait implemented for `T: FfiConverter` as
    // well as `()` so no different codegen is needed?
    let (output, return_expr);
    match &item.sig.output {
        ReturnType::Default => {
            output = None;
            return_expr = fn_call;
        }
        ReturnType::Type(_, ty) => {
            output = Some(quote! {
                -> <#ty as ::uniffi::FfiConverter>::FfiType
            });
            return_expr = quote! {
                <#ty as ::uniffi::FfiConverter>::lower(#fn_call)
            };
        }
    }

    Ok(quote! {
        #[doc(hidden)]
        #[no_mangle]
        pub extern "C" fn #ffi_name(
            #(#params,)*
            call_status: &mut ::uniffi::RustCallStatus,
        ) #output {
            ::uniffi::deps::log::debug!(#name_s);
            ::uniffi::call_with_output(call_status, || {
                #return_expr
            })
        }
    })
}
