/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, TokenStream};
use quote::quote;
use std::iter;

use super::attributes::AsyncRuntime;
use crate::fnsig::{FnKind, FnSignature};

pub(super) fn gen_fn_scaffolding(
    sig: FnSignature,
    ar: &Option<AsyncRuntime>,
    udl_mode: bool,
) -> syn::Result<TokenStream> {
    if sig.receiver.is_some() {
        return Err(syn::Error::new(
            sig.span,
            "Unexpected self param (Note: uniffi::export must be used on the impl block, not its containing fn's)"
        ));
    }
    if !sig.is_async {
        if let Some(async_runtime) = ar {
            return Err(syn::Error::new_spanned(
                async_runtime,
                "this attribute is only allowed on async functions",
            ));
        }
    }
    let metadata_items = (!udl_mode).then(|| {
        sig.metadata_items()
            .unwrap_or_else(syn::Error::into_compile_error)
    });
    let scaffolding_func = gen_ffi_function(&sig, ar, udl_mode)?;
    Ok(quote! {
        #scaffolding_func
        #metadata_items
    })
}

pub(super) fn gen_constructor_scaffolding(
    sig: FnSignature,
    ar: &Option<AsyncRuntime>,
    udl_mode: bool,
) -> syn::Result<TokenStream> {
    if sig.receiver.is_some() {
        return Err(syn::Error::new(
            sig.span,
            "constructors must not have a self parameter",
        ));
    }
    let metadata_items = (!udl_mode).then(|| {
        sig.metadata_items()
            .unwrap_or_else(syn::Error::into_compile_error)
    });
    let scaffolding_func = gen_ffi_function(&sig, ar, udl_mode)?;
    Ok(quote! {
        #scaffolding_func
        #metadata_items
    })
}

pub(super) fn gen_method_scaffolding(
    sig: FnSignature,
    ar: &Option<AsyncRuntime>,
    udl_mode: bool,
) -> syn::Result<TokenStream> {
    let scaffolding_func = if sig.receiver.is_none() {
        return Err(syn::Error::new(
            sig.span,
            "associated functions are not currently supported",
        ));
    } else {
        gen_ffi_function(&sig, ar, udl_mode)?
    };

    let metadata_items = (!udl_mode).then(|| {
        sig.metadata_items()
            .unwrap_or_else(syn::Error::into_compile_error)
    });
    Ok(quote! {
        #scaffolding_func
        #metadata_items
    })
}

// Pieces of code for the scaffolding function
struct ScaffoldingBits {
    /// Parameter names for the scaffolding function
    param_names: Vec<TokenStream>,
    /// Parameter types for the scaffolding function
    param_types: Vec<TokenStream>,
    /// Lift closure.  See `FnSignature::lift_closure` for an explanation of this.
    lift_closure: TokenStream,
    /// Expression to call the Rust function after a successful lift.
    rust_fn_call: TokenStream,
    /// Convert the result of `rust_fn_call`, stored in a variable named `uniffi_result` into its final value.
    /// This is used to do things like error conversion / Arc wrapping
    convert_result: TokenStream,
}

impl ScaffoldingBits {
    fn new_for_function(sig: &FnSignature, udl_mode: bool) -> Self {
        let ident = &sig.ident;
        let call_params = sig.rust_call_params(false);
        let rust_fn_call = quote! { #ident(#call_params) };
        // UDL mode adds an extra conversion (#1749)
        let convert_result = if udl_mode && sig.looks_like_result {
            quote! { uniffi_result.map_err(::std::convert::Into::into) }
        } else {
            quote! { uniffi_result }
        };

        Self {
            param_names: sig.scaffolding_param_names().collect(),
            param_types: sig.scaffolding_param_types().collect(),
            lift_closure: sig.lift_closure(None),
            rust_fn_call,
            convert_result,
        }
    }

    fn new_for_method(
        sig: &FnSignature,
        self_ident: &Ident,
        is_trait: bool,
        udl_mode: bool,
    ) -> Self {
        let ident = &sig.ident;
        let lift_impl = if is_trait {
            quote! {
                <::std::sync::Arc<dyn #self_ident> as ::uniffi::Lift<crate::UniFfiTag>>
            }
        } else {
            quote! {
                <::std::sync::Arc<#self_ident> as ::uniffi::Lift<crate::UniFfiTag>>
            }
        };
        let try_lift_self = if is_trait {
            // For trait interfaces we need to special case this.  Trait interfaces normally lift
            // foreign trait impl pointers.  However, for a method call, we want to lift a Rust
            // pointer.
            quote! {
                {
                    let boxed_foreign_arc = unsafe { Box::from_raw(uniffi_self_lowered as *mut ::std::sync::Arc<dyn #self_ident>) };
                    // Take a clone for our own use.
                    Ok(*boxed_foreign_arc)
                }
            }
        } else {
            quote! { #lift_impl::try_lift(uniffi_self_lowered) }
        };

        let lift_closure = sig.lift_closure(Some(quote! {
            match #try_lift_self {
                Ok(v) => v,
                Err(e) => return Err(("self", e))
            }
        }));
        let call_params = sig.rust_call_params(true);
        let rust_fn_call = quote! { uniffi_args.0.#ident(#call_params) };
        // UDL mode adds an extra conversion (#1749)
        let convert_result = if udl_mode && sig.looks_like_result {
            quote! { uniffi_result .map_err(::std::convert::Into::into) }
        } else {
            quote! { uniffi_result }
        };

        Self {
            param_names: iter::once(quote! { uniffi_self_lowered })
                .chain(sig.scaffolding_param_names())
                .collect(),
            param_types: iter::once(quote! { #lift_impl::FfiType })
                .chain(sig.scaffolding_param_types())
                .collect(),
            lift_closure,
            rust_fn_call,
            convert_result,
        }
    }

    fn new_for_constructor(sig: &FnSignature, self_ident: &Ident, udl_mode: bool) -> Self {
        let ident = &sig.ident;
        let call_params = sig.rust_call_params(false);
        let rust_fn_call = quote! { #self_ident::#ident(#call_params) };
        // UDL mode adds extra conversions (#1749)
        let convert_result = match (udl_mode, sig.looks_like_result) {
            // For UDL
            (true, false) => quote! { ::std::sync::Arc::new(uniffi_result) },
            (true, true) => {
                quote! { uniffi_result.map(::std::sync::Arc::new).map_err(::std::convert::Into::into) }
            }
            (false, _) => quote! { uniffi_result },
        };

        Self {
            param_names: sig.scaffolding_param_names().collect(),
            param_types: sig.scaffolding_param_types().collect(),
            lift_closure: sig.lift_closure(None),
            rust_fn_call,
            convert_result,
        }
    }
}

/// Generate a scaffolding function
///
/// `pre_fn_call` is the statements that we should execute before the rust call
/// `rust_fn` is the Rust function to call.
pub(super) fn gen_ffi_function(
    sig: &FnSignature,
    ar: &Option<AsyncRuntime>,
    udl_mode: bool,
) -> syn::Result<TokenStream> {
    let ScaffoldingBits {
        param_names,
        param_types,
        lift_closure,
        rust_fn_call,
        convert_result,
    } = match &sig.kind {
        FnKind::Function => ScaffoldingBits::new_for_function(sig, udl_mode),
        FnKind::Method { self_ident } => {
            ScaffoldingBits::new_for_method(sig, self_ident, false, udl_mode)
        }
        FnKind::TraitMethod { self_ident, .. } => {
            ScaffoldingBits::new_for_method(sig, self_ident, true, udl_mode)
        }
        FnKind::Constructor { self_ident } => {
            ScaffoldingBits::new_for_constructor(sig, self_ident, udl_mode)
        }
    };
    // Scaffolding functions are logically `pub`, but we don't use that in UDL mode since UDL has
    // historically not required types to be `pub`
    let vis = match udl_mode {
        false => quote! { pub },
        true => quote! {},
    };

    let ffi_ident = sig.scaffolding_fn_ident()?;
    let name = &sig.name;
    let return_ty = &sig.return_ty;
    let return_impl = &sig.lower_return_impl();

    Ok(if !sig.is_async {
        let scaffolding_fn_ffi_buffer_version = ffi_buffer_scaffolding_fn(
            &ffi_ident,
            &quote! { <#return_ty as ::uniffi::LowerReturn<crate::UniFfiTag>>::ReturnType },
            &param_types,
            true,
        );
        quote! {
            #[doc(hidden)]
            #[no_mangle]
            #vis extern "C" fn #ffi_ident(
                #(#param_names: #param_types,)*
                call_status: &mut ::uniffi::RustCallStatus,
            ) -> #return_impl::ReturnType {
                ::uniffi::deps::log::debug!(#name);
                let uniffi_lift_args = #lift_closure;
                ::uniffi::rust_call(call_status, || {
                    #return_impl::lower_return(
                        match uniffi_lift_args() {
                            Ok(uniffi_args) => {
                                let uniffi_result = #rust_fn_call;
                                #convert_result
                            }
                            Err((arg_name, anyhow_error)) => {
                                #return_impl::handle_failed_lift(arg_name, anyhow_error)
                            },
                        }
                    )
                })
            }

            #scaffolding_fn_ffi_buffer_version
        }
    } else {
        let mut future_expr = rust_fn_call;
        if matches!(ar, Some(AsyncRuntime::Tokio(_))) {
            future_expr = quote! { ::uniffi::deps::async_compat::Compat::new(#future_expr) }
        }
        let scaffolding_fn_ffi_buffer_version =
            ffi_buffer_scaffolding_fn(&ffi_ident, &quote! { ::uniffi::Handle}, &param_types, false);

        quote! {
            #[doc(hidden)]
            #[no_mangle]
            pub extern "C" fn #ffi_ident(#(#param_names: #param_types,)*) -> ::uniffi::Handle {
                ::uniffi::deps::log::debug!(#name);
                let uniffi_lift_args = #lift_closure;
                match uniffi_lift_args() {
                    Ok(uniffi_args) => {
                        ::uniffi::rust_future_new::<_, #return_ty, _>(
                            async move {
                                let uniffi_result = #future_expr.await;
                                #convert_result
                            },
                            crate::UniFfiTag
                        )
                    },
                    Err((arg_name, anyhow_error)) => {
                        ::uniffi::rust_future_new::<_, #return_ty, _>(
                            async move {
                                #return_impl::handle_failed_lift(arg_name, anyhow_error)
                            },
                            crate::UniFfiTag,
                        )
                    },
                }
            }

            #scaffolding_fn_ffi_buffer_version
        }
    })
}

#[cfg(feature = "scaffolding-ffi-buffer-fns")]
fn ffi_buffer_scaffolding_fn(
    fn_ident: &Ident,
    return_type: &TokenStream,
    param_types: &[TokenStream],
    has_rust_call_status: bool,
) -> TokenStream {
    let fn_name = fn_ident.to_string();
    let ffi_buffer_fn_name = uniffi_meta::ffi_buffer_symbol_name(&fn_name);
    let ident = Ident::new(&ffi_buffer_fn_name, proc_macro2::Span::call_site());
    let type_list: Vec<_> = param_types.iter().map(|ty| quote! { #ty }).collect();
    if has_rust_call_status {
        quote! {
            #[doc(hidden)]
            #[no_mangle]
            pub unsafe extern "C" fn #ident(
                arg_ptr: *mut ::uniffi::FfiBufferElement,
                return_ptr: *mut ::uniffi::FfiBufferElement,
            ) {
                let mut arg_buf = unsafe { ::std::slice::from_raw_parts(arg_ptr, ::uniffi::ffi_buffer_size!(#(#type_list),*)) };
                let mut return_buf = unsafe { ::std::slice::from_raw_parts_mut(return_ptr, ::uniffi::ffi_buffer_size!(#return_type, ::uniffi::RustCallStatus)) };
                let mut out_status = ::uniffi::RustCallStatus::default();

                let return_value = #fn_ident(
                    #(
                        <#type_list as ::uniffi::FfiSerialize>::read(&mut arg_buf),
                    )*
                    &mut out_status,
                );
                <#return_type as ::uniffi::FfiSerialize>::write(&mut return_buf, return_value);
                <::uniffi::RustCallStatus as ::uniffi::FfiSerialize>::write(&mut return_buf, out_status);
            }
        }
    } else {
        quote! {
            #[doc(hidden)]
            #[no_mangle]
            pub unsafe extern "C" fn #ident(
                arg_ptr: *mut ::uniffi::FfiBufferElement,
                return_ptr: *mut ::uniffi::FfiBufferElement,
            ) {
                let mut arg_buf = unsafe { ::std::slice::from_raw_parts(arg_ptr, ::uniffi::ffi_buffer_size!(#(#type_list),*)) };
                let mut return_buf = unsafe { ::std::slice::from_raw_parts_mut(return_ptr, ::uniffi::ffi_buffer_size!(#return_type)) };

                let return_value = #fn_ident(#(
                    <#type_list as ::uniffi::FfiSerialize>::read(&mut arg_buf),
                )*);
                <#return_type as ::uniffi::FfiSerialize>::put(&mut return_buf, return_value);
            }
        }
    }
}

#[cfg(not(feature = "scaffolding-ffi-buffer-fns"))]
fn ffi_buffer_scaffolding_fn(
    _fn_ident: &Ident,
    _return_type: &TokenStream,
    _param_types: &[TokenStream],
    _add_rust_call_status: bool,
) -> TokenStream {
    quote! {}
}
