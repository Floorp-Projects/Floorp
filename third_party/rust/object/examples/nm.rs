use object::{Object, ObjectSection, SectionIndex, SectionKind, Symbol, SymbolKind};
use std::collections::HashMap;
use std::{env, fs, process};

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

        let section_kinds = file.sections().map(|s| (s.index(), s.kind())).collect();

        println!("Debugging symbols:");
        for (_, symbol) in file.symbols() {
            print_symbol(&symbol, &section_kinds);
        }
        println!();

        println!("Dynamic symbols:");
        for (_, symbol) in file.dynamic_symbols() {
            print_symbol(&symbol, &section_kinds);
        }
    }
}

fn print_symbol(symbol: &Symbol<'_>, section_kinds: &HashMap<SectionIndex, SectionKind>) {
    if let SymbolKind::Section | SymbolKind::File = symbol.kind() {
        return;
    }

    let mut kind = match symbol
        .section_index()
        .and_then(|index| section_kinds.get(&index))
    {
        Some(SectionKind::Unknown)
        | Some(SectionKind::Other)
        | Some(SectionKind::OtherString)
        | Some(SectionKind::Debug)
        | Some(SectionKind::Linker)
        | Some(SectionKind::Metadata) => '?',
        Some(SectionKind::Text) => 't',
        Some(SectionKind::Data) | Some(SectionKind::Tls) | Some(SectionKind::TlsVariables) => 'd',
        Some(SectionKind::ReadOnlyData) | Some(SectionKind::ReadOnlyString) => 'r',
        Some(SectionKind::UninitializedData) | Some(SectionKind::UninitializedTls) => 'b',
        None => 'U',
    };

    if symbol.is_global() {
        kind = kind.to_ascii_uppercase();
    }

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
