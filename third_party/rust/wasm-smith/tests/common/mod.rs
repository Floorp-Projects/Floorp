use wasm_smith::Config;
use wasmparser::{types::Types, Validator, WasmFeatures};

pub fn parser_features_from_config(config: &Config) -> WasmFeatures {
    let mut features = WasmFeatures::MUTABLE_GLOBAL | WasmFeatures::FLOATS;
    features.set(
        WasmFeatures::SATURATING_FLOAT_TO_INT,
        config.saturating_float_to_int_enabled,
    );
    features.set(
        WasmFeatures::SIGN_EXTENSION,
        config.sign_extension_ops_enabled,
    );
    features.set(
        WasmFeatures::REFERENCE_TYPES,
        config.reference_types_enabled,
    );
    features.set(WasmFeatures::MULTI_VALUE, config.multi_value_enabled);
    features.set(WasmFeatures::BULK_MEMORY, config.bulk_memory_enabled);
    features.set(WasmFeatures::SIMD, config.simd_enabled);
    features.set(WasmFeatures::RELAXED_SIMD, config.relaxed_simd_enabled);
    features.set(WasmFeatures::MULTI_MEMORY, config.max_memories > 1);
    features.set(WasmFeatures::EXCEPTIONS, config.exceptions_enabled);
    features.set(WasmFeatures::MEMORY64, config.memory64_enabled);
    features.set(WasmFeatures::TAIL_CALL, config.tail_call_enabled);
    features.set(WasmFeatures::FUNCTION_REFERENCES, config.gc_enabled);
    features.set(WasmFeatures::GC, config.gc_enabled);
    features.set(
        WasmFeatures::CUSTOM_PAGE_SIZES,
        config.custom_page_sizes_enabled,
    );
    features
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
