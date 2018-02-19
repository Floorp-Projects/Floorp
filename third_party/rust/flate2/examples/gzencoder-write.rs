extern crate flate2;

use std::io::prelude::*;
use flate2::Compression;
use flate2::write::GzEncoder;

// Vec<u8> implements Write to print the compressed bytes of sample string
fn main() {
    let mut e = GzEncoder::new(Vec::new(), Compression::default());
    e.write(b"Hello World").unwrap();
    println!("{:?}", e.finish().unwrap());
}
