use crate::add_helpers::{struct_exprs, tuple_exprs};
use crate::utils::{
    add_extra_type_param_bound_op_output, field_idents, named_to_vec, numbered_vars,
    unnamed_to_vec,
};
use proc_macro2::{Span, TokenStream};
use quote::{quote, ToTokens};
use std::iter;
use syn::{Data, DataEnum, DeriveInput, Field, Fields, Ident};

pub fn expand(input: &DeriveInput, trait_name: &str) -> TokenStream {
    let trait_name = trait_name.trim_end_matches("Self");
    let trait_ident = Ident::new(trait_name, Span::call_site());
    let method_name = trait_name.to_lowercase();
    let method_ident = Ident::new(&method_name, Span::call_site());
    let input_type = &input.ident;

    let generics = add_extra_type_param_bound_op_output(&input.generics, &trait_ident);
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let (output_type, block) = match input.data {
        Data::Struct(ref data_struct) => match data_struct.fields {
            Fields::Unnamed(ref fields) => (
                quote!(#input_type#ty_generics),
                tuple_content(input_type, &unnamed_to_vec(fields), &method_ident),
            ),
            Fields::Named(ref fields) => (
                quote!(#input_type#ty_generics),
                struct_content(input_type, &named_to_vec(fields), &method_ident),
            ),
            _ => panic!(format!("Unit structs cannot use derive({})", trait_name)),
        },
        Data::Enum(ref data_enum) => (
            quote!(::core::result::Result<#input_type#ty_generics, &'static str>),
            enum_content(input_type, data_enum, &method_ident),
        ),

        _ => panic!(format!(
            "Only structs and enums can use derive({})",
            trait_name
        )),
    };

    quote!(
        impl#impl_generics ::core::ops::#trait_ident for #input_type#ty_generics #where_clause {
            type Output = #output_type;
            #[inline]
            fn #method_ident(self, rhs: #input_type#ty_generics) -> #output_type {
                #block
            }
        }
    )
}

fn tuple_content<T: ToTokens>(
    input_type: &T,
    fields: &[&Field],
    method_ident: &Ident,
) -> TokenStream {
    let exprs = tuple_exprs(fields, method_ident);
    quote!(#input_type(#(#exprs),*))
}

fn struct_content(
    input_type: &Ident,
    fields: &[&Field],
    method_ident: &Ident,
) -> TokenStream {
    // It's safe to unwrap because struct fields always have an identifier
    let exprs = struct_exprs(fields, method_ident);
    let field_names = field_idents(fields);

    quote!(#input_type{#(#field_names: #exprs),*})
}

#[allow(clippy::cognitive_complexity)]
fn enum_content(
    input_type: &Ident,
    data_enum: &DataEnum,
    method_ident: &Ident,
) -> TokenStream {
    let mut matches = vec![];
    let mut method_iter = iter::repeat(method_ident);

    for variant in &data_enum.variants {
        let subtype = &variant.ident;
        let subtype = quote!(#input_type::#subtype);

        match variant.fields {
            Fields::Unnamed(ref fields) => {
                // The patern that is outputted should look like this:
                // (Subtype(left_vars), TypePath(right_vars)) => Ok(TypePath(exprs))
                let size = unnamed_to_vec(fields).len();
                let l_vars = &numbered_vars(size, "l_");
                let r_vars = &numbered_vars(size, "r_");
                let method_iter = method_iter.by_ref();
                let matcher = quote! {
                    (#subtype(#(#l_vars),*),
                     #subtype(#(#r_vars),*)) => {
                        ::core::result::Result::Ok(#subtype(#(#l_vars.#method_iter(#r_vars)),*))
                    }
                };
                matches.push(matcher);
            }
            Fields::Named(ref fields) => {
                // The patern that is outputted should look like this:
                // (Subtype{a: __l_a, ...}, Subtype{a: __r_a, ...} => {
                //     Ok(Subtype{a: __l_a.add(__r_a), ...})
                // }
                let field_vec = named_to_vec(fields);
                let size = field_vec.len();
                let field_names = &field_idents(&field_vec);
                let l_vars = &numbered_vars(size, "l_");
                let r_vars = &numbered_vars(size, "r_");
                let method_iter = method_iter.by_ref();
                let matcher = quote! {
                    (#subtype{#(#field_names: #l_vars),*},
                     #subtype{#(#field_names: #r_vars),*}) => {
                        ::core::result::Result::Ok(#subtype{#(#field_names: #l_vars.#method_iter(#r_vars)),*})
                    }
                };
                matches.push(matcher);
            }
            Fields::Unit => {
                let message =
                    format!("Cannot {}() unit variants", method_ident.to_string());
                matches.push(quote!((#subtype, #subtype) => ::core::result::Result::Err(#message)));
            }
        }
    }

    if data_enum.variants.len() > 1 {
        // In the strange case where there's only one enum variant this is would be an unreachable
        // match.
        let message = format!(
            "Trying to {} mismatched enum variants",
            method_ident.to_string()
        );
        matches.push(quote!(_ => ::core::result::Result::Err(#message)));
    }
    quote!(
        match (self, rhs) {
            #(#matches),*
        }
    )
}
