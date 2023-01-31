/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use camino::{Utf8Path, Utf8PathBuf};
use proc_macro::TokenStream;
use quote::{format_ident, quote};
use std::env;
use syn::{parse_macro_input, punctuated::Punctuated, LitStr, Token};

pub(crate) fn build_foreign_language_testcases(tokens: TokenStream) -> TokenStream {
    let input = parse_macro_input!(tokens as BuildForeignLanguageTestCaseInput);
    // we resolve each path relative to the crate root directory.
    let pkg_dir = env::var("CARGO_MANIFEST_DIR")
        .expect("Missing $CARGO_MANIFEST_DIR, cannot build tests for generated bindings");

    // For each test file found, generate a matching testcase.
    let test_functions = input
        .test_scripts
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
            let run_test = match test_file_pathbuf.extension() {
                Some("kts") => quote! {
                    uniffi::kotlin_run_test
                },
                Some("swift") => quote! {
                    uniffi::swift_run_test
                },
                Some("py") => quote! {
                    uniffi::python_run_test
                },
                Some("rb") => quote! {
                    uniffi::ruby_run_test
                },
                _ => panic!("Unexpected extension for test script: {test_file_name}"),
            };
            let maybe_ignore = if should_skip_path(&test_file_pathbuf) {
                quote! { #[ignore] }
            } else {
                quote! {}
            };
            quote! {
                #maybe_ignore
                #[test]
                fn #test_name () -> uniffi::deps::anyhow::Result<()> {
                    #run_test(
                        std::env!("CARGO_TARGET_TMPDIR"),
                        std::env!("CARGO_PKG_NAME"),
                        #test_file_path)
                }
            }
        })
        .collect::<Vec<proc_macro2::TokenStream>>();
    let test_module = quote! {
        #(#test_functions)*
    };
    TokenStream::from(test_module)
}

// UNIFFI_TESTS_DISABLE_EXTENSIONS contains a comma-sep'd list of extensions (without leading `.`)
fn should_skip_path(path: &Utf8Path) -> bool {
    let ext = path.extension().expect("File has no extension!");
    env::var("UNIFFI_TESTS_DISABLE_EXTENSIONS")
        .map(|v| v.split(',').any(|look| look == ext))
        .unwrap_or(false)
}

struct BuildForeignLanguageTestCaseInput {
    test_scripts: Vec<String>,
}

impl syn::parse::Parse for BuildForeignLanguageTestCaseInput {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let test_scripts = Punctuated::<LitStr, Token![,]>::parse_terminated(input)?
            .iter()
            .map(|s| s.value())
            .collect();

        Ok(Self { test_scripts })
    }
}
