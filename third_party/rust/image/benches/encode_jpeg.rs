#![cfg(feature = "benchmarks")]
#![feature(test)]
extern crate image;
extern crate test;

use test::Bencher;

const W: u32 = 1000;
const H: u32 = 1000;

fn run_benchmark(b: &mut Bencher, color_type: image::ColorType) {
    let mut v = Vec::with_capacity((W * H) as usize);
    let i = vec![0; (W * H) as usize];

    b.iter(|| {
        v.clear();
        let mut e = image::jpeg::JPEGEncoder::new(&mut v);
        e.encode(&i[..], W, H, color_type).unwrap();
    });
}

#[bench]
fn bench_rgb(b: &mut Bencher) {
    run_benchmark(b, image::RGB(8));
}

#[bench]
fn bench_gray(b: &mut Bencher) {
    run_benchmark(b, image::Gray(8));
}
