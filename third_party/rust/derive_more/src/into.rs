use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{Data, DeriveInput, Field, Fields};
use utils::{field_idents, get_field_types, named_to_vec, number_idents, unnamed_to_vec};

/// Provides the hook to expand `#[derive(Into)]` into an implementation of `Into`
pub fn expand(input: &DeriveInput, _: &str) -> TokenStream {
    let input_type = &input.ident;
    let field_vec: Vec<_>;
    let (impl_generics, ty_generics, where_clause) = input.generics.split_for_impl();
    let (field_names, fields) = match input.data {
        Data::Struct(ref data_struct) => match data_struct.fields {
            Fields::Unnamed(ref fields) => {
                field_vec = unnamed_to_vec(fields);
                (tuple_field_names(&field_vec), field_vec)
            }
            Fields::Named(ref fields) => {
                field_vec = named_to_vec(fields);
                (struct_field_names(&field_vec), field_vec)
            }
            Fields::Unit => (vec![], vec![]),
        },
        _ => panic!("Only structs can derive Into"),
    };

    let original_types = &get_field_types(&fields);

    quote!{
        impl#impl_generics ::std::convert::From<#input_type#ty_generics> for
            (#(#original_types),*) #where_clause {

            #[allow(unused_variables)]
            #[inline]
            fn from(original: #input_type#ty_generics) -> (#(#original_types),*) {
                (#(original.#field_names),*)
            }
        }
    }
}

fn tuple_field_names(fields: &[&Field]) -> Vec<TokenStream> {
    number_idents(fields.len())
        .iter()
        .map(|f| f.into_token_stream())
        .collect()
}

fn struct_field_names(fields: &[&Field]) -> Vec<TokenStream> {
    field_idents(fields)
        .iter()
        .map(|f| (*f).into_token_stream())
        .collect()
}
