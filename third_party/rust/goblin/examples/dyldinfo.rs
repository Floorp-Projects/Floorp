extern crate goblin;

use goblin::mach;
use std::env;
use std::process;
use std::path::Path;
use std::fs::File;
use std::io::Read;
use std::borrow::Cow;

fn usage() -> ! {
    println!("usage: dyldinfo <options> <mach-o file>");
    println!("    -bind             print binds as seen by macho::imports()");
    println!("    -lazy_bind        print lazy binds as seen by macho::imports()");
    process::exit(1);
}

fn name_to_str<'a>(name: &'a [u8; 16]) -> Cow<'a, str> {
    for i in 0..16 {
        if name[i] == 0 {
            return String::from_utf8_lossy(&name[0..i])
        }
    }
    String::from_utf8_lossy(&name[..])
}

fn dylib_name(name: &str) -> &str {
    // observed behavior:
    //   "/usr/lib/libc++.1.dylib" => "libc++"
    //   "/usr/lib/libSystem.B.dylib" => "libSystem"
    //   "/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation" => "CoreFoundation"
    name
        .rsplit('/').next().unwrap()
        .split('.').next().unwrap()
}

fn print_binds(sections: &[mach::segment::Section], imports: &[mach::imports::Import]) {
    println!("bind information:");

    println!(
        "{:7} {:16} {:14} {:7} {:6} {:16} {}",
        "segment",
        "section",
        "address",
        "type",
        "addend",
        "dylib",
        "symbol"
    );

    for import in imports.iter().filter(|i| !i.is_lazy) {
        // find the section that imported this symbol
        let section = sections.iter()
            .filter(|s| import.address >= s.addr && import.address < (s.addr + s.size))
            .next();

        // get &strs for its name
        let (segname, sectname) = section
            .map(|sect|  (name_to_str(&sect.segname), name_to_str(&sect.sectname)))
            .unwrap_or((Cow::Borrowed("?"), Cow::Borrowed("?")));

        println!(
            "{:7} {:16} 0x{:<12X} {:7} {:6} {:16} {}{}",
            segname,
            sectname,
            import.address,
            "pointer",
            import.addend,
            dylib_name(import.dylib),
            import.name,
            if import.is_weak { " (weak import)" } else { "" }
        );
    }
}

fn print_lazy_binds(sections: &[mach::segment::Section], imports: &[mach::imports::Import]) {
    println!("lazy binding information (from lazy_bind part of dyld info):");

    println!(
        "{:7} {:16} {:10} {:6} {:16} {}",
        "segment",
        "section",
        "address",
        "index",
        "dylib",
        "symbol"
    );

    for import in imports.iter().filter(|i| i.is_lazy) {
        // find the section that imported this symbol
        let section = sections.iter()
            .filter(|s| import.address >= s.addr && import.address < (s.addr + s.size))
            .next();

        // get &strs for its name
        let (segname, sectname) = section
            .map(|sect|  (name_to_str(&sect.segname), name_to_str(&sect.sectname)))
            .unwrap_or((Cow::Borrowed("?"), Cow::Borrowed("?")));

        println!(
        "{:7} {:16} 0x{:<8X} {:<06} {:16} {}",
            segname,
            sectname,
            import.address,
            format!("0x{:04X}", import.start_of_sequence_offset),
            dylib_name(import.dylib),
            import.name
        );
    }
}

fn main () {
    let len = env::args().len();

    let mut bind = false;
    let mut lazy_bind = false;

    if len <= 2 {
        usage();
    } else {
        // parse flags
        {
            let mut flags = env::args().collect::<Vec<_>>();
            flags.pop();
            flags.remove(0);
            for option in flags {
                match option.as_str() {
                    "-bind" => { bind = true }
                    "-lazy_bind" => { lazy_bind = true }
                    other => {
                        println!("unknown flag: {}", other);
                        println!("");
                        usage();
                    }
                }
            }
        }

        // open the file
        let path = env::args_os().last().unwrap();
        let path = Path::new(&path);
        let buffer = { let mut v = Vec::new(); let mut f = File::open(&path).unwrap(); f.read_to_end(&mut v).unwrap(); v};
        match mach::MachO::parse(&buffer, 0) {
            Ok(macho) => {
                // collect sections and sort by address
                let mut sections: Vec<mach::segment::Section> = Vec::new();
                for sects in macho.segments.sections() {
                    sections.extend(sects.map(|r| r.expect("section").0));
                }
                sections.sort_by_key(|s| s.addr);

                // get the imports
                let imports = macho.imports().expect("imports");

                if bind {
                    print_binds(&sections, &imports);
                }
                if lazy_bind {
                    print_lazy_binds(&sections, &imports);
                }
            },
            Err(err) => {
                println!("err: {:?}", err);
                process::exit(2);
            }
        }
    }
}
