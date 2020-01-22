use proc_macro2::TokenStream;
use quote::quote;
use syn::{punctuated::Punctuated, DataEnum, DataStruct, Field, Fields, Type, Variant};

/// Calculates size expression for punctuated fields
fn get_max_size_expr_for_punctuated_field<T>(fields: &Punctuated<Field, T>) -> TokenStream {
    if fields.is_empty() {
        return quote! { 0 };
    } else {
        let types = fields.iter().map(|field| &field.ty).collect::<Vec<&Type>>();
        quote! { #(<#types>::max_size())+* }
    }
}

/// Calculates size expression for fields
fn get_max_size_expr_for_fields(fields: &Fields) -> TokenStream {
    match fields {
        Fields::Unit => quote! { 0 },
        Fields::Named(named_fields) => {
            get_max_size_expr_for_punctuated_field(&named_fields.named)
        }
        Fields::Unnamed(unnamed_fields) => {
            get_max_size_expr_for_punctuated_field(&unnamed_fields.unnamed)
        }
    }
}

/// Calculates size expression for punctuated variants
fn get_max_size_expr_for_punctuated_variant<T>(variants: &Punctuated<Variant, T>) -> TokenStream {
    if variants.is_empty() {
        return quote! { 0 };
    } else {
        let count_size_expr = get_variant_count_max_size_expr(variants.len());
        let max_size_expr = get_variant_max_size_expr(variants);

        quote! { #count_size_expr + #max_size_expr }
    }
}

/// Calculates size expression for variant
#[allow(unused)]
fn get_max_size_expr_for_variant(variant: &Variant) -> TokenStream {
    get_max_size_expr_for_fields(&variant.fields)
}

/// Calculates size expression for number of variants (used for enums)
fn get_variant_count_max_size_expr(len: usize) -> TokenStream {
    let size_type = get_variant_count_max_size_type(len);
    quote! { <#size_type>::max_size() }
}

/// Calculates size expression for maximum sized variant
fn get_variant_max_size_expr<T>(variants: &Punctuated<Variant, T>) -> TokenStream {
    let mut max_size_expr = quote! { 0 };

    for variant in variants {
        let variant_size_expr = get_max_size_expr_for_variant(variant);
        max_size_expr = quote! { core::cmp::max(#max_size_expr, #variant_size_expr) };
    }

    max_size_expr
}

/// Calculates size type for number of variants (used for enums)
pub fn get_variant_count_max_size_type(len: usize) -> TokenStream {
    if len <= <u8>::max_value() as usize {
        quote! { u8 }
    } else if len <= <u16>::max_value() as usize {
        quote! { u16 }
    } else if len <= <u32>::max_value() as usize {
        quote! { u32 }
    } else if len <= <u64>::max_value() as usize {
        quote! { u64 }
    } else {
        quote! { u128 }
    }
}

/// Calculates size expression for [`DataStruct`](syn::DataStruct)
pub fn for_struct(struct_data: &DataStruct) -> TokenStream {
    get_max_size_expr_for_fields(&struct_data.fields)
}

/// Calculates size expression for [`DataEnum`](syn::DataEnum)
pub fn for_enum(enum_data: &DataEnum) -> TokenStream {
    get_max_size_expr_for_punctuated_variant(&enum_data.variants)
}
