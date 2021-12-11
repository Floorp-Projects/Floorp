#![feature(test)]

extern crate dtoa;
extern crate test;

macro_rules! benches {
    ($($name:ident($value:expr),)*) => {
        mod bench_dtoa {
            use test::{Bencher, black_box};
            $(
                #[bench]
                fn $name(b: &mut Bencher) {
                    use dtoa;

                    let mut buf = Vec::with_capacity(20);

                    b.iter(|| {
                        buf.clear();
                        dtoa::write(&mut buf, black_box($value)).unwrap()
                    });
                }
            )*
        }

        mod bench_fmt {
            use test::{Bencher, black_box};
            $(
                #[bench]
                fn $name(b: &mut Bencher) {
                    use std::io::Write;

                    let mut buf = Vec::with_capacity(20);

                    b.iter(|| {
                        buf.clear();
                        write!(&mut buf, "{}", black_box($value)).unwrap()
                    });
                }
            )*
        }
    }
}

benches!(
    bench_0_f64(0f64),
    bench_short_f64(0.1234f64),
    bench_e_f64(2.718281828459045f64),
    bench_max_f64(::std::f64::MAX),

    bench_0_f32(0f32),
    bench_short_f32(0.1234f32),
    bench_e_f32(2.718281828459045f32),
    bench_max_f32(::std::f32::MAX),
);
