#![feature(test)]
extern crate hibitset;
extern crate rand;
#[cfg(feature = "parallel")]
extern crate rayon;
extern crate test;

#[cfg(feature = "parallel")]
use rayon::iter::ParallelIterator;

use hibitset::{BitSet, BitSetLike};

use test::{black_box, Bencher};

use rand::prelude::*;

use self::Mode::*;

enum Mode {
    Seq,
    #[cfg(feature = "parallel")]
    Par(u8),
}

fn bench(n: usize, mode: Mode, b: &mut Bencher) {
    let mut rng = thread_rng();
    let mut bitset = BitSet::with_capacity(1048576);
    for _ in 0..n {
        let index = rng.gen_range(0, 1048576);
        bitset.add(index);
    }
    match mode {
        Seq => b.iter(|| black_box((&bitset).iter().map(black_box).count())),
        #[cfg(feature = "parallel")]
        Par(splits) => b.iter(|| {
            black_box(
                (&bitset)
                    .par_iter()
                    .layers_split(splits)
                    .map(black_box)
                    .count(),
            )
        }),
    }
}

#[bench]
fn iter_100(b: &mut Bencher) {
    bench(100, Seq, b);
}

#[bench]
fn iter_1000(b: &mut Bencher) {
    bench(1000, Seq, b);
}

#[bench]
fn iter_10000(b: &mut Bencher) {
    bench(10000, Seq, b);
}

#[bench]
fn iter_100000(b: &mut Bencher) {
    bench(100000, Seq, b);
}

#[bench]
fn iter_1000000(b: &mut Bencher) {
    bench(1000000, Seq, b);
}

#[cfg(feature = "parallel")]
mod par {
    use super::*;

    #[bench]
    fn par_iter_3_100(b: &mut Bencher) {
        bench(100, Par(3), b);
    }

    #[bench]
    fn par_iter_3_1000(b: &mut Bencher) {
        bench(1000, Par(3), b);
    }

    #[bench]
    fn par_iter_3_10000(b: &mut Bencher) {
        bench(10000, Par(3), b);
    }

    #[bench]
    fn par_iter_3_100000(b: &mut Bencher) {
        bench(100000, Par(3), b);
    }

    #[bench]
    fn par_iter_3_1000000(b: &mut Bencher) {
        bench(1000000, Par(3), b);
    }

    #[bench]
    fn par_iter_2_100(b: &mut Bencher) {
        bench(100, Par(2), b);
    }

    #[bench]
    fn par_iter_2_1000(b: &mut Bencher) {
        bench(1000, Par(2), b);
    }

    #[bench]
    fn par_iter_2_10000(b: &mut Bencher) {
        bench(10000, Par(2), b);
    }

    #[bench]
    fn par_iter_2_100000(b: &mut Bencher) {
        bench(100000, Par(2), b);
    }

    #[bench]
    fn par_iter_2_1000000(b: &mut Bencher) {
        bench(1000000, Par(2), b);
    }

    fn bench_payload(n: usize, splits: u8, payload: u32, b: &mut Bencher) {
        let mut rng = thread_rng();
        let mut bitset = BitSet::with_capacity(1048576);
        for _ in 0..n {
            let index = rng.gen_range(0, 1048576);
            bitset.add(index);
        }
        b.iter(|| {
            black_box(
                (&bitset)
                    .par_iter()
                    .layers_split(splits)
                    .map(|mut n| {
                        for i in 0..payload {
                            n += black_box(i);
                        }
                        black_box(n)
                    })
                    .count(),
            )
        });
    }

    #[bench]
    fn par_3_payload_1000_iter_100(b: &mut Bencher) {
        bench_payload(100, 3, 1000, b);
    }

    #[bench]
    fn par_3_payload_1000_iter_1000(b: &mut Bencher) {
        bench_payload(1000, 3, 1000, b);
    }

    #[bench]
    fn par_3_payload_1000_iter_10000(b: &mut Bencher) {
        bench_payload(10000, 3, 1000, b);
    }

    #[bench]
    fn par_3_payload_1000_iter_100000(b: &mut Bencher) {
        bench_payload(100000, 3, 1000, b);
    }

    #[bench]
    fn par_3_payload_1000_iter_1000000(b: &mut Bencher) {
        bench_payload(1000000, 3, 1000, b);
    }

    #[bench]
    fn par_2_payload_1000_iter_100(b: &mut Bencher) {
        bench_payload(100, 2, 1000, b);
    }

    #[bench]
    fn par_2_payload_1000_iter_1000(b: &mut Bencher) {
        bench_payload(1000, 2, 1000, b);
    }

    #[bench]
    fn par_2_payload_1000_iter_10000(b: &mut Bencher) {
        bench_payload(10000, 2, 1000, b);
    }

    #[bench]
    fn par_2_payload_1000_iter_100000(b: &mut Bencher) {
        bench_payload(100000, 2, 1000, b);
    }

    #[bench]
    fn par_2_payload_1000_iter_1000000(b: &mut Bencher) {
        bench_payload(1000000, 2, 1000, b);
    }

    #[bench]
    fn par_3_payload_100_iter_100(b: &mut Bencher) {
        bench_payload(100, 3, 100, b);
    }

    #[bench]
    fn par_3_payload_100_iter_1000(b: &mut Bencher) {
        bench_payload(1000, 3, 100, b);
    }

    #[bench]
    fn par_3_payload_100_iter_10000(b: &mut Bencher) {
        bench_payload(10000, 3, 100, b);
    }

    #[bench]
    fn par_3_payload_100_iter_100000(b: &mut Bencher) {
        bench_payload(100000, 3, 100, b);
    }

    #[bench]
    fn par_3_payload_100_iter_1000000(b: &mut Bencher) {
        bench_payload(1000000, 3, 100, b);
    }

    #[bench]
    fn par_2_payload_100_iter_100(b: &mut Bencher) {
        bench_payload(100, 2, 100, b);
    }

    #[bench]
    fn par_2_payload_100_iter_1000(b: &mut Bencher) {
        bench_payload(1000, 2, 100, b);
    }

    #[bench]
    fn par_2_payload_100_iter_10000(b: &mut Bencher) {
        bench_payload(10000, 2, 100, b);
    }

    #[bench]
    fn par_2_payload_100_iter_100000(b: &mut Bencher) {
        bench_payload(100000, 2, 100, b);
    }

    #[bench]
    fn par_2_payload_100_iter_1000000(b: &mut Bencher) {
        bench_payload(1000000, 2, 100, b);
    }
}
