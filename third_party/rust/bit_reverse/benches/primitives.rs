#![allow(overflowing_literals)]
#![feature(test)]

extern crate bit_reverse;
extern crate test;


static SEED: u64 = 0x0123456789ABCDEF;
static NUM_ITERS: usize = 1024;

macro_rules! benchmark_suite {
    ($name:ident, $algo:ident) => (
        #[cfg(test)]
        mod $name {
            use bit_reverse::$algo;
            use super::test::Bencher;
            use std::mem::size_of;

            use SEED;
            use NUM_ITERS;

            #[bench]
            fn reverse_u8(b: &mut Bencher) {
                let mut num = SEED as u8;

                b.bytes = (NUM_ITERS * size_of::<u8>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_u16(b: &mut Bencher) {
                let mut num = SEED as u16;

                b.bytes = (NUM_ITERS * size_of::<u16>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_u32(b: &mut Bencher) {
                let mut num = SEED as u32;

                b.bytes = (NUM_ITERS * size_of::<u32>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_u64(b: &mut Bencher) {
                let mut num = SEED as u64;

                b.bytes = (NUM_ITERS * size_of::<u64>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_usize(b: &mut Bencher) {
                let mut num = SEED as usize;
            
                b.bytes = (NUM_ITERS * size_of::<usize>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_i8(b: &mut Bencher) {
                let mut num = SEED as i8;

                b.bytes = (NUM_ITERS * size_of::<i8>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_i16(b: &mut Bencher) {
                let mut num = SEED as i16;

                b.bytes = (NUM_ITERS * size_of::<i16>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_i32(b: &mut Bencher) {
                let mut num = SEED as i32;

                b.bytes = (NUM_ITERS * size_of::<i32>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_i64(b: &mut Bencher) {
                let mut num = SEED as i64;

                b.bytes = (NUM_ITERS * size_of::<i64>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }

            #[bench]
            fn reverse_isize(b: &mut Bencher) {
                let mut num = SEED as isize;

                b.bytes = (NUM_ITERS * size_of::<isize>()) as u64;
                b.iter(|| {
                    for _ in 0..NUM_ITERS {
                        num = num.swap_bits();
                    }

                    num
                });
            }
        }
    )
}

benchmark_suite!(parallel, ParallelReverse);
benchmark_suite!(lookup, LookupReverse);
benchmark_suite!(bitwise, BitwiseReverse);