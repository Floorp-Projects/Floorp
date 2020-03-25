extern crate wasmparser;

use std::env;
use std::fs::File;
use std::io;
use std::io::prelude::*;
use std::str;
use wasmparser::Parser;
use wasmparser::ParserState;
use wasmparser::WasmDecoder;

fn main() {
    let args = env::args().collect::<Vec<_>>();
    if args.len() != 2 {
        println!("Usage: {} in.wasm", args[0]);
        return;
    }

    let buf: Vec<u8> = read_wasm(&args[1]).unwrap();
    let mut parser = Parser::new(&buf);
    loop {
        print!("0x{:08x}\t", parser.current_position());
        let state = parser.read();
        match *state {
            ParserState::ExportSectionEntry {
                field,
                ref kind,
                index,
            } => {
                println!(
                    "ExportSectionEntry {{ field: \"{}\", kind: {:?}, index: {} }}",
                    field, kind, index
                );
            }
            ParserState::ImportSectionEntry {
                module,
                field,
                ref ty,
            } => {
                println!(
                    "ImportSectionEntry {{ module: \"{}\", field: \"{}\", ty: {:?} }}",
                    module, field, ty
                );
            }
            ParserState::EndWasm => break,
            ParserState::Error(ref err) => panic!("Error: {:?}", err),
            _ => println!("{:?}", state),
        }
    }
}

fn read_wasm(file: &str) -> io::Result<Vec<u8>> {
    let mut data = Vec::new();
    let mut f = File::open(file)?;
    f.read_to_end(&mut data)?;
    Ok(data)
}
