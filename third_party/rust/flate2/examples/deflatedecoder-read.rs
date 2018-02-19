extern crate flate2;

use std::io::prelude::*;
use std::io;
use flate2::Compression;
use flate2::write::DeflateEncoder;
use flate2::read::DeflateDecoder;

// Compress a sample string and print it after transformation.
fn main() {
    let mut e = DeflateEncoder::new(Vec::new(), Compression::default());
    e.write(b"Hello World").unwrap();
    let bytes = e.finish().unwrap();
    println!("{}", decode_reader(bytes).unwrap());
}

// Uncompresses a Deflate Encoded vector of bytes and returns a string or error
// Here &[u8] implements Read
fn decode_reader(bytes: Vec<u8>) -> io::Result<String> {
    let mut deflater = DeflateDecoder::new(&bytes[..]);
    let mut s = String::new();
    deflater.read_to_string(&mut s)?;
    Ok(s)
}
