pub fn read_file_data(path: &PathBuf) -> Vec<u8> {
    let mut data = Vec::new();
    let mut f = File::open(path).ok().unwrap();
    f.read_to_end(&mut data).unwrap();
    data
}

const VALIDATOR_CONFIG: Option<ValidatingParserConfig> = Some(ValidatingParserConfig {
    operator_config: OperatorValidatorConfig {
        enable_threads: true,
        enable_reference_types: true,
        enable_simd: true,
        enable_bulk_memory: true,
        enable_multi_value: true,
    },
});

#[macro_use]
extern crate criterion;
extern crate wasmparser;

use criterion::Criterion;
use wasmparser::{
    validate, OperatorValidatorConfig, Parser, ParserState, ValidatingParser,
    ValidatingParserConfig, WasmDecoder,
};

use std::fs::{read_dir, File};
use std::io::Read;
use std::path::PathBuf;

fn read_all_wasm<'a, T>(mut d: T)
where
    T: WasmDecoder<'a>,
{
    loop {
        match *d.read() {
            ParserState::Error(e) => panic!("unexpected error {:?}", e),
            ParserState::EndWasm => return,
            _ => (),
        }
    }
}

fn it_works_benchmark(c: &mut Criterion) {
    let mut data: Vec<Vec<u8>> = vec![];
    for entry in read_dir("tests").unwrap() {
        let dir = entry.unwrap();
        if !dir.file_type().unwrap().is_file() {
            continue;
        }
        data.push(read_file_data(&dir.path()));
    }
    c.bench_function("it works benchmark", move |b| {
        for d in &mut data {
            b.iter(|| read_all_wasm(Parser::new(d.as_slice())));
        }
    });
}

fn validator_not_fails_benchmark(c: &mut Criterion) {
    let mut data: Vec<Vec<u8>> = vec![];
    for entry in read_dir("tests").unwrap() {
        let dir = entry.unwrap();
        if !dir.file_type().unwrap().is_file() {
            continue;
        }
        data.push(read_file_data(&dir.path()));
    }
    c.bench_function("validator no fails benchmark", move |b| {
        for d in &mut data {
            b.iter(|| read_all_wasm(ValidatingParser::new(d.as_slice(), VALIDATOR_CONFIG)));
        }
    });
}

fn validate_benchmark(c: &mut Criterion) {
    let mut data: Vec<Vec<u8>> = vec![vec![]];
    for entry in read_dir("tests").unwrap() {
        let dir = entry.unwrap();
        if !dir.file_type().unwrap().is_file() {
            continue;
        }
        data.push(read_file_data(&dir.path()));
    }
    c.bench_function("validate benchmark", move |b| {
        for d in &mut data {
            b.iter(|| validate(&d, VALIDATOR_CONFIG));
        }
    });
}

criterion_group!(
    benchmark,
    it_works_benchmark,
    validator_not_fails_benchmark,
    validate_benchmark
);
criterion_main!(benchmark);
