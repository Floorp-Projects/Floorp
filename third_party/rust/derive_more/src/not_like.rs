use crate::utils::{
    add_extra_type_param_bound_op_output, named_to_vec, unnamed_to_vec,
};
use proc_macro2::{Span, TokenStream};
use quote::{quote, ToTokens};
use std::iter;
use syn::{Data, DataEnum, DeriveInput, Field, Fields, Ident, Index};

pub fn expand(input: &DeriveInput, trait_name: &str) -> TokenStream {
    let trait_ident = Ident::new(trait_name, Span::call_site());
    let method_name = trait_name.to_lowercase();
    let method_ident = &Ident::new(&method_name, Span::call_site());
    let input_type = &input.ident;

    let generics = add_extra_type_param_bound_op_output(&input.generics, &trait_ident);
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let (output_type, block) = match input.data {
        Data::Struct(ref data_struct) => match data_struct.fields {
            Fields::Unnamed(ref fields) => (
                quote!(#input_type#ty_generics),
                tuple_content(input_type, &unnamed_to_vec(fields), method_ident),
            ),
            Fields::Named(ref fields) => (
                quote!(#input_type#ty_generics),
                struct_content(input_type, &named_to_vec(fields), method_ident),
            ),
            _ => panic!("Unit structs cannot use derive({})", trait_name),
        },
        Data::Enum(ref data_enum) => {
            enum_output_type_and_content(input, data_enum, method_ident)
        }

        _ => panic!("Only structs and enums can use derive({})", trait_name),
    };

    quote!(
        impl#impl_generics ::core::ops::#trait_ident for #input_type#ty_generics #where_clause {
            type Output = #output_type;
            #[inline]
            fn #method_ident(self) -> #output_type {
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
    let mut exprs = vec![];

    for i in 0..fields.len() {
        let i = Index::from(i);
        // generates `self.0.add()`
        let expr = quote!(self.#i.#method_ident());
        exprs.push(expr);
    }

    quote!(#input_type(#(#exprs),*))
}

fn struct_content(
    input_type: &Ident,
    fields: &[&Field],
    method_ident: &Ident,
) -> TokenStream {
    let mut exprs = vec![];

    for field in fields {
        // It's safe to unwrap because struct fields always have an identifier
        let field_id = field.ident.as_ref();
        // generates `x: self.x.not()`
        let expr = quote!(#field_id: self.#field_id.#method_ident());
        exprs.push(expr)
    }

    quote!(#input_type{#(#exprs),*})
}

fn enum_output_type_and_content(
    input: &DeriveInput,
    data_enum: &DataEnum,
    method_ident: &Ident,
) -> (TokenStream, TokenStream) {
    let input_type = &input.ident;
    let (_, ty_generics, _) = input.generics.split_for_impl();
    let mut matches = vec![];
    let mut method_iter = iter::repeat(method_ident);
    // If the enum contains unit types that means it can error.
    let has_unit_type = data_enum.variants.iter().any(|v| v.fields == Fields::Unit);

    for variant in &data_enum.variants {
        let subtype = &variant.ident;
        let subtype = quote!(#input_type::#subtype);

        match variant.fields {
            Fields::Unnamed(ref fields) => {
                // The patern that is outputted should look like this:
                // (Subtype(vars)) => Ok(TypePath(exprs))
                let size = unnamed_to_vec(fields).len();
                let vars: &Vec<_> = &(0..size)
                    .map(|i| Ident::new(&format!("__{}", i), Span::call_site()))
                    .collect();
                let method_iter = method_iter.by_ref();
                let mut body = quote!(#subtype(#(#vars.#method_iter()),*));
                if has_unit_type {
                    body = quote!(::core::result::Result::Ok(#body))
                }
                let matcher = quote! {
                    #subtype(#(#vars),*) => {
                        #body
                    }
                };
                matches.push(matcher);
            }
            Fields::Named(ref fields) => {
                // The patern that is outputted should look like this:
                // (Subtype{a: __l_a, ...} => {
                //     Ok(Subtype{a: __l_a.neg(__r_a), ...})
                // }
                let field_vec = named_to_vec(fields);
                let size = field_vec.len();
                let field_names: &Vec<_> = &field_vec
                    .iter()
                    .map(|f| f.ident.as_ref().unwrap())
                    .collect();
                let vars: &Vec<_> = &(0..size)
                    .map(|i| Ident::new(&format!("__{}", i), Span::call_site()))
                    .collect();
                let method_iter = method_iter.by_ref();
                let mut body =
                    quote!(#subtype{#(#field_names: #vars.#method_iter()),*});
                if has_unit_type {
                    body = quote!(::core::result::Result::Ok(#body))
                }
                let matcher = quote! {
                    #subtype{#(#field_names: #vars),*} => {
                        #body
                    }
                };
                matches.push(matcher);
            }
            Fields::Unit => {
                let message = format!("Cannot {}() unit variants", method_ident);
                matches.push(quote!(#subtype => ::core::result::Result::Err(#message)));
            }
        }
    }

    let body = quote!(
        match self {
            #(#matches),*
        }
    );

    let output_type = if has_unit_type {
        quote!(::core::result::Result<#input_type#ty_generics, &'static str>)
    } else {
        quote!(#input_type#ty_generics)
    };

    (output_type, body)
}
