use proc_macro2::{Span, TokenStream};
use syn::{Data, DeriveInput, Field, Fields, Ident};
use utils::{add_where_clauses_for_new_ident, named_to_vec, unnamed_to_vec};

/// Provides the hook to expand `#[derive(IndexMut)]` into an implementation of `From`
pub fn expand(input: &DeriveInput, trait_name: &str) -> TokenStream {
    let trait_ident = Ident::new(trait_name, Span::call_site());
    let index_type = &Ident::new("__IdxT", Span::call_site());
    let trait_path = &quote!(::std::ops::#trait_ident<#index_type>);
    let input_type = &input.ident;
    let field_vec: Vec<&Field>;
    let member = match input.data {
        Data::Struct(ref data_struct) => match data_struct.fields {
            Fields::Unnamed(ref fields) => {
                field_vec = unnamed_to_vec(fields);
                tuple_from_str(trait_name, &field_vec)
            }
            Fields::Named(ref fields) => {
                field_vec = named_to_vec(fields);
                struct_from_str(trait_name, &field_vec)
            }
            Fields::Unit => panic_one_field(trait_name),
        },
        _ => panic_one_field(trait_name),
    };
    let field_type = &field_vec[0].ty;
    let type_where_clauses = quote!{
        where #field_type: #trait_path
    };

    let new_generics = add_where_clauses_for_new_ident(
        &input.generics,
        &field_vec,
        index_type,
        type_where_clauses,
    );

    let (impl_generics, _, where_clause) = new_generics.split_for_impl();
    let (_, ty_generics, _) = input.generics.split_for_impl();
    // let generics = add_extra_ty_param_bound(&input.generics, trait_path);
    let casted_trait = &quote!(<#field_type as #trait_path>);
    quote!{
        impl#impl_generics #trait_path for #input_type#ty_generics #where_clause
        {
            #[inline]
            fn index_mut(&mut self, idx: #index_type) -> &mut Self::Output {
                #casted_trait::index_mut(&mut #member, idx)
            }
        }
    }
}

fn panic_one_field(trait_name: &str) -> ! {
    panic!(format!(
        "Only structs with one field can derive({})",
        trait_name
    ))
}

fn tuple_from_str<'a>(trait_name: &str, fields: &[&'a Field]) -> (TokenStream) {
    if fields.len() != 1 {
        panic_one_field(trait_name)
    };
    quote!(self.0)
}

fn struct_from_str<'a>(trait_name: &str, fields: &[&'a Field]) -> TokenStream {
    if fields.len() != 1 {
        panic_one_field(trait_name)
    };
    let field = &fields[0];
    let field_ident = &field.ident;
    quote!(self.#field_ident)
}
