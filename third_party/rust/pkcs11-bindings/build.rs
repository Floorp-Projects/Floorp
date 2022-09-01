/* -*- Mode: rust; rust-indent-offset: 4 -*- */

extern crate bindgen;

use bindgen::callbacks::*;
use bindgen::*;

use std::env;
use std::path::PathBuf;

#[derive(Debug)]
struct PKCS11TypesParseCallbacks;

impl ParseCallbacks for PKCS11TypesParseCallbacks {
    fn will_parse_macro(&self, _name: &str) -> MacroParsingBehavior {
        MacroParsingBehavior::Default
    }

    fn int_macro(&self, name: &str, _value: i64) -> Option<IntKind> {
        if name == "CK_TRUE" {
            Some(IntKind::U8)
        } else {
            Some(IntKind::ULong)
        }
    }

    fn str_macro(&self, _name: &str, _value: &[u8]) {}

    fn func_macro(&self, _name: &str, _value: &[&[u8]]) {}

    fn enum_variant_behavior(
        &self,
        _enum_name: Option<&str>,
        _original_variant_name: &str,
        _variant_value: EnumVariantValue,
    ) -> Option<EnumVariantCustomBehavior> {
        None
    }

    fn enum_variant_name(
        &self,
        _enum_name: Option<&str>,
        _original_variant_name: &str,
        _variant_value: EnumVariantValue,
    ) -> Option<String> {
        None
    }

    fn item_name(&self, _original_item_name: &str) -> Option<String> {
        None
    }

    fn include_file(&self, _filename: &str) {}

    fn blocklisted_type_implements_trait(
        &self,
        _name: &str,
        _derive_trait: DeriveTrait,
    ) -> Option<ImplementsTrait> {
        None
    }

    fn add_derives(&self, _name: &str) -> Vec<String> {
        Vec::new()
    }
}

fn main() {
    println!("cargo:rerun-if-changed=wrapper.h");

    let bindings = Builder::default()
        .header("wrapper.h")
        .allowlist_function("C_GetFunctionList")
        .allowlist_type("CK_RSA_PKCS_PSS_PARAMS")
        .allowlist_type("CK_OBJECT_CLASS")
        .allowlist_type("CK_KEY_TYPE")
        .allowlist_type("CK_C_INITIALIZE_ARGS_PTR")
        .allowlist_var("CK_TRUE")
        .allowlist_var("CKA_.*")
        .allowlist_var("CKF_.*")
        .allowlist_var("CKK_.*")
        .allowlist_var("CKM_.*")
        .allowlist_var("CKO_.*")
        .allowlist_var("CKR_.*")
        .derive_default(true)
        .parse_callbacks(Box::new(PKCS11TypesParseCallbacks))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR should be set in env"));
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
