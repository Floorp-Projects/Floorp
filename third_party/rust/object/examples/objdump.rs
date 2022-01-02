use object::{Object, ObjectSection};
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
                return;
            }
        };
        let file = match unsafe { memmap::Mmap::map(&file) } {
            Ok(mmap) => mmap,
            Err(err) => {
                println!("Failed to map file '{}': {}", file_path, err,);
                return;
            }
        };
        let file = match object::File::parse(&*file) {
            Ok(file) => file,
            Err(err) => {
                println!("Failed to parse file '{}': {}", file_path, err);
                return;
            }
        };

        if let Some(uuid) = file.mach_uuid() {
            println!("Mach UUID: {}", uuid);
        }
        if let Some(build_id) = file.build_id() {
            println!("Build ID: {:x?}", build_id);
        }
        if let Some((filename, crc)) = file.gnu_debuglink() {
            println!(
                "GNU debug link: {} CRC: {:08x}",
                String::from_utf8_lossy(filename),
                crc
            );
        }

        for segment in file.segments() {
            println!("{:?}", segment);
        }

        for (index, section) in file.sections().enumerate() {
            println!("{}: {:?}", index, section);
        }

        for (index, symbol) in file.symbols() {
            println!("{}: {:?}", index.0, symbol);
        }

        for section in file.sections() {
            if section.relocations().next().is_some() {
                println!(
                    "\n{} relocations",
                    section.name().unwrap_or("<invalid name>")
                );
                for relocation in section.relocations() {
                    println!("{:?}", relocation);
                }
            }
        }
    }
}
