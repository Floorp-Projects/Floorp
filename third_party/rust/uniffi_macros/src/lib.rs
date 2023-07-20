/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#![cfg_attr(feature = "nightly", feature(proc_macro_expand))]

//! Macros for `uniffi`.
//!
//! Currently this is just for easily generating integration tests, but maybe
//! we'll put some other code-annotation helper macros in here at some point.

use camino::Utf8Path;
use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, LitStr};
use util::rewrite_self_type;

mod enum_;
mod error;
mod export;
mod object;
mod record;
mod test;
mod util;

use self::{
    enum_::expand_enum, error::expand_error, export::expand_export, object::expand_object,
    record::expand_record,
};

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

#[proc_macro_attribute]
pub fn export(_attr: TokenStream, input: TokenStream) -> TokenStream {
    let input2 = proc_macro2::TokenStream::from(input.clone());

    let gen_output = || {
        let mod_path = util::mod_path()?;
        let mut item = syn::parse(input)?;

        // If the input is an `impl` block, rewrite any uses of the `Self` type
        // alias to the actual type, so we don't have to special-case it in the
        // metadata collection or scaffolding code generation (which generates
        // new functions outside of the `impl`).
        rewrite_self_type(&mut item);

        let metadata = export::gen_metadata(item, &mod_path)?;
        Ok(expand_export(metadata, &mod_path))
    };
    let output = gen_output().unwrap_or_else(syn::Error::into_compile_error);

    quote! {
        #input2
        #output
    }
    .into()
}

#[proc_macro_derive(Record)]
pub fn derive_record(input: TokenStream) -> TokenStream {
    let mod_path = match util::mod_path() {
        Ok(p) => p,
        Err(e) => return e.into_compile_error().into(),
    };
    let input = parse_macro_input!(input);

    expand_record(input, mod_path).into()
}

#[proc_macro_derive(Enum)]
pub fn derive_enum(input: TokenStream) -> TokenStream {
    let mod_path = match util::mod_path() {
        Ok(p) => p,
        Err(e) => return e.into_compile_error().into(),
    };
    let input = parse_macro_input!(input);

    expand_enum(input, mod_path).into()
}

#[proc_macro_derive(Object)]
pub fn derive_object(input: TokenStream) -> TokenStream {
    let mod_path = match util::mod_path() {
        Ok(p) => p,
        Err(e) => return e.into_compile_error().into(),
    };
    let input = parse_macro_input!(input);

    expand_object(input, mod_path).into()
}

#[proc_macro_derive(Error, attributes(uniffi))]
pub fn derive_error(input: TokenStream) -> TokenStream {
    let mod_path = match util::mod_path() {
        Ok(p) => p,
        Err(e) => return e.into_compile_error().into(),
    };
    let input = parse_macro_input!(input);

    expand_error(input, mod_path).into()
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
///
#[proc_macro]
pub fn include_scaffolding(component_name: TokenStream) -> TokenStream {
    let name = syn::parse_macro_input!(component_name as syn::LitStr);
    if std::env::var("OUT_DIR").is_err() {
        quote! {
            compile_error!("This macro assumes the crate has a build.rs script, but $OUT_DIR is not present");
        }
    } else {
        quote! {
            include!(concat!(env!("OUT_DIR"), "/", #name, ".uniffi.rs"));
        }
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
///
#[proc_macro]
pub fn generate_and_include_scaffolding(udl_file: TokenStream) -> TokenStream {
    let udl_file = syn::parse_macro_input!(udl_file as syn::LitStr);
    let udl_file_string = udl_file.value();
    let udl_file_path = Utf8Path::new(&udl_file_string);
    if std::env::var("OUT_DIR").is_err() {
        quote! {
            compile_error!("This macro assumes the crate has a build.rs script, but $OUT_DIR is not present");
        }
    } else if uniffi_build::generate_scaffolding(udl_file_path).is_err() {
        quote! {
            compile_error!(concat!("Failed to generate scaffolding from UDL file at ", #udl_file));
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
