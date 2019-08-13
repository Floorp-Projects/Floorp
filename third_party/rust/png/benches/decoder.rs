#![cfg(feature = "benchmarks")]
#![feature(test)]

extern crate png;
extern crate test;

use std::fs::File;
use std::io::Read;

use png::Decoder;

#[bench]
fn bench_big(b: &mut test::Bencher) {
    let mut data = Vec::new();
    File::open("tests/pngsuite/PngSuite.png").unwrap().read_to_end(&mut data).unwrap();
    let decoder = Decoder::new(&*data);
    let (info, _) = decoder.read_info().unwrap();
    let mut image = vec![0; info.buffer_size()];
    b.iter(|| {
        let decoder = Decoder::new(&*data);
        let (_, mut decoder) = decoder.read_info().unwrap();
        test::black_box(decoder.next_frame(&mut image)).unwrap();
    });
    b.bytes = info.buffer_size() as u64
}
