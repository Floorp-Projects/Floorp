/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, TokenStream};
use quote::quote;
use std::iter;

use super::attributes::{AsyncRuntime, ExportAttributeArguments};
use crate::fnsig::{FnKind, FnSignature, NamedArg};

pub(super) fn gen_fn_scaffolding(
    sig: FnSignature,
    arguments: &ExportAttributeArguments,
) -> syn::Result<TokenStream> {
    if sig.receiver.is_some() {
        return Err(syn::Error::new(
            sig.span,
            "Unexpected self param (Note: uniffi::export must be used on the impl block, not its containing fn's)"
        ));
    }
    let metadata_items = sig.metadata_items()?;
    let scaffolding_func = gen_ffi_function(&sig, arguments)?;
    Ok(quote! {
        #scaffolding_func
        #metadata_items
    })
}

pub(super) fn gen_constructor_scaffolding(
    sig: FnSignature,
    arguments: &ExportAttributeArguments,
) -> syn::Result<TokenStream> {
    if sig.receiver.is_some() {
        return Err(syn::Error::new(
            sig.span,
            "constructors must not have a self parameter",
        ));
    }
    let metadata_items = sig.metadata_items()?;
    let scaffolding_func = gen_ffi_function(&sig, arguments)?;
    Ok(quote! {
        #scaffolding_func
        #metadata_items
    })
}

pub(super) fn gen_method_scaffolding(
    sig: FnSignature,
    arguments: &ExportAttributeArguments,
) -> syn::Result<TokenStream> {
    let scaffolding_func = if sig.receiver.is_none() {
        return Err(syn::Error::new(
            sig.span,
            "associated functions are not currently supported",
        ));
    } else {
        gen_ffi_function(&sig, arguments)?
    };

    let metadata_items = sig.metadata_items()?;
    Ok(quote! {
        #scaffolding_func
        #metadata_items
    })
}

// Pieces of code for the scaffolding function
struct ScaffoldingBits {
    /// Parameters for the scaffolding function
    params: Vec<TokenStream>,
    /// Statements to execute before `rust_fn_call`
    pre_fn_call: TokenStream,
    /// Tokenstream for the call to the actual Rust function
    rust_fn_call: TokenStream,
}

impl ScaffoldingBits {
    fn new_for_function(sig: &FnSignature) -> Self {
        let ident = &sig.ident;
        let params: Vec<_> = sig.args.iter().map(NamedArg::scaffolding_param).collect();
        let param_lifts = sig.lift_exprs();

        Self {
            params,
            pre_fn_call: quote! {},
            rust_fn_call: quote! { #ident(#(#param_lifts,)*) },
        }
    }

    fn new_for_method(sig: &FnSignature, self_ident: &Ident, is_trait: bool) -> Self {
        let ident = &sig.ident;
        let ffi_converter = if is_trait {
            quote! {
                <::std::sync::Arc<dyn #self_ident> as ::uniffi::FfiConverter<crate::UniFfiTag>>
            }
        } else {
            quote! {
                <::std::sync::Arc<#self_ident> as ::uniffi::FfiConverter<crate::UniFfiTag>>
            }
        };
        let params: Vec<_> = iter::once(quote! { uniffi_self_lowered: #ffi_converter::FfiType })
            .chain(sig.scaffolding_params())
            .collect();
        let param_lifts = sig.lift_exprs();

        Self {
            params,
            pre_fn_call: quote! {
                let uniffi_self = #ffi_converter::try_lift(uniffi_self_lowered).unwrap_or_else(|err| {
                    ::std::panic!("Failed to convert arg 'self': {}", err)
                });
            },
            rust_fn_call: quote! { uniffi_self.#ident(#(#param_lifts,)*) },
        }
    }

    fn new_for_constructor(sig: &FnSignature, self_ident: &Ident) -> Self {
        let ident = &sig.ident;
        let params: Vec<_> = sig.args.iter().map(NamedArg::scaffolding_param).collect();
        let param_lifts = sig.lift_exprs();

        Self {
            params,
            pre_fn_call: quote! {},
            rust_fn_call: quote! { #self_ident::#ident(#(#param_lifts,)*) },
        }
    }
}

/// Generate a scaffolding function
///
/// `pre_fn_call` is the statements that we should execute before the rust call
/// `rust_fn` is the Rust function to call.
fn gen_ffi_function(
    sig: &FnSignature,
    arguments: &ExportAttributeArguments,
) -> syn::Result<TokenStream> {
    let ScaffoldingBits {
        params,
        pre_fn_call,
        rust_fn_call,
    } = match &sig.kind {
        FnKind::Function => ScaffoldingBits::new_for_function(sig),
        FnKind::Method { self_ident } => ScaffoldingBits::new_for_method(sig, self_ident, false),
        FnKind::TraitMethod { self_ident, .. } => {
            ScaffoldingBits::new_for_method(sig, self_ident, true)
        }
        FnKind::Constructor { self_ident } => ScaffoldingBits::new_for_constructor(sig, self_ident),
    };

    let ffi_ident = sig.scaffolding_fn_ident()?;
    let name = &sig.name;
    let return_ty = &sig.return_ty;

    Ok(if !sig.is_async {
        if let Some(async_runtime) = &arguments.async_runtime {
            return Err(syn::Error::new_spanned(
                async_runtime,
                "this attribute is only allowed on async functions",
            ));
        }

        quote! {
            #[doc(hidden)]
            #[no_mangle]
            pub extern "C" fn #ffi_ident(
                #(#params,)*
                call_status: &mut ::uniffi::RustCallStatus,
            ) -> <#return_ty as ::uniffi::FfiConverter<crate::UniFfiTag>>::ReturnType {
                ::uniffi::deps::log::debug!(#name);
                ::uniffi::rust_call(call_status, || {
                    #pre_fn_call
                    <#return_ty as ::uniffi::FfiConverter<crate::UniFfiTag>>::lower_return(#rust_fn_call)
                })
            }
        }
    } else {
        let mut future_expr = rust_fn_call;
        if matches!(arguments.async_runtime, Some(AsyncRuntime::Tokio(_))) {
            future_expr = quote! { ::uniffi::deps::async_compat::Compat::new(#future_expr) }
        }

        quote! {
            #[doc(hidden)]
            #[no_mangle]
            pub extern "C" fn #ffi_ident(
                #(#params,)*
                uniffi_executor_handle: ::uniffi::ForeignExecutorHandle,
                uniffi_callback: <#return_ty as ::uniffi::FfiConverter<crate::UniFfiTag>>::FutureCallback,
                uniffi_callback_data: *const (),
                uniffi_call_status: &mut ::uniffi::RustCallStatus,
            ) {
                ::uniffi::deps::log::debug!(#name);
                ::uniffi::rust_call(uniffi_call_status, || {
                    #pre_fn_call;
                    let uniffi_rust_future = ::uniffi::RustFuture::<_, #return_ty, crate::UniFfiTag>::new(
                        #future_expr,
                        uniffi_executor_handle,
                        uniffi_callback,
                        uniffi_callback_data
                    );
                    uniffi_rust_future.wake();
                    Ok(())
                });
            }
        }
    })
}
