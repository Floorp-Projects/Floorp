#![cfg(feature = "wasmparser")]

use arbitrary::{Arbitrary, Unstructured};
use rand::{rngs::SmallRng, RngCore, SeedableRng};
use wasm_smith::{Config, Module};
use wasmparser::{
    types::EntityType, CompositeType, FuncType, GlobalType, Parser, Validator, WasmFeatures,
};

mod common;
use common::{parser_features_from_config, validate};

#[derive(Debug, PartialEq)]
enum ExportType {
    Func(FuncType),
    Global(GlobalType),
}

#[test]
fn smoke_test_single_export() {
    let test = r#"
        (module
            (func (export "foo") (param i32) (result i64)
                unreachable
            )
        )
        "#;
    smoke_test_exports(test, 11)
}

#[test]
fn smoke_test_multiple_exports() {
    let test = r#"
        (module
            (func (export "a") (param i32) (result i64)
                unreachable
            )
            (func (export "b")
                unreachable
            )
            (func (export "c")
                unreachable
            )
        )
        "#;
    smoke_test_exports(test, 12)
}

#[test]
fn smoke_test_exported_global() {
    let test = r#"
        (module
            (func (export "a") (param i32 i32 f32 f64) (result f32)
                unreachable
            )
            (global (export "glob") f64 f64.const 0)
        )
        "#;
    smoke_test_exports(test, 20)
}

#[test]
fn smoke_test_export_with_imports() {
    let test = r#"
        (module
            (import "" "foo" (func (param i32)))
            (import "" "bar" (global (mut f32)))
            (func (param i64) unreachable)
            (global i32 (i32.const 0))
            (export "a" (func 0))
            (export "b" (global 0))
            (export "c" (func 1))
            (export "d" (global 1))
        )
            "#;
    smoke_test_exports(test, 21)
}

#[test]
fn smoke_test_with_mutable_global_exports() {
    let test = r#"
        (module
            (global (export "1i32") (mut i32) (i32.const 0))
            (global (export "2i32") (mut i32) (i32.const 0))
            (global (export "1i64") (mut i64) (i64.const 0))
            (global (export "2i64") (mut i64) (i64.const 0))
            (global (export "3i32") (mut i32) (i32.const 0))
            (global (export "3i64") (mut i64) (i64.const 0))
            (global (export "4i32") i32 (i32.const 0))
            (global (export "4i64") i64 (i64.const 0))
        )"#;
    smoke_test_exports(test, 22)
}

fn get_func_and_global_exports(features: WasmFeatures, module: &[u8]) -> Vec<(String, ExportType)> {
    let mut validator = Validator::new_with_features(features);
    let types = validate(&mut validator, module);
    let mut exports = vec![];

    for payload in Parser::new(0).parse_all(module) {
        let payload = payload.unwrap();
        if let wasmparser::Payload::ExportSection(rdr) = payload {
            for export in rdr {
                let export = export.unwrap();
                match types.entity_type_from_export(&export).unwrap() {
                    EntityType::Func(core_id) => {
                        let sub_type = types.get(core_id).expect("Failed to lookup core id");
                        assert!(sub_type.is_final);
                        assert!(sub_type.supertype_idx.is_none());
                        let CompositeType::Func(func_type) = &sub_type.composite_type else {
                            panic!("Expected Func CompositeType, but found {:?}", sub_type);
                        };
                        exports
                            .push((export.name.to_string(), ExportType::Func(func_type.clone())));
                    }
                    EntityType::Global(global_type) => {
                        exports.push((export.name.to_string(), ExportType::Global(global_type)))
                    }
                    other => {
                        panic!("Unexpected entity type {:?}", other)
                    }
                }
            }
        }
    }
    exports
}

fn smoke_test_exports(exports_test_case: &str, seed: u64) {
    let mut rng = SmallRng::seed_from_u64(seed);
    let mut buf = vec![0; 512];
    let wasm = wat::parse_str(exports_test_case).unwrap();
    let expected_exports = get_func_and_global_exports(WasmFeatures::default(), &wasm);

    for _ in 0..1024 {
        rng.fill_bytes(&mut buf);
        let mut u = Unstructured::new(&buf);

        let mut config = Config::arbitrary(&mut u).expect("arbitrary config");
        config.exports = Some(wasm.clone());

        let features = parser_features_from_config(&config);
        let module = Module::new(config, &mut u).unwrap();
        let wasm_bytes = module.to_bytes();

        let generated_exports = get_func_and_global_exports(features, &wasm_bytes);
        assert_eq!(expected_exports, generated_exports);
    }
}
