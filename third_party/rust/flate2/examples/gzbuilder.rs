extern crate flate2;

use std::io::prelude::*;
use std::io;
use std::fs::File;
use flate2::GzBuilder;
use flate2::Compression;

// Open file and debug print the contents compressed with gzip
fn main() {
    sample_builder().unwrap();
}

// GzBuilder opens a file and writes a sample string using Builder pattern
fn sample_builder() -> Result<(), io::Error> {
    let f = File::create("examples/hello_world.gz")?;
    let mut gz = GzBuilder::new()
                 .filename("hello_world.txt")
                 .comment("test file, please delete")
                 .write(f, Compression::default());
    gz.write(b"hello world")?;
    gz.finish()?;
    Ok(())
}
