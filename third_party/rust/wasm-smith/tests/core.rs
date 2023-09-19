use arbitrary::{Arbitrary, Unstructured};
use rand::{rngs::SmallRng, RngCore, SeedableRng};
use std::collections::HashMap;
use wasm_smith::{Config, ConfiguredModule, Module, SwarmConfig};
use wasmparser::{Parser, TypeRef, ValType, Validator, WasmFeatures};

#[test]
fn smoke_test_module() {
    let mut rng = SmallRng::seed_from_u64(0);
    let mut buf = vec![0; 2048];
    for _ in 0..1024 {
        rng.fill_bytes(&mut buf);
        let u = Unstructured::new(&buf);
        if let Ok(module) = Module::arbitrary_take_rest(u) {
            let wasm_bytes = module.to_bytes();

            let mut validator = Validator::new_with_features(wasm_features());
            validate(&mut validator, &wasm_bytes);
        }
    }
}

#[test]
fn smoke_test_ensure_termination() {
    let mut rng = SmallRng::seed_from_u64(0);
    let mut buf = vec![0; 2048];
    for _ in 0..1024 {
        rng.fill_bytes(&mut buf);
        let u = Unstructured::new(&buf);
        if let Ok(mut module) = Module::arbitrary_take_rest(u) {
            module.ensure_termination(10);
            let wasm_bytes = module.to_bytes();

            let mut validator = Validator::new_with_features(wasm_features());
            validate(&mut validator, &wasm_bytes);
        }
    }
}

#[test]
fn smoke_test_swarm_config() {
    let mut rng = SmallRng::seed_from_u64(0);
    let mut buf = vec![0; 2048];
    for _ in 0..1024 {
        rng.fill_bytes(&mut buf);
        let u = Unstructured::new(&buf);
        if let Ok(module) = ConfiguredModule::<SwarmConfig>::arbitrary_take_rest(u) {
            let module = module.module;
            let wasm_bytes = module.to_bytes();

            let mut validator = Validator::new_with_features(wasm_features());
            validate(&mut validator, &wasm_bytes);
        }
    }
}

#[test]
fn multi_value_disabled() {
    let mut rng = SmallRng::seed_from_u64(42);
    let mut buf = vec![0; 2048];
    for _ in 0..10 {
        rng.fill_bytes(&mut buf);
        let mut u = Unstructured::new(&buf);
        let mut cfg = SwarmConfig::arbitrary(&mut u).unwrap();
        cfg.multi_value_enabled = false;
        if let Ok(module) = Module::new(cfg, &mut u) {
            let wasm_bytes = module.to_bytes();
            let mut features = wasm_features();
            features.multi_value = false;
            let mut validator = Validator::new_with_features(features);
            validate(&mut validator, &wasm_bytes);
        }
    }
}

#[test]
fn smoke_can_smith_valid_webassembly_one_point_oh() {
    let mut rng = SmallRng::seed_from_u64(42);
    let mut buf = vec![0; 10240];
    for _ in 0..100 {
        rng.fill_bytes(&mut buf);
        let mut u = Unstructured::new(&buf);
        let mut cfg = SwarmConfig::arbitrary(&mut u).unwrap();
        cfg.sign_extension_enabled = false;
        cfg.saturating_float_to_int_enabled = false;
        cfg.reference_types_enabled = false;
        cfg.multi_value_enabled = false;
        cfg.bulk_memory_enabled = false;
        cfg.simd_enabled = false;
        cfg.relaxed_simd_enabled = false;
        cfg.exceptions_enabled = false;
        cfg.memory64_enabled = false;
        cfg.max_memories = 1;
        cfg.max_tables = 1;
        let features = parser_features_from_config(&cfg);
        if let Ok(module) = Module::new(cfg, &mut u) {
            let wasm_bytes = module.to_bytes();
            // This table should set to `true` only features specified in wasm-core-1 spec.
            let mut validator = Validator::new_with_features(features);
            validate(&mut validator, &wasm_bytes);
        }
    }
}

#[test]
fn smoke_test_imports_config() {
    let mut n_partial = 0;
    let mut global_imports_seen = HashMap::<_, bool>::new();
    let mut rng = SmallRng::seed_from_u64(11);
    let mut buf = vec![0; 512];
    for _ in 0..1024 {
        rng.fill_bytes(&mut buf);

        let mut u = Unstructured::new(&buf);
        let (config, available) = import_config(&mut u);
        let features = parser_features_from_config(&config);

        if let Ok(module) = Module::new(config, &mut u) {
            let wasm_bytes = module.to_bytes();
            let mut validator = Validator::new_with_features(features);
            validate(&mut validator, &wasm_bytes);
            let mut imports_seen = available
                .iter()
                .map(|(m, f, t)| ((*m, *f), (false, t)))
                .collect::<HashMap<_, _>>();
            let mut sig_types = Vec::new();

            for payload in Parser::new(0).parse_all(&wasm_bytes) {
                let payload = payload.unwrap();
                if let wasmparser::Payload::TypeSection(rdr) = payload {
                    // Gather the signature types to later check function types against.
                    for ty in rdr.into_iter_err_on_gc_types() {
                        sig_types.push(ty.unwrap());
                    }
                } else if let wasmparser::Payload::ImportSection(rdr) = payload {
                    // Read out imports, checking that they all are within the list of expected
                    // imports (i.e. we don't generate arbitrary ones), and that we handle the
                    // logic correctly (i.e. signature types are as expected)
                    for import in rdr {
                        let import = import.unwrap();
                        use AvailableImportKind as I;
                        let entry = imports_seen.get_mut(&(import.module, import.name));
                        match (entry, &import.ty) {
                            (Some((true, _)), _) => panic!("duplicate import of {:?}", import),
                            (Some((seen, I::Memory)), TypeRef::Memory(_)) => *seen = true,
                            (Some((seen, I::Global(t))), TypeRef::Global(gt))
                                if *t == gt.content_type =>
                            {
                                *seen = true
                            }
                            (Some((seen, I::Table(t))), TypeRef::Table(tt))
                                if *t == ValType::Ref(tt.element_type) =>
                            {
                                *seen = true
                            }
                            (Some((seen, I::Func(p, r))), TypeRef::Func(sig_idx))
                                if sig_types[*sig_idx as usize].params() == *p
                                    && sig_types[*sig_idx as usize].results() == *r =>
                            {
                                *seen = true
                            }
                            (
                                Some((seen, I::Tag(p))),
                                TypeRef::Tag(wasmparser::TagType { func_type_idx, .. }),
                            ) if sig_types[*func_type_idx as usize].params() == *p
                                && sig_types[*func_type_idx as usize].results().is_empty() =>
                            {
                                *seen = true
                            }
                            (Some((_, expected)), _) => panic!(
                                "import {:?} type mismatch, expected: {:?}",
                                import, expected
                            ),
                            (None, _) => panic!("import of an unknown entity: {:?}", import),
                        }
                    }
                }
            }

            // Verify that we have seen both instances with partial imports (i.e. we don't always
            // just copy over all the imports from the example module) and also that we eventually
            // observe all of the imports being used (i.e. selection is reasonably random)
            for (m, f, _) in &available[..] {
                let seen = imports_seen[&(*m, *f)];
                let global_seen = global_imports_seen
                    .entry((m.to_string(), f.to_string()))
                    .or_default();
                *global_seen |= seen.0;
            }
            if !imports_seen.values().all(|v| v.0) {
                n_partial += 1;
            }
        }
    }
    assert!(global_imports_seen.values().all(|v| *v));
    assert!(n_partial > 0);
}

#[test]
fn smoke_test_no_trapping_mode() {
    let mut rng = SmallRng::seed_from_u64(0);
    let mut buf = vec![0; 2048];
    for _ in 0..1024 {
        rng.fill_bytes(&mut buf);
        let mut u = Unstructured::new(&buf);
        let mut cfg = SwarmConfig::arbitrary(&mut u).unwrap();
        cfg.disallow_traps = true;
        if let Ok(module) = Module::new(cfg, &mut u) {
            let wasm_bytes = module.to_bytes();
            let mut validator = Validator::new_with_features(wasm_features());
            validate(&mut validator, &wasm_bytes);
        }
    }
}

fn wasm_features() -> WasmFeatures {
    WasmFeatures {
        multi_memory: true,
        relaxed_simd: true,
        memory64: true,
        exceptions: true,
        tail_call: true,
        ..WasmFeatures::default()
    }
}

#[derive(Debug)]
enum AvailableImportKind {
    Func(&'static [ValType], &'static [ValType]),
    Tag(&'static [ValType]),
    Global(ValType),
    Table(ValType),
    Memory,
}

fn import_config(
    u: &mut Unstructured,
) -> (
    SwarmConfig,
    Vec<(&'static str, &'static str, AvailableImportKind)>,
) {
    let mut config = SwarmConfig::arbitrary(u).expect("arbitrary swarm");
    config.exceptions_enabled = u.arbitrary().expect("exceptions enabled for swarm");
    let available = {
        use {AvailableImportKind::*, ValType::*};
        vec![
            ("env", "pi", Func(&[I32], &[])),
            ("env", "pi2", Func(&[I32], &[])),
            ("env", "pipi2", Func(&[I32, I32], &[])),
            ("env", "po", Func(&[], &[I32])),
            ("env", "pipo", Func(&[I32], &[I32])),
            ("env", "popo", Func(&[], &[I32, I32])),
            ("env", "mem", Memory),
            ("env", "tbl", Table(ValType::FUNCREF)),
            ("vars", "g", Global(I64)),
            ("tags", "tag1", Tag(&[I32])),
        ]
    };
    config.available_imports = Some(
        wat::parse_str(
            r#"
            (module
                (import "env" "pi" (func (param i32)))
                (import "env" "pi2" (func (param i32)))
                (import "env" "pipi2" (func (param i32 i32)))
                (import "env" "po" (func (result i32)))
                (import "env" "pipo" (func (param i32) (result i32)))
                (import "env" "popo" (func (result i32 i32)))
                (import "env" "mem" (memory 1 16))
                (import "env" "tbl" (table 1 16 funcref))
                (import "vars" "g" (global i64))
                (import "tags" "tag1" (tag (param i32)))
            )
            "#,
        )
        .unwrap()
        .into(),
    );
    (config, available)
}

fn parser_features_from_config(config: &impl Config) -> WasmFeatures {
    WasmFeatures {
        mutable_global: true,
        saturating_float_to_int: config.saturating_float_to_int_enabled(),
        sign_extension: config.sign_extension_ops_enabled(),
        reference_types: config.reference_types_enabled(),
        multi_value: config.multi_value_enabled(),
        bulk_memory: config.bulk_memory_enabled(),
        simd: config.simd_enabled(),
        relaxed_simd: config.relaxed_simd_enabled(),
        multi_memory: config.max_memories() > 1,
        exceptions: config.exceptions_enabled(),
        memory64: config.memory64_enabled(),
        tail_call: config.tail_call_enabled(),

        threads: false,
        floats: true,
        extended_const: false,
        component_model: false,
        function_references: false,
        memory_control: false,
        gc: false,
        component_model_values: false,
    }
}

fn validate(validator: &mut Validator, bytes: &[u8]) {
    let err = match validator.validate_all(bytes) {
        Ok(_) => return,
        Err(e) => e,
    };
    drop(std::fs::write("test.wasm", &bytes));
    if let Ok(text) = wasmprinter::print_bytes(bytes) {
        drop(std::fs::write("test.wat", &text));
    }
    panic!("wasm failed to validate {:?}", err);
}
