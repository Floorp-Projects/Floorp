use goblin::error;
use std::path::Path;
use std::env;
use std::fs::File;
use std::io::Read;

fn run () -> error::Result<()> {
    for (i, arg) in env::args().enumerate() {
        if i == 1 {
            let path = Path::new(arg.as_str());
            let mut fd = File::open(path)?;
            let buffer = { let mut v = Vec::new(); fd.read_to_end(&mut v).unwrap(); v};
            let res = goblin::Object::parse(&buffer)?;
            println!("{:#?}", res);
        }
    }
    Ok(())
}

pub fn main () {
    match run() {
        Ok(()) => (),
        Err(err) => println!("{:#}", err)
    }
}
