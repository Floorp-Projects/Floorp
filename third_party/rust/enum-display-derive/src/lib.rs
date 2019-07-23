//! # derive `Display` for simple enums
//!
//! You can derive the `Display` trait for simple enums.
//!
//! Actually, the most complex enum definition that this crate supports is like this one:
//!
//! ```rust,ignore
//! #[derive(Display)]
//! pub enum FooBar {
//!     Foo,
//!     Bar(),
//!     FooBar(i32), // some wrapped type which implements Display
//! }
//! ```
//!
//! The above code will be expanded (roughly, without the enum definition) into this code:
//!
//! ```rust,ignore
//! impl Display for FooBar {
//!     fn fmt(&self, f: &mut ::std::fmt::Formatter) -> Result<(), ::std::fmt::Error> {
//!         match *self {
//!             FooBar::Foo => f.write_str("Foo"),
//!             FooBar::Bar => f.write_str("Bar()"),
//!             FooBar::FooBar(ref inner) => ::std::fmt::Display::fmt(inner, f),
//!         }
//!     }
//! }
//! ```
//!
//! ## Examples
//!
//! ```rust,ignore
//! #[macro_use]
//! extern crate enum_display_derive;
//!
//! use std::fmt::Display;
//!
//! #[derive(Display)]
//! enum FizzBuzz {
//!    Fizz,
//!    Buzz,
//!    FizzBuzz,
//!    Number(u64),
//! }
//!
//! fn fb(i: u64) {
//!    match (i % 3, i % 5) {
//!        (0, 0) => FizzBuzz::FizzBuzz,
//!        (0, _) => FizzBuzz::Fizz,
//!        (_, 0) => FizzBuzz::Buzz,
//!        (_, _) => FizzBuzz::Number(i),
//!    }
//! }
//!
//! fn main() {
//!    for i in 0..100 {
//!        println!("{}", fb(i));
//!    }
//! }
//! ```
//!

extern crate proc_macro;
extern crate syn;
#[macro_use]
extern crate quote;

use proc_macro::TokenStream;

#[proc_macro_derive(Display)]
#[doc(hidden)]
pub fn display(input: TokenStream) -> TokenStream {
    // Construct a string representation of the type definition
    let s = input.to_string();

    // Parse the string representation
    let ast = syn::parse_macro_input(&s).unwrap();

    let gen = match ast.body {
        syn::Body::Enum(ref variants) => {
            let name = &ast.ident;
            impl_display(name, variants)
        }
        _ => panic!("#[derive(Display)] works only on enums"),
    };

    gen.parse().unwrap()
}

fn impl_display(name: &syn::Ident, variants: &[syn::Variant]) -> quote::Tokens {
    let variants = variants.iter()
        .map(|variant| impl_display_for_variant(name, variant));

    quote! {
        impl Display for #name {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> Result<(), ::std::fmt::Error> {
                match *self {
                    #(#variants)*
                }
            }
        }
    }
}

fn impl_display_for_variant(name: &syn::Ident, variant: &syn::Variant) -> quote::Tokens {
    let id = &variant.ident;
    match variant.data {
        syn::VariantData::Unit => {
            quote! {
                #name::#id => {
                    f.write_str(stringify!(#id))
                }
            }
        }
        syn::VariantData::Tuple(ref fields) => {
            match fields.len() {
                0 => {
                    quote! {
                        #name::#id() => {
                            f.write_str(stringify!(#id))?;
                            f.write_str("()")
                        }
                    }
                }
                1 => {
                    quote! {
                        #name::#id(ref inner) => {
                            ::std::fmt::Display::fmt(inner, f)
                        }
                    }
                }
                _ => {
                    panic!("#[derive(Display)] does not support tuple variants with more than one \
                            fields")
                }
            }
        }
        _ => panic!("#[derive(Display)] works only with unit and tuple variants"),
    }
}
