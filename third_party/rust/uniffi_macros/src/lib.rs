/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#![cfg_attr(feature = "nightly", feature(proc_macro_expand))]
#![warn(rust_2018_idioms, unused_qualifications)]

//! Macros for `uniffi`.
//!
//! Currently this is just for easily generating integration tests, but maybe
//! we'll put some other code-annotation helper macros in here at some point.

use camino::Utf8Path;
use proc_macro::TokenStream;
use quote::quote;
use syn::{
    parse::{Parse, ParseStream},
    parse_macro_input, Ident, LitStr, Path, Token,
};

mod custom;
mod enum_;
mod error;
mod export;
mod fnsig;
mod object;
mod record;
mod setup_scaffolding;
mod test;
mod util;

use self::{
    enum_::expand_enum, error::expand_error, export::expand_export, object::expand_object,
    record::expand_record,
};

struct IdentPair {
    lhs: Ident,
    rhs: Ident,
}

impl Parse for IdentPair {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let lhs = input.parse()?;
        input.parse::<Token![,]>()?;
        let rhs = input.parse()?;
        Ok(Self { lhs, rhs })
    }
}

struct CustomTypeInfo {
    ident: Ident,
    builtin: Path,
}

impl Parse for CustomTypeInfo {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let ident = input.parse()?;
        input.parse::<Token![,]>()?;
        let builtin = input.parse()?;
        Ok(Self { ident, builtin })
    }
}

/// A macro to build testcases for a component's generated bindings.
///
/// This macro provides some plumbing to write automated tests for the generated
/// foreign language bindings of a component. As a component author, you can write
/// script files in the target foreign language(s) that exercise you component API,
/// and then call this macro to produce a `cargo test` testcase from each one.
/// The generated code will execute your script file with appropriate configuration and
/// environment to let it load the component bindings, and will pass iff the script
/// exits successfully.
///
/// To use it, invoke the macro with the name of a fixture/example crate as the first argument,
/// then one or more file paths relative to the crate root directory. It will produce one `#[test]`
/// function per file, in a manner designed to play nicely with `cargo test` and its test filtering
/// options.
#[proc_macro]
pub fn build_foreign_language_testcases(tokens: TokenStream) -> TokenStream {
    test::build_foreign_language_testcases(tokens)
}

/// Top-level initialization macro
///
/// The optional namespace argument is only used by the scaffolding templates to pass in the
/// CI namespace.
#[proc_macro]
pub fn setup_scaffolding(tokens: TokenStream) -> TokenStream {
    let namespace = match syn::parse_macro_input!(tokens as Option<LitStr>) {
        Some(lit_str) => lit_str.value(),
        None => match util::mod_path() {
            Ok(v) => v,
            Err(e) => return e.into_compile_error().into(),
        },
    };
    setup_scaffolding::setup_scaffolding(namespace)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[proc_macro_attribute]
pub fn export(attr_args: TokenStream, input: TokenStream) -> TokenStream {
    do_export(attr_args, input, false)
}

fn do_export(attr_args: TokenStream, input: TokenStream, udl_mode: bool) -> TokenStream {
    let copied_input = (!udl_mode).then(|| proc_macro2::TokenStream::from(input.clone()));

    let gen_output = || {
        let args = syn::parse(attr_args)?;
        let item = syn::parse(input)?;
        expand_export(item, args, udl_mode)
    };
    let output = gen_output().unwrap_or_else(syn::Error::into_compile_error);

    quote! {
        #copied_input
        #output
    }
    .into()
}

#[proc_macro_derive(Record, attributes(uniffi))]
pub fn derive_record(input: TokenStream) -> TokenStream {
    expand_record(parse_macro_input!(input), false)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[proc_macro_derive(Enum)]
pub fn derive_enum(input: TokenStream) -> TokenStream {
    expand_enum(parse_macro_input!(input), false)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[proc_macro_derive(Object)]
pub fn derive_object(input: TokenStream) -> TokenStream {
    expand_object(parse_macro_input!(input), false)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[proc_macro_derive(Error, attributes(uniffi))]
pub fn derive_error(input: TokenStream) -> TokenStream {
    expand_error(parse_macro_input!(input), None, false)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

/// Generate the `FfiConverter` implementation for a Custom Type - ie,
/// for a `<T>` which implements `UniffiCustomTypeConverter`.
#[proc_macro]
pub fn custom_type(tokens: TokenStream) -> TokenStream {
    let input: CustomTypeInfo = syn::parse_macro_input!(tokens);
    custom::expand_ffi_converter_custom_type(&input.ident, &input.builtin, true)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

/// Generate the `FfiConverter` and the `UniffiCustomTypeConverter` implementations for a
/// Custom Type - ie, for a `<T>` which implements `UniffiCustomTypeConverter` via the
/// newtype idiom.
#[proc_macro]
pub fn custom_newtype(tokens: TokenStream) -> TokenStream {
    let input: CustomTypeInfo = syn::parse_macro_input!(tokens);
    custom::expand_ffi_converter_custom_newtype(&input.ident, &input.builtin, true)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

// == derive_for_udl and export_for_udl ==
//
// The Askama templates generate placeholder items wrapped with these attributes. The goal is to
// have all scaffolding generation go through the same code path.
//
// The one difference is that derive-style attributes are not allowed inside attribute macro
// inputs.  Instead, we take the attributes from the macro invocation itself.
//
// Instead of:
//
// ```
// #[derive(Error)
// #[uniffi(flat_error])
// enum { .. }
// ```
//
// We have:
//
// ```
// #[derive_error_for_udl(flat_error)]
// enum { ... }
//  ```
//
// # Differences between UDL-mode and normal mode
//
// ## Metadata symbols / checksum functions
//
// In UDL mode, we don't export the static metadata symbols or generate the checksum
// functions.  This could be changed, but there doesn't seem to be much benefit at this point.
//
// ## The FfiConverter<UT> parameter
//
// In UDL-mode, we only implement `FfiConverter` for the local tag (`FfiConverter<crate::UniFfiTag>`)
//
// The reason for this split is remote types, i.e. types defined in remote crates that we
// don't control and therefore can't define a blanket impl on because of the orphan rules.
//
// With UDL, we handle this by only implementing `FfiConverter<crate::UniFfiTag>` for the
// type.  This gets around the orphan rules since a local type is in the trait, but requires
// a `uniffi::ffi_converter_forward!` call if the type is used in a second local crate (an
// External typedef).  This is natural for UDL-based generation, since you always need to
// define the external type in the UDL file.
//
// With proc-macros this system isn't so natural.  Instead, we create a blanket implementation
// for all UT and support for remote types is still TODO.

#[doc(hidden)]
#[proc_macro_attribute]
pub fn derive_record_for_udl(_attrs: TokenStream, input: TokenStream) -> TokenStream {
    expand_record(syn::parse_macro_input!(input), true)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[doc(hidden)]
#[proc_macro_attribute]
pub fn derive_enum_for_udl(_attrs: TokenStream, input: TokenStream) -> TokenStream {
    expand_enum(syn::parse_macro_input!(input), true)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[doc(hidden)]
#[proc_macro_attribute]
pub fn derive_error_for_udl(attrs: TokenStream, input: TokenStream) -> TokenStream {
    expand_error(
        syn::parse_macro_input!(input),
        Some(syn::parse_macro_input!(attrs)),
        true,
    )
    .unwrap_or_else(syn::Error::into_compile_error)
    .into()
}

#[doc(hidden)]
#[proc_macro_attribute]
pub fn derive_object_for_udl(_attrs: TokenStream, input: TokenStream) -> TokenStream {
    expand_object(syn::parse_macro_input!(input), true)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[doc(hidden)]
#[proc_macro_attribute]
pub fn export_for_udl(attrs: TokenStream, input: TokenStream) -> TokenStream {
    do_export(attrs, input, true)
}

/// Generate various support elements, including the FfiConverter implementation,
/// for a trait interface for the scaffolding code
#[doc(hidden)]
#[proc_macro]
pub fn expand_trait_interface_support(tokens: TokenStream) -> TokenStream {
    export::ffi_converter_trait_impl(&syn::parse_macro_input!(tokens), true).into()
}

/// Generate the FfiConverter implementation for an trait interface for the scaffolding code
#[doc(hidden)]
#[proc_macro]
pub fn scaffolding_ffi_converter_callback_interface(tokens: TokenStream) -> TokenStream {
    let input: IdentPair = syn::parse_macro_input!(tokens);
    export::ffi_converter_callback_interface_impl(&input.lhs, &input.rhs, true).into()
}

/// A helper macro to include generated component scaffolding.
///
/// This is a simple convenience macro to include the UniFFI component
/// scaffolding as built by `uniffi_build::generate_scaffolding`.
/// Use it like so:
///
/// ```rs
/// uniffi_macros::include_scaffolding!("my_component_name");
/// ```
///
/// This will expand to the appropriate `include!` invocation to include
/// the generated `my_component_name.uniffi.rs` (which it assumes has
/// been successfully built by your crate's `build.rs` script).
#[proc_macro]
pub fn include_scaffolding(udl_stem: TokenStream) -> TokenStream {
    let udl_stem = syn::parse_macro_input!(udl_stem as LitStr);
    if std::env::var("OUT_DIR").is_err() {
        quote! {
            compile_error!("This macro assumes the crate has a build.rs script, but $OUT_DIR is not present");
        }
    } else {
        let toml_path = match util::manifest_path() {
            Ok(path) => path.display().to_string(),
            Err(_) => {
                return quote! {
                    compile_error!("This macro assumes the crate has a build.rs script, but $OUT_DIR is not present");
                }.into();
            }
        };

        quote! {
            // FIXME(HACK):
            // Include the `Cargo.toml` file into the build.
            // That way cargo tracks the file and other tools relying on file
            // tracking see it as well.
            // See https://bugzilla.mozilla.org/show_bug.cgi?id=1846223
            // In the future we should handle that by using the `track_path::path` API,
            // see https://github.com/rust-lang/rust/pull/84029
            #[allow(dead_code)]
            mod __unused {
                const _: &[u8] = include_bytes!(#toml_path);
            }

            include!(concat!(env!("OUT_DIR"), "/", #udl_stem, ".uniffi.rs"));
        }
    }.into()
}

// Use a UniFFI types from dependent crates that uses UDL files
// See the derive_for_udl and export_for_udl section for a discussion of why this is needed.
#[proc_macro]
pub fn use_udl_record(tokens: TokenStream) -> TokenStream {
    use_udl_simple_type(tokens)
}

#[proc_macro]
pub fn use_udl_enum(tokens: TokenStream) -> TokenStream {
    use_udl_simple_type(tokens)
}

#[proc_macro]
pub fn use_udl_error(tokens: TokenStream) -> TokenStream {
    use_udl_simple_type(tokens)
}

fn use_udl_simple_type(tokens: TokenStream) -> TokenStream {
    let util::ExternalTypeItem {
        crate_ident,
        type_ident,
        ..
    } = parse_macro_input!(tokens);
    quote! {
        ::uniffi::ffi_converter_forward!(#type_ident, #crate_ident::UniFfiTag, crate::UniFfiTag);
    }
    .into()
}

#[proc_macro]
pub fn use_udl_object(tokens: TokenStream) -> TokenStream {
    let util::ExternalTypeItem {
        crate_ident,
        type_ident,
        ..
    } = parse_macro_input!(tokens);
    quote! {
        ::uniffi::ffi_converter_arc_forward!(#type_ident, #crate_ident::UniFfiTag, crate::UniFfiTag);
    }.into()
}

/// A helper macro to generate and include component scaffolding.
///
/// This is a convenience macro designed for writing `trybuild`-style tests and
/// probably shouldn't be used for production code. Given the path to a `.udl` file,
/// if will run `uniffi-bindgen` to produce the corresponding Rust scaffolding and then
/// include it directly into the calling file. Like so:
///
/// ```rs
/// uniffi_macros::generate_and_include_scaffolding!("path/to/my/interface.udl");
/// ```
#[proc_macro]
pub fn generate_and_include_scaffolding(udl_file: TokenStream) -> TokenStream {
    let udl_file = syn::parse_macro_input!(udl_file as LitStr);
    let udl_file_string = udl_file.value();
    let udl_file_path = Utf8Path::new(&udl_file_string);
    if std::env::var("OUT_DIR").is_err() {
        quote! {
            compile_error!("This macro assumes the crate has a build.rs script, but $OUT_DIR is not present");
        }
    } else if let Err(e) = uniffi_build::generate_scaffolding(udl_file_path) {
        let err = format!("{e:#}");
        quote! {
            compile_error!(concat!("Failed to generate scaffolding from UDL file at ", #udl_file, ": ", #err));
        }
    } else {
        // We know the filename is good because `generate_scaffolding` succeeded,
        // so this `unwrap` will never fail.
        let name = LitStr::new(udl_file_path.file_stem().unwrap(), udl_file.span());
        quote! {
            uniffi_macros::include_scaffolding!(#name);
        }
    }.into()
}

/// A dummy macro that does nothing.
///
/// This exists so `#[uniffi::export]` can emit its input verbatim without
/// causing unexpected errors, plus some extra code in case everything is okay.
///
/// It is important for `#[uniffi::export]` to not raise unexpected errors if it
/// fails to parse the input as this happens very often when the proc-macro is
/// run on an incomplete input by rust-analyzer while the developer is typing.
#[proc_macro_attribute]
pub fn constructor(_attrs: TokenStream, input: TokenStream) -> TokenStream {
    input
}
