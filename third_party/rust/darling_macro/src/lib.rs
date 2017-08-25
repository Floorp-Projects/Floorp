extern crate proc_macro;
#[macro_use]
extern crate quote;
extern crate syn;

extern crate darling_core;

use proc_macro::TokenStream;
use syn::parse_derive_input;

use darling_core::{options, codegen};

#[proc_macro_derive(FromMetaItem, attributes(darling))]
pub fn derive_from_meta_item(input: TokenStream) -> TokenStream {
    let ast = parse_derive_input(&input.to_string()).unwrap();
    
    let container = options::FmiOptions::new(&ast).unwrap();
    let trait_impl = codegen::FmiImpl::from(&container);
    let result = quote!(#trait_impl);

    result.parse().expect(&format!("Couldn't parse `{}` to tokens", result))
}

#[proc_macro_derive(FromDeriveInput, attributes(darling))]
pub fn derive_from_input(input: TokenStream) -> TokenStream {
    let ast = parse_derive_input(&input.to_string()).unwrap();
    
    let container = options::FdiOptions::new(&ast).unwrap();
    let trait_impl = codegen::FromDeriveInputImpl::from(&container);
    let result = quote!(#trait_impl);

    result.parse().expect(&format!("Couldn't parse `{}` to tokens", result))
}

#[proc_macro_derive(FromField, attributes(darling))]
pub fn derive_field(input: TokenStream) -> TokenStream {
    let ast = parse_derive_input(&input.to_string()).unwrap();
    
    let fdic = options::FromFieldOptions::new(&ast).unwrap();
    let trait_impl = codegen::FromFieldImpl::from(&fdic);
    let result = quote!(#trait_impl);

    result.parse().expect(&format!("Couldn't parse `{}` to tokens", result))
}

#[proc_macro_derive(FromVariant, attributes(darling))]
pub fn derive_variant(input: TokenStream) -> TokenStream {
    let ast = parse_derive_input(&input.to_string()).unwrap();
    
    let fdic = options::FromVariantOptions::new(&ast).unwrap();
    let trait_impl = codegen::FromVariantImpl::from(&fdic);
    let result = quote!(#trait_impl);

    result.parse().expect(&format!("Couldn't parse `{}` to tokens", result))
}