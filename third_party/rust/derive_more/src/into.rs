use crate::utils::{add_extra_generic_param, AttrParams, MultiFieldData, State};
use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::{parse::Result, DeriveInput};

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
            struct_: vec!["ignore", "owned", "ref", "ref_mut"],
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

        let into = quote! {
            impl#impl_generics ::core::convert::From<#reference_with_lifetime #input_type#ty_generics> for
                (#(#reference_with_lifetime #field_types),*) #where_clause {

                #[allow(unused_variables)]
                #[inline]
                fn from(original: #reference_with_lifetime #input_type#ty_generics) -> (#(#reference_with_lifetime #field_types),*) {
                    (#(#reference original.#field_idents),*)
                }
            }
        };
        into.to_tokens(&mut tokens);
    }
    Ok(tokens)
}
