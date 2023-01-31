/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, Span, TokenStream};
use quote::{format_ident, quote, quote_spanned, ToTokens};
use syn::{
    parse::{Parse, ParseStream},
    punctuated::Punctuated,
    spanned::Spanned,
    visit_mut::VisitMut,
    Attribute, Item, Token, Type,
};
use uniffi_meta::Metadata;

#[cfg(not(feature = "nightly"))]
pub fn mod_path() -> syn::Result<Vec<String>> {
    // Without the nightly feature and TokenStream::expand_expr, just return the crate name

    use std::path::Path;

    use fs_err as fs;
    use once_cell::sync::Lazy;
    use serde::Deserialize;

    #[derive(Deserialize)]
    struct CargoToml {
        package: Package,
        #[serde(default)]
        lib: Lib,
    }

    #[derive(Deserialize)]
    struct Package {
        name: String,
    }

    #[derive(Default, Deserialize)]
    struct Lib {
        name: Option<String>,
    }

    static LIB_CRATE_MOD_PATH: Lazy<Result<Vec<String>, String>> = Lazy::new(|| {
        let manifest_dir =
            std::env::var_os("CARGO_MANIFEST_DIR").ok_or("`CARGO_MANIFEST_DIR` is not set")?;

        let cargo_toml_bytes =
            fs::read(Path::new(&manifest_dir).join("Cargo.toml")).map_err(|e| e.to_string())?;
        let cargo_toml = toml::from_slice::<CargoToml>(&cargo_toml_bytes)
            .map_err(|e| format!("Failed to parse `Cargo.toml`: {e}"))?;

        let lib_crate_name = cargo_toml
            .lib
            .name
            .unwrap_or_else(|| cargo_toml.package.name.replace('-', "_"));

        Ok(vec![lib_crate_name])
    });

    LIB_CRATE_MOD_PATH
        .clone()
        .map_err(|e| syn::Error::new(Span::call_site(), e))
}

#[cfg(feature = "nightly")]
pub fn mod_path() -> syn::Result<Vec<String>> {
    use proc_macro::TokenStream;

    let module_path_invoc = TokenStream::from(quote! { ::core::module_path!() });
    // We ask the compiler what `module_path!()` expands to here.
    // This is a nightly feature, tracked at https://github.com/rust-lang/rust/issues/90765
    let expanded_module_path = TokenStream::expand_expr(&module_path_invoc)
        .map_err(|e| syn::Error::new(Span::call_site(), e))?;
    Ok(syn::parse::<syn::LitStr>(expanded_module_path)?
        .value()
        .split("::")
        .collect())
}

/// Rewrite Self type alias usage in an impl block to the type itself.
///
/// For example,
///
/// ```ignore
/// impl some::module::Foo {
///     fn method(
///         self: Arc<Self>,
///         arg: Option<Bar<(), Self>>,
///     ) -> Result<Self, Error> {
///         todo!()
///     }
/// }
/// ```
///
/// will be rewritten to
///
///  ```ignore
/// impl some::module::Foo {
///     fn method(
///         self: Arc<some::module::Foo>,
///         arg: Option<Bar<(), some::module::Foo>>,
///     ) -> Result<some::module::Foo, Error> {
///         todo!()
///     }
/// }
/// ```
pub fn rewrite_self_type(item: &mut Item) {
    let item = match item {
        Item::Impl(i) => i,
        _ => return,
    };

    struct RewriteSelfVisitor<'a>(&'a Type);

    impl<'a> VisitMut for RewriteSelfVisitor<'a> {
        fn visit_type_mut(&mut self, i: &mut Type) {
            match i {
                Type::Path(p) if p.qself.is_none() && p.path.is_ident("Self") => {
                    *i = self.0.clone();
                }
                _ => syn::visit_mut::visit_type_mut(self, i),
            }
        }
    }

    let mut visitor = RewriteSelfVisitor(&item.self_ty);
    for item in &mut item.items {
        visitor.visit_impl_item_mut(item);
    }
}

pub fn try_read_field(f: &syn::Field) -> TokenStream {
    let ident = &f.ident;
    let ty = &f.ty;

    quote! {
        #ident: <#ty as ::uniffi::FfiConverter>::try_read(buf)?,
    }
}

pub fn create_metadata_static_var(name: &Ident, val: Metadata) -> TokenStream {
    let data: Vec<u8> = bincode::serialize(&val).expect("Error serializing metadata item");
    let count = data.len();
    let var_name = format_ident!("UNIFFI_META_{}", name);

    quote! {
        #[no_mangle]
        #[doc(hidden)]
        pub static #var_name: [u8; #count] = [#(#data),*];
    }
}

pub fn assert_type_eq(a: impl ToTokens + Spanned, b: impl ToTokens) -> TokenStream {
    quote_spanned! {a.span()=>
        #[allow(unused_qualifications)]
        const _: () = {
            ::uniffi::deps::static_assertions::assert_type_eq_all!(#a, #b);
        };
    }
}

pub fn chain<T>(
    a: impl IntoIterator<Item = T>,
    b: impl IntoIterator<Item = T>,
) -> impl Iterator<Item = T> {
    a.into_iter().chain(b)
}

pub trait UniffiAttribute: Default + Parse {
    fn merge(self, other: Self) -> syn::Result<Self>;
}

#[derive(Default)]
struct AttributeNotAllowedHere;

impl Parse for AttributeNotAllowedHere {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        Err(syn::Error::new(
            input.span(),
            "UniFFI attributes are not currently recognized in this position",
        ))
    }
}

impl UniffiAttribute for AttributeNotAllowedHere {
    fn merge(self, _other: Self) -> syn::Result<Self> {
        Ok(Self)
    }
}

pub trait AttributeSliceExt {
    fn parse_uniffi_attributes<T: UniffiAttribute>(&self) -> syn::Result<T>;
    fn attributes_not_allowed_here(&self) -> Option<syn::Error>;
}

impl AttributeSliceExt for [Attribute] {
    fn parse_uniffi_attributes<T: UniffiAttribute>(&self) -> syn::Result<T> {
        self.iter()
            .filter(|attr| attr.path.is_ident("uniffi"))
            .try_fold(T::default(), |res, attr| {
                let list: Punctuated<T, Token![,]> =
                    attr.parse_args_with(Punctuated::parse_terminated)?;
                list.into_iter().try_fold(res, T::merge)
            })
    }

    fn attributes_not_allowed_here(&self) -> Option<syn::Error> {
        self.parse_uniffi_attributes::<AttributeNotAllowedHere>()
            .err()
    }
}

pub fn either_attribute_arg<T: ToTokens>(a: Option<T>, b: Option<T>) -> syn::Result<Option<T>> {
    match (a, b) {
        (None, None) => Ok(None),
        (Some(val), None) | (None, Some(val)) => Ok(Some(val)),
        (Some(a), Some(b)) => {
            let mut error = syn::Error::new_spanned(a, "redundant attribute argument");
            error.combine(syn::Error::new_spanned(b, "note: first one here"));
            Err(error)
        }
    }
}
