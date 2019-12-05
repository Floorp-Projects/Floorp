use crate::utils::{
    add_where_clauses_for_new_ident, AttrParams, DeriveType, MultiFieldData, State,
};
use proc_macro2::{Span, TokenStream};
use quote::{quote, ToTokens};
use std::collections::HashMap;
use syn::{parse::Result, DeriveInput, Ident, Index};

/// Provides the hook to expand `#[derive(From)]` into an implementation of `From`
pub fn expand(input: &DeriveInput, trait_name: &'static str) -> Result<TokenStream> {
    let state = State::with_attr_params(
        input,
        trait_name,
        quote!(::core::convert),
        trait_name.to_lowercase(),
        AttrParams {
            enum_: vec!["forward", "ignore"],
            variant: vec!["forward", "ignore"],
            struct_: vec!["forward"],
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
        infos,
        input_type,
        trait_path,
        ..
    } = multi_field_data.clone();

    let mut new_generics = input.generics.clone();
    let sub_items: Vec<_> = infos
        .iter()
        .zip(fields.iter())
        .enumerate()
        .map(|(i, (info, field))| {
            let field_type = &field.ty;
            let variable = if fields.len() == 1 {
                quote!(original)
            } else {
                let tuple_index = Index::from(i);
                quote!(original.#tuple_index)
            };
            if info.forward {
                let type_param =
                    &Ident::new(&format!("__FromT{}", i), Span::call_site());
                let sub_trait_path = quote!(#trait_path<#type_param>);
                let type_where_clauses = quote! {
                    where #field_type: #sub_trait_path
                };
                new_generics = add_where_clauses_for_new_ident(
                    &input.generics,
                    &[field],
                    type_param,
                    type_where_clauses,
                    true,
                );
                let casted_trait = quote!(<#field_type as #sub_trait_path>);
                (quote!(#casted_trait::from(#variable)), quote!(#type_param))
            } else {
                (variable, quote!(#field_type))
            }
        })
        .collect();
    let initializers: Vec<_> = sub_items.iter().map(|i| &i.0).collect();
    let from_types: Vec<_> = sub_items.iter().map(|i| &i.1).collect();

    let body = multi_field_data.initializer(&initializers);
    let (impl_generics, _, where_clause) = new_generics.split_for_impl();
    let (_, ty_generics, _) = input.generics.split_for_impl();

    quote! {
        impl#impl_generics #trait_path<(#(#from_types),*)> for
            #input_type#ty_generics #where_clause {

            #[allow(unused_variables)]
            #[inline]
            fn from(original: (#(#from_types),*)) -> #input_type#ty_generics {
                #body
            }
        }
    }
}

fn enum_from(input: &DeriveInput, state: State) -> TokenStream {
    let mut tokens = TokenStream::new();

    let mut variants_per_types = HashMap::new();
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
            // variants that are not explicitely enabled or have explicitely enabled
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
