extern crate memmap;
extern crate object;

use std::{env, fs, process};

use object::{Object, SectionKind, Symbol, SymbolKind};

fn main() {
    let arg_len = env::args().len();
    if arg_len <= 1 {
        eprintln!("Usage: {} <file> ...", env::args().next().unwrap());
        process::exit(1);
    }

    for file_path in env::args().skip(1) {
        if arg_len > 2 {
            println!();
            println!("{}:", file_path);
        }

        let file = match fs::File::open(&file_path) {
            Ok(file) => file,
            Err(err) => {
                println!("Failed to open file '{}': {}", file_path, err,);
                continue;
            }
        };
        let file = match unsafe { memmap::Mmap::map(&file) } {
            Ok(mmap) => mmap,
            Err(err) => {
                println!("Failed to map file '{}': {}", file_path, err,);
                continue;
            }
        };
        let file = match object::File::parse(&*file) {
            Ok(file) => file,
            Err(err) => {
                println!("Failed to parse file '{}': {}", file_path, err);
                continue;
            }
        };

        println!("Debugging symbols:");
        for symbol in file.symbols() {
            print_symbol(&symbol);
        }
        println!();

        println!("Dynamic symbols:");
        for symbol in file.dynamic_symbols() {
            print_symbol(&symbol);
        }
    }
}

fn print_symbol(symbol: &Symbol) {
    match symbol.kind() {
        SymbolKind::Section | SymbolKind::File => return,
        _ => {}
    }
    let kind = match symbol.section_kind() {
        Some(SectionKind::Unknown) => '?',
        Some(SectionKind::Text) => if symbol.is_global() {
            'T'
        } else {
            't'
        },
        Some(SectionKind::Data) => if symbol.is_global() {
            'D'
        } else {
            'd'
        },
        Some(SectionKind::ReadOnlyData) => if symbol.is_global() {
            'R'
        } else {
            'r'
        },
        Some(SectionKind::UninitializedData) => if symbol.is_global() {
            'B'
        } else {
            'b'
        },
        Some(SectionKind::Other) => if symbol.is_global() {
            'S'
        } else {
            's'
        },
        None => 'U',
    };
    if symbol.is_undefined() {
        print!("{:16} ", "");
    } else {
        print!("{:016x} ", symbol.address());
    }
    println!(
        "{:016x} {} {}",
        symbol.size(),
        kind,
        symbol.name().unwrap_or("<unknown>"),
    );
}
