/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    export::ImplItem,
    fnsig::{FnKind, FnSignature, ReceiverArg},
    util::{
        create_metadata_items, derive_ffi_traits, ident_to_string, mod_path, tagged_impl_header,
    },
};
use proc_macro2::{Span, TokenStream};
use quote::{format_ident, quote};
use std::iter;
use syn::Ident;

/// Generate a trait impl that calls foreign callbacks
///
/// This generates:
///    * A `repr(C)` VTable struct where each field is the FFI function for the trait method.
///    * A FFI function for foreign code to set their VTable for the interface
///    * An implementation of the trait using that VTable
pub(super) fn trait_impl(
    mod_path: &str,
    trait_ident: &Ident,
    items: &[ImplItem],
) -> syn::Result<TokenStream> {
    let trait_name = ident_to_string(trait_ident);
    let trait_impl_ident = trait_impl_ident(&trait_name);
    let vtable_type = format_ident!("UniFfiTraitVtable{trait_name}");
    let vtable_cell = format_ident!("UNIFFI_TRAIT_CELL_{}", trait_name.to_uppercase());
    let init_ident = Ident::new(
        &uniffi_meta::init_callback_vtable_fn_symbol_name(mod_path, &trait_name),
        Span::call_site(),
    );
    let methods = items
        .iter()
        .map(|item| match item {
            ImplItem::Constructor(sig) => Err(syn::Error::new(
                sig.span,
                "Constructors not allowed in trait interfaces",
            )),
            ImplItem::Method(sig) => Ok(sig),
        })
        .collect::<syn::Result<Vec<_>>>()?;

    let vtable_fields = methods.iter()
        .map(|sig| {
            let ident = &sig.ident;
            let param_names = sig.scaffolding_param_names();
            let param_types = sig.scaffolding_param_types();
            let lift_return = sig.lift_return_impl();
            if !sig.is_async {
                quote! {
                    #ident: extern "C" fn(
                        uniffi_handle: u64,
                        #(#param_names: #param_types,)*
                        uniffi_out_return: &mut #lift_return::ReturnType,
                        uniffi_out_call_status: &mut ::uniffi::RustCallStatus,
                    ),
                }
            } else {
                quote! {
                    #ident: extern "C" fn(
                        uniffi_handle: u64,
                        #(#param_names: #param_types,)*
                        uniffi_future_callback: ::uniffi::ForeignFutureCallback<#lift_return::ReturnType>,
                        uniffi_callback_data: u64,
                        uniffi_out_return: &mut ::uniffi::ForeignFuture,
                    ),
                }
            }
        });

    let trait_impl_methods = methods
        .iter()
        .map(|sig| gen_method_impl(sig, &vtable_cell))
        .collect::<syn::Result<Vec<_>>>()?;
    let has_async_method = methods.iter().any(|m| m.is_async);
    let impl_attributes = has_async_method.then(|| quote! { #[::async_trait::async_trait] });

    Ok(quote! {
        struct #vtable_type {
            #(#vtable_fields)*
            uniffi_free: extern "C" fn(handle: u64),
        }

        static #vtable_cell: ::uniffi::UniffiForeignPointerCell::<#vtable_type> = ::uniffi::UniffiForeignPointerCell::<#vtable_type>::new();

        #[no_mangle]
        extern "C" fn #init_ident(vtable: ::std::ptr::NonNull<#vtable_type>) {
            #vtable_cell.set(vtable);
        }

        #[derive(Debug)]
        struct #trait_impl_ident {
            handle: u64,
        }

        impl #trait_impl_ident {
            fn new(handle: u64) -> Self {
                Self { handle }
            }
        }

        ::uniffi::deps::static_assertions::assert_impl_all!(#trait_impl_ident: ::core::marker::Send);

        #impl_attributes
        impl #trait_ident for #trait_impl_ident {
            #(#trait_impl_methods)*
        }

        impl ::std::ops::Drop for #trait_impl_ident {
            fn drop(&mut self) {
                let vtable = #vtable_cell.get();
                (vtable.uniffi_free)(self.handle);
            }
        }
    })
}

pub fn trait_impl_ident(trait_name: &str) -> Ident {
    Ident::new(
        &format!("UniFFICallbackHandler{trait_name}"),
        Span::call_site(),
    )
}

pub fn ffi_converter_callback_interface_impl(
    trait_ident: &Ident,
    trait_impl_ident: &Ident,
    udl_mode: bool,
) -> TokenStream {
    let trait_name = ident_to_string(trait_ident);
    let dyn_trait = quote! { dyn #trait_ident };
    let box_dyn_trait = quote! { ::std::boxed::Box<#dyn_trait> };
    let lift_impl_spec = tagged_impl_header("Lift", &box_dyn_trait, udl_mode);
    let derive_ffi_traits = derive_ffi_traits(&box_dyn_trait, udl_mode, &["LiftRef", "LiftReturn"]);
    let mod_path = match mod_path() {
        Ok(p) => p,
        Err(e) => return e.into_compile_error(),
    };

    quote! {
        #[doc(hidden)]
        #[automatically_derived]
        unsafe #lift_impl_spec {
            type FfiType = u64;

            fn try_lift(v: Self::FfiType) -> ::uniffi::deps::anyhow::Result<Self> {
                Ok(::std::boxed::Box::new(<#trait_impl_ident>::new(v)))
            }

            fn try_read(buf: &mut &[u8]) -> ::uniffi::deps::anyhow::Result<Self> {
                use uniffi::deps::bytes::Buf;
                ::uniffi::check_remaining(buf, 8)?;
                <Self as ::uniffi::Lift<crate::UniFfiTag>>::try_lift(buf.get_u64())
            }

            const TYPE_ID_META: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(
                ::uniffi::metadata::codes::TYPE_CALLBACK_INTERFACE,
            )
            .concat_str(#mod_path)
            .concat_str(#trait_name);
        }

        #derive_ffi_traits
    }
}

/// Generate a single method for [trait_impl].  This implements a trait method by invoking a
/// foreign-supplied callback.
fn gen_method_impl(sig: &FnSignature, vtable_cell: &Ident) -> syn::Result<TokenStream> {
    let FnSignature {
        ident,
        is_async,
        return_ty,
        kind,
        receiver,
        name,
        span,
        ..
    } = sig;

    if !matches!(kind, FnKind::TraitMethod { .. }) {
        return Err(syn::Error::new(
            *span,
            format!(
                "Internal UniFFI error: Unexpected function kind for callback interface {name}: {kind:?}",
            ),
        ));
    }

    let self_param = match receiver {
        Some(ReceiverArg::Ref) => quote! { &self },
        Some(ReceiverArg::Arc) => quote! { self: Arc<Self> },
        None => {
            return Err(syn::Error::new(
                *span,
                "callback interface methods must take &self as their first argument",
            ));
        }
    };

    let params = sig.params();
    let lower_exprs = sig.args.iter().map(|a| {
        let lower_impl = a.lower_impl();
        let ident = &a.ident;
        quote! { #lower_impl::lower(#ident) }
    });

    let lift_return = sig.lift_return_impl();

    if !is_async {
        Ok(quote! {
            fn #ident(#self_param, #(#params),*) -> #return_ty {
                let vtable = #vtable_cell.get();
                let mut uniffi_call_status = ::uniffi::RustCallStatus::new();
                let mut uniffi_return_value: #lift_return::ReturnType = ::uniffi::FfiDefault::ffi_default();
                (vtable.#ident)(self.handle, #(#lower_exprs,)* &mut uniffi_return_value, &mut uniffi_call_status);
                #lift_return::lift_foreign_return(uniffi_return_value, uniffi_call_status)
            }
        })
    } else {
        Ok(quote! {
            async fn #ident(#self_param, #(#params),*) -> #return_ty {
                let vtable = #vtable_cell.get();
                ::uniffi::foreign_async_call::<_, #return_ty, crate::UniFfiTag>(move |uniffi_future_callback, uniffi_future_callback_data| {
                    let mut uniffi_foreign_future: ::uniffi::ForeignFuture = ::uniffi::FfiDefault::ffi_default();
                    (vtable.#ident)(self.handle, #(#lower_exprs,)* uniffi_future_callback, uniffi_future_callback_data, &mut uniffi_foreign_future);
                    uniffi_foreign_future
                }).await
            }
        })
    }
}

pub(super) fn metadata_items(
    self_ident: &Ident,
    items: &[ImplItem],
    module_path: &str,
    docstring: String,
) -> syn::Result<Vec<TokenStream>> {
    let trait_name = ident_to_string(self_ident);
    let callback_interface_items = create_metadata_items(
        "callback_interface",
        &trait_name,
        quote! {
            ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::CALLBACK_INTERFACE)
                .concat_str(#module_path)
                .concat_str(#trait_name)
                .concat_long_str(#docstring)
        },
        None,
    );

    iter::once(Ok(callback_interface_items))
        .chain(items.iter().map(|item| match item {
            ImplItem::Method(sig) => sig.metadata_items_for_callback_interface(),
            _ => unreachable!("traits have no constructors"),
        }))
        .collect()
}
