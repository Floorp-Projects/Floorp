/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, Span, TokenStream};
use quote::{format_ident, quote, ToTokens};
use std::path::{Path as StdPath, PathBuf};
use syn::{
    ext::IdentExt,
    parse::{Parse, ParseStream},
    Attribute, Expr, Lit, Token,
};

pub fn manifest_path() -> Result<PathBuf, String> {
    let manifest_dir =
        std::env::var_os("CARGO_MANIFEST_DIR").ok_or("`CARGO_MANIFEST_DIR` is not set")?;

    Ok(StdPath::new(&manifest_dir).join("Cargo.toml"))
}

#[cfg(not(feature = "nightly"))]
pub fn mod_path() -> syn::Result<String> {
    // Without the nightly feature and TokenStream::expand_expr, just return the crate name

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

    static LIB_CRATE_MOD_PATH: Lazy<Result<String, String>> = Lazy::new(|| {
        let file = manifest_path()?;
        let cargo_toml_bytes = fs::read(file).map_err(|e| e.to_string())?;

        let cargo_toml = toml::from_slice::<CargoToml>(&cargo_toml_bytes)
            .map_err(|e| format!("Failed to parse `Cargo.toml`: {e}"))?;

        let lib_crate_name = cargo_toml
            .lib
            .name
            .unwrap_or_else(|| cargo_toml.package.name.replace('-', "_"));

        Ok(lib_crate_name)
    });

    LIB_CRATE_MOD_PATH
        .clone()
        .map_err(|e| syn::Error::new(Span::call_site(), e))
}

#[cfg(feature = "nightly")]
pub fn mod_path() -> syn::Result<String> {
    use proc_macro::TokenStream;

    let module_path_invoc = TokenStream::from(quote! { ::core::module_path!() });
    // We ask the compiler what `module_path!()` expands to here.
    // This is a nightly feature, tracked at https://github.com/rust-lang/rust/issues/90765
    let expanded_module_path = TokenStream::expand_expr(&module_path_invoc)
        .map_err(|e| syn::Error::new(Span::call_site(), e))?;
    Ok(syn::parse::<syn::LitStr>(expanded_module_path)?.value())
}

pub fn try_read_field(f: &syn::Field) -> TokenStream {
    let ident = &f.ident;
    let ty = &f.ty;

    match ident {
        Some(ident) => quote! {
            #ident: <#ty as ::uniffi::Lift<crate::UniFfiTag>>::try_read(buf)?,
        },
        None => quote! {
            <#ty as ::uniffi::Lift<crate::UniFfiTag>>::try_read(buf)?,
        },
    }
}

pub fn ident_to_string(ident: &Ident) -> String {
    ident.unraw().to_string()
}

pub fn crate_name() -> String {
    std::env::var("CARGO_CRATE_NAME").unwrap().replace('-', "_")
}

pub fn create_metadata_items(
    kind: &str,
    name: &str,
    metadata_expr: TokenStream,
    checksum_fn_name: Option<String>,
) -> TokenStream {
    let crate_name = crate_name();
    let crate_name_upper = crate_name.to_uppercase();
    let kind_upper = kind.to_uppercase();
    let name_upper = name.to_uppercase();
    let const_ident =
        format_ident!("UNIFFI_META_CONST_{crate_name_upper}_{kind_upper}_{name_upper}");
    let static_ident = format_ident!("UNIFFI_META_{crate_name_upper}_{kind_upper}_{name_upper}");
    let checksum_fn = checksum_fn_name.map(|name| {
        let ident = Ident::new(&name, Span::call_site());
        quote! {
            #[doc(hidden)]
            #[no_mangle]
            pub extern "C" fn #ident() -> u16 {
                #const_ident.checksum()
            }
        }
    });

    quote! {
        const #const_ident: ::uniffi::MetadataBuffer = #metadata_expr;
        #[no_mangle]
        #[doc(hidden)]
        pub static #static_ident: [u8; #const_ident.size] = #const_ident.into_array();

        #checksum_fn
    }
}

pub fn try_metadata_value_from_usize(value: usize, error_message: &str) -> syn::Result<u8> {
    value
        .try_into()
        .map_err(|_| syn::Error::new(Span::call_site(), error_message))
}

pub fn chain<T>(
    a: impl IntoIterator<Item = T>,
    b: impl IntoIterator<Item = T>,
) -> impl Iterator<Item = T> {
    a.into_iter().chain(b)
}

pub trait UniffiAttributeArgs: Default {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self>;
    fn merge(self, other: Self) -> syn::Result<Self>;
}

pub fn parse_comma_separated<T: UniffiAttributeArgs>(input: ParseStream<'_>) -> syn::Result<T> {
    let punctuated = input.parse_terminated(T::parse_one, Token![,])?;
    punctuated.into_iter().try_fold(T::default(), T::merge)
}

#[derive(Default)]
struct ArgumentNotAllowedHere;

impl UniffiAttributeArgs for ArgumentNotAllowedHere {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self> {
        Err(syn::Error::new(
            input.span(),
            "attribute arguments are not currently recognized in this position",
        ))
    }

    fn merge(self, _other: Self) -> syn::Result<Self> {
        Ok(Self)
    }
}

pub trait AttributeSliceExt {
    fn parse_uniffi_attr_args<T: UniffiAttributeArgs>(&self) -> syn::Result<T>;
    fn uniffi_attr_args_not_allowed_here(&self) -> Option<syn::Error> {
        self.parse_uniffi_attr_args::<ArgumentNotAllowedHere>()
            .err()
    }
}

impl AttributeSliceExt for [Attribute] {
    fn parse_uniffi_attr_args<T: UniffiAttributeArgs>(&self) -> syn::Result<T> {
        self.iter()
            .filter(|attr| attr.path().is_ident("uniffi"))
            .try_fold(T::default(), |res, attr| {
                let parsed = attr.parse_args_with(parse_comma_separated)?;
                res.merge(parsed)
            })
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

pub(crate) fn tagged_impl_header(
    trait_name: &str,
    ident: &impl ToTokens,
    udl_mode: bool,
) -> TokenStream {
    let trait_name = Ident::new(trait_name, Span::call_site());
    if udl_mode {
        quote! { impl ::uniffi::#trait_name<crate::UniFfiTag> for #ident }
    } else {
        quote! { impl<T> ::uniffi::#trait_name<T> for #ident }
    }
}

pub(crate) fn derive_all_ffi_traits(ty: &Ident, udl_mode: bool) -> TokenStream {
    if udl_mode {
        quote! { ::uniffi::derive_ffi_traits!(local #ty); }
    } else {
        quote! { ::uniffi::derive_ffi_traits!(blanket #ty); }
    }
}

pub(crate) fn derive_ffi_traits(
    ty: impl ToTokens,
    udl_mode: bool,
    trait_names: &[&str],
) -> TokenStream {
    let trait_idents = trait_names
        .iter()
        .map(|name| Ident::new(name, Span::call_site()));
    if udl_mode {
        quote! {
            #(
                ::uniffi::derive_ffi_traits!(impl #trait_idents<crate::UniFfiTag> for #ty);
            )*
        }
    } else {
        quote! {
            #(
                ::uniffi::derive_ffi_traits!(impl<UT> #trait_idents<UT> for #ty);
            )*
        }
    }
}

/// Custom keywords
pub mod kw {
    syn::custom_keyword!(async_runtime);
    syn::custom_keyword!(callback_interface);
    syn::custom_keyword!(with_foreign);
    syn::custom_keyword!(default);
    syn::custom_keyword!(flat_error);
    syn::custom_keyword!(None);
    syn::custom_keyword!(Some);
    syn::custom_keyword!(with_try_read);
    syn::custom_keyword!(name);
    syn::custom_keyword!(non_exhaustive);
    syn::custom_keyword!(Debug);
    syn::custom_keyword!(Display);
    syn::custom_keyword!(Eq);
    syn::custom_keyword!(Hash);
    // Not used anymore
    syn::custom_keyword!(handle_unknown_callback_error);
}

/// Specifies a type from a dependent crate
pub struct ExternalTypeItem {
    pub crate_ident: Ident,
    pub sep: Token![,],
    pub type_ident: Ident,
}

impl Parse for ExternalTypeItem {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        Ok(Self {
            crate_ident: input.parse()?,
            sep: input.parse()?,
            type_ident: input.parse()?,
        })
    }
}

pub(crate) fn extract_docstring(attrs: &[Attribute]) -> syn::Result<String> {
    return attrs
        .iter()
        .filter(|attr| attr.path().is_ident("doc"))
        .map(|attr| {
            let name_value = attr.meta.require_name_value()?;
            if let Expr::Lit(expr) = &name_value.value {
                if let Lit::Str(lit_str) = &expr.lit {
                    return Ok(lit_str.value().trim().to_owned());
                }
            }
            Err(syn::Error::new_spanned(attr, "Cannot parse doc attribute"))
        })
        .collect::<syn::Result<Vec<_>>>()
        .map(|lines| lines.join("\n"));
}
