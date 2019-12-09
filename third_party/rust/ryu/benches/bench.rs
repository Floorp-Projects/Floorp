// cargo bench

#![feature(test)]

extern crate ryu;
extern crate test;

macro_rules! benches {
    ($($name:ident($value:expr),)*) => {
        mod bench_ryu {
            use test::{Bencher, black_box};
            $(
                #[bench]
                fn $name(b: &mut Bencher) {
                    use ryu;

                    let mut buf = ryu::Buffer::new();

                    b.iter(move || {
                        let value = black_box($value);
                        let formatted = buf.format_finite(value);
                        black_box(formatted);
                    });
                }
            )*
        }

        mod bench_std_fmt {
            use test::{Bencher, black_box};
            $(
                #[bench]
                fn $name(b: &mut Bencher) {
                    use std::io::Write;

                    let mut buf = Vec::with_capacity(20);

                    b.iter(|| {
                        buf.clear();
                        let value = black_box($value);
                        write!(&mut buf, "{}", value).unwrap();
                        black_box(buf.as_slice());
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
