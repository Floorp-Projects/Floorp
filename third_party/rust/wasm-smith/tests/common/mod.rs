use wasm_smith::Config;
use wasmparser::{types::Types, Validator, WasmFeatures};

pub fn parser_features_from_config(config: &Config) -> WasmFeatures {
    WasmFeatures {
        mutable_global: true,
        saturating_float_to_int: config.saturating_float_to_int_enabled,
        sign_extension: config.sign_extension_ops_enabled,
        reference_types: config.reference_types_enabled,
        multi_value: config.multi_value_enabled,
        bulk_memory: config.bulk_memory_enabled,
        simd: config.simd_enabled,
        relaxed_simd: config.relaxed_simd_enabled,
        multi_memory: config.max_memories > 1,
        exceptions: config.exceptions_enabled,
        memory64: config.memory64_enabled,
        tail_call: config.tail_call_enabled,
        function_references: config.gc_enabled,
        gc: config.gc_enabled,

        threads: false,
        floats: true,
        extended_const: false,
        component_model: false,
        memory_control: false,
        component_model_values: false,
        component_model_nested_names: false,
    }
}

pub fn validate(validator: &mut Validator, bytes: &[u8]) -> Types {
    let err = match validator.validate_all(bytes) {
        Ok(types) => return types,
        Err(e) => e,
    };
    eprintln!("Writing Wasm to `test.wasm`");
    drop(std::fs::write("test.wasm", &bytes));
    if let Ok(text) = wasmprinter::print_bytes(bytes) {
        eprintln!("Writing WAT to `test.wat`");
        drop(std::fs::write("test.wat", &text));
    }
    panic!("wasm failed to validate: {}", err);
}
