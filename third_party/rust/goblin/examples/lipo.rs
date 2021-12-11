use goblin::mach::{self, Mach};
use std::env;
use std::process;
use std::path::Path;
use std::fs::File;
use std::io::{Read, Write};

fn usage() -> ! {
    println!("usage: lipo <options> <mach-o fat file>");
    println!("    -m64              Extracts and writes the 64-bit binary in this fat container, if any");
    process::exit(1);
}

fn main () {
    let len = env::args().len();

    if len <= 1 {
        usage();
    } else {
        let mut m64 = false;
        {
            let mut flags = env::args().collect::<Vec<_>>();
            flags.pop();
            flags.remove(0);
            for option in flags {
                match option.as_str() {
                    "-m64" => { m64 = true }
                    other => {
                        println!("unknown flag: {}", other);
                        println!();
                        usage();
                    }
                }
            }
        }

        let path_name = env::args_os().last().unwrap();
        let path = Path::new(&path_name);
        let buffer = { let mut v = Vec::new(); let mut f = File::open(&path).unwrap(); f.read_to_end(&mut v).unwrap(); v};
        match mach::Mach::parse(&buffer) {
            Ok(Mach::Binary(_macho)) => {
                println!("Already a single arch binary");
                process::exit(2);
            },
            Ok(Mach::Fat(fat)) => {
                for (i, arch) in fat.iter_arches().enumerate() {
                    let arch = arch.unwrap();
                    let name = format!("{}.{}", &path_name.to_string_lossy(), i);
                    let path = Path::new(&name);
                    if arch.is_64() && m64 {
                        let bytes = &buffer[arch.offset as usize..][..arch.size as usize];
                        let mut file = File::create(path).unwrap();
                        file.write_all(bytes).unwrap();
                        break;
                    } else if !m64 {
                        let bytes = &buffer[arch.offset as usize..][..arch.size as usize];
                        let mut file = File::create(path).unwrap();
                        file.write_all(bytes).unwrap();
                    }
                }
            },
            Err(err) => {
                println!("err: {:?}", err);
                process::exit(2);
            }
        }
    }
}
