use crate::add_helpers::{struct_exprs, tuple_exprs};
use crate::utils::{add_extra_ty_param_bound_op, named_to_vec, unnamed_to_vec};
use proc_macro2::{Span, TokenStream};
use quote::quote;
use syn::{Data, DeriveInput, Fields, Ident};

pub fn expand(input: &DeriveInput, trait_name: &str) -> TokenStream {
    let trait_ident = Ident::new(trait_name, Span::call_site());
    let method_name = trait_name.to_string();
    let method_name = method_name.trim_end_matches("Assign");
    let method_name = method_name.to_lowercase();
    let method_ident = Ident::new(&(method_name + "_assign"), Span::call_site());
    let input_type = &input.ident;

    let generics = add_extra_ty_param_bound_op(&input.generics, &trait_ident);
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let exprs = match input.data {
        Data::Struct(ref data_struct) => match data_struct.fields {
            Fields::Unnamed(ref fields) => {
                tuple_exprs(&unnamed_to_vec(fields), &method_ident)
            }
            Fields::Named(ref fields) => {
                struct_exprs(&named_to_vec(fields), &method_ident)
            }
            _ => panic!("Unit structs cannot use derive({})", trait_name),
        },

        _ => panic!("Only structs can use derive({})", trait_name),
    };

    quote!(
        impl#impl_generics ::core::ops::#trait_ident for #input_type#ty_generics #where_clause {
            #[inline]
            fn #method_ident(&mut self, rhs: #input_type#ty_generics) {
                #(#exprs;
                  )*
            }
        }
    )
}
