#[macro_use]
extern crate criterion;

use anyhow::Result;
use criterion::{black_box, Criterion};
use std::fs;
use std::path::Path;
use std::path::PathBuf;
use wasmparser::{
    validate, OperatorValidatorConfig, Parser, ParserState, ValidatingParser,
    ValidatingParserConfig, WasmDecoder,
};

const VALIDATOR_CONFIG: Option<ValidatingParserConfig> = Some(ValidatingParserConfig {
    operator_config: OperatorValidatorConfig {
        enable_threads: true,
        enable_reference_types: true,
        enable_simd: true,
        enable_bulk_memory: true,
        enable_multi_value: true,
        enable_tail_call: true,
        enable_module_linking: true,
    },
});

/// A benchmark input.
pub struct BenchmarkInput {
    /// The path to the benchmark file important for handling errors.
    pub path: PathBuf,
    /// The encoded Wasm module that is run by the benchmark.
    pub wasm: Vec<u8>,
}

impl BenchmarkInput {
    /// Creates a new benchmark input.
    pub fn new(test_path: PathBuf, encoded_wasm: Vec<u8>) -> Self {
        Self {
            path: test_path,
            wasm: encoded_wasm,
        }
    }
}

/// Returns a vector of all found benchmark input files under the given directory.
///
/// Benchmark input files can be `.wat` or `.wast` formatted files.
/// For `.wast` files we pull out all the module directives and run them in the benchmarks.
fn collect_test_files(path: &Path, list: &mut Vec<BenchmarkInput>) -> Result<()> {
    for entry in path.read_dir()? {
        let entry = entry?;
        let path = entry.path();
        if path.is_dir() {
            collect_test_files(&path, list)?;
            continue;
        }
        match path.extension().and_then(|ext| ext.to_str()) {
            Some("wasm") => {
                let wasm = fs::read(&path)?;
                list.push(BenchmarkInput::new(path, wasm));
            }
            Some("wat") | Some("txt") => {
                if let Ok(wasm) = wat::parse_file(&path) {
                    list.push(BenchmarkInput::new(path, wasm));
                }
            }
            Some("wast") => {
                let contents = fs::read_to_string(&path)?;
                let buf = match wast::parser::ParseBuffer::new(&contents) {
                    Ok(buf) => buf,
                    Err(_) => continue,
                };
                let wast: wast::Wast<'_> = match wast::parser::parse(&buf) {
                    Ok(wast) => wast,
                    Err(_) => continue,
                };
                for directive in wast.directives {
                    match directive {
                        wast::WastDirective::Module(mut module) => {
                            let wasm = module.encode()?;
                            list.push(BenchmarkInput::new(path.clone(), wasm));
                        }
                        _ => continue,
                    }
                }
            }
            _ => (),
        }
    }
    Ok(())
}

/// Reads the input given the Wasm parser or validator.
///
/// The `path` specifies which benchmark input file we are currently operating on
/// so that we can report better errors in case of failures.
fn read_all_wasm<'a, T>(path: &PathBuf, mut d: T)
where
    T: WasmDecoder<'a>,
{
    loop {
        match *d.read() {
            ParserState::Error(ref e) => {
                panic!("unexpected error while reading Wasm from {:?}: {}", path, e)
            }
            ParserState::EndWasm => return,
            _ => (),
        }
    }
}

/// Returns the default benchmark inputs that are proper `wasmparser` benchmark
/// test inputs.
fn collect_benchmark_inputs() -> Vec<BenchmarkInput> {
    let mut ret = Vec::new();
    collect_test_files("../../tests".as_ref(), &mut ret).unwrap();
    return ret;
}

fn it_works_benchmark(c: &mut Criterion) {
    let mut inputs = collect_benchmark_inputs();
    // Filter out all benchmark inputs that fail to parse via `wasmparser`.
    inputs.retain(|input| {
        let mut parser = Parser::new(input.wasm.as_slice());
        'outer: loop {
            match parser.read() {
                ParserState::Error(_) => break 'outer false,
                ParserState::EndWasm => break 'outer true,
                _ => continue,
            }
        }
    });
    c.bench_function("it works benchmark", move |b| {
        b.iter(|| {
            for input in &mut inputs {
                let _ = black_box(read_all_wasm(
                    &input.path,
                    Parser::new(input.wasm.as_slice()),
                ));
            }
        })
    });
}

fn validator_not_fails_benchmark(c: &mut Criterion) {
    let mut inputs = collect_benchmark_inputs();
    // Filter out all benchmark inputs that fail to validate via `wasmparser`.
    inputs.retain(|input| {
        let mut parser = ValidatingParser::new(input.wasm.as_slice(), VALIDATOR_CONFIG);
        'outer: loop {
            match parser.read() {
                ParserState::Error(_) => break 'outer false,
                ParserState::EndWasm => break 'outer true,
                _ => continue,
            }
        }
    });
    c.bench_function("validator no fails benchmark", move |b| {
        b.iter(|| {
            for input in &mut inputs {
                let _ = black_box(read_all_wasm(
                    &input.path,
                    ValidatingParser::new(input.wasm.as_slice(), VALIDATOR_CONFIG),
                ));
            }
        });
    });
}

fn validate_benchmark(c: &mut Criterion) {
    let mut inputs = collect_benchmark_inputs();
    // Filter out all benchmark inputs that fail to validate via `wasmparser`.
    inputs.retain(|input| validate(input.wasm.as_slice(), VALIDATOR_CONFIG).is_ok());
    c.bench_function("validate benchmark", move |b| {
        b.iter(|| {
            for input in &mut inputs {
                let _ = black_box(validate(input.wasm.as_slice(), VALIDATOR_CONFIG));
            }
        })
    });
}

criterion_group!(
    benchmark,
    it_works_benchmark,
    validator_not_fails_benchmark,
    validate_benchmark
);
criterion_main!(benchmark);
