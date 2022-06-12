use std::iter;

use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::{parse::Result, DeriveInput};

use crate::utils::{add_extra_generic_param, AttrParams, MultiFieldData, State};

/// Provides the hook to expand `#[derive(Into)]` into an implementation of `Into`
pub fn expand(input: &DeriveInput, trait_name: &'static str) -> Result<TokenStream> {
    let state = State::with_attr_params(
        input,
        trait_name,
        quote!(::core::convert),
        trait_name.to_lowercase(),
        AttrParams {
            enum_: vec!["ignore", "owned", "ref", "ref_mut"],
            variant: vec!["ignore", "owned", "ref", "ref_mut"],
            struct_: vec!["ignore", "owned", "ref", "ref_mut", "types"],
            field: vec!["ignore"],
        },
    )?;
    let MultiFieldData {
        variant_info,
        field_types,
        field_idents,
        input_type,
        ..
    } = state.enabled_fields_data();

    let mut tokens = TokenStream::new();

    for ref_type in variant_info.ref_types() {
        let reference = ref_type.reference();
        let lifetime = ref_type.lifetime();
        let reference_with_lifetime = ref_type.reference_with_lifetime();

        let generics_impl;
        let (_, ty_generics, where_clause) = input.generics.split_for_impl();
        let (impl_generics, _, _) = if ref_type.is_ref() {
            generics_impl = add_extra_generic_param(&input.generics, lifetime);
            generics_impl.split_for_impl()
        } else {
            input.generics.split_for_impl()
        };

        let additional_types = variant_info.additional_types(ref_type);
        for explicit_type in iter::once(None).chain(additional_types.iter().map(Some)) {
            let into_types: Vec<_> = field_types
                .iter()
                .map(|field_type| {
                    // No, `.unwrap_or()` won't work here, because we use different types.
                    if let Some(type_) = explicit_type {
                        quote! { #reference_with_lifetime #type_ }
                    } else {
                        quote! { #reference_with_lifetime #field_type }
                    }
                })
                .collect();

            let initializers = field_idents.iter().map(|field_ident| {
                if let Some(type_) = explicit_type {
                    quote! { <#reference #type_>::from(#reference original.#field_ident) }
                } else {
                    quote! { #reference original.#field_ident }
                }
            });

            (quote! {
                #[automatically_derived]
                impl#impl_generics ::core::convert::From<#reference_with_lifetime #input_type#ty_generics> for
                    (#(#into_types),*) #where_clause {

                    #[inline]
                    fn from(original: #reference_with_lifetime #input_type#ty_generics) -> Self {
                        (#(#initializers),*)
                    }
                }
            }).to_tokens(&mut tokens);
        }
    }
    Ok(tokens)
}
