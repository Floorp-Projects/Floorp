use crate::{max_size_expr, peek_poke::Generate};
use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use std::{fmt::Display, str::FromStr};
use syn::{DataEnum, DataStruct, Fields, Ident, Index};

/// Calculates serialize expression for fields
fn get_peek_from_expr_for_fields<T: ToTokens + Display>(
    field_prefix: T,
    fields: &Fields,
) -> (TokenStream, TokenStream) {
    match fields {
        Fields::Unit => (quote! {}, quote! {}),
        Fields::Named(named_fields) => {
            if named_fields.named.is_empty() {
                (quote! {}, quote! {})
            } else {
                let mut exprs = Vec::with_capacity(named_fields.named.len());
                let mut fields = Vec::with_capacity(named_fields.named.len());

                for field in named_fields.named.iter() {
                    let field_name = match &field.ident {
                        None => unreachable!(),
                        Some(ref ident) => quote! { #ident },
                    };

                    let field_ref =
                        TokenStream::from_str(&format!("{}{}", field_prefix, field_name)).unwrap();

                    exprs.push(quote! {
                        let bytes = #field_ref.peek_from(bytes);
                    });
                    fields.push(field_name);
                }

                (
                    quote! {
                        #(#exprs;)*
                    },
                    quote! {
                        #(#fields),*
                    },
                )
            }
        }
        Fields::Unnamed(unnamed_fields) => {
            if unnamed_fields.unnamed.is_empty() {
                (quote! {}, quote! {})
            } else {
                let mut fields = Vec::with_capacity(unnamed_fields.unnamed.len());
                let mut exprs = Vec::with_capacity(unnamed_fields.unnamed.len());

                for n in 0..unnamed_fields.unnamed.len() {
                    let field_name =
                        TokenStream::from_str(&format!("{}{}", field_prefix, n)).unwrap();

                    exprs.push(quote! {
                        let bytes = #field_name.peek_from(bytes);
                    });
                    fields.push(field_name);
                }

                (
                    quote! {
                        #(#exprs)*
                    },
                    quote! {
                        #(#fields),*
                    },
                )
            }
        }
    }
}

fn get_peek_from_init_expr_for_fields(fields: &Fields, gen: Generate) -> TokenStream {
    match fields {
        Fields::Unit => quote! {},
        Fields::Named(named_fields) => {
            if named_fields.named.is_empty() {
                quote! {}
            } else {
                let mut exprs = Vec::with_capacity(named_fields.named.len());

                for field in &named_fields.named {
                    let field_name = match &field.ident {
                        None => unreachable!(),
                        Some(ref ident) => quote! { #ident },
                    };

                    let field_type = &field.ty;

                    let init = if gen == Generate::PeekDefault {
                        quote! {
                            let mut #field_name = #field_type::default();
                        }
                    } else {
                        quote! {
                            let mut #field_name: #field_type = unsafe { core::mem::uninitialized() };
                        }
                    };
                    exprs.push(init);
                }
                quote! {
                    #(#exprs)*
                }
            }
        }
        Fields::Unnamed(unnamed_fields) => {
            if unnamed_fields.unnamed.is_empty() {
                quote! {}
            } else {
                let mut exprs = Vec::with_capacity(unnamed_fields.unnamed.len());

                for (n, field) in unnamed_fields.unnamed.iter().enumerate() {
                    let field_name = TokenStream::from_str(&format!("__self_{}", n)).unwrap();
                    let field_type = &field.ty;

                    let init = if gen == Generate::PeekDefault {
                        quote! {
                            let mut #field_name = #field_type::default();
                        }
                    } else {
                        quote! {
                            let mut #field_name: #field_type = unsafe { core::mem::uninitialized() };
                        }
                    };
                    exprs.push(init);
                }

                quote! {
                    #(#exprs)*
                }
            }
        }
    }
}

/// Calculates size expression for [`DataStruct`](syn::DataStruct)
pub fn for_struct(struct_data: &DataStruct) -> TokenStream {
    let (exprs, _) = get_peek_from_expr_for_fields(quote! { self. }, &struct_data.fields);
    quote! {
        #exprs
        bytes
    }
}

/// Calculates size expression for [`DataEnum`](syn::DataEnum)
pub fn for_enum(name: &Ident, enum_data: &DataEnum, gen: Generate) -> TokenStream {
    let variant_count = enum_data.variants.len();

    let size_type = max_size_expr::get_variant_count_max_size_type(variant_count);
    let mut match_exprs = Vec::with_capacity(variant_count);

    let variant_expr = quote! {
        let mut variant: #size_type = 0;
        let bytes = variant.peek_from(bytes);
    };

    for (i, variant) in enum_data.variants.iter().enumerate() {
        let variant_name = &variant.ident;
        let prefix = match &variant.fields {
            Fields::Unnamed(..) => quote! {__self_},
            _ => quote! {},
        };
        let (variant_expr, fields_expr) = get_peek_from_expr_for_fields(prefix, &variant.fields);

        let index = Index::from(i);
        let init_expr = get_peek_from_init_expr_for_fields(&variant.fields, gen);
        let self_assign_expr = match &variant.fields {
            Fields::Named(..) => quote! {
                *self = #name:: #variant_name { #fields_expr };
            },
            Fields::Unnamed(..) => quote! {
                *self = #name:: #variant_name(#fields_expr);
            },
            Fields::Unit => quote! {
                *self = #name:: #variant_name;
            },
        };

        match_exprs.push(quote! {
            #index => {
                #init_expr
                #variant_expr
                #self_assign_expr
                bytes
            }
        });
    }

    match_exprs.push(quote! {
        _ => unreachable!()
    });

    let match_expr = quote! {
        match variant {
            #(#match_exprs),*
        }
    };

    quote! {
        #variant_expr
        #match_expr
    }
}
