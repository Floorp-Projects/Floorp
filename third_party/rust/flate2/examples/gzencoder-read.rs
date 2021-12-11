extern crate flate2;

use flate2::read::GzEncoder;
use flate2::Compression;
use std::io;
use std::io::prelude::*;

// Print the GZ compressed representation of hello world
fn main() {
    println!("{:?}", gzencoder_read_hello_world().unwrap());
}

// Return a vector containing the GZ compressed version of hello world
fn gzencoder_read_hello_world() -> io::Result<Vec<u8>> {
    let mut ret_vec = [0; 100];
    let c = b"hello world";
    let mut z = GzEncoder::new(&c[..], Compression::fast());
    let count = z.read(&mut ret_vec)?;
    Ok(ret_vec[0..count].to_vec())
}
