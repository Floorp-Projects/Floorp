extern crate proc_macro;
#[macro_use]
extern crate quote;
#[macro_use]
extern crate syn;

extern crate darling_core;

use proc_macro::TokenStream;

use darling_core::{codegen, options};

#[proc_macro_derive(FromMeta, attributes(darling))]
pub fn derive_from_meta(input: TokenStream) -> TokenStream {
    let ast = parse_macro_input!(input as syn::DeriveInput);

    let container = options::FromMetaOptions::new(&ast).unwrap();
    let trait_impl = codegen::FromMetaImpl::from(&container);
    let result = quote!(#trait_impl);

    result.into()
}

#[proc_macro_derive(FromMetaItem, attributes(darling))]
pub fn derive_from_meta_item(_input: TokenStream) -> TokenStream {
    panic!("darling::FromMetaItem has been replaced by darling::FromMeta");
}


#[proc_macro_derive(FromDeriveInput, attributes(darling))]
pub fn derive_from_input(input: TokenStream) -> TokenStream {
    let ast = parse_macro_input!(input as syn::DeriveInput);

    let container = options::FdiOptions::new(&ast).unwrap();
    let trait_impl = codegen::FromDeriveInputImpl::from(&container);
    let result = quote!(#trait_impl);

    result.into()
}

#[proc_macro_derive(FromField, attributes(darling))]
pub fn derive_field(input: TokenStream) -> TokenStream {
    let ast = parse_macro_input!(input as syn::DeriveInput);

    let fdic = options::FromFieldOptions::new(&ast).unwrap();
    let trait_impl = codegen::FromFieldImpl::from(&fdic);
    let result = quote!(#trait_impl);

    result.into()
}

#[proc_macro_derive(FromTypeParam, attributes(darling))]
pub fn derive_type_param(input: TokenStream) -> TokenStream {
    let ast = parse_macro_input!(input as syn::DeriveInput);

    let fdic = options::FromTypeParamOptions::new(&ast).unwrap();
    let trait_impl = codegen::FromTypeParamImpl::from(&fdic);
    let result = quote!(#trait_impl);

    result.into()
}

#[proc_macro_derive(FromVariant, attributes(darling))]
pub fn derive_variant(input: TokenStream) -> TokenStream {
    let ast = parse_macro_input!(input as syn::DeriveInput);

    let fdic = options::FromVariantOptions::new(&ast).unwrap();
    let trait_impl = codegen::FromVariantImpl::from(&fdic);
    let result = quote!(#trait_impl);

    result.into()
}
