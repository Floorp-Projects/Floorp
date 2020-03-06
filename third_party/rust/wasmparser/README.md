# The WebAssembly binary file decoder in Rust

**A [Bytecode Alliance](https://bytecodealliance.org/) project**

[![Build Status](https://travis-ci.org/yurydelendik/wasmparser.rs.svg?branch=master)](https://travis-ci.org/yurydelendik/wasmparser.rs)
[![crates.io link](https://img.shields.io/crates/v/wasmparser.svg)](https://crates.io/crates/wasmparser)

The decoder library provides lightweight and fast decoding/parsing of WebAssembly binary files.

The other goal is minimal memory footprint. For this reason, there is no AST or IR of WebAssembly data.

See also its sibling at https://github.com/wasdk/wasmparser


## Documentation

The documentation and examples can be found at the https://docs.rs/wasmparser/


## Example

```rust
use wasmparser::WasmDecoder;
use wasmparser::Parser;
use wasmparser::ParserState;

fn get_name(bytes: &[u8]) -> &str {
  str::from_utf8(bytes).ok().unwrap()
}

fn main() {
  let ref buf: Vec<u8> = read_wasm_bytes();
  let mut parser = Parser::new(buf);
  loop {
    let state = parser.read();
    match *state {
        ParserState::BeginWasm { .. } => {
            println!("====== Module");
        }
        ParserState::ExportSectionEntry { field, ref kind, .. } => {
            println!("  Export {} {:?}", get_name(field), kind);
        }
        ParserState::ImportSectionEntry { module, field, .. } => {
            println!("  Import {}::{}", get_name(module), get_name(field))
        }
        ParserState::EndWasm => break,
        _ => ( /* println!(" Other {:?}", state) */ )
    }
  }
}
```


## Fuzzing

To fuzz test wasmparser.rs, switch to a nightly Rust compiler and install [cargo-fuzz]:

```
cargo install cargo-fuzz
```

Then, from the root of the repository, run:

```
cargo fuzz run parse
```

If you want to use files as seeds for the fuzzer, add them to `fuzz/corpus/parse/` and restart cargo-fuzz.

[cargo-fuzz]: https://github.com/rust-fuzz/cargo-fuzz
