// Copyright 2012-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![crate_type = "proc-macro"]
#![doc(html_root_url = "https://docs.rs/num-derive/0.2")]
#![recursion_limit = "512"]

//! Procedural macros to derive numeric traits in Rust.
//!
//! ## Usage
//!
//! Add this to your `Cargo.toml`:
//!
//! ```toml
//! [dependencies]
//! num-traits = "0.2"
//! num-derive = "0.2"
//! ```
//!
//! Then you can derive traits on your own types:
//!
//! ```rust
//! #[macro_use]
//! extern crate num_derive;
//!
//! #[derive(FromPrimitive, ToPrimitive)]
//! enum Color {
//!     Red,
//!     Blue,
//!     Green,
//! }
//! # fn main() {}
//! ```

extern crate proc_macro;

extern crate proc_macro2;
#[macro_use]
extern crate quote;
extern crate syn;

use proc_macro::TokenStream;
use proc_macro2::Span;

use syn::{Data, Fields, Ident};

// Within `exp`, you can bring things into scope with `extern crate`.
//
// We don't want to assume that `num_traits::` is in scope - the user may have imported it under a
// different name, or may have imported it in a non-toplevel module (common when putting impls
// behind a feature gate).
//
// Solution: let's just generate `extern crate num_traits as _num_traits` and then refer to
// `_num_traits` in the derived code.  However, macros are not allowed to produce `extern crate`
// statements at the toplevel.
//
// Solution: let's generate `mod _impl_foo` and import num_traits within that.  However, now we
// lose access to private members of the surrounding module.  This is a problem if, for example,
// we're deriving for a newtype, where the inner type is defined in the same module, but not
// exported.
//
// Solution: use the dummy const trick.  For some reason, `extern crate` statements are allowed
// here, but everything from the surrounding module is in scope.  This trick is taken from serde.
fn dummy_const_trick<T: quote::ToTokens>(
    trait_: &str,
    name: &proc_macro2::Ident,
    exp: T,
) -> proc_macro2::TokenStream {
    let dummy_const = Ident::new(
        &format!("_IMPL_NUM_{}_FOR_{}", trait_, unraw(name)),
        Span::call_site(),
    );
    quote! {
        #[allow(non_upper_case_globals, unused_attributes, unused_qualifications)]
        const #dummy_const: () = {
            #[allow(unknown_lints)]
            #[cfg_attr(feature = "cargo-clippy", allow(useless_attribute))]
            #[allow(rust_2018_idioms)]
            extern crate num_traits as _num_traits;
            #exp
        };
    }
}

#[allow(deprecated)]
fn unraw(ident: &proc_macro2::Ident) -> String {
    // str::trim_start_matches was added in 1.30, trim_left_matches deprecated
    // in 1.33. We currently support rustc back to 1.15 so we need to continue
    // to use the deprecated one.
    ident.to_string().trim_left_matches("r#").to_owned()
}

// If `data` is a newtype, return the type it's wrapping.
fn newtype_inner(data: &syn::Data) -> Option<syn::Type> {
    match *data {
        Data::Struct(ref s) => {
            match s.fields {
                Fields::Unnamed(ref fs) => {
                    if fs.unnamed.len() == 1 {
                        Some(fs.unnamed[0].ty.clone())
                    } else {
                        None
                    }
                }
                Fields::Named(ref fs) => {
                    if fs.named.len() == 1 {
                        panic!("num-derive doesn't know how to handle newtypes with named fields yet. \
                           Please use a tuple-style newtype, or submit a PR!");
                    }
                    None
                }
                _ => None,
            }
        }
        _ => None,
    }
}

/// Derives [`num_traits::FromPrimitive`][from] for simple enums and newtypes.
///
/// [from]: https://docs.rs/num-traits/0.2/num_traits/cast/trait.FromPrimitive.html
///
/// # Examples
///
/// Simple enums can be derived:
///
/// ```rust
/// # #[macro_use]
/// # extern crate num_derive;
///
/// #[derive(FromPrimitive)]
/// enum Color {
///     Red,
///     Blue,
///     Green = 42,
/// }
/// # fn main() {}
/// ```
///
/// Enums that contain data are not allowed:
///
/// ```compile_fail
/// # #[macro_use]
/// # extern crate num_derive;
///
/// #[derive(FromPrimitive)]
/// enum Color {
///     Rgb(u8, u8, u8),
///     Hsv(u8, u8, u8),
/// }
/// # fn main() {}
/// ```
///
/// Structs are not allowed:
///
/// ```compile_fail
/// # #[macro_use]
/// # extern crate num_derive;
/// #[derive(FromPrimitive)]
/// struct Color {
///     r: u8,
///     g: u8,
///     b: u8,
/// }
/// # fn main() {}
/// ```
#[proc_macro_derive(FromPrimitive)]
pub fn from_primitive(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;

    let impl_ = if let Some(inner_ty) = newtype_inner(&ast.data) {
        let i128_fns = if cfg!(has_i128) {
            quote! {
                fn from_i128(n: i128) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_i128(n).map(#name)
                }
                fn from_u128(n: u128) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_u128(n).map(#name)
                }
            }
        } else {
            quote! {}
        };

        quote! {
            impl _num_traits::FromPrimitive for #name {
                fn from_i64(n: i64) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_i64(n).map(#name)
                }
                fn from_u64(n: u64) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_u64(n).map(#name)
                }
                fn from_isize(n: isize) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_isize(n).map(#name)
                }
                fn from_i8(n: i8) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_i8(n).map(#name)
                }
                fn from_i16(n: i16) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_i16(n).map(#name)
                }
                fn from_i32(n: i32) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_i32(n).map(#name)
                }
                fn from_usize(n: usize) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_usize(n).map(#name)
                }
                fn from_u8(n: u8) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_u8(n).map(#name)
                }
                fn from_u16(n: u16) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_u16(n).map(#name)
                }
                fn from_u32(n: u32) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_u32(n).map(#name)
                }
                fn from_f32(n: f32) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_f32(n).map(#name)
                }
                fn from_f64(n: f64) -> Option<Self> {
                    <#inner_ty as _num_traits::FromPrimitive>::from_f64(n).map(#name)
                }
                #i128_fns
            }
        }
    } else {
        let variants = match ast.data {
            Data::Enum(ref data_enum) => &data_enum.variants,
            _ => panic!(
                "`FromPrimitive` can be applied only to enums and newtypes, {} is neither",
                name
            ),
        };

        let from_i64_var = quote! { n };
        let clauses: Vec<_> = variants
            .iter()
            .map(|variant| {
                let ident = &variant.ident;
                match variant.fields {
                    Fields::Unit => (),
                    _ => panic!(
                        "`FromPrimitive` can be applied only to unitary enums and newtypes, \
                         {}::{} is either struct or tuple",
                        name, ident
                    ),
                }

                quote! {
                    if #from_i64_var == #name::#ident as i64 {
                        Some(#name::#ident)
                    }
                }
            })
            .collect();

        let from_i64_var = if clauses.is_empty() {
            quote!(_)
        } else {
            from_i64_var
        };

        quote! {
            impl _num_traits::FromPrimitive for #name {
                #[allow(trivial_numeric_casts)]
                fn from_i64(#from_i64_var: i64) -> Option<Self> {
                    #(#clauses else)* {
                        None
                    }
                }

                fn from_u64(n: u64) -> Option<Self> {
                    Self::from_i64(n as i64)
                }
            }
        }
    };

    dummy_const_trick("FromPrimitive", &name, impl_).into()
}

/// Derives [`num_traits::ToPrimitive`][to] for simple enums and newtypes.
///
/// [to]: https://docs.rs/num-traits/0.2/num_traits/cast/trait.ToPrimitive.html
///
/// # Examples
///
/// Simple enums can be derived:
///
/// ```rust
/// # #[macro_use]
/// # extern crate num_derive;
///
/// #[derive(ToPrimitive)]
/// enum Color {
///     Red,
///     Blue,
///     Green = 42,
/// }
/// # fn main() {}
/// ```
///
/// Enums that contain data are not allowed:
///
/// ```compile_fail
/// # #[macro_use]
/// # extern crate num_derive;
///
/// #[derive(ToPrimitive)]
/// enum Color {
///     Rgb(u8, u8, u8),
///     Hsv(u8, u8, u8),
/// }
/// # fn main() {}
/// ```
///
/// Structs are not allowed:
///
/// ```compile_fail
/// # #[macro_use]
/// # extern crate num_derive;
/// #[derive(ToPrimitive)]
/// struct Color {
///     r: u8,
///     g: u8,
///     b: u8,
/// }
/// # fn main() {}
/// ```
#[proc_macro_derive(ToPrimitive)]
pub fn to_primitive(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;

    let impl_ = if let Some(inner_ty) = newtype_inner(&ast.data) {
        let i128_fns = if cfg!(has_i128) {
            quote! {
                fn to_i128(&self) -> Option<i128> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_i128(&self.0)
                }
                fn to_u128(&self) -> Option<u128> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_u128(&self.0)
                }
            }
        } else {
            quote! {}
        };

        quote! {
            impl _num_traits::ToPrimitive for #name {
                fn to_i64(&self) -> Option<i64> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_i64(&self.0)
                }
                fn to_u64(&self) -> Option<u64> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_u64(&self.0)
                }
                fn to_isize(&self) -> Option<isize> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_isize(&self.0)
                }
                fn to_i8(&self) -> Option<i8> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_i8(&self.0)
                }
                fn to_i16(&self) -> Option<i16> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_i16(&self.0)
                }
                fn to_i32(&self) -> Option<i32> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_i32(&self.0)
                }
                fn to_usize(&self) -> Option<usize> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_usize(&self.0)
                }
                fn to_u8(&self) -> Option<u8> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_u8(&self.0)
                }
                fn to_u16(&self) -> Option<u16> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_u16(&self.0)
                }
                fn to_u32(&self) -> Option<u32> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_u32(&self.0)
                }
                fn to_f32(&self) -> Option<f32> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_f32(&self.0)
                }
                fn to_f64(&self) -> Option<f64> {
                    <#inner_ty as _num_traits::ToPrimitive>::to_f64(&self.0)
                }
                #i128_fns
            }
        }
    } else {
        let variants = match ast.data {
            Data::Enum(ref data_enum) => &data_enum.variants,
            _ => panic!(
                "`ToPrimitive` can be applied only to enums and newtypes, {} is neither",
                name
            ),
        };

        let variants: Vec<_> = variants
            .iter()
            .map(|variant| {
                let ident = &variant.ident;
                match variant.fields {
                    Fields::Unit => (),
                    _ => {
                        panic!("`ToPrimitive` can be applied only to unitary enums and newtypes, {}::{} is either struct or tuple", name, ident)
                    },
                }

                // NB: We have to check each variant individually, because we'll only have `&self`
                // for the input.  We can't move from that, and it might not be `Clone` or `Copy`.
                // (Otherwise we could just do `*self as i64` without a `match` at all.)
                quote!(#name::#ident => #name::#ident as i64)
            })
            .collect();

        let match_expr = if variants.is_empty() {
            // No variants found, so do not use Some to not to trigger `unreachable_code` lint
            quote! {
                match *self {}
            }
        } else {
            quote! {
                Some(match *self {
                    #(#variants,)*
                })
            }
        };

        quote! {
            impl _num_traits::ToPrimitive for #name {
                #[allow(trivial_numeric_casts)]
                fn to_i64(&self) -> Option<i64> {
                    #match_expr
                }

                fn to_u64(&self) -> Option<u64> {
                    self.to_i64().map(|x| x as u64)
                }
            }
        }
    };

    dummy_const_trick("ToPrimitive", &name, impl_).into()
}

#[allow(renamed_and_removed_lints)]
#[cfg_attr(feature = "cargo-clippy", allow(const_static_lifetime))]
const NEWTYPE_ONLY: &'static str = "This trait can only be derived for newtypes";

/// Derives [`num_traits::NumOps`][num_ops] for newtypes.  The inner type must already implement
/// `NumOps`.
///
/// [num_ops]: https://docs.rs/num-traits/0.2/num_traits/trait.NumOps.html
///
/// Note that, since `NumOps` is really a trait alias for `Add + Sub + Mul + Div + Rem`, this macro
/// generates impls for _those_ traits.  Furthermore, in all generated impls, `RHS=Self` and
/// `Output=Self`.
#[proc_macro_derive(NumOps)]
pub fn num_ops(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;
    let inner_ty = newtype_inner(&ast.data).expect(NEWTYPE_ONLY);
    dummy_const_trick(
        "NumOps",
        &name,
        quote! {
            impl ::std::ops::Add for #name {
                type Output = Self;
                fn add(self, other: Self) -> Self {
                    #name(<#inner_ty as ::std::ops::Add>::add(self.0, other.0))
                }
            }
            impl ::std::ops::Sub for #name {
                type Output = Self;
                fn sub(self, other: Self) -> Self {
                    #name(<#inner_ty as ::std::ops::Sub>::sub(self.0, other.0))
                }
            }
            impl ::std::ops::Mul for #name {
                type Output = Self;
                fn mul(self, other: Self) -> Self {
                    #name(<#inner_ty as ::std::ops::Mul>::mul(self.0, other.0))
                }
            }
            impl ::std::ops::Div for #name {
                type Output = Self;
                fn div(self, other: Self) -> Self {
                    #name(<#inner_ty as ::std::ops::Div>::div(self.0, other.0))
                }
            }
            impl ::std::ops::Rem for #name {
                type Output = Self;
                fn rem(self, other: Self) -> Self {
                    #name(<#inner_ty as ::std::ops::Rem>::rem(self.0, other.0))
                }
            }
        },
    )
    .into()
}

/// Derives [`num_traits::NumCast`][num_cast] for newtypes.  The inner type must already implement
/// `NumCast`.
///
/// [num_cast]: https://docs.rs/num-traits/0.2/num_traits/cast/trait.NumCast.html
#[proc_macro_derive(NumCast)]
pub fn num_cast(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;
    let inner_ty = newtype_inner(&ast.data).expect(NEWTYPE_ONLY);
    dummy_const_trick(
        "NumCast",
        &name,
        quote! {
            impl _num_traits::NumCast for #name {
                fn from<T: _num_traits::ToPrimitive>(n: T) -> Option<Self> {
                    <#inner_ty as _num_traits::NumCast>::from(n).map(#name)
                }
            }
        },
    )
    .into()
}

/// Derives [`num_traits::Zero`][zero] for newtypes.  The inner type must already implement `Zero`.
///
/// [zero]: https://docs.rs/num-traits/0.2/num_traits/identities/trait.Zero.html
#[proc_macro_derive(Zero)]
pub fn zero(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;
    let inner_ty = newtype_inner(&ast.data).expect(NEWTYPE_ONLY);
    dummy_const_trick(
        "Zero",
        &name,
        quote! {
            impl _num_traits::Zero for #name {
                fn zero() -> Self {
                    #name(<#inner_ty as _num_traits::Zero>::zero())
                }
                fn is_zero(&self) -> bool {
                    <#inner_ty as _num_traits::Zero>::is_zero(&self.0)
                }
            }
        },
    )
    .into()
}

/// Derives [`num_traits::One`][one] for newtypes.  The inner type must already implement `One`.
///
/// [one]: https://docs.rs/num-traits/0.2/num_traits/identities/trait.One.html
#[proc_macro_derive(One)]
pub fn one(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;
    let inner_ty = newtype_inner(&ast.data).expect(NEWTYPE_ONLY);
    dummy_const_trick(
        "One",
        &name,
        quote! {
            impl _num_traits::One for #name {
                fn one() -> Self {
                    #name(<#inner_ty as _num_traits::One>::one())
                }
                fn is_one(&self) -> bool {
                    <#inner_ty as _num_traits::One>::is_one(&self.0)
                }
            }
        },
    )
    .into()
}

/// Derives [`num_traits::Num`][num] for newtypes.  The inner type must already implement `Num`.
///
/// [num]: https://docs.rs/num-traits/0.2/num_traits/trait.Num.html
#[proc_macro_derive(Num)]
pub fn num(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;
    let inner_ty = newtype_inner(&ast.data).expect(NEWTYPE_ONLY);
    dummy_const_trick(
        "Num",
        &name,
        quote! {
            impl _num_traits::Num for #name {
                type FromStrRadixErr = <#inner_ty as _num_traits::Num>::FromStrRadixErr;
                fn from_str_radix(s: &str, radix: u32) -> Result<Self, Self::FromStrRadixErr> {
                    <#inner_ty as _num_traits::Num>::from_str_radix(s, radix).map(#name)
                }
            }
        },
    )
    .into()
}

/// Derives [`num_traits::Float`][float] for newtypes.  The inner type must already implement
/// `Float`.
///
/// [float]: https://docs.rs/num-traits/0.2/num_traits/float/trait.Float.html
#[proc_macro_derive(Float)]
pub fn float(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;
    let inner_ty = newtype_inner(&ast.data).expect(NEWTYPE_ONLY);
    dummy_const_trick(
        "Float",
        &name,
        quote! {
            impl _num_traits::Float for #name {
                fn nan() -> Self {
                    #name(<#inner_ty as _num_traits::Float>::nan())
                }
                fn infinity() -> Self {
                    #name(<#inner_ty as _num_traits::Float>::infinity())
                }
                fn neg_infinity() -> Self {
                    #name(<#inner_ty as _num_traits::Float>::neg_infinity())
                }
                fn neg_zero() -> Self {
                    #name(<#inner_ty as _num_traits::Float>::neg_zero())
                }
                fn min_value() -> Self {
                    #name(<#inner_ty as _num_traits::Float>::min_value())
                }
                fn min_positive_value() -> Self {
                    #name(<#inner_ty as _num_traits::Float>::min_positive_value())
                }
                fn max_value() -> Self {
                    #name(<#inner_ty as _num_traits::Float>::max_value())
                }
                fn is_nan(self) -> bool {
                    <#inner_ty as _num_traits::Float>::is_nan(self.0)
                }
                fn is_infinite(self) -> bool {
                    <#inner_ty as _num_traits::Float>::is_infinite(self.0)
                }
                fn is_finite(self) -> bool {
                    <#inner_ty as _num_traits::Float>::is_finite(self.0)
                }
                fn is_normal(self) -> bool {
                    <#inner_ty as _num_traits::Float>::is_normal(self.0)
                }
                fn classify(self) -> ::std::num::FpCategory {
                    <#inner_ty as _num_traits::Float>::classify(self.0)
                }
                fn floor(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::floor(self.0))
                }
                fn ceil(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::ceil(self.0))
                }
                fn round(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::round(self.0))
                }
                fn trunc(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::trunc(self.0))
                }
                fn fract(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::fract(self.0))
                }
                fn abs(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::abs(self.0))
                }
                fn signum(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::signum(self.0))
                }
                fn is_sign_positive(self) -> bool {
                    <#inner_ty as _num_traits::Float>::is_sign_positive(self.0)
                }
                fn is_sign_negative(self) -> bool {
                    <#inner_ty as _num_traits::Float>::is_sign_negative(self.0)
                }
                fn mul_add(self, a: Self, b: Self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::mul_add(self.0, a.0, b.0))
                }
                fn recip(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::recip(self.0))
                }
                fn powi(self, n: i32) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::powi(self.0, n))
                }
                fn powf(self, n: Self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::powf(self.0, n.0))
                }
                fn sqrt(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::sqrt(self.0))
                }
                fn exp(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::exp(self.0))
                }
                fn exp2(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::exp2(self.0))
                }
                fn ln(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::ln(self.0))
                }
                fn log(self, base: Self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::log(self.0, base.0))
                }
                fn log2(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::log2(self.0))
                }
                fn log10(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::log10(self.0))
                }
                fn max(self, other: Self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::max(self.0, other.0))
                }
                fn min(self, other: Self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::min(self.0, other.0))
                }
                fn abs_sub(self, other: Self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::abs_sub(self.0, other.0))
                }
                fn cbrt(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::cbrt(self.0))
                }
                fn hypot(self, other: Self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::hypot(self.0, other.0))
                }
                fn sin(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::sin(self.0))
                }
                fn cos(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::cos(self.0))
                }
                fn tan(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::tan(self.0))
                }
                fn asin(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::asin(self.0))
                }
                fn acos(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::acos(self.0))
                }
                fn atan(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::atan(self.0))
                }
                fn atan2(self, other: Self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::atan2(self.0, other.0))
                }
                fn sin_cos(self) -> (Self, Self) {
                    let (x, y) = <#inner_ty as _num_traits::Float>::sin_cos(self.0);
                    (#name(x), #name(y))
                }
                fn exp_m1(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::exp_m1(self.0))
                }
                fn ln_1p(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::ln_1p(self.0))
                }
                fn sinh(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::sinh(self.0))
                }
                fn cosh(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::cosh(self.0))
                }
                fn tanh(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::tanh(self.0))
                }
                fn asinh(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::asinh(self.0))
                }
                fn acosh(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::acosh(self.0))
                }
                fn atanh(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::atanh(self.0))
                }
                fn integer_decode(self) -> (u64, i16, i8) {
                    <#inner_ty as _num_traits::Float>::integer_decode(self.0)
                }
                fn epsilon() -> Self {
                    #name(<#inner_ty as _num_traits::Float>::epsilon())
                }
                fn to_degrees(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::to_degrees(self.0))
                }
                fn to_radians(self) -> Self {
                    #name(<#inner_ty as _num_traits::Float>::to_radians(self.0))
                }
            }
        },
    )
    .into()
}
