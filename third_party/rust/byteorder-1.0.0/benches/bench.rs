#![feature(test)]

extern crate byteorder;
extern crate test;

macro_rules! bench_num {
    ($name:ident, $read:ident, $bytes:expr, $data:expr) => (
        mod $name {
            use byteorder::{ByteOrder, BigEndian, NativeEndian, LittleEndian};
            use super::test::Bencher;
            use super::test::black_box as bb;

            const NITER: usize = 100_000;

            #[bench]
            fn read_big_endian(b: &mut Bencher) {
                let buf = $data;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(BigEndian::$read(&buf, $bytes));
                    }
                });
            }

            #[bench]
            fn read_little_endian(b: &mut Bencher) {
                let buf = $data;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(LittleEndian::$read(&buf, $bytes));
                    }
                });
            }

            #[bench]
            fn read_native_endian(b: &mut Bencher) {
                let buf = $data;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(NativeEndian::$read(&buf, $bytes));
                    }
                });
            }
        }
    );
    ($ty:ident, $max:ident,
     $read:ident, $write:ident, $size:expr, $data:expr) => (
        mod $ty {
            use std::$ty;
            use byteorder::{ByteOrder, BigEndian, NativeEndian, LittleEndian};
            use super::test::Bencher;
            use super::test::black_box as bb;

            const NITER: usize = 100_000;

            #[bench]
            fn read_big_endian(b: &mut Bencher) {
                let buf = $data;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(BigEndian::$read(&buf));
                    }
                });
            }

            #[bench]
            fn read_little_endian(b: &mut Bencher) {
                let buf = $data;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(LittleEndian::$read(&buf));
                    }
                });
            }

            #[bench]
            fn read_native_endian(b: &mut Bencher) {
                let buf = $data;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(NativeEndian::$read(&buf));
                    }
                });
            }

            #[bench]
            fn write_big_endian(b: &mut Bencher) {
                let mut buf = $data;
                let n = $ty::$max;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(BigEndian::$write(&mut buf, n));
                    }
                });
            }

            #[bench]
            fn write_little_endian(b: &mut Bencher) {
                let mut buf = $data;
                let n = $ty::$max;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(LittleEndian::$write(&mut buf, n));
                    }
                });
            }

            #[bench]
            fn write_native_endian(b: &mut Bencher) {
                let mut buf = $data;
                let n = $ty::$max;
                b.iter(|| {
                    for _ in 0..NITER {
                        bb(NativeEndian::$write(&mut buf, n));
                    }
                });
            }
        }
    );
}

bench_num!(u16, MAX, read_u16, write_u16, 2, [1, 2]);
bench_num!(i16, MAX, read_i16, write_i16, 2, [1, 2]);
bench_num!(u32, MAX, read_u32, write_u32, 4, [1, 2, 3, 4]);
bench_num!(i32, MAX, read_i32, write_i32, 4, [1, 2, 3, 4]);
bench_num!(u64, MAX, read_u64, write_u64, 8, [1, 2, 3, 4, 5, 6, 7, 8]);
bench_num!(i64, MAX, read_i64, write_i64, 8, [1, 2, 3, 4, 5, 6, 7, 8]);
bench_num!(f32, MAX, read_f32, write_f32, 4, [1, 2, 3, 4]);
bench_num!(f64, MAX, read_f64, write_f64, 8,
           [1, 2, 3, 4, 5, 6, 7, 8]);

bench_num!(uint_1, read_uint, 1, [1]);
bench_num!(uint_2, read_uint, 2, [1, 2]);
bench_num!(uint_3, read_uint, 3, [1, 2, 3]);
bench_num!(uint_4, read_uint, 4, [1, 2, 3, 4]);
bench_num!(uint_5, read_uint, 5, [1, 2, 3, 4, 5]);
bench_num!(uint_6, read_uint, 6, [1, 2, 3, 4, 5, 6]);
bench_num!(uint_7, read_uint, 7, [1, 2, 3, 4, 5, 6, 7]);
bench_num!(uint_8, read_uint, 8, [1, 2, 3, 4, 5, 6, 7, 8]);

bench_num!(int_1, read_int, 1, [1]);
bench_num!(int_2, read_int, 2, [1, 2]);
bench_num!(int_3, read_int, 3, [1, 2, 3]);
bench_num!(int_4, read_int, 4, [1, 2, 3, 4]);
bench_num!(int_5, read_int, 5, [1, 2, 3, 4, 5]);
bench_num!(int_6, read_int, 6, [1, 2, 3, 4, 5, 6]);
bench_num!(int_7, read_int, 7, [1, 2, 3, 4, 5, 6, 7]);
bench_num!(int_8, read_int, 8, [1, 2, 3, 4, 5, 6, 7, 8]);
