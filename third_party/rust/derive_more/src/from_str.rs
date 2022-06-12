use crate::utils::{SingleFieldData, State};
use proc_macro2::TokenStream;
use quote::quote;
use syn::{parse::Result, DeriveInput};

/// Provides the hook to expand `#[derive(FromStr)]` into an implementation of `FromStr`
pub fn expand(input: &DeriveInput, trait_name: &'static str) -> Result<TokenStream> {
    let state = State::new(
        input,
        trait_name,
        quote!(::core::str),
        trait_name.to_lowercase(),
    )?;

    // We cannot set defaults for fields, once we do we can remove this check
    if state.fields.len() != 1 || state.enabled_fields().len() != 1 {
        panic_one_field(trait_name);
    }

    let single_field_data = state.assert_single_enabled_field();
    let SingleFieldData {
        input_type,
        field_type,
        trait_path,
        casted_trait,
        impl_generics,
        ty_generics,
        where_clause,
        ..
    } = single_field_data.clone();

    let initializers = [quote!(#casted_trait::from_str(src)?)];
    let body = single_field_data.initializer(&initializers);

    Ok(quote! {
        impl#impl_generics #trait_path for #input_type#ty_generics #where_clause
        {
            type Err = <#field_type as #trait_path>::Err;
            #[inline]
            fn from_str(src: &str) -> ::core::result::Result<Self, Self::Err> {
                Ok(#body)
            }
        }
    })
}

fn panic_one_field(trait_name: &str) -> ! {
    panic!("Only structs with one field can derive({})", trait_name)
}
