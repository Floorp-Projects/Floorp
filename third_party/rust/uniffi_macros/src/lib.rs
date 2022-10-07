/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#![cfg_attr(feature = "nightly", feature(proc_macro_expand))]

//! Macros for `uniffi`.
//!
//! Currently this is just for easily generating integration tests, but maybe
//! we'll put some other code-annotation helper macros in here at some point.

use camino::{Utf8Path, Utf8PathBuf};
use quote::{format_ident, quote};
use std::env;
use syn::{bracketed, punctuated::Punctuated, LitStr, Token};

mod export;
mod util;

use self::export::expand_export;

#[proc_macro_attribute]
pub fn export(
    _attr: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let input2 = proc_macro2::TokenStream::from(input.clone());

    let gen_output = || {
        let mod_path = util::mod_path()?;
        let item = syn::parse(input)?;
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
/// To use it, invoke the macro with one or more udl files as the first argument, then
/// one or more file paths relative to the crate root directory.
/// It will produce one `#[test]` function per file, in a manner designed to
/// play nicely with `cargo test` and its test filtering options.
#[proc_macro]
pub fn build_foreign_language_testcases(paths: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let paths = syn::parse_macro_input!(paths as FilePaths);
    // We resolve each path relative to the crate root directory.
    let pkg_dir = env::var("CARGO_MANIFEST_DIR")
        .expect("Missing $CARGO_MANIFEST_DIR, cannot build tests for generated bindings");

    // Create an array of UDL files.
    let udl_files = &paths
        .udl_files
        .iter()
        .map(|file_path| {
            let pathbuf: Utf8PathBuf = [&pkg_dir, file_path].iter().collect();
            let path = pathbuf.to_string();
            quote! { #path }
        })
        .collect::<Vec<proc_macro2::TokenStream>>();

    // For each test file found, generate a matching testcase.
    let test_functions = paths.test_scripts
        .iter()
        .map(|file_path| {
            let test_file_pathbuf: Utf8PathBuf = [&pkg_dir, file_path].iter().collect();
            let test_file_path = test_file_pathbuf.to_string();
            let test_file_name = test_file_pathbuf
                .file_name()
                .expect("Test file has no name, cannot build tests for generated bindings");
            let test_name = format_ident!(
                "uniffi_foreign_language_testcase_{}",
                test_file_name.replace(|c: char| !c.is_alphanumeric(), "_")
            );
            let maybe_ignore = if should_skip_path(&test_file_pathbuf) {
                quote! { #[ignore] }
            } else {
                quote! { }
            };
            quote! {
                #maybe_ignore
                #[test]
                fn #test_name () -> uniffi::deps::anyhow::Result<()> {
                    uniffi::testing::run_foreign_language_testcase(#pkg_dir, &[ #(#udl_files),* ], #test_file_path)
                }
            }
        })
        .collect::<Vec<proc_macro2::TokenStream>>();
    let test_module = quote! {
        #(#test_functions)*
    };
    proc_macro::TokenStream::from(test_module)
}

// UNIFFI_TESTS_DISABLE_EXTENSIONS contains a comma-sep'd list of extensions (without leading `.`)
fn should_skip_path(path: &Utf8Path) -> bool {
    let ext = path.extension().expect("File has no extension!");
    env::var("UNIFFI_TESTS_DISABLE_EXTENSIONS")
        .map(|v| v.split(',').any(|look| look == ext))
        .unwrap_or(false)
}

/// Newtype to simplifying parsing a list of file paths from macro input.
#[derive(Debug)]
struct FilePaths {
    udl_files: Vec<String>,
    test_scripts: Vec<String>,
}

impl syn::parse::Parse for FilePaths {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let udl_array;
        bracketed!(udl_array in input);
        let udl_files = Punctuated::<LitStr, Token![,]>::parse_terminated(&udl_array)?
            .iter()
            .map(|s| s.value())
            .collect();

        let _comma: Token![,] = input.parse()?;

        let scripts_array;
        bracketed!(scripts_array in input);
        let test_scripts = Punctuated::<LitStr, Token![,]>::parse_terminated(&scripts_array)?
            .iter()
            .map(|s| s.value())
            .collect();

        Ok(FilePaths {
            udl_files,
            test_scripts,
        })
    }
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
pub fn include_scaffolding(component_name: proc_macro::TokenStream) -> proc_macro::TokenStream {
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
pub fn generate_and_include_scaffolding(
    udl_file: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
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
