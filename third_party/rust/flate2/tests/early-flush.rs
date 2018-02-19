extern crate flate2;

use std::io::{Read, Write};

use flate2::write::GzEncoder;
use flate2::read::GzDecoder;

#[test]
fn smoke() {
    let mut w = GzEncoder::new(Vec::new(), flate2::Compression::default());
    w.flush().unwrap();
    w.write(b"hello").unwrap();

    let bytes = w.finish().unwrap();

    let mut r = GzDecoder::new(&bytes[..]);
    let mut s = String::new();
    r.read_to_string(&mut s).unwrap();
    assert_eq!(s, "hello");
}
