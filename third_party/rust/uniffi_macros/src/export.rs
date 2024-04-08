/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, Span, TokenStream};
use quote::{quote, quote_spanned};
use syn::{visit_mut::VisitMut, Item, Type};

mod attributes;
mod callback_interface;
mod item;
mod scaffolding;
mod utrait;

use self::{
    item::{ExportItem, ImplItem},
    scaffolding::{
        gen_constructor_scaffolding, gen_ffi_function, gen_fn_scaffolding, gen_method_scaffolding,
    },
};
use crate::{
    object::interface_meta_static_var,
    util::{ident_to_string, mod_path, tagged_impl_header},
};
pub use attributes::ExportAttributeArguments;
pub use callback_interface::ffi_converter_callback_interface_impl;
use uniffi_meta::free_fn_symbol_name;

// TODO(jplatte): Ensure no generics, …
// TODO(jplatte): Aggregate errors instead of short-circuiting, wherever possible

pub(crate) fn expand_export(
    mut item: Item,
    args: ExportAttributeArguments,
    udl_mode: bool,
) -> syn::Result<TokenStream> {
    let mod_path = mod_path()?;
    // If the input is an `impl` block, rewrite any uses of the `Self` type
    // alias to the actual type, so we don't have to special-case it in the
    // metadata collection or scaffolding code generation (which generates
    // new functions outside of the `impl`).
    rewrite_self_type(&mut item);

    let metadata = ExportItem::new(item, &args)?;

    match metadata {
        ExportItem::Function { sig } => gen_fn_scaffolding(sig, &args, udl_mode),
        ExportItem::Impl { items, self_ident } => {
            if let Some(rt) = &args.async_runtime {
                if items
                    .iter()
                    .all(|item| !matches!(item, ImplItem::Method(sig) if sig.is_async))
                {
                    return Err(syn::Error::new_spanned(
                        rt,
                        "no async methods in this impl block",
                    ));
                }
            }

            let item_tokens: TokenStream = items
                .into_iter()
                .map(|item| match item {
                    ImplItem::Constructor(sig) => gen_constructor_scaffolding(sig, &args, udl_mode),
                    ImplItem::Method(sig) => gen_method_scaffolding(sig, &args, udl_mode),
                })
                .collect::<syn::Result<_>>()?;
            Ok(quote_spanned! { self_ident.span() => #item_tokens })
        }
        ExportItem::Trait {
            items,
            self_ident,
            callback_interface: false,
        } => {
            if let Some(rt) = args.async_runtime {
                return Err(syn::Error::new_spanned(rt, "not supported for traits"));
            }

            let name = ident_to_string(&self_ident);
            let free_fn_ident =
                Ident::new(&free_fn_symbol_name(&mod_path, &name), Span::call_site());

            let free_tokens = quote! {
                #[doc(hidden)]
                #[no_mangle]
                pub extern "C" fn #free_fn_ident(
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
                    ImplItem::Method(sig) => {
                        if sig.is_async {
                            return Err(syn::Error::new(
                                sig.span,
                                "async trait methods are not supported",
                            ));
                        }
                        gen_method_scaffolding(sig, &args, udl_mode)
                    }
                    _ => unreachable!("traits have no constructors"),
                })
                .collect::<syn::Result<_>>()?;

            let meta_static_var = (!udl_mode).then(|| {
                interface_meta_static_var(&self_ident, true, &mod_path)
                    .unwrap_or_else(syn::Error::into_compile_error)
            });
            let ffi_converter_tokens = ffi_converter_trait_impl(&self_ident, false);

            Ok(quote_spanned! { self_ident.span() =>
                #meta_static_var
                #free_tokens
                #ffi_converter_tokens
                #impl_tokens
            })
        }
        ExportItem::Trait {
            items,
            self_ident,
            callback_interface: true,
        } => {
            let trait_name = ident_to_string(&self_ident);
            let trait_impl_ident = Ident::new(
                &format!("UniFFICallbackHandler{trait_name}"),
                Span::call_site(),
            );
            let internals_ident = Ident::new(
                &format!(
                    "UNIFFI_FOREIGN_CALLBACK_INTERNALS_{}",
                    trait_name.to_ascii_uppercase()
                ),
                Span::call_site(),
            );

            let trait_impl = callback_interface::trait_impl(
                &trait_impl_ident,
                &self_ident,
                &internals_ident,
                &items,
            )
            .unwrap_or_else(|e| e.into_compile_error());
            let metadata_items = callback_interface::metadata_items(&self_ident, &items, &mod_path)
                .unwrap_or_else(|e| vec![e.into_compile_error()]);

            let init_ident = Ident::new(
                &uniffi_meta::init_callback_fn_symbol_name(&mod_path, &trait_name),
                Span::call_site(),
            );

            Ok(quote! {
                #[doc(hidden)]
                static #internals_ident: ::uniffi::ForeignCallbackInternals = ::uniffi::ForeignCallbackInternals::new();

                #[doc(hidden)]
                #[no_mangle]
                pub extern "C" fn #init_ident(callback: ::uniffi::ForeignCallback, _: &mut ::uniffi::RustCallStatus) {
                    #internals_ident.set_callback(callback);
                }

                #trait_impl

                #(#metadata_items)*
            })
        }
        ExportItem::Struct {
            self_ident,
            uniffi_traits,
        } => {
            assert!(!udl_mode);
            utrait::expand_uniffi_trait_export(self_ident, uniffi_traits)
        }
    }
}

pub(crate) fn ffi_converter_trait_impl(trait_ident: &Ident, udl_mode: bool) -> TokenStream {
    let impl_spec = tagged_impl_header("FfiConverterArc", &quote! { dyn #trait_ident }, udl_mode);
    let lift_ref_impl_spec = tagged_impl_header("LiftRef", &quote! { dyn #trait_ident }, udl_mode);
    let name = ident_to_string(trait_ident);
    let mod_path = match mod_path() {
        Ok(p) => p,
        Err(e) => return e.into_compile_error(),
    };

    quote! {
        // All traits must be `Sync + Send`. The generated scaffolding will fail to compile
        // if they are not, but unfortunately it fails with an unactionably obscure error message.
        // By asserting the requirement explicitly, we help Rust produce a more scrutable error message
        // and thus help the user debug why the requirement isn't being met.
        uniffi::deps::static_assertions::assert_impl_all!(dyn #trait_ident: Sync, Send);

        unsafe #impl_spec {
            type FfiType = *const ::std::os::raw::c_void;

            fn lower(obj: ::std::sync::Arc<Self>) -> Self::FfiType {
                ::std::boxed::Box::into_raw(::std::boxed::Box::new(obj)) as *const ::std::os::raw::c_void
            }

            fn try_lift(v: Self::FfiType) -> ::uniffi::Result<::std::sync::Arc<Self>> {
                let foreign_arc = ::std::boxed::Box::leak(unsafe { Box::from_raw(v as *mut ::std::sync::Arc<Self>) });
                // Take a clone for our own use.
                Ok(::std::sync::Arc::clone(foreign_arc))
            }

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

            const TYPE_ID_META: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::TYPE_INTERFACE)
                .concat_str(#mod_path)
                .concat_str(#name)
                .concat_bool(true);
        }

        unsafe #lift_ref_impl_spec {
            type LiftType = ::std::sync::Arc<dyn #trait_ident>;
        }
    }
}

/// Rewrite Self type alias usage in an impl block to the type itself.
///
/// For example,
///
/// ```ignore
/// impl some::module::Foo {
///     fn method(
///         self: Arc<Self>,
///         arg: Option<Bar<(), Self>>,
///     ) -> Result<Self, Error> {
///         todo!()
///     }
/// }
/// ```
///
/// will be rewritten to
///
///  ```ignore
/// impl some::module::Foo {
///     fn method(
///         self: Arc<some::module::Foo>,
///         arg: Option<Bar<(), some::module::Foo>>,
///     ) -> Result<some::module::Foo, Error> {
///         todo!()
///     }
/// }
/// ```
pub fn rewrite_self_type(item: &mut Item) {
    let item = match item {
        Item::Impl(i) => i,
        _ => return,
    };

    struct RewriteSelfVisitor<'a>(&'a Type);

    impl<'a> VisitMut for RewriteSelfVisitor<'a> {
        fn visit_type_mut(&mut self, i: &mut Type) {
            match i {
                Type::Path(p) if p.qself.is_none() && p.path.is_ident("Self") => {
                    *i = self.0.clone();
                }
                _ => syn::visit_mut::visit_type_mut(self, i),
            }
        }
    }

    let mut visitor = RewriteSelfVisitor(&item.self_ty);
    for item in &mut item.items {
        visitor.visit_impl_item_mut(item);
    }
}
