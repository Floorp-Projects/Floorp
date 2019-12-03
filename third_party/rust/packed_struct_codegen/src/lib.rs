#![recursion_limit = "192"]

extern crate proc_macro;
extern crate packed_struct;


extern crate syn;
#[macro_use]
extern crate quote;

use proc_macro::TokenStream;


mod pack;
mod pack_codegen;
mod pack_codegen_docs;
mod pack_parse;
mod pack_parse_attributes;

mod primitive_enum;
mod common;
mod utils;

#[proc_macro_derive(PackedStruct, attributes(packed_struct, packed_field))]
pub fn derive_packable_bytes(input: TokenStream) -> TokenStream {
    let ast = syn::parse_macro_input(&input.to_string()).unwrap();
    let parsed = pack_parse::parse_struct(&ast);
    let pack = pack_codegen::derive_pack(&parsed);

    quote!(#pack).to_string().parse().unwrap()        
}

#[proc_macro_derive(PrimitiveEnum)]
pub fn derive_primitive_detect(input: TokenStream) -> TokenStream {
    derive_primitive(input, None)
}

#[proc_macro_derive(PrimitiveEnum_u8)]
pub fn derive_primitive_u8(input: TokenStream) -> TokenStream {
    derive_primitive(input, Some(syn::parse_type("u8").unwrap()))
}

#[proc_macro_derive(PrimitiveEnum_u16)]
pub fn derive_primitive_u16(input: TokenStream) -> TokenStream {
    derive_primitive(input, Some(syn::parse_type("u16").unwrap()))
}

#[proc_macro_derive(PrimitiveEnum_u32)]
pub fn derive_primitive_u32(input: TokenStream) -> TokenStream {
    derive_primitive(input, Some(syn::parse_type("u32").unwrap()))
}

#[proc_macro_derive(PrimitiveEnum_u64)]
pub fn derive_primitive_u64(input: TokenStream) -> TokenStream {
    derive_primitive(input, Some(syn::parse_type("u64").unwrap()))
}

#[proc_macro_derive(PrimitiveEnum_i8)]
pub fn derive_primitive_i8(input: TokenStream) -> TokenStream {
    derive_primitive(input, Some(syn::parse_type("i8").unwrap()))
}

#[proc_macro_derive(PrimitiveEnum_i16)]
pub fn derive_primitive_i16(input: TokenStream) -> TokenStream {
    derive_primitive(input, Some(syn::parse_type("i16").unwrap()))
}

#[proc_macro_derive(PrimitiveEnum_i32)]
pub fn derive_primitive_i32(input: TokenStream) -> TokenStream {
    derive_primitive(input, Some(syn::parse_type("i32").unwrap()))
}

#[proc_macro_derive(PrimitiveEnum_i64)]
pub fn derive_primitive_i64(input: TokenStream) -> TokenStream {
    derive_primitive(input, Some(syn::parse_type("i64").unwrap()))
}

fn derive_primitive(input: TokenStream, ty: Option<syn::Ty>) -> TokenStream {
    let input = match syn::parse_derive_input(&input.to_string()) {
        Ok(tokens) => tokens,
        Err(msg) => panic!("Internal error from `syn`: {}", msg),
    };

    let prim = primitive_enum::derive(&input, ty);

    quote!(#prim).to_string().parse().unwrap()
}
