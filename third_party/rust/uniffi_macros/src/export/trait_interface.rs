/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, Span, TokenStream};
use quote::{quote, quote_spanned};

use uniffi_meta::ObjectImpl;

use crate::{
    export::{
        attributes::ExportTraitArgs, callback_interface, gen_method_scaffolding, item::ImplItem,
    },
    object::interface_meta_static_var,
    util::{ident_to_string, tagged_impl_header},
};

pub(super) fn gen_trait_scaffolding(
    mod_path: &str,
    args: ExportTraitArgs,
    self_ident: Ident,
    items: Vec<ImplItem>,
    udl_mode: bool,
    with_foreign: bool,
    docstring: String,
) -> syn::Result<TokenStream> {
    if let Some(rt) = args.async_runtime {
        return Err(syn::Error::new_spanned(rt, "not supported for traits"));
    }
    let trait_name = ident_to_string(&self_ident);
    let trait_impl = with_foreign.then(|| {
        callback_interface::trait_impl(mod_path, &self_ident, &items)
            .unwrap_or_else(|e| e.into_compile_error())
    });

    let clone_fn_ident = Ident::new(
        &uniffi_meta::clone_fn_symbol_name(mod_path, &trait_name),
        Span::call_site(),
    );
    let free_fn_ident = Ident::new(
        &uniffi_meta::free_fn_symbol_name(mod_path, &trait_name),
        Span::call_site(),
    );

    let helper_fn_tokens = quote! {
        #[doc(hidden)]
        #[no_mangle]
        /// Clone a pointer to this object type
        ///
        /// Safety: Only pass pointers returned by a UniFFI call.  Do not pass pointers that were
        /// passed to the free function.
        pub unsafe extern "C" fn #clone_fn_ident(
            ptr: *const ::std::ffi::c_void,
            call_status: &mut ::uniffi::RustCallStatus
        ) -> *const ::std::ffi::c_void {
            uniffi::rust_call(call_status, || {
                let ptr = ptr as *mut std::sync::Arc<dyn #self_ident>;
                let arc = unsafe { ::std::sync::Arc::clone(&*ptr) };
                Ok(::std::boxed::Box::into_raw(::std::boxed::Box::new(arc)) as  *const ::std::ffi::c_void)
            })
        }

        #[doc(hidden)]
        #[no_mangle]
        /// Free a pointer to this object type
        ///
        /// Safety: Only pass pointers returned by a UniFFI call.  Do not pass pointers that were
        /// passed to the free function.
        ///
        /// Note: clippy doesn't complain about this being unsafe, but it definitely is since it
        /// calls `Box::from_raw`.
        pub unsafe extern "C" fn #free_fn_ident(
            ptr: *const ::std::ffi::c_void,
            call_status: &mut ::uniffi::RustCallStatus
        ) {
            uniffi::rust_call(call_status, || {
                assert!(!ptr.is_null());
                drop(unsafe { ::std::boxed::Box::from_raw(ptr as *mut std::sync::Arc<dyn #self_ident>) });
                Ok(())
            });
        }
    };

    let impl_tokens: TokenStream = items
        .into_iter()
        .map(|item| match item {
            ImplItem::Method(sig) => gen_method_scaffolding(sig, &None, udl_mode),
            _ => unreachable!("traits have no constructors"),
        })
        .collect::<syn::Result<_>>()?;

    let meta_static_var = (!udl_mode).then(|| {
        let imp = if with_foreign {
            ObjectImpl::CallbackTrait
        } else {
            ObjectImpl::Trait
        };
        interface_meta_static_var(&self_ident, imp, mod_path, docstring)
            .unwrap_or_else(syn::Error::into_compile_error)
    });
    let ffi_converter_tokens = ffi_converter(mod_path, &self_ident, udl_mode, with_foreign);

    Ok(quote_spanned! { self_ident.span() =>
        #meta_static_var
        #helper_fn_tokens
        #trait_impl
        #impl_tokens
        #ffi_converter_tokens
    })
}

pub(crate) fn ffi_converter(
    mod_path: &str,
    trait_ident: &Ident,
    udl_mode: bool,
    with_foreign: bool,
) -> TokenStream {
    let impl_spec = tagged_impl_header("FfiConverterArc", &quote! { dyn #trait_ident }, udl_mode);
    let lift_ref_impl_spec = tagged_impl_header("LiftRef", &quote! { dyn #trait_ident }, udl_mode);
    let trait_name = ident_to_string(trait_ident);
    let try_lift = if with_foreign {
        let trait_impl_ident = callback_interface::trait_impl_ident(&trait_name);
        quote! {
            fn try_lift(v: Self::FfiType) -> ::uniffi::deps::anyhow::Result<::std::sync::Arc<Self>> {
                Ok(::std::sync::Arc::new(<#trait_impl_ident>::new(v as u64)))
            }
        }
    } else {
        quote! {
            fn try_lift(v: Self::FfiType) -> ::uniffi::deps::anyhow::Result<::std::sync::Arc<Self>> {
                unsafe {
                    Ok(*::std::boxed::Box::from_raw(v as *mut ::std::sync::Arc<Self>))
                }
            }
        }
    };
    let metadata_code = if with_foreign {
        quote! { ::uniffi::metadata::codes::TYPE_CALLBACK_TRAIT_INTERFACE }
    } else {
        quote! { ::uniffi::metadata::codes::TYPE_TRAIT_INTERFACE }
    };

    quote! {
        // All traits must be `Sync + Send`. The generated scaffolding will fail to compile
        // if they are not, but unfortunately it fails with an unactionably obscure error message.
        // By asserting the requirement explicitly, we help Rust produce a more scrutable error message
        // and thus help the user debug why the requirement isn't being met.
        uniffi::deps::static_assertions::assert_impl_all!(dyn #trait_ident: ::core::marker::Sync, ::core::marker::Send);

        unsafe #impl_spec {
            type FfiType = *const ::std::os::raw::c_void;

            fn lower(obj: ::std::sync::Arc<Self>) -> Self::FfiType {
                ::std::boxed::Box::into_raw(::std::boxed::Box::new(obj)) as *const ::std::os::raw::c_void
            }

            #try_lift

            fn write(obj: ::std::sync::Arc<Self>, buf: &mut Vec<u8>) {
                ::uniffi::deps::static_assertions::const_assert!(::std::mem::size_of::<*const ::std::ffi::c_void>() <= 8);
                ::uniffi::deps::bytes::BufMut::put_u64(
                    buf,
                    <Self as ::uniffi::FfiConverterArc<crate::UniFfiTag>>::lower(obj) as u64,
                );
            }

            fn try_read(buf: &mut &[u8]) -> ::uniffi::Result<::std::sync::Arc<Self>> {
                ::uniffi::deps::static_assertions::const_assert!(::std::mem::size_of::<*const ::std::ffi::c_void>() <= 8);
                ::uniffi::check_remaining(buf, 8)?;
                <Self as ::uniffi::FfiConverterArc<crate::UniFfiTag>>::try_lift(
                    ::uniffi::deps::bytes::Buf::get_u64(buf) as Self::FfiType)
            }

            const TYPE_ID_META: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(#metadata_code)
                .concat_str(#mod_path)
                .concat_str(#trait_name);
        }

        unsafe #lift_ref_impl_spec {
            type LiftType = ::std::sync::Arc<dyn #trait_ident>;
        }
    }
}
