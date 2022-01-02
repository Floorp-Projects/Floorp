use std::iter;

use proc_macro2::{Span, TokenStream};
use quote::{quote, ToTokens};
use syn::{parse::Result, DeriveInput, Ident, Index};

use crate::utils::{
    add_where_clauses_for_new_ident, AttrParams, DeriveType, HashMap, MultiFieldData,
    RefType, State,
};

/// Provides the hook to expand `#[derive(From)]` into an implementation of `From`
pub fn expand(input: &DeriveInput, trait_name: &'static str) -> Result<TokenStream> {
    let state = State::with_attr_params(
        input,
        trait_name,
        quote!(::core::convert),
        trait_name.to_lowercase(),
        AttrParams {
            enum_: vec!["forward", "ignore"],
            variant: vec!["forward", "ignore", "types"],
            struct_: vec!["forward", "types"],
            field: vec!["forward"],
        },
    )?;
    if state.derive_type == DeriveType::Enum {
        Ok(enum_from(input, state))
    } else {
        Ok(struct_from(input, &state))
    }
}

pub fn struct_from(input: &DeriveInput, state: &State) -> TokenStream {
    let multi_field_data = state.enabled_fields_data();
    let MultiFieldData {
        fields,
        variant_info,
        infos,
        input_type,
        trait_path,
        ..
    } = multi_field_data.clone();

    let additional_types = variant_info.additional_types(RefType::No);
    let mut impls = Vec::with_capacity(additional_types.len() + 1);
    for explicit_type in iter::once(None).chain(additional_types.iter().map(Some)) {
        let mut new_generics = input.generics.clone();

        let mut initializers = Vec::with_capacity(infos.len());
        let mut from_types = Vec::with_capacity(infos.len());
        for (i, (info, field)) in infos.iter().zip(fields.iter()).enumerate() {
            let field_type = &field.ty;
            let variable = if fields.len() == 1 {
                quote! { original }
            } else {
                let tuple_index = Index::from(i);
                quote! { original.#tuple_index }
            };
            if let Some(type_) = explicit_type {
                initializers.push(quote! {
                    <#field_type as #trait_path<#type_>>::from(#variable)
                });
                from_types.push(quote! { #type_ });
            } else if info.forward {
                let type_param =
                    &Ident::new(&format!("__FromT{}", i), Span::call_site());
                let sub_trait_path = quote! { #trait_path<#type_param> };
                let type_where_clauses = quote! {
                    where #field_type: #sub_trait_path
                };
                new_generics = add_where_clauses_for_new_ident(
                    &new_generics,
                    &[field],
                    type_param,
                    type_where_clauses,
                    true,
                );
                let casted_trait = quote! { <#field_type as #sub_trait_path> };
                initializers.push(quote! { #casted_trait::from(#variable) });
                from_types.push(quote! { #type_param });
            } else {
                initializers.push(variable);
                from_types.push(quote! { #field_type });
            }
        }

        let body = multi_field_data.initializer(&initializers);
        let (impl_generics, _, where_clause) = new_generics.split_for_impl();
        let (_, ty_generics, _) = input.generics.split_for_impl();

        impls.push(quote! {
            #[automatically_derived]
            impl#impl_generics #trait_path<(#(#from_types),*)> for
                #input_type#ty_generics #where_clause {

                #[inline]
                fn from(original: (#(#from_types),*)) -> #input_type#ty_generics {
                    #body
                }
            }
        });
    }

    quote! { #( #impls )* }
}

fn enum_from(input: &DeriveInput, state: State) -> TokenStream {
    let mut tokens = TokenStream::new();

    let mut variants_per_types = HashMap::default();
    for variant_state in state.enabled_variant_data().variant_states {
        let multi_field_data = variant_state.enabled_fields_data();
        let MultiFieldData { field_types, .. } = multi_field_data.clone();
        variants_per_types
            .entry(field_types.clone())
            .or_insert_with(Vec::new)
            .push(variant_state);
    }
    for (ref field_types, ref variant_states) in variants_per_types {
        for variant_state in variant_states {
            let multi_field_data = variant_state.enabled_fields_data();
            let MultiFieldData {
                variant_info,
                infos,
                ..
            } = multi_field_data.clone();
            // If there would be a conflict on a empty tuple derive, ignore the
            // variants that are not explicitly enabled or have explicitly enabled
            // or disabled fields
            if field_types.is_empty()
                && variant_states.len() > 1
                && !std::iter::once(variant_info)
                    .chain(infos)
                    .any(|info| info.info.enabled.is_some())
            {
                continue;
            }
            struct_from(input, variant_state).to_tokens(&mut tokens);
        }
    }
    tokens
}
