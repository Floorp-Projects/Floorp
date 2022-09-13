/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use proc_macro2::Span;

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
    use quote::quote;

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
