#![feature(test)]

extern crate line_wrap;
extern crate test;

use test::Bencher;

#[bench]
fn wrap_10_10_lf(b: &mut Bencher) {
    do_wrap_bench(b, 10, 10, &line_wrap::lf());
}

#[bench]
fn wrap_10_10_crlf(b: &mut Bencher) {
    do_wrap_bench(b, 10, 10, &line_wrap::crlf());
}

#[bench]
fn wrap_10_1_lf(b: &mut Bencher) {
    do_wrap_bench(b, 10, 1, &line_wrap::lf());
}

#[bench]
fn wrap_10_1_crlf(b: &mut Bencher) {
    do_wrap_bench(b, 10, 1, &line_wrap::crlf());
}

#[bench]
fn wrap_100_10_lf(b: &mut Bencher) {
    do_wrap_bench(b, 100, 10, &line_wrap::lf());
}

#[bench]
fn wrap_100_10_crlf(b: &mut Bencher) {
    do_wrap_bench(b, 100, 10, &line_wrap::crlf());
}

#[bench]
fn wrap_100_10_slice_10(b: &mut Bencher) {
    let bytes = vec![b'\n', 10];
    let line_ending = line_wrap::SliceLineEnding::new(&bytes);
    do_wrap_bench(b, 100, 10, &line_ending);
}

#[bench]
fn wrap_100_1_lf(b: &mut Bencher) {
    do_wrap_bench(b, 100, 1, &line_wrap::lf());
}

#[bench]
fn wrap_100_1_crlf(b: &mut Bencher) {
    do_wrap_bench(b, 100, 1, &line_wrap::crlf());
}

#[bench]
fn wrap_1000_100_lf(b: &mut Bencher) {
    do_wrap_bench(b, 1000, 100, &line_wrap::lf());
}

#[bench]
fn wrap_1000_100_crlf(b: &mut Bencher) {
    do_wrap_bench(b, 1000, 100, &line_wrap::crlf());
}

#[bench]
fn wrap_10000_100_lf(b: &mut Bencher) {
    do_wrap_bench(b, 10000, 100, &line_wrap::lf());
}

#[bench]
fn wrap_10000_100_crlf(b: &mut Bencher) {
    do_wrap_bench(b, 10000, 100, &line_wrap::crlf());
}

#[bench]
fn wrap_10000_100_slice_1(b: &mut Bencher) {
    let line_ending = line_wrap::SliceLineEnding::new(b"\n");

    do_wrap_bench(b, 10000, 100, &line_ending);
}

#[bench]
fn wrap_10000_100_slice_10(b: &mut Bencher) {
    let bytes = vec![b'\n', 10];
    let line_ending = line_wrap::SliceLineEnding::new(&bytes);

    do_wrap_bench(b, 10000, 100, &line_ending);
}

fn do_wrap_bench<L: line_wrap::LineEnding>(b: &mut Bencher, input_len: usize, line_len: usize, line_ending: &L) {
    let mut v = vec![0_u8; input_len + input_len / line_len * line_ending.len()];

    b.bytes = input_len as u64;
    b.iter(|| {
        line_wrap::line_wrap(&mut v, input_len, line_len, line_ending);
    })
}