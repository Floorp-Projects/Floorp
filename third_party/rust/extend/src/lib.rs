//! Create extensions for types you don't own with [extension traits] but without the boilerplate.
//!
//! Example:
//!
//! ```rust
//! use extend::ext;
//!
//! #[ext]
//! impl<T: Ord> Vec<T> {
//!     fn sorted(mut self) -> Self {
//!         self.sort();
//!         self
//!     }
//! }
//!
//! fn main() {
//!     assert_eq!(
//!         vec![1, 2, 3],
//!         vec![2, 3, 1].sorted(),
//!     );
//! }
//! ```
//!
//! # How does it work?
//!
//! Under the hood it generates a trait with methods in your `impl` and implements those for the
//! type you specify. The code shown above expands roughly to:
//!
//! ```rust
//! trait VecExt<T: Ord> {
//!     fn sorted(self) -> Self;
//! }
//!
//! impl<T: Ord> VecExt<T> for Vec<T> {
//!     fn sorted(mut self) -> Self {
//!         self.sort();
//!         self
//!     }
//! }
//! ```
//!
//! # Supported items
//!
//! Extensions can contain methods or associated constants:
//!
//! ```rust
//! use extend::ext;
//!
//! #[ext]
//! impl String {
//!     const CONSTANT: &'static str = "FOO";
//!
//!     fn method() {
//!         // ...
//!         # todo!()
//!     }
//! }
//! ```
//!
//! # Configuration
//!
//! You can configure:
//!
//! - The visibility of the trait. Use `pub impl ...` to generate `pub trait ...`. The default
//! visibility is private.
//! - The name of the generated extension trait. Example: `#[ext(name = MyExt)]`. By default we
//! generate a name based on what you extend.
//! - Which supertraits the generated extension trait should have. Default is no supertraits.
//! Example: `#[ext(supertraits = Default + Clone)]`.
//!
//! More examples:
//!
//! ```rust
//! use extend::ext;
//!
//! #[ext(name = SortedVecExt)]
//! impl<T: Ord> Vec<T> {
//!     fn sorted(mut self) -> Self {
//!         self.sort();
//!         self
//!     }
//! }
//!
//! #[ext]
//! pub(crate) impl i32 {
//!     fn double(self) -> i32 {
//!         self * 2
//!     }
//! }
//!
//! #[ext(name = ResultSafeUnwrapExt)]
//! pub impl<T> Result<T, std::convert::Infallible> {
//!     fn safe_unwrap(self) -> T {
//!         match self {
//!             Ok(t) => t,
//!             Err(_) => unreachable!(),
//!         }
//!     }
//! }
//!
//! #[ext(supertraits = Default + Clone)]
//! impl String {
//!     fn my_length(self) -> usize {
//!         self.len()
//!     }
//! }
//! ```
//!
//! For backwards compatibility you can also declare the visibility as the first argument to `#[ext]`:
//!
//! ```
//! use extend::ext;
//!
//! #[ext(pub)]
//! impl i32 {
//!     fn double(self) -> i32 {
//!         self * 2
//!     }
//! }
//! ```
//!
//! # async-trait compatibility
//!
//! Async extensions are supported via [async-trait](https://crates.io/crates/async-trait).
//!
//! Be aware that you need to add `#[async_trait]` _below_ `#[ext]`. Otherwise the `ext` macro
//! cannot see the `#[async_trait]` attribute and pass it along in the generated code.
//!
//! Example:
//!
//! ```
//! use extend::ext;
//! use async_trait::async_trait;
//!
//! #[ext]
//! #[async_trait]
//! impl String {
//!     async fn read_file() -> String {
//!         // ...
//!         # todo!()
//!     }
//! }
//! ```
//!
//! # Other attributes
//!
//! Other attributes provided _below_ `#[ext]` will be passed along to both the generated trait and
//! the implementation. See [async-trait compatibility](#async-trait-compatibility) above for an
//! example.
//!
//! [extension traits]: https://dev.to/matsimitsu/extending-existing-functionality-in-rust-with-traits-in-rust-3622

#![doc(html_root_url = "https://docs.rs/extend/1.1.2")]
#![allow(clippy::let_and_return)]
#![deny(
    unused_variables,
    mutable_borrow_reservation_conflict,
    dead_code,
    unused_must_use,
    unused_imports
)]

use proc_macro2::TokenStream;
use proc_macro_error::*;
use quote::{format_ident, quote, ToTokens};
use syn::{
    parse::{self, Parse, ParseStream},
    parse_macro_input, parse_quote,
    punctuated::Punctuated,
    spanned::Spanned,
    token::{Add, Semi},
    Ident, ImplItem, ItemImpl, Token, TraitItemConst, TraitItemMethod, Type, TypeArray, TypeBareFn,
    TypeGroup, TypeNever, TypeParamBound, TypeParen, TypePath, TypePtr, TypeReference, TypeSlice,
    TypeTraitObject, TypeTuple, Visibility,
};

#[derive(Debug)]
struct Input {
    item_impl: ItemImpl,
    vis: Option<Visibility>,
}

impl Parse for Input {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let mut attributes = Vec::new();
        if input.peek(syn::Token![#]) {
            attributes.extend(syn::Attribute::parse_outer(input)?);
        }

        let vis = input
            .parse::<Visibility>()
            .ok()
            .filter(|vis| vis != &Visibility::Inherited);

        let mut item_impl = input.parse::<ItemImpl>()?;
        item_impl.attrs.extend(attributes);

        Ok(Self { item_impl, vis })
    }
}

/// See crate docs for more info.
#[proc_macro_attribute]
#[proc_macro_error]
#[allow(clippy::unneeded_field_pattern)]
pub fn ext(
    attr: proc_macro::TokenStream,
    item: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let item = parse_macro_input!(item as Input);
    let config = parse_macro_input!(attr as Config);
    go(item, config)
}

/// Like [`ext`](macro@crate::ext) but always add `Sized` as a supertrait.
///
/// This is provided as a convenience for generating extension traits that require `Self: Sized`
/// such as:
///
/// ```
/// use extend::ext_sized;
///
/// #[ext_sized]
/// impl i32 {
///     fn requires_sized(self) -> Option<Self> {
///         Some(self)
///     }
/// }
/// ```
#[proc_macro_attribute]
#[proc_macro_error]
#[allow(clippy::unneeded_field_pattern)]
pub fn ext_sized(
    attr: proc_macro::TokenStream,
    item: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let item = parse_macro_input!(item as Input);
    let mut config: Config = parse_macro_input!(attr as Config);

    config.supertraits = if let Some(supertraits) = config.supertraits.take() {
        Some(parse_quote!(#supertraits + Sized))
    } else {
        Some(parse_quote!(Sized))
    };

    go(item, config)
}

fn go(item: Input, mut config: Config) -> proc_macro::TokenStream {
    if let Some(vis) = item.vis {
        if config.visibility != Visibility::Inherited {
            abort!(
                config.visibility.span(),
                "Cannot set visibility on `#[ext]` and `impl` block"
            );
        }

        config.visibility = vis;
    }

    let ItemImpl {
        attrs,
        unsafety,
        generics,
        trait_,
        self_ty,
        items,
        // What is defaultness?
        defaultness: _,
        impl_token: _,
        brace_token: _,
    } = item.item_impl;

    if let Some((_, path, _)) = trait_ {
        abort!(path.span(), "Trait impls cannot be used for #[ext]");
    }

    let self_ty = parse_self_ty(&self_ty);

    let ext_trait_name = config
        .ext_trait_name
        .unwrap_or_else(|| ext_trait_name(&self_ty));

    let MethodsAndConsts {
        trait_methods,
        trait_consts,
    } = extract_allowed_items(&items);

    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let visibility = &config.visibility;

    let mut all_supertraits = Vec::<TypeParamBound>::new();

    if let Some(supertraits_from_config) = config.supertraits {
        all_supertraits.extend(supertraits_from_config);
    }

    let supertraits_quoted = if all_supertraits.is_empty() {
        quote! {}
    } else {
        let supertraits_quoted = punctuated_from_iter::<_, _, Add>(all_supertraits);
        quote! { : #supertraits_quoted }
    };

    let code = (quote! {
        #[allow(non_camel_case_types)]
        #(#attrs)*
        #visibility
        #unsafety
        trait #ext_trait_name #impl_generics #supertraits_quoted #where_clause {
            #(
                #trait_consts
            )*

            #(
                #[allow(
                    patterns_in_fns_without_body,
                    clippy::inline_fn_without_body,
                    unused_attributes
                )]
                #trait_methods
            )*
        }

        #(#attrs)*
        impl #impl_generics #ext_trait_name #ty_generics for #self_ty #where_clause {
            #(#items)*
        }
    })
    .into();

    code
}

#[derive(Debug, Clone)]
enum ExtType<'a> {
    Array(&'a TypeArray),
    Group(&'a TypeGroup),
    Never(&'a TypeNever),
    Paren(&'a TypeParen),
    Path(&'a TypePath),
    Ptr(&'a TypePtr),
    Reference(&'a TypeReference),
    Slice(&'a TypeSlice),
    Tuple(&'a TypeTuple),
    BareFn(&'a TypeBareFn),
    TraitObject(&'a TypeTraitObject),
}

#[allow(clippy::wildcard_in_or_patterns)]
fn parse_self_ty(self_ty: &Type) -> ExtType {
    match self_ty {
        Type::Array(inner) => ExtType::Array(inner),
        Type::Group(inner) => ExtType::Group(inner),
        Type::Never(inner) => ExtType::Never(inner),
        Type::Paren(inner) => ExtType::Paren(inner),
        Type::Path(inner) => ExtType::Path(inner),
        Type::Ptr(inner) => ExtType::Ptr(inner),
        Type::Reference(inner) => ExtType::Reference(inner),
        Type::Slice(inner) => ExtType::Slice(inner),
        Type::Tuple(inner) => ExtType::Tuple(inner),
        Type::BareFn(inner) => ExtType::BareFn(inner),
        Type::TraitObject(inner) => ExtType::TraitObject(inner),

        Type::ImplTrait(_) | Type::Infer(_) | Type::Macro(_) | Type::Verbatim(_) | _ => abort!(
            self_ty.span(),
            "#[ext] is not supported for this kind of type"
        ),
    }
}

impl<'a> From<&'a Type> for ExtType<'a> {
    fn from(inner: &'a Type) -> ExtType<'a> {
        parse_self_ty(inner)
    }
}

impl<'a> ToTokens for ExtType<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        match self {
            ExtType::Array(inner) => inner.to_tokens(tokens),
            ExtType::Group(inner) => inner.to_tokens(tokens),
            ExtType::Never(inner) => inner.to_tokens(tokens),
            ExtType::Paren(inner) => inner.to_tokens(tokens),
            ExtType::Path(inner) => inner.to_tokens(tokens),
            ExtType::Ptr(inner) => inner.to_tokens(tokens),
            ExtType::Reference(inner) => inner.to_tokens(tokens),
            ExtType::Slice(inner) => inner.to_tokens(tokens),
            ExtType::Tuple(inner) => inner.to_tokens(tokens),
            ExtType::BareFn(inner) => inner.to_tokens(tokens),
            ExtType::TraitObject(inner) => inner.to_tokens(tokens),
        }
    }
}

fn ext_trait_name(self_ty: &ExtType) -> Ident {
    fn inner_self_ty(self_ty: &ExtType) -> Ident {
        match self_ty {
            ExtType::Path(inner) => find_and_combine_idents(inner),
            ExtType::Reference(inner) => {
                let name = inner_self_ty(&(&*inner.elem).into());
                if inner.mutability.is_some() {
                    format_ident!("RefMut{}", name)
                } else {
                    format_ident!("Ref{}", name)
                }
            }
            ExtType::Array(inner) => {
                let name = inner_self_ty(&(&*inner.elem).into());
                format_ident!("ListOf{}", name)
            }
            ExtType::Group(inner) => {
                let name = inner_self_ty(&(&*inner.elem).into());
                format_ident!("Group{}", name)
            }
            ExtType::Paren(inner) => {
                let name = inner_self_ty(&(&*inner.elem).into());
                format_ident!("Paren{}", name)
            }
            ExtType::Ptr(inner) => {
                let name = inner_self_ty(&(&*inner.elem).into());
                format_ident!("PointerTo{}", name)
            }
            ExtType::Slice(inner) => {
                let name = inner_self_ty(&(&*inner.elem).into());
                format_ident!("SliceOf{}", name)
            }
            ExtType::Tuple(inner) => {
                let mut name = format_ident!("TupleOf");
                for elem in &inner.elems {
                    name = format_ident!("{}{}", name, inner_self_ty(&elem.into()));
                }
                name
            }
            ExtType::Never(_) => format_ident!("Never"),
            ExtType::BareFn(inner) => {
                let mut name = format_ident!("BareFn");
                for input in inner.inputs.iter() {
                    name = format_ident!("{}{}", name, inner_self_ty(&(&input.ty).into()));
                }
                match &inner.output {
                    syn::ReturnType::Default => {
                        name = format_ident!("{}Unit", name);
                    }
                    syn::ReturnType::Type(_, ty) => {
                        name = format_ident!("{}{}", name, inner_self_ty(&(&**ty).into()));
                    }
                }
                name
            }
            ExtType::TraitObject(inner) => {
                let mut name = format_ident!("TraitObject");
                for bound in inner.bounds.iter() {
                    match bound {
                        TypeParamBound::Trait(bound) => {
                            for segment in bound.path.segments.iter() {
                                name = format_ident!("{}{}", name, segment.ident);
                            }
                        }
                        TypeParamBound::Lifetime(lifetime) => {
                            name = format_ident!("{}{}", name, lifetime.ident);
                        }
                    }
                }
                name
            }
        }
    }

    format_ident!("{}Ext", inner_self_ty(self_ty))
}

fn find_and_combine_idents(type_path: &TypePath) -> Ident {
    use syn::visit::{self, Visit};

    struct IdentVisitor<'a>(Vec<&'a Ident>);

    impl<'a> Visit<'a> for IdentVisitor<'a> {
        fn visit_ident(&mut self, i: &'a Ident) {
            self.0.push(i);
        }
    }

    let mut visitor = IdentVisitor(Vec::new());
    visit::visit_type_path(&mut visitor, type_path);
    let idents = visitor.0;

    if idents.is_empty() {
        abort!(type_path.span(), "Empty type path")
    } else {
        let start = &idents[0].span();
        let combined_span = idents
            .iter()
            .map(|i| i.span())
            .fold(*start, |a, b| a.join(b).unwrap_or(a));

        let combined_name = idents.iter().map(|i| i.to_string()).collect::<String>();

        Ident::new(&combined_name, combined_span)
    }
}

#[derive(Debug, Default)]
struct MethodsAndConsts {
    trait_methods: Vec<TraitItemMethod>,
    trait_consts: Vec<TraitItemConst>,
}

#[allow(clippy::wildcard_in_or_patterns)]
fn extract_allowed_items(items: &[ImplItem]) -> MethodsAndConsts {
    let mut acc = MethodsAndConsts::default();
    for item in items {
        match item {
            ImplItem::Method(method) => acc.trait_methods.push(TraitItemMethod {
                attrs: method.attrs.clone(),
                sig: method.sig.clone(),
                default: None,
                semi_token: Some(Semi::default()),
            }),
            ImplItem::Const(const_) => acc.trait_consts.push(TraitItemConst {
                attrs: const_.attrs.clone(),
                const_token: Default::default(),
                ident: const_.ident.clone(),
                colon_token: Default::default(),
                ty: const_.ty.clone(),
                default: None,
                semi_token: Default::default(),
            }),
            ImplItem::Type(_) => abort!(
                item.span(),
                "Associated types are not allowed in #[ext] impls"
            ),
            ImplItem::Macro(_) => abort!(item.span(), "Macros are not allowed in #[ext] impls"),
            ImplItem::Verbatim(_) | _ => abort!(item.span(), "Not allowed in #[ext] impls"),
        }
    }
    acc
}

#[derive(Debug)]
struct Config {
    ext_trait_name: Option<Ident>,
    visibility: Visibility,
    supertraits: Option<Punctuated<TypeParamBound, Add>>,
}

impl Parse for Config {
    fn parse(input: ParseStream) -> parse::Result<Self> {
        let mut config = Config::default();

        if let Ok(visibility) = input.parse::<Visibility>() {
            config.visibility = visibility;
        }

        input.parse::<Token![,]>().ok();

        while !input.is_empty() {
            let ident = input.parse::<Ident>()?;
            input.parse::<Token![=]>()?;

            match &*ident.to_string() {
                "name" => {
                    config.ext_trait_name = Some(input.parse()?);
                }
                "supertraits" => {
                    config.supertraits =
                        Some(Punctuated::<TypeParamBound, Add>::parse_terminated(input)?);
                }
                _ => abort!(ident.span(), "Unknown configuration name"),
            }

            input.parse::<Token![,]>().ok();
        }

        Ok(config)
    }
}

impl Default for Config {
    fn default() -> Self {
        Self {
            ext_trait_name: None,
            visibility: Visibility::Inherited,
            supertraits: None,
        }
    }
}

fn punctuated_from_iter<I, T, P>(i: I) -> Punctuated<T, P>
where
    P: Default,
    I: IntoIterator<Item = T>,
{
    let mut iter = i.into_iter().peekable();
    let mut acc = Punctuated::default();

    while let Some(item) = iter.next() {
        acc.push_value(item);

        if iter.peek().is_some() {
            acc.push_punct(P::default());
        }
    }

    acc
}

#[cfg(test)]
mod test {
    #[allow(unused_imports)]
    use super::*;

    #[test]
    fn test_ui() {
        let t = trybuild::TestCases::new();
        t.pass("tests/compile_pass/*.rs");
        t.compile_fail("tests/compile_fail/*.rs");
    }
}
