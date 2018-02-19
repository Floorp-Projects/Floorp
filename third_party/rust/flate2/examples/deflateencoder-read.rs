extern crate flate2;

use std::io::prelude::*;
use std::io;
use flate2::Compression;
use flate2::read::DeflateEncoder;

// Print the Deflate compressed representation of hello world
fn main() {
    println!("{:?}", deflateencoder_read_hello_world().unwrap());
}

// Return a vector containing the Defalte compressed version of hello world
fn deflateencoder_read_hello_world() -> io::Result<Vec<u8>> {
    let mut ret_vec = [0;100];
    let c = b"hello world";
    let mut deflater = DeflateEncoder::new(&c[..], Compression::fast());
    let count = deflater.read(&mut ret_vec)?;
    Ok(ret_vec[0..count].to_vec())
}
