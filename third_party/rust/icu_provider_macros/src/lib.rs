// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/docs/process/boilerplate.md#library-annotations
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        // Panics are OK in proc macros
        // clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        missing_debug_implementations,
    )
)]
#![warn(missing_docs)]

//! Proc macros for the ICU4X data provider.
//!
//! These macros are re-exported from `icu_provider`.

extern crate proc_macro;
use proc_macro::TokenStream;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::parse_macro_input;
use syn::spanned::Spanned;
use syn::AttributeArgs;
use syn::DeriveInput;
use syn::Lit;
use syn::Meta;
use syn::NestedMeta;

#[cfg(test)]
mod tests;

#[proc_macro_attribute]
/// The `#[data_struct]` attribute should be applied to all types intended
/// for use in a `DataStruct`.
///
/// It does the following things:
///
/// - `Apply #[derive(Yokeable, ZeroFrom)]`. The `ZeroFrom` derive can
///    be customized with `#[zerofrom(clone)]` on non-ZeroFrom fields.
///
/// In addition, the attribute can be used to implement `DataMarker` and/or `KeyedDataMarker`
/// by adding symbols with optional key strings:
///
/// ```
/// use icu_provider::prelude::*;
/// use std::borrow::Cow;
///
/// #[icu_provider::data_struct(
///     FooV1Marker,
///     BarV1Marker = "demo/bar@1",
///     marker(
///         BazV1Marker,
///         "demo/baz@1",
///         fallback_by = "region",
///         extension_key = "ca"
///     )
/// )]
/// pub struct FooV1<'data> {
///     message: Cow<'data, str>,
/// };
///
/// // Note: FooV1Marker implements `DataMarker` but not `KeyedDataMarker`.
/// // The other two implement `KeyedDataMarker`.
///
/// assert_eq!(&*BarV1Marker::KEY.path(), "demo/bar@1");
/// assert_eq!(
///     BarV1Marker::KEY.metadata().fallback_priority,
///     icu_provider::FallbackPriority::Language
/// );
/// assert_eq!(BarV1Marker::KEY.metadata().extension_key, None);
///
/// assert_eq!(&*BazV1Marker::KEY.path(), "demo/baz@1");
/// assert_eq!(
///     BazV1Marker::KEY.metadata().fallback_priority,
///     icu_provider::FallbackPriority::Region
/// );
/// assert_eq!(
///     BazV1Marker::KEY.metadata().extension_key,
///     Some(icu::locid::extensions_unicode_key!("ca"))
/// );
/// ```
///
/// If the `#[databake(path = ...)]` attribute is present on the data struct, this will also
/// implement it on the markers.
pub fn data_struct(attr: TokenStream, item: TokenStream) -> TokenStream {
    TokenStream::from(data_struct_impl(
        parse_macro_input!(attr as AttributeArgs),
        parse_macro_input!(item as DeriveInput),
    ))
}

fn data_struct_impl(attr: AttributeArgs, input: DeriveInput) -> TokenStream2 {
    if input.generics.type_params().count() > 0 {
        return syn::Error::new(
            input.generics.span(),
            "#[data_struct] does not support type parameters",
        )
        .to_compile_error();
    }
    let lifetimes = input.generics.lifetimes().collect::<Vec<_>>();

    let name = &input.ident;

    let name_with_lt = if lifetimes.get(0).is_some() {
        quote!(#name<'static>)
    } else {
        quote!(#name)
    };

    if lifetimes.len() > 1 {
        return syn::Error::new(
            input.generics.span(),
            "#[data_struct] does not support more than one lifetime parameter",
        )
        .to_compile_error();
    }

    let bake_derive = input
        .attrs
        .iter()
        .find(|a| a.path.is_ident("databake"))
        .map(|a| {
            quote! {
                #[derive(databake::Bake)]
                #a
            }
        })
        .unwrap_or_else(|| quote! {});

    let mut result = TokenStream2::new();

    for single_attr in attr.into_iter() {
        let mut marker_name: Option<syn::Path> = None;
        let mut key_lit: Option<syn::LitStr> = None;
        let mut fallback_by: Option<syn::LitStr> = None;
        let mut extension_key: Option<syn::LitStr> = None;
        let mut fallback_supplement: Option<syn::LitStr> = None;

        match single_attr {
            NestedMeta::Meta(Meta::List(meta_list)) => {
                match meta_list.path.get_ident() {
                    Some(ident) if ident.to_string().as_str() == "marker" => (),
                    _ => panic!("Meta list must be `marker(...)`"),
                }
                for inner_meta in meta_list.nested.into_iter() {
                    match inner_meta {
                        NestedMeta::Meta(Meta::Path(path)) => {
                            marker_name = Some(path);
                        }
                        NestedMeta::Lit(Lit::Str(lit_str)) => {
                            key_lit = Some(lit_str);
                        }
                        NestedMeta::Meta(Meta::NameValue(name_value)) => {
                            let lit_str = match name_value.lit {
                                Lit::Str(lit_str) => lit_str,
                                _ => panic!("Values in marker() must be strings"),
                            };
                            let name_ident_str = match name_value.path.get_ident() {
                                Some(ident) => ident.to_string(),
                                None => panic!("Names in marker() must be identifiers"),
                            };
                            match name_ident_str.as_str() {
                                "fallback_by" => fallback_by = Some(lit_str),
                                "extension_key" => extension_key = Some(lit_str),
                                "fallback_supplement" => fallback_supplement = Some(lit_str),
                                _ => panic!("Invalid argument name in marker()"),
                            }
                        }
                        _ => panic!("Invalid argument in marker()"),
                    }
                }
            }
            NestedMeta::Meta(Meta::NameValue(name_value)) => {
                marker_name = Some(name_value.path);
                match name_value.lit {
                    syn::Lit::Str(lit_str) => key_lit = Some(lit_str),
                    _ => panic!("Key must be a string"),
                };
            }
            NestedMeta::Meta(Meta::Path(path)) => {
                marker_name = Some(path);
            }
            _ => {
                panic!("Invalid attribute to #[data_struct]")
            }
        }

        let marker_name = match marker_name {
            Some(path) => path,
            None => panic!("#[data_struct] arguments must include a marker name"),
        };

        let docs = if let Some(key_lit) = &key_lit {
            let fallback_by_docs_str = match &fallback_by {
                Some(fallback_by) => fallback_by.value(),
                None => "language (default)".to_string(),
            };
            let extension_key_docs_str = match &extension_key {
                Some(extension_key) => extension_key.value(),
                None => "none (default)".to_string(),
            };
            format!("Marker type for [`{}`]: \"{}\"\n\n- Fallback priority: {}\n- Extension keyword: {}", name, key_lit.value(), fallback_by_docs_str, extension_key_docs_str)
        } else {
            format!("Marker type for [`{name}`]")
        };

        result.extend(quote!(
            #[doc = #docs]
            #bake_derive
            pub struct #marker_name;
            impl icu_provider::DataMarker for #marker_name {
                type Yokeable = #name_with_lt;
            }
        ));

        if let Some(key_lit) = &key_lit {
            let key_str = key_lit.value();
            let fallback_by_expr = if let Some(fallback_by_lit) = fallback_by {
                match fallback_by_lit.value().as_str() {
                    "region" => quote! {icu_provider::FallbackPriority::Region},
                    "collation" => quote! {icu_provider::FallbackPriority::Collation},
                    "language" => quote! {icu_provider::FallbackPriority::Language},
                    _ => panic!("Invalid value for fallback_by"),
                }
            } else {
                quote! {icu_provider::FallbackPriority::const_default()}
            };
            let extension_key_expr = if let Some(extension_key_lit) = extension_key {
                quote! {Some(icu_provider::_internal::extensions_unicode_key!(#extension_key_lit))}
            } else {
                quote! {None}
            };
            let fallback_supplement_expr =
                if let Some(fallback_supplement_lit) = fallback_supplement {
                    match fallback_supplement_lit.value().as_str() {
                        "collation" => quote! {Some(icu_provider::FallbackSupplement::Collation)},
                        _ => panic!("Invalid value for fallback_supplement"),
                    }
                } else {
                    quote! {None}
                };
            result.extend(quote!(
                impl icu_provider::KeyedDataMarker for #marker_name {
                    const KEY: icu_provider::DataKey = icu_provider::data_key!(#key_str, icu_provider::DataKeyMetadata::construct_internal(
                        #fallback_by_expr,
                        #extension_key_expr,
                        #fallback_supplement_expr
                    ));
                }
            ));
        }
    }

    result.extend(quote!(
        #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
        #input
    ));

    result
}
