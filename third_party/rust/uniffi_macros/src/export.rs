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

use self::{
    attributes::ExportAttributeArguments,
    item::{ExportItem, ImplItem},
    scaffolding::{gen_constructor_scaffolding, gen_fn_scaffolding, gen_method_scaffolding},
};
use crate::{object::interface_meta_static_var, util::ident_to_string};
use uniffi_meta::free_fn_symbol_name;

// TODO(jplatte): Ensure no generics, …
// TODO(jplatte): Aggregate errors instead of short-circuiting, wherever possible

pub(crate) fn expand_export(
    mut item: Item,
    args: ExportAttributeArguments,
    mod_path: String,
) -> syn::Result<TokenStream> {
    // If the input is an `impl` block, rewrite any uses of the `Self` type
    // alias to the actual type, so we don't have to special-case it in the
    // metadata collection or scaffolding code generation (which generates
    // new functions outside of the `impl`).
    rewrite_self_type(&mut item);

    let metadata = ExportItem::new(item, &args)?;

    match metadata {
        ExportItem::Function { sig } => gen_fn_scaffolding(sig, &args),
        ExportItem::Impl { items, self_ident } => {
            let item_tokens: TokenStream = items
                .into_iter()
                .map(|item| match item? {
                    ImplItem::Constructor(sig) => gen_constructor_scaffolding(sig, &args),
                    ImplItem::Method(sig) => gen_method_scaffolding(sig, &args),
                })
                .collect::<syn::Result<_>>()?;
            Ok(quote_spanned! { self_ident.span() => #item_tokens })
        }
        ExportItem::Trait {
            items,
            self_ident,
            callback_interface: false,
        } => {
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
                .map(|item| match item? {
                    ImplItem::Method(sig) => gen_method_scaffolding(sig, &args),
                    _ => unreachable!("traits have no constructors"),
                })
                .collect::<syn::Result<_>>()?;

            let meta_static_var = interface_meta_static_var(&self_ident, true, &mod_path)
                .unwrap_or_else(syn::Error::into_compile_error);
            let macro_tokens = quote! {
                ::uniffi::ffi_converter_trait_decl!(dyn #self_ident, stringify!(#self_ident), crate::UniFfiTag);
            };

            Ok(quote_spanned! { self_ident.span() =>
                #meta_static_var
                #free_tokens
                #macro_tokens
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

            let items = items.into_iter().collect::<syn::Result<Vec<_>>>();
            let trait_impl_and_metadata_tokens = match items {
                Ok(items) => {
                    let trait_impl = callback_interface::trait_impl(
                        &trait_impl_ident,
                        &self_ident,
                        &internals_ident,
                        &items,
                    )
                    .unwrap_or_else(|e| e.into_compile_error());
                    let metadata_items =
                        callback_interface::metadata_items(&self_ident, &items, &mod_path)
                            .unwrap_or_else(|e| vec![e.into_compile_error()]);

                    quote! {
                        #trait_impl

                        #(#metadata_items)*
                    }
                }
                Err(e) => e.into_compile_error(),
            };

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

                #trait_impl_and_metadata_tokens
            })
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
