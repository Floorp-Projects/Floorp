// Translated from C to Rust. The original C code can be found at
// https://github.com/ulfjack/ryu and carries the following license:
//
// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

extern crate rand;
extern crate ryu;

#[macro_use]
mod macros;

use std::{f64, str};

fn print(f: f64) -> String {
    let mut bytes = [0u8; 24];
    let n = unsafe { ryu::raw::d2s_buffered_n(f, &mut bytes[0]) };
    let s = str::from_utf8(&bytes[..n]).unwrap();
    s.to_owned()
}

fn pretty(f: f64) -> String {
    ryu::Buffer::new().format(f).to_owned()
}

#[test]
fn test_ryu() {
    check!(3E-1, 0.3);
    check!(1.234E15, 1234000000000000.0);
    check!(1.234E16, 1.234e16);
    check!(2.71828E0, 2.71828);
    check!(1.1E128, 1.1e128);
    check!(1.1E-64, 1.1e-64);
    check!(2.718281828459045E0, 2.718281828459045);
    check!(5E-324, 5e-324);
    check!(1.7976931348623157E308, 1.7976931348623157e308);
}

#[test]
fn test_random() {
    let mut bytes = [0u8; 24];
    let mut buffer = ryu::Buffer::new();
    for _ in 0..1000000 {
        let f = rand::random();
        let n = unsafe { ryu::raw::d2s_buffered_n(f, &mut bytes[0]) };
        assert_eq!(f, str::from_utf8(&bytes[..n]).unwrap().parse().unwrap());
        assert_eq!(f, buffer.format(f).parse().unwrap());
    }
}

#[test]
fn test_non_finite() {
    for i in 0u64..1 << 23 {
        let f = f64::from_bits((((1 << 11) - 1) << 52) + (i << 29));
        assert!(!f.is_finite(), "f={}", f);
        ryu::Buffer::new().format(f);
    }
}

#[test]
fn test_basic() {
    check!(0E0, 0.0);
    check!(-0E0, -0.0);
    check!(1E0, 1.0);
    check!(-1E0, -1.0);
    assert_eq!(print(f64::NAN), "NaN");
    assert_eq!(print(f64::INFINITY), "Infinity");
    assert_eq!(print(f64::NEG_INFINITY), "-Infinity");
}

#[test]
fn test_switch_to_subnormal() {
    check!(2.2250738585072014E-308, 2.2250738585072014e-308);
}

#[test]
fn test_min_and_max() {
    assert_eq!(f64::from_bits(0x7fefffffffffffff), 1.7976931348623157e308);
    check!(1.7976931348623157E308, 1.7976931348623157e308);
    assert_eq!(f64::from_bits(1), 5e-324);
    check!(5E-324, 5e-324);
}

#[test]
fn test_lots_of_trailing_zeros() {
    check!(2.9802322387695312E-8, 2.9802322387695312e-8);
}

#[test]
fn test_regression() {
    check!(-2.109808898695963E16, -2.109808898695963e16);
    check!(4.940656E-318, 4.940656e-318);
    check!(1.18575755E-316, 1.18575755e-316);
    check!(2.989102097996E-312, 2.989102097996e-312);
    check!(9.0608011534336E15, 9060801153433600.0);
    check!(4.708356024711512E18, 4.708356024711512e18);
    check!(9.409340012568248E18, 9.409340012568248e18);
    check!(1.2345678E0, 1.2345678);
}

#[test]
fn test_looks_like_pow5() {
    // These numbers have a mantissa that is a multiple of the largest power of
    // 5 that fits, and an exponent that causes the computation for q to result
    // in 22, which is a corner case for Ryu.
    assert_eq!(f64::from_bits(0x4830F0CF064DD592), 5.764607523034235e39);
    check!(5.764607523034235E39, 5.764607523034235e39);
    assert_eq!(f64::from_bits(0x4840F0CF064DD592), 1.152921504606847e40);
    check!(1.152921504606847E40, 1.152921504606847e40);
    assert_eq!(f64::from_bits(0x4850F0CF064DD592), 2.305843009213694e40);
    check!(2.305843009213694E40, 2.305843009213694e40);
}

#[test]
fn test_output_length() {
    check!(1E0, 1.0); // already tested in Basic
    check!(1.2E0, 1.2);
    check!(1.23E0, 1.23);
    check!(1.234E0, 1.234);
    check!(1.2345E0, 1.2345);
    check!(1.23456E0, 1.23456);
    check!(1.234567E0, 1.234567);
    check!(1.2345678E0, 1.2345678); // already tested in Regression
    check!(1.23456789E0, 1.23456789);
    check!(1.234567895E0, 1.234567895); // 1.234567890 would be trimmed
    check!(1.2345678901E0, 1.2345678901);
    check!(1.23456789012E0, 1.23456789012);
    check!(1.234567890123E0, 1.234567890123);
    check!(1.2345678901234E0, 1.2345678901234);
    check!(1.23456789012345E0, 1.23456789012345);
    check!(1.234567890123456E0, 1.234567890123456);
    check!(1.2345678901234567E0, 1.2345678901234567);

    // Test 32-bit chunking
    check!(4.294967294E0, 4.294967294); // 2^32 - 2
    check!(4.294967295E0, 4.294967295); // 2^32 - 1
    check!(4.294967296E0, 4.294967296); // 2^32
    check!(4.294967297E0, 4.294967297); // 2^32 + 1
    check!(4.294967298E0, 4.294967298); // 2^32 + 2
}
