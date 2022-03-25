extern crate proc_macro;

use proc_macro::TokenStream;

use proc_macro_hack::proc_macro_hack;
use quote::quote;
use syn::{parse_macro_input, LitStr};

use unic_langid_impl::{subtags, LanguageIdentifier};

#[proc_macro_hack]
pub fn lang(input: TokenStream) -> TokenStream {
    let id = parse_macro_input!(input as LitStr);
    let parsed: subtags::Language = id.value().parse().expect("Malformed Language Subtag");

    let lang: Option<u64> = parsed.into();
    let lang = if let Some(lang) = lang {
        quote!(unsafe { $crate::subtags::Language::from_raw_unchecked(#lang) })
    } else {
        quote!(None)
    };

    TokenStream::from(quote! {
        #lang
    })
}

#[proc_macro_hack]
pub fn script(input: TokenStream) -> TokenStream {
    let id = parse_macro_input!(input as LitStr);
    let parsed: subtags::Script = id.value().parse().expect("Malformed Script Subtag");

    let script: u32 = parsed.into();

    TokenStream::from(quote! {
        unsafe { $crate::subtags::Script::from_raw_unchecked(#script) }
    })
}

#[proc_macro_hack]
pub fn region(input: TokenStream) -> TokenStream {
    let id = parse_macro_input!(input as LitStr);
    let parsed: subtags::Region = id.value().parse().expect("Malformed Region Subtag");

    let region: u32 = parsed.into();

    TokenStream::from(quote! {
        unsafe { $crate::subtags::Region::from_raw_unchecked(#region) }
    })
}

#[proc_macro_hack]
pub fn variant_fn(input: TokenStream) -> TokenStream {
    let id = parse_macro_input!(input as LitStr);
    let parsed: subtags::Variant = id.value().parse().expect("Malformed Variant Subtag");

    let variant: u64 = parsed.into();

    TokenStream::from(quote! {
        unsafe { $crate::subtags::Variant::from_raw_unchecked(#variant) }
    })
}

#[proc_macro_hack]
pub fn langid(input: TokenStream) -> TokenStream {
    let id = parse_macro_input!(input as LitStr);
    let parsed: LanguageIdentifier = id.value().parse().expect("Malformed Language Identifier");

    let (lang, script, region, variants) = parsed.into_parts();

    let lang: Option<u64> = lang.into();
    let lang = if let Some(lang) = lang {
        quote!(unsafe { $crate::subtags::Language::from_raw_unchecked(#lang) })
    } else {
        quote!($crate::subtags::Language::default())
    };

    let script = if let Some(script) = script {
        let script: u32 = script.into();
        quote!(Some(unsafe { $crate::subtags::Script::from_raw_unchecked(#script) }))
    } else {
        quote!(None)
    };

    let region = if let Some(region) = region {
        let region: u32 = region.into();
        quote!(Some(unsafe { $crate::subtags::Region::from_raw_unchecked(#region) }))
    } else {
        quote!(None)
    };

    let variants = if !variants.is_empty() {
        let v: Vec<_> = variants
            .iter()
            .map(|v| {
                let variant: u64 = v.into();
                quote!(unsafe { $crate::subtags::Variant::from_raw_unchecked(#variant) })
            })
            .collect();
        quote!(Some(Box::new([#(#v,)*])))
    } else {
        quote!(None)
    };

    TokenStream::from(quote! {
        unsafe { $crate::LanguageIdentifier::from_raw_parts_unchecked(#lang, #script, #region, #variants) }
    })
}
