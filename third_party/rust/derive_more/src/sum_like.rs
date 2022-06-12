use crate::utils::{
    add_extra_ty_param_bound, add_extra_where_clauses, MultiFieldData, State,
};
use proc_macro2::{Span, TokenStream};
use quote::quote;
use syn::{DeriveInput, Ident, Result};

pub fn expand(input: &DeriveInput, trait_name: &'static str) -> Result<TokenStream> {
    let state = State::new(
        input,
        trait_name,
        quote!(::core::iter),
        trait_name.to_lowercase(),
    )?;
    let multi_field_data = state.enabled_fields_data();
    let MultiFieldData {
        input_type,
        field_types,
        trait_path,
        method_ident,
        ..
    } = multi_field_data.clone();

    let op_trait_name = if trait_name == "Sum" { "Add" } else { "Mul" };
    let op_trait_ident = Ident::new(op_trait_name, Span::call_site());
    let op_path = quote!(::core::ops::#op_trait_ident);
    let op_method_ident =
        Ident::new(&(op_trait_name.to_lowercase()), Span::call_site());
    let has_type_params = input.generics.type_params().next().is_none();
    let generics = if has_type_params {
        input.generics.clone()
    } else {
        let (_, ty_generics, _) = input.generics.split_for_impl();
        let generics = add_extra_ty_param_bound(&input.generics, trait_path);
        let operator_where_clause = quote! {
            where #input_type#ty_generics: #op_path<Output=#input_type#ty_generics>
        };
        add_extra_where_clauses(&generics, operator_where_clause)
    };
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let initializers: Vec<_> = field_types
        .iter()
        .map(|field_type| quote!(#trait_path::#method_ident(::core::iter::empty::<#field_type>())))
        .collect();
    let identity = multi_field_data.initializer(&initializers);

    Ok(quote!(
        impl#impl_generics #trait_path for #input_type#ty_generics #where_clause {
            #[inline]
            fn #method_ident<I: ::core::iter::Iterator<Item = Self>>(iter: I) -> Self {
                iter.fold(#identity, #op_path::#op_method_ident)
            }
        }
    ))
}
