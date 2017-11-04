extern crate flate2;

use flate2::{FlateReadExt, Compression};
use std::io::prelude::*;
use std::io;
use std::fs::File;

fn main() {
    println!("{}", run().unwrap());
}

fn run() -> io::Result<String> {
    let f = File::open("examples/hello_world.txt")?;

    //gz_encode method comes from FlateReadExt and applies to a std::fs::File
    let data = f.gz_encode(Compression::Default);
    let mut buffer = String::new();

    //gz_decode method comes from FlateReadExt and applies to a &[u8]
    &data.gz_decode()?.read_to_string(&mut buffer)?;
    Ok(buffer)
}
