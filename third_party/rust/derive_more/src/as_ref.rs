use crate::utils::{
    add_where_clauses_for_new_ident, AttrParams, MultiFieldData, State,
};
use proc_macro2::{Span, TokenStream};
use quote::quote;
use syn::{parse::Result, DeriveInput, Ident};

pub fn expand(input: &DeriveInput, trait_name: &'static str) -> Result<TokenStream> {
    let as_ref_type = &Ident::new("__AsRefT", Span::call_site());
    let state = State::with_type_bound(
        input,
        trait_name,
        quote!(::core::convert),
        String::from("as_ref"),
        AttrParams::ignore_and_forward(),
        false,
    )?;
    let MultiFieldData {
        fields,
        input_type,
        members,
        infos,
        trait_path,
        impl_generics,
        ty_generics,
        where_clause,
        ..
    } = state.enabled_fields_data();
    let sub_items: Vec<_> = infos
        .iter()
        .zip(members.iter())
        .zip(fields)
        .map(|((info, member), field)| {
            let field_type = &field.ty;
            if info.forward {
                let trait_path = quote!(#trait_path<#as_ref_type>);
                let type_where_clauses = quote! {
                    where #field_type: #trait_path
                };
                let new_generics = add_where_clauses_for_new_ident(
                    &input.generics,
                    &[field],
                    as_ref_type,
                    type_where_clauses,
                    false,
                );
                let (impl_generics, _, where_clause) = new_generics.split_for_impl();
                let casted_trait = quote!(<#field_type as #trait_path>);
                (
                    quote!(#casted_trait::as_ref(&#member)),
                    quote!(#impl_generics),
                    quote!(#where_clause),
                    quote!(#trait_path),
                    quote!(#as_ref_type),
                )
            } else {
                (
                    quote!(&#member),
                    quote!(#impl_generics),
                    quote!(#where_clause),
                    quote!(#trait_path<#field_type>),
                    quote!(#field_type),
                )
            }
        })
        .collect();
    let bodies = sub_items.iter().map(|i| &i.0);
    let impl_generics = sub_items.iter().map(|i| &i.1);
    let where_clauses = sub_items.iter().map(|i| &i.2);
    let trait_paths = sub_items.iter().map(|i| &i.3);
    let return_types = sub_items.iter().map(|i| &i.4);

    Ok(quote! {#(
        impl#impl_generics #trait_paths for #input_type#ty_generics
        #where_clauses
        {
            #[inline]
            fn as_ref(&self) -> &#return_types {
                #bodies
            }
        }
    )*})
}
