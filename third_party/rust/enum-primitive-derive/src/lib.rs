// Copyright (c) 2017 Doug Goldstein <cardoe@cardoe.com>

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// “Software”), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//! This crate provides a custom derive `Primitive` that helps people
//! providing native Rust bindings to C code by allowing a C-like `enum`
//! declaration to convert to its primitve values and back from them. You
//! can selectively include `num_traits::ToPrimitive` and
//! `num_traits::FromPrimitive` to get these features.
//!
//! # Example
//!
//! ```rust
//! use enum_primitive_derive::Primitive;
//! use num_traits::{FromPrimitive, ToPrimitive};
//!
//! #[derive(Debug, Eq, PartialEq, Primitive)]
//! enum Foo {
//!     Bar = 32,
//!     Dead = 42,
//!     Beef = 50,
//! }
//!
//! fn main() {
//!     assert_eq!(Foo::from_i32(32), Some(Foo::Bar));
//!     assert_eq!(Foo::from_i32(42), Some(Foo::Dead));
//!     assert_eq!(Foo::from_i64(50), Some(Foo::Beef));
//!     assert_eq!(Foo::from_isize(17), None);
//!
//!     let bar = Foo::Bar;
//!     assert_eq!(bar.to_i32(), Some(32));
//!
//!     let dead = Foo::Dead;
//!     assert_eq!(dead.to_isize(), Some(42));
//! }
//! ```
//!
//! # Complex Example
//!
//! ```rust
//! use enum_primitive_derive::Primitive;
//! use num_traits::{FromPrimitive, ToPrimitive};
//!
//! pub const ABC: ::std::os::raw::c_uint = 1;
//! pub const DEF: ::std::os::raw::c_uint = 2;
//! pub const GHI: ::std::os::raw::c_uint = 4;
//!
//! #[derive(Clone, Copy, Debug, Eq, PartialEq, Primitive)]
//! enum BindGenLike {
//!     ABC = ABC as isize,
//!     DEF = DEF as isize,
//!     GHI = GHI as isize,
//! }
//!
//! fn main() {
//!     assert_eq!(BindGenLike::from_isize(4), Some(BindGenLike::GHI));
//!     assert_eq!(BindGenLike::from_u32(2), Some(BindGenLike::DEF));
//!     assert_eq!(BindGenLike::from_u32(8), None);
//!
//!     let abc = BindGenLike::ABC;
//!     assert_eq!(abc.to_u32(), Some(1));
//! }
//! ```
//!
//! # TryFrom Example
//!
//! ```rust
//! use enum_primitive_derive::Primitive;
//! use core::convert::TryFrom;
//!
//! #[derive(Debug, Eq, PartialEq, Primitive)]
//! enum Foo {
//!     Bar = 32,
//!     Dead = 42,
//!     Beef = 50,
//! }
//!
//! fn main() {
//!     let bar = Foo::try_from(32);
//!     assert_eq!(bar, Ok(Foo::Bar));
//!
//!     let dead = Foo::try_from(42);
//!     assert_eq!(dead, Ok(Foo::Dead));
//!
//!     let unknown = Foo::try_from(12);
//!     assert!(unknown.is_err());
//! }
//! ```

extern crate proc_macro;

use proc_macro::TokenStream;

/// Provides implementation of `num_traits::ToPrimitive` and
/// `num_traits::FromPrimitive`
#[proc_macro_derive(Primitive)]
pub fn primitive(input: TokenStream) -> TokenStream {
    let ast = syn::parse_macro_input!(input as syn::DeriveInput);
    impl_primitive(&ast)
}

fn impl_primitive(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;

    // Check if derive(Primitive) was specified for a struct
    if let syn::Data::Enum(ref variant) = ast.data {
        let (var_u64, dis_u64): (Vec<_>, Vec<_>) = variant
            .variants
            .iter()
            .map(|v| {
                match v.fields {
                    syn::Fields::Unit => (),
                    _ => panic!("#[derive(Primitive) can only operate on C-like enums"),
                }
                if v.discriminant.is_none() {
                    panic!(
                        "#[derive(Primitive) requires C-like enums with \
                       discriminants for all enum variants"
                    );
                }

                let discrim = match v.discriminant.clone().map(|(_eq, expr)| expr).unwrap() {
                    syn::Expr::Cast(real) => *real.expr,
                    orig => orig,
                };
                (v.ident.clone(), discrim)
            })
            .unzip();

        // quote!{} needs this to be a vec since its in #( )*
        let enum_u64 = vec![name.clone(); variant.variants.len()];

        // can't reuse variables in quote!{} body
        let var_i64 = var_u64.clone();
        let dis_i64 = dis_u64.clone();
        let enum_i64 = enum_u64.clone();

        let to_name = name.clone();
        let to_enum_u64 = enum_u64.clone();
        let to_var_u64 = var_u64.clone();
        let to_dis_u64 = dis_u64.clone();

        let to_enum_i64 = enum_u64.clone();
        let to_var_i64 = var_u64.clone();
        let to_dis_i64 = dis_u64.clone();

        TokenStream::from(quote::quote! {
            impl ::num_traits::FromPrimitive for #name {
                fn from_u64(val: u64) -> Option<Self> {
                    match val as _ {
                        #( #dis_u64 => Some(#enum_u64::#var_u64), )*
                        _ => None,
                    }
                }

                fn from_i64(val: i64) -> Option<Self> {
                    match val as _ {
                        #( #dis_i64 => Some(#enum_i64::#var_i64), )*
                        _ => None,
                    }
                }
            }

            impl ::num_traits::ToPrimitive for #to_name {
                fn to_u64(&self) -> Option<u64> {
                    match *self {
                        #( #to_enum_u64::#to_var_u64 => Some(#to_dis_u64 as u64), )*
                    }
                }

                fn to_i64(&self) -> Option<i64> {
                    match *self {
                        #( #to_enum_i64::#to_var_i64 => Some(#to_dis_i64 as i64), )*
                    }
                }
            }

            impl ::core::convert::TryFrom<u64> for #to_name {
                type Error = &'static str;

                fn try_from(value: u64) -> ::core::result::Result<Self, Self::Error> {
                    use ::num_traits::FromPrimitive;

                    #to_name::from_u64(value).ok_or_else(|| "Unknown variant")
                }
            }

            impl ::core::convert::TryFrom<u32> for #to_name {
                type Error = &'static str;

                fn try_from(value: u32) -> ::core::result::Result<Self, Self::Error> {
                    use ::num_traits::FromPrimitive;

                    #to_name::from_u32(value).ok_or_else(|| "Unknown variant")
                }
            }

            impl ::core::convert::TryFrom<u16> for #to_name {
                type Error = &'static str;

                fn try_from(value: u16) -> ::core::result::Result<Self, Self::Error> {
                    use ::num_traits::FromPrimitive;

                    #to_name::from_u16(value).ok_or_else(|| "Unknown variant")
                }
            }

            impl ::core::convert::TryFrom<u8> for #to_name {
                type Error = &'static str;

                fn try_from(value: u8) -> ::core::result::Result<Self, Self::Error> {
                    use ::num_traits::FromPrimitive;

                    #to_name::from_u8(value).ok_or_else(|| "Unknown variant")
                }
            }

            impl ::core::convert::TryFrom<i64> for #name {
                type Error = &'static str;

                fn try_from(value: i64) -> ::core::result::Result<Self, Self::Error> {
                    use ::num_traits::FromPrimitive;

                    #to_name::from_i64(value).ok_or_else(|| "Unknown variant")
                }
            }

            impl ::core::convert::TryFrom<i32> for #name {
                type Error = &'static str;

                fn try_from(value: i32) -> ::core::result::Result<Self, Self::Error> {
                    use ::num_traits::FromPrimitive;

                    #to_name::from_i32(value).ok_or_else(|| "Unknown variant")
                }
            }

            impl ::core::convert::TryFrom<i16> for #name {
                type Error = &'static str;

                fn try_from(value: i16) -> ::core::result::Result<Self, Self::Error> {
                    use ::num_traits::FromPrimitive;

                    #to_name::from_i16(value).ok_or_else(|| "Unknown variant")
                }
            }

            impl ::core::convert::TryFrom<i8> for #name {
                type Error = &'static str;

                fn try_from(value: i8) -> ::core::result::Result<Self, Self::Error> {
                    use ::num_traits::FromPrimitive;

                    #to_name::from_i8(value).ok_or_else(|| "Unknown variant")
                }
            }
        })
    } else {
        panic!("#[derive(Primitive)] is only valid for C-like enums");
    }
}
