use crate::max_size_expr;
use proc_macro2::{Span, TokenStream};
use quote::{quote, ToTokens};
use std::{fmt::Display, str::FromStr};
use syn::{DataEnum, DataStruct, Fields, Ident, Index};

/// Calculates serialize expression for fields
fn get_poke_into_expr_for_fields<T: ToTokens + Display>(
    container_prefix: T,
    fields: &Fields,
) -> TokenStream {
    match fields {
        Fields::Unit => quote! { bytes },
        Fields::Named(named_fields) => {
            let mut exprs = Vec::with_capacity(named_fields.named.len());

            for field in named_fields.named.iter() {
                let field_name = match &field.ident {
                    None => unreachable!(),
                    Some(ref ident) => quote! { #ident },
                };

                let field_ref =
                    TokenStream::from_str(&format!("{}{}", container_prefix, field_name)).unwrap();

                exprs.push(quote! {
                    let bytes = #field_ref.poke_into(bytes);
                });
            }

            quote! {
                #(#exprs)*
                bytes
            }
        }
        Fields::Unnamed(unnamed_fields) => {
            let mut exprs = Vec::with_capacity(unnamed_fields.unnamed.len());

            for i in 0..unnamed_fields.unnamed.len() {
                let field_ref =
                    TokenStream::from_str(&format!("{}{}", container_prefix, i)).unwrap();

                exprs.push(quote! {
                    let bytes = #field_ref.poke_into(bytes);
                });
            }

            quote! {
                #(#exprs)*
                bytes
            }
        }
    }
}

/// Calculates  expression for [`DataStruct`](syn::DataStruct)
pub fn for_struct(_: &Ident, struct_data: &DataStruct) -> TokenStream {
    get_poke_into_expr_for_fields(quote! { self. }, &struct_data.fields)
}

/// Calculates serialize expression for [`DataEnum`](syn::DataEnum)
pub fn for_enum(name: &Ident, enum_data: &DataEnum) -> TokenStream {
    let variant_count = enum_data.variants.len();

    let size_type = max_size_expr::get_variant_count_max_size_type(variant_count);
    let mut match_exprs = Vec::with_capacity(variant_count);

    for (i, variant) in enum_data.variants.iter().enumerate() {
        let index = Index::from(i);

        let field_prefix = match variant.fields {
            Fields::Unit => quote! {},
            Fields::Named(_) => quote! {},
            Fields::Unnamed(_) => quote! { __self_ },
        };

        let fields_expr = match &variant.fields {
            Fields::Unit => quote! {},
            Fields::Named(named_fields) => {
                let mut exprs = Vec::with_capacity(named_fields.named.len());

                for field in named_fields.named.iter() {
                    let field_name = match &field.ident {
                        None => unreachable!(),
                        Some(ref ident) => quote! { #ident },
                    };

                    exprs.push(quote! { ref #field_name })
                }

                quote! { { #(#exprs),* } }
            }
            Fields::Unnamed(unnamed_fields) => {
                let len = unnamed_fields.unnamed.len();
                let mut exprs = Vec::with_capacity(len);

                for j in 0..len {
                    let name = Ident::new(&format!("{}{}", field_prefix, j), Span::call_site());
                    exprs.push(quote! { ref #name });
                }

                quote! { ( #(#exprs),* ) }
            }
        };

        let variant_name = &variant.ident;
        let variant_init_expr = quote! {
            let bytes = (#index as #size_type).poke_into(bytes);
        };
        let variant_impl_expr = get_poke_into_expr_for_fields(field_prefix, &variant.fields);

        let variant_expr = quote! {
            #variant_init_expr
            #variant_impl_expr
        };

        match_exprs.push(quote! {
            #name:: #variant_name #fields_expr => { #variant_expr }
        });
    }

    quote! {
        match self {
            #(#match_exprs),*
        }
    }
}
