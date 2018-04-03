#![feature(test)]
#![allow(non_snake_case)]

extern crate itoa;
extern crate test;

macro_rules! benches {
    ($($name:ident($value:expr),)*) => {
        mod bench_itoa {
            use test::{Bencher, black_box};
            $(
                #[bench]
                fn $name(b: &mut Bencher) {
                    use itoa;

                    let mut buf = Vec::with_capacity(20);

                    b.iter(|| {
                        buf.clear();
                        itoa::write(&mut buf, black_box($value)).unwrap()
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
    bench_0u64(0u64),
    bench_HALFu64(<u32>::max_value() as u64),
    bench_MAXu64(<u64>::max_value()),

    bench_0i16(0i16),
    bench_MINi16(<i16>::min_value()),
);
