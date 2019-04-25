#![cfg(feature = "benchmarks")]
#![feature(test)]

extern crate image;
extern crate test;

use image::ImageFormat;
use std::io::Read;
use std::{fs, path};

struct BenchDef<'a> {
    dir: &'a [&'a str],
    format: ImageFormat,
}

const IMAGE_DIR: [&'static str; 3] = [".", "tests", "images"];
const BMP: BenchDef<'static> = BenchDef {
    dir: &["bmp", "images"],
    format: ImageFormat::BMP,
};

fn bench_load(b: &mut test::Bencher, def: &BenchDef, filename: &str) {
    let mut path: path::PathBuf = IMAGE_DIR.iter().collect();
    for d in def.dir {
        path.push(d);
    }
    path.push(filename);
    let mut fin = fs::File::open(path).unwrap();
    let mut buf = Vec::new();
    fin.read_to_end(&mut buf).unwrap();
    b.iter(|| {
        image::load_from_memory_with_format(&buf, def.format).unwrap();
    })
}

#[bench]
fn bench_load_bmp_1bit(b: &mut test::Bencher) {
    bench_load(b, &BMP, "Core_1_Bit.bmp");
}

#[bench]
fn bench_load_bmp_4bit(b: &mut test::Bencher) {
    bench_load(b, &BMP, "Core_4_Bit.bmp");
}

#[bench]
fn bench_load_bmp_8bit(b: &mut test::Bencher) {
    bench_load(b, &BMP, "Core_8_Bit.bmp");
}

#[bench]
fn bench_load_bmp_16bit(b: &mut test::Bencher) {
    bench_load(b, &BMP, "rgb16.bmp");
}

#[bench]
fn bench_load_bmp_24bit(b: &mut test::Bencher) {
    bench_load(b, &BMP, "rgb24.bmp");
}

#[bench]
fn bench_load_bmp_32bit(b: &mut test::Bencher) {
    bench_load(b, &BMP, "rgb32.bmp");
}

#[bench]
fn bench_load_bmp_4rle(b: &mut test::Bencher) {
    bench_load(b, &BMP, "pal4rle.bmp");
}

#[bench]
fn bench_load_bmp_8rle(b: &mut test::Bencher) {
    bench_load(b, &BMP, "pal8rle.bmp");
}

#[bench]
fn bench_load_bmp_16bf(b: &mut test::Bencher) {
    bench_load(b, &BMP, "rgb16-565.bmp");
}

#[bench]
fn bench_load_bmp_32bf(b: &mut test::Bencher) {
    bench_load(b, &BMP, "rgb32bf.bmp");
}
