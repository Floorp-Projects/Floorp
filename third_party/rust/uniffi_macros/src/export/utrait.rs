/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, TokenStream};
use quote::quote;
use syn::ext::IdentExt;

use super::gen_ffi_function;
use crate::export::ExportedImplFnArgs;
use crate::fnsig::FnSignature;
use crate::util::extract_docstring;
use uniffi_meta::UniffiTraitDiscriminants;

pub(crate) fn expand_uniffi_trait_export(
    self_ident: Ident,
    uniffi_traits: Vec<UniffiTraitDiscriminants>,
) -> syn::Result<TokenStream> {
    let udl_mode = false;
    let mut impl_items = Vec::new();
    let mut global_items = Vec::new();
    for trait_id in uniffi_traits {
        match trait_id {
            UniffiTraitDiscriminants::Debug => {
                let method = quote! {
                    fn uniffi_trait_debug(&self) -> String {
                        ::uniffi::deps::static_assertions::assert_impl_all!(#self_ident: ::std::fmt::Debug);
                        format!("{:?}", self)
                    }
                };
                let (ffi_func, method_meta) =
                    process_uniffi_trait_method(&method, &self_ident, udl_mode)?;
                // metadata for the trait - which includes metadata for the method.
                let discr = UniffiTraitDiscriminants::Debug as u8;
                let trait_meta = crate::util::create_metadata_items(
                    "uniffi_trait",
                    &format!("{}_Debug", self_ident.unraw()),
                    quote! {
                        ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::UNIFFI_TRAIT)
                        .concat_value(#discr)
                        .concat(#method_meta)
                    },
                    None,
                );
                impl_items.push(method);
                global_items.push(ffi_func);
                global_items.push(trait_meta);
            }
            UniffiTraitDiscriminants::Display => {
                let method = quote! {
                    fn uniffi_trait_display(&self) -> String {
                        ::uniffi::deps::static_assertions::assert_impl_all!(#self_ident: ::std::fmt::Display);
                        format!("{}", self)
                    }
                };
                let (ffi_func, method_meta) =
                    process_uniffi_trait_method(&method, &self_ident, udl_mode)?;
                // metadata for the trait - which includes metadata for the method.
                let discr = UniffiTraitDiscriminants::Display as u8;
                let trait_meta = crate::util::create_metadata_items(
                    "uniffi_trait",
                    &format!("{}_Display", self_ident.unraw()),
                    quote! {
                        ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::UNIFFI_TRAIT)
                        .concat_value(#discr)
                        .concat(#method_meta)
                    },
                    None,
                );
                impl_items.push(method);
                global_items.push(ffi_func);
                global_items.push(trait_meta);
            }
            UniffiTraitDiscriminants::Hash => {
                let method = quote! {
                    fn uniffi_trait_hash(&self) -> u64 {
                        use ::std::hash::{Hash, Hasher};
                        ::uniffi::deps::static_assertions::assert_impl_all!(#self_ident: Hash);
                        let mut s = ::std::collections::hash_map::DefaultHasher::new();
                        Hash::hash(self, &mut s);
                        s.finish()
                    }
                };
                let (ffi_func, method_meta) =
                    process_uniffi_trait_method(&method, &self_ident, udl_mode)?;
                // metadata for the trait - which includes metadata for the hash method.
                let discr = UniffiTraitDiscriminants::Hash as u8;
                let trait_meta = crate::util::create_metadata_items(
                    "uniffi_trait",
                    &format!("{}_Hash", self_ident.unraw()),
                    quote! {
                        ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::UNIFFI_TRAIT)
                        .concat_value(#discr)
                        .concat(#method_meta)
                    },
                    None,
                );
                impl_items.push(method);
                global_items.push(ffi_func);
                global_items.push(trait_meta);
            }
            UniffiTraitDiscriminants::Eq => {
                let method_eq = quote! {
                    fn uniffi_trait_eq_eq(&self, other: &#self_ident) -> bool {
                        use ::std::cmp::PartialEq;
                        uniffi::deps::static_assertions::assert_impl_all!(#self_ident: PartialEq); // This object has a trait method which requires `PartialEq` be implemented.
                        PartialEq::eq(self, other)
                    }
                };
                let method_ne = quote! {
                    fn uniffi_trait_eq_ne(&self, other: &#self_ident) -> bool {
                        use ::std::cmp::PartialEq;
                        uniffi::deps::static_assertions::assert_impl_all!(#self_ident: PartialEq); // This object has a trait method which requires `PartialEq` be implemented.
                        PartialEq::ne(self, other)
                    }
                };
                let (ffi_func_eq, method_meta_eq) =
                    process_uniffi_trait_method(&method_eq, &self_ident, udl_mode)?;
                let (ffi_func_ne, method_meta_ne) =
                    process_uniffi_trait_method(&method_ne, &self_ident, udl_mode)?;
                // metadata for the trait itself.
                let discr = UniffiTraitDiscriminants::Eq as u8;
                let trait_meta = crate::util::create_metadata_items(
                    "uniffi_trait",
                    &format!("{}_Eq", self_ident.unraw()),
                    quote! {
                        ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::UNIFFI_TRAIT)
                        .concat_value(#discr)
                        .concat(#method_meta_eq)
                        .concat(#method_meta_ne)
                    },
                    None,
                );
                impl_items.push(method_eq);
                impl_items.push(method_ne);
                global_items.push(ffi_func_eq);
                global_items.push(ffi_func_ne);
                global_items.push(trait_meta);
            }
        }
    }
    Ok(quote! {
        #[doc(hidden)]
        impl #self_ident {
            #(#impl_items)*
        }
        #(#global_items)*
    })
}

fn process_uniffi_trait_method(
    method: &TokenStream,
    self_ident: &Ident,
    udl_mode: bool,
) -> syn::Result<(TokenStream, TokenStream)> {
    let item = syn::parse(method.clone().into())?;

    let syn::Item::Fn(item) = item else {
        unreachable!()
    };

    let docstring = extract_docstring(&item.attrs)?;

    let ffi_func = gen_ffi_function(
        &FnSignature::new_method(
            self_ident.clone(),
            item.sig.clone(),
            ExportedImplFnArgs::default(),
            docstring.clone(),
        )?,
        &None,
        udl_mode,
    )?;
    // metadata for the method, which will be packed inside metadata for the trait.
    let method_meta = FnSignature::new_method(
        self_ident.clone(),
        item.sig,
        ExportedImplFnArgs::default(),
        docstring,
    )?
    .metadata_expr()?;
    Ok((ffi_func, method_meta))
}
