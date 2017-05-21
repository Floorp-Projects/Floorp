//! This crate provides helper methods for matching against enum variants, and
//! extracting bindings to each of the fields in the deriving Struct or Enum in
//! a generic way.
//!
//! If you are writing a `#[derive]` which needs to perform some operation on
//! every field, then you have come to the right place!
//!
//! # Example
//!
//! ```
//! extern crate syn;
//! extern crate synstructure;
//! #[macro_use]
//! extern crate quote;
//! extern crate proc_macro;
//!
//! use synstructure::{each_field, BindStyle};
//! use proc_macro::TokenStream;
//!
//! fn sum_fields_derive(input: TokenStream) -> TokenStream {
//!     let source = input.to_string();
//!     let ast = syn::parse_macro_input(&source).unwrap();
//!
//!     let match_body = each_field(&ast, &BindStyle::Ref.into(), |bi| quote! {
//!         sum += #bi as i64;
//!     });
//!
//!     let name = &ast.ident;
//!     let (impl_generics, ty_generics, where_clause) = ast.generics.split_for_impl();
//!     let result = quote! {
//!         impl #impl_generics ::sum_fields::SumFields for #name #ty_generics #where_clause {
//!             fn sum_fields(&self) -> i64 {
//!                 let mut sum = 0i64;
//!                 match *self { #match_body }
//!                 sum
//!             }
//!         }
//!     };
//!
//!     result.to_string().parse().unwrap()
//! }
//! #
//! # fn main() {}
//! ```
//!
//! For more example usage, consider investigating the `abomonation_derive` crate,
//! which makes use of this crate, and is fairly simple.

extern crate syn;
#[macro_use]
extern crate quote;

use std::borrow::Cow;
use syn::{Body, Field, Ident, MacroInput, VariantData, Variant};
use quote::{Tokens, ToTokens};

/// The type of binding to use when generating a pattern.
#[derive(Debug, Copy, Clone)]
pub enum BindStyle {
    /// `x`
    Move,
    /// `mut x`
    MoveMut,
    /// `ref x`
    Ref,
    /// `ref mut x`
    RefMut,
}

impl ToTokens for BindStyle {
    fn to_tokens(&self, tokens: &mut Tokens) {
        match *self {
            BindStyle::Move => {}
            BindStyle::MoveMut => tokens.append("mut"),
            BindStyle::Ref => tokens.append("ref"),
            BindStyle::RefMut => {
                tokens.append("ref");
                tokens.append("mut");
            }
        }
    }
}

/// Binding options to use when generating a pattern.
/// Configuration options used for generating binding patterns.
///
/// `bind_style` controls the type of binding performed in the pattern, for
/// example: `ref` or `ref mut`.
///
/// `prefix` controls the name which is used for the binding. This can be used
/// to avoid name conflicts with nested match patterns.
#[derive(Debug, Clone)]
pub struct BindOpts {
    bind_style: BindStyle,
    prefix: Cow<'static, str>,
}

impl BindOpts {
    /// Create a BindOpts with the given style, and the default prefix: "__binding".
    pub fn new(bind_style: BindStyle) -> BindOpts {
        BindOpts {
            bind_style: bind_style,
            prefix: "__binding".into(),
        }
    }

    /// Create a BindOpts with the given style and prefix.
    pub fn with_prefix(bind_style: BindStyle, prefix: String) -> BindOpts {
        BindOpts {
            bind_style: bind_style,
            prefix: prefix.into(),
        }
    }
}

impl From<BindStyle> for BindOpts {
    fn from(style: BindStyle) -> Self {
        BindOpts::new(style)
    }
}

/// Information about a specific binding. This contains both an `Ident`
/// reference to the given field, and the syn `&'a Field` descriptor for that
/// field.
///
/// This type supports `quote::ToTokens`, so can be directly used within the
/// `quote!` macro. It expands to a reference to the matched field.
#[derive(Debug)]
pub struct BindingInfo<'a> {
    pub ident: Ident,
    pub field: &'a Field,
}

impl<'a> ToTokens for BindingInfo<'a> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        self.ident.to_tokens(tokens);
    }
}

/// Generate a match pattern for binding to the given VariantData This function
/// returns a tuple of the tokens which make up that match pattern, and a
/// `BindingInfo` object for each of the bindings which were made. The `bind`
/// parameter controls the type of binding which is made.
///
/// # Example
///
/// ```
/// extern crate syn;
/// extern crate synstructure;
/// #[macro_use]
/// extern crate quote;
/// use synstructure::{match_pattern, BindStyle};
///
/// fn main() {
///     let ast = syn::parse_macro_input("struct A { a: i32, b: i32 }").unwrap();
///     let vd = if let syn::Body::Struct(ref vd) = ast.body {
///         vd
///     } else { unreachable!() };
///
///     let (tokens, bindings) = match_pattern(&ast.ident, vd, &BindStyle::Ref.into());
///     assert_eq!(tokens.to_string(), quote! {
///          A{ a: ref __binding_0, b: ref __binding_1, }
///     }.to_string());
///     assert_eq!(bindings.len(), 2);
///     assert_eq!(&bindings[0].ident.to_string(), "__binding_0");
///     assert_eq!(&bindings[1].ident.to_string(), "__binding_1");
/// }
/// ```
pub fn match_pattern<'a, N: ToTokens>(name: &N,
                                      vd: &'a VariantData,
                                      options: &BindOpts)
                                      -> (Tokens, Vec<BindingInfo<'a>>) {
    let mut t = Tokens::new();
    let mut matches = Vec::new();

    let binding = options.bind_style;
    name.to_tokens(&mut t);
    match *vd {
        VariantData::Unit => {}
        VariantData::Tuple(ref fields) => {
            t.append("(");
            for (i, field) in fields.iter().enumerate() {
                let ident: Ident = format!("{}_{}", options.prefix, i).into();
                quote!(#binding #ident ,).to_tokens(&mut t);
                matches.push(BindingInfo {
                    ident: ident,
                    field: field,
                });
            }
            t.append(")");
        }
        VariantData::Struct(ref fields) => {
            t.append("{");
            for (i, field) in fields.iter().enumerate() {
                let ident: Ident = format!("{}_{}", options.prefix, i).into();
                {
                    let field_name = field.ident.as_ref().unwrap();
                    quote!(#field_name : #binding #ident ,).to_tokens(&mut t);
                }
                matches.push(BindingInfo {
                    ident: ident,
                    field: field,
                });
            }
            t.append("}");
        }
    }

    (t, matches)
}

/// This method calls `func` once per variant in the struct or enum, and generates
/// a series of match branches which will destructure a the input, and run the result
/// of `func` once for each of the variants.
///
/// The second argument to `func` is a syn `Variant` object. This object is
/// fabricated for struct-like `MacroInput` parameters.
///
/// # Example
///
/// ```
/// extern crate syn;
/// extern crate synstructure;
/// #[macro_use]
/// extern crate quote;
/// use synstructure::{each_variant, BindStyle};
///
/// fn main() {
///     let mut ast = syn::parse_macro_input("enum A { B(i32, i32), C }").unwrap();
///
///     let tokens = each_variant(&mut ast, &BindStyle::Ref.into(), |bindings, variant| {
///         let name_str = variant.ident.as_ref();
///         if name_str == "B" {
///             assert_eq!(bindings.len(), 2);
///             assert_eq!(bindings[0].ident.as_ref(), "__binding_0");
///             assert_eq!(bindings[1].ident.as_ref(), "__binding_1");
///         } else {
///             assert_eq!(name_str, "C");
///             assert_eq!(bindings.len(), 0);
///         }
///         quote!(#name_str)
///     });
///     assert_eq!(tokens.to_string(), quote! {
///         A::B(ref __binding_0, ref __binding_1,) => { "B" }
///         A::C => { "C" }
///     }.to_string());
/// }
/// ```
pub fn each_variant<F, T: ToTokens>(input: &MacroInput,
                                    options: &BindOpts,
                                    mut func: F)
                                    -> Tokens
    where F: FnMut(Vec<BindingInfo>, &Variant) -> T {
    let ident = &input.ident;

    let struct_variant;
    // Generate patterns for matching against all of the variants
    let variants = match input.body {
        Body::Enum(ref variants) => {
            variants.iter()
                .map(|variant| {
                    let variant_ident = &variant.ident;
                    let (pat, bindings) = match_pattern(&quote!(#ident :: #variant_ident),
                                                        &variant.data,
                                                        options);
                    (pat, bindings, variant)
                })
                .collect()
        }
        Body::Struct(ref vd) => {
            struct_variant = Variant {
                ident: ident.clone(),
                attrs: input.attrs.clone(),
                data: vd.clone(),
                discriminant: None,
            };

            let (pat, bindings) = match_pattern(&ident, &vd, options);
            vec![(pat, bindings, &struct_variant)]
        }
    };

    // Now that we have the patterns, generate the actual branches of the match
    // expression
    let mut t = Tokens::new();
    for (pat, bindings, variant) in variants {
        let body = func(bindings, variant);
        quote!(#pat => { #body }).to_tokens(&mut t);
    }

    t
}

/// This method generates a match branch for each of the substructures of the
/// given `MacroInput`. It will call `func` for each of these substructures,
/// passing in the bindings which were made for each of the fields in the
/// substructure. The return value of `func` is then used as the value of each
/// branch
///
/// # Example
///
/// ```
/// extern crate syn;
/// extern crate synstructure;
/// #[macro_use]
/// extern crate quote;
/// use synstructure::{match_substructs, BindStyle};
///
/// fn main() {
///     let mut ast = syn::parse_macro_input("struct A { a: i32, b: i32 }").unwrap();
///
///     let tokens = match_substructs(&mut ast, &BindStyle::Ref.into(), |bindings| {
///         assert_eq!(bindings.len(), 2);
///         assert_eq!(bindings[0].ident.as_ref(), "__binding_0");
///         assert_eq!(bindings[1].ident.as_ref(), "__binding_1");
///         quote!("some_random_string")
///     });
///     assert_eq!(tokens.to_string(), quote! {
///         A { a: ref __binding_0, b: ref __binding_1, } => { "some_random_string" }
///     }.to_string());
/// }
/// ```
pub fn match_substructs<F, T: ToTokens>(input: &MacroInput,
                                        options: &BindOpts,
                                        mut func: F)
                                        -> Tokens
    where F: FnMut(Vec<BindingInfo>) -> T
{
    each_variant(input, options, |bindings, _| func(bindings))
}

/// This method calls `func` once per field in the struct or enum, and generates
/// a series of match branches which will destructure match argument, and run
/// the result of `func` once on each of the bindings.
///
/// # Example
///
/// ```
/// extern crate syn;
/// extern crate synstructure;
/// #[macro_use]
/// extern crate quote;
/// use synstructure::{each_field, BindStyle};
///
/// fn main() {
///     let mut ast = syn::parse_macro_input("struct A { a: i32, b: i32 }").unwrap();
///
///     let tokens = each_field(&mut ast, &BindStyle::Ref.into(), |bi| quote! {
///         println!("Saw: {:?}", #bi);
///     });
///     assert_eq!(tokens.to_string(), quote! {
///         A{ a: ref __binding_0, b: ref __binding_1, } => {
///             { println!("Saw: {:?}", __binding_0); }
///             { println!("Saw: {:?}", __binding_1); }
///             ()
///         }
///     }.to_string());
/// }
/// ```
pub fn each_field<F, T: ToTokens>(input: &MacroInput, options: &BindOpts, mut func: F) -> Tokens
    where F: FnMut(BindingInfo) -> T
{
    each_variant(input, options, |bindings, _| {
        let mut t = Tokens::new();
        for binding in bindings {
            t.append("{");
            func(binding).to_tokens(&mut t);
            t.append("}");
        }
        quote!(()).to_tokens(&mut t);
        t
    })
}
