/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::{Ident, Span, TokenStream};
use quote::{format_ident, quote, ToTokens};
use std::path::{Path as StdPath, PathBuf};
use syn::{
    ext::IdentExt,
    parse::{Parse, ParseStream},
    Attribute, Path, Token,
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

    quote! {
        #ident: <#ty as ::uniffi::FfiConverter<crate::UniFfiTag>>::try_read(buf)?,
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
pub struct ArgumentNotAllowedHere;

impl Parse for ArgumentNotAllowedHere {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        parse_comma_separated(input)
    }
}

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
    ident: &Ident,
    tag: Option<&Path>,
) -> TokenStream {
    let trait_name = Ident::new(trait_name, Span::call_site());
    match tag {
        Some(tag) => quote! { impl ::uniffi::#trait_name<#tag> for #ident },
        None => quote! { impl<T> ::uniffi::#trait_name<T> for #ident },
    }
}

mod kw {
    syn::custom_keyword!(tag);
}

#[derive(Default)]
pub(crate) struct CommonAttr {
    /// Specifies the `UniFfiTag` used when implementing `FfiConverter`
    ///   - When things are defined with proc-macros, this is `None` which means create a blanket
    ///     impl for all types.
    ///   - When things are defined with UDL files this is `Some(crate::UniFfiTag)`, which means only
    ///     implement it for the local tag in the crate
    ///
    /// The reason for this split is remote types, i.e. types defined in remote crates that we
    /// don't control and therefore can't define a blanket impl on because of the orphan rules.
    ///
    /// With UDL, we handle this by only implementing `FfiConverter<crate::UniFfiTag>` for the
    /// type.  This gets around the orphan rules since a local type is in the trait, but requires
    /// a `uniffi::ffi_converter_forward!` call if the type is used in a second local crate (an
    /// External typedef).  This is natural for UDL-based generation, since you always need to
    /// define the external type in the UDL file.
    ///
    /// With proc-macros this system isn't so natural.  Instead, we plan to use this system:
    ///   - Most of the time, types aren't remote and we use the blanket impl.
    ///   - When types are remote, we'll need a special syntax to define an `FfiConverter` for them
    ///     and also a special declaration to use the types in other crates.  This requires some
    ///     extra work for the consumer, but it should be rare.
    pub tag: Option<Path>,
}

impl UniffiAttributeArgs for CommonAttr {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(kw::tag) {
            let _: kw::tag = input.parse()?;
            let _: Token![=] = input.parse()?;
            Ok(Self {
                tag: Some(input.parse()?),
            })
        } else {
            Err(lookahead.error())
        }
    }

    fn merge(self, other: Self) -> syn::Result<Self> {
        Ok(Self {
            tag: either_attribute_arg(self.tag, other.tag)?,
        })
    }
}

// So CommonAttr can be used with `parse_macro_input!`
impl Parse for CommonAttr {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        parse_comma_separated(input)
    }
}
