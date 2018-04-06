// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/*

A cautionary note about these benchmarks:

Many of the operations we're attempting to measure take less than one
nanosecond. That's why we run them thousands of times in a loop just to get a
single iteration that Rust's statistical benchmarking can work with. At that
scale, any change anywhere in the library can produce durable performance
regressions on the order of half a nanosecond, i.e. "500 ns" in the output for
a test like eq_x_1000.

We can't get anything done if we rachet on these numbers! They are more useful
for selecting between alternatives, and for noticing large regressions or
inconsistencies.

Furthermore, a large part of the point of interning is to make strings small
and cheap to move around, which isn't reflected in these tests.

*/

use atom::tests::TestAtom;
use test::{Bencher, black_box};

// Just shorthand
fn mk(x: &str) -> TestAtom {
    TestAtom::from(x)
}

macro_rules! check_type (($name:ident, $x:expr, $p:pat) => (
    // NB: "cargo bench" does not run these!
    #[test]
    fn $name() {
        match unsafe { $x.unpack() } {
            $p => (),
            _ => panic!("atom has wrong type"),
        }
    }
));

macro_rules! bench_tiny_op (($name:ident, $op:ident, $ctor_x:expr, $ctor_y:expr) => (
    #[bench]
    fn $name(b: &mut Bencher) {
        const n: usize = 1000;
        let xs: Vec<_> = repeat($ctor_x).take(n).collect();
        let ys: Vec<_> = repeat($ctor_y).take(n).collect();

        b.iter(|| {
            for (x, y) in xs.iter().zip(ys.iter()) {
                black_box(x.$op(y));
            }
        });
    }
));

macro_rules! bench_one (
    (x_static   $x:expr, $y:expr) => (check_type!(check_type_x, $x, Static(..)););
    (x_inline   $x:expr, $y:expr) => (check_type!(check_type_x, $x, Inline(..)););
    (x_dynamic  $x:expr, $y:expr) => (check_type!(check_type_x, $x, Dynamic(..)););
    (y_static   $x:expr, $y:expr) => (check_type!(check_type_y, $y, Static(..)););
    (y_inline   $x:expr, $y:expr) => (check_type!(check_type_y, $y, Inline(..)););
    (y_dynamic  $x:expr, $y:expr) => (check_type!(check_type_y, $y, Dynamic(..)););
    (is_static  $x:expr, $y:expr) => (bench_one!(x_static  $x, $y); bench_one!(y_static  $x, $y););
    (is_inline  $x:expr, $y:expr) => (bench_one!(x_inline  $x, $y); bench_one!(y_inline  $x, $y););
    (is_dynamic $x:expr, $y:expr) => (bench_one!(x_dynamic $x, $y); bench_one!(y_dynamic $x, $y););

    (eq $x:expr, $_y:expr) => (bench_tiny_op!(eq_x_1000, eq, $x, $x););
    (ne $x:expr, $y:expr)  => (bench_tiny_op!(ne_x_1000, ne, $x, $y););
    (lt $x:expr, $y:expr)  => (bench_tiny_op!(lt_x_1000, lt, $x, $y););

    (intern $x:expr, $_y:expr) => (
        #[bench]
        fn intern(b: &mut Bencher) {
            let x = $x.to_string();
            b.iter(|| {
                black_box(TestAtom::from(&*x));
            });
        }
    );

    (as_ref $x:expr, $_y:expr) => (
        #[bench]
        fn as_ref_x_1000(b: &mut Bencher) {
            let x = $x;
            b.iter(|| {
                for _ in 0..1000 {
                    black_box(x.as_ref());
                }
            });
        }
    );

    (clone $x:expr, $_y:expr) => (
        #[bench]
        fn clone_x_1000(b: &mut Bencher) {
            let x = $x;
            b.iter(|| {
                for _ in 0..1000 {
                    black_box(x.clone());
                }
            });
        }
    );

    (clone_string $x:expr, $_y:expr) => (
        #[bench]
        fn clone_x_1000(b: &mut Bencher) {
            let x = $x.to_string();
            b.iter(|| {
                for _ in 0..1000 {
                    black_box(x.clone());
                }
            });
        }
    );
);

macro_rules! bench_all (
    ([ $($which:ident)+ ] for $name:ident = $x:expr, $y:expr) => (
        // FIXME: This module works around rust-lang/rust#12249 so we don't
        // have to repeat the names for eq and neq.
        mod $name {
            #![allow(unused_imports)]

            use test::{Bencher, black_box};
            use std::string::ToString;
            use std::iter::repeat;

            use atom::tests::TestAtom;
            use atom::UnpackedAtom::{Static, Inline, Dynamic};

            use super::mk;

            $(
                bench_one!($which $x, $y);
            )+
        }
    );
);

pub const longer_dynamic_a: &'static str
    = "Thee Silver Mt. Zion Memorial Orchestra & Tra-La-La Band";
pub const longer_dynamic_b: &'static str
    = "Thee Silver Mt. Zion Memorial Orchestra & Tra-La-La Ban!";

bench_all!([eq ne lt clone_string] for short_string = "e", "f");
bench_all!([eq ne lt clone_string] for medium_string = "xyzzy01", "xyzzy02");
bench_all!([eq ne lt clone_string]
    for longer_string = super::longer_dynamic_a, super::longer_dynamic_b);

bench_all!([eq ne intern as_ref clone is_static lt]
    for static_atom = test_atom!("a"), test_atom!("b"));

bench_all!([intern as_ref clone is_inline]
    for short_inline_atom = mk("e"), mk("f"));

bench_all!([eq ne intern as_ref clone is_inline lt]
    for medium_inline_atom = mk("xyzzy01"), mk("xyzzy02"));

bench_all!([intern as_ref clone is_dynamic]
    for min_dynamic_atom = mk("xyzzy001"), mk("xyzzy002"));

bench_all!([eq ne intern as_ref clone is_dynamic lt]
    for longer_dynamic_atom = mk(super::longer_dynamic_a), mk(super::longer_dynamic_b));

bench_all!([intern as_ref clone is_static]
    for static_at_runtime = mk("a"), mk("b"));

bench_all!([ne lt x_static y_inline]
    for static_vs_inline  = test_atom!("a"), mk("f"));

bench_all!([ne lt x_static y_dynamic]
    for static_vs_dynamic = test_atom!("a"), mk(super::longer_dynamic_b));

bench_all!([ne lt x_inline y_dynamic]
    for inline_vs_dynamic = mk("e"), mk(super::longer_dynamic_b));

macro_rules! bench_rand ( ($name:ident, $len:expr) => (
    #[bench]
    fn $name(b: &mut Bencher) {
        use std::str;
        use rand;
        use rand::Rng;

        let mut gen = rand::weak_rng();
        b.iter(|| {
            // We have to generate new atoms on every iter, because
            // the dynamic atom table isn't reset.
            //
            // I measured the overhead of random string generation
            // as about 3-12% at one point.

            let mut buf: [u8; $len] = [0; $len];
            gen.fill_bytes(&mut buf);
            for n in buf.iter_mut() {
                // shift into printable ASCII
                *n = (*n % 0x40) + 0x20;
            }
            let s = str::from_utf8(&buf[..]).unwrap();
            black_box(TestAtom::from(s));
        });
    }
));

bench_rand!(intern_rand_008,   8);
bench_rand!(intern_rand_032,  32);
bench_rand!(intern_rand_128, 128);
bench_rand!(intern_rand_512, 512);
