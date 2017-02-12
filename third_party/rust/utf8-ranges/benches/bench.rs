#![feature(test)]

extern crate test;
extern crate utf8_ranges;

use test::Bencher;
use utf8_ranges::Utf8Sequences;

#[bench]
fn no_reuse(b: &mut Bencher) {
    b.iter(|| {
        let count = Utf8Sequences::new('\u{0}', '\u{10FFFF}').count();
        assert_eq!(count, 9);
    })
}

#[bench]
fn reuse(b: &mut Bencher) {
    let mut seqs = Utf8Sequences::new('\u{0}', '\u{10FFFF}');
    b.iter(|| {
        seqs.reset('\u{0}', '\u{10FFFF}');
        let count = (&mut seqs).count();
        assert_eq!(count, 9);
    })
}
