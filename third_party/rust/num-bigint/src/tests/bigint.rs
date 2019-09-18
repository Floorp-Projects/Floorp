use {BigDigit, BigUint, big_digit};
use {Sign, BigInt, RandBigInt, ToBigInt};
use Sign::{Minus, NoSign, Plus};

use std::cmp::Ordering::{Less, Equal, Greater};
use std::{f32, f64};
use std::{i8, i16, i32, i64, isize};
use std::iter::repeat;
use std::{u8, u16, u32, u64, usize};
use std::ops::Neg;

use rand::thread_rng;

use integer::Integer;
use traits::{Zero, One, Signed, ToPrimitive, FromPrimitive, Num, Float};

/// Assert that an op works for all val/ref combinations
macro_rules! assert_op {
    ($left:ident $op:tt $right:ident == $expected:expr) => {
        assert_eq!((&$left) $op (&$right), $expected);
        assert_eq!((&$left) $op $right.clone(), $expected);
        assert_eq!($left.clone() $op (&$right), $expected);
        assert_eq!($left.clone() $op $right.clone(), $expected);
    };
}

/// Assert that an op works for scalar left or right
macro_rules! assert_scalar_op {
    (($($to:ident),*) $left:ident $op:tt $right:ident == $expected:expr) => {
        $(
            if let Some(left) = $left.$to() {
                assert_op!(left $op $right == $expected);
            }
            if let Some(right) = $right.$to() {
                assert_op!($left $op right == $expected);
            }
        )*
    };
    ($left:ident $op:tt $right:ident == $expected:expr) => {
        assert_scalar_op!((to_u8, to_u16, to_u32, to_u64, to_usize,
                           to_i8, to_i16, to_i32, to_i64, to_isize)
                          $left $op $right == $expected);
    };
}

#[test]
fn test_from_biguint() {
    fn check(inp_s: Sign, inp_n: usize, ans_s: Sign, ans_n: usize) {
        let inp = BigInt::from_biguint(inp_s, FromPrimitive::from_usize(inp_n).unwrap());
        let ans = BigInt {
            sign: ans_s,
            data: FromPrimitive::from_usize(ans_n).unwrap(),
        };
        assert_eq!(inp, ans);
    }
    check(Plus, 1, Plus, 1);
    check(Plus, 0, NoSign, 0);
    check(Minus, 1, Minus, 1);
    check(NoSign, 1, NoSign, 0);
}

#[test]
fn test_from_slice() {
    fn check(inp_s: Sign, inp_n: u32, ans_s: Sign, ans_n: u32) {
        let inp = BigInt::from_slice(inp_s, &[inp_n]);
        let ans = BigInt {
            sign: ans_s,
            data: FromPrimitive::from_u32(ans_n).unwrap(),
        };
        assert_eq!(inp, ans);
    }
    check(Plus, 1, Plus, 1);
    check(Plus, 0, NoSign, 0);
    check(Minus, 1, Minus, 1);
    check(NoSign, 1, NoSign, 0);
}

#[test]
fn test_assign_from_slice() {
    fn check(inp_s: Sign, inp_n: u32, ans_s: Sign, ans_n: u32) {
        let mut inp = BigInt::from_slice(Minus, &[2627_u32, 0_u32, 9182_u32, 42_u32]);
        inp.assign_from_slice(inp_s, &[inp_n]);
        let ans = BigInt {
            sign: ans_s,
            data: FromPrimitive::from_u32(ans_n).unwrap(),
        };
        assert_eq!(inp, ans);
    }
    check(Plus, 1, Plus, 1);
    check(Plus, 0, NoSign, 0);
    check(Minus, 1, Minus, 1);
    check(NoSign, 1, NoSign, 0);
}

#[test]
fn test_from_bytes_be() {
    fn check(s: &str, result: &str) {
        assert_eq!(BigInt::from_bytes_be(Plus, s.as_bytes()),
                   BigInt::parse_bytes(result.as_bytes(), 10).unwrap());
    }
    check("A", "65");
    check("AA", "16705");
    check("AB", "16706");
    check("Hello world!", "22405534230753963835153736737");
    assert_eq!(BigInt::from_bytes_be(Plus, &[]), Zero::zero());
    assert_eq!(BigInt::from_bytes_be(Minus, &[]), Zero::zero());
}

#[test]
fn test_to_bytes_be() {
    fn check(s: &str, result: &str) {
        let b = BigInt::parse_bytes(result.as_bytes(), 10).unwrap();
        let (sign, v) = b.to_bytes_be();
        assert_eq!((Plus, s.as_bytes()), (sign, &*v));
    }
    check("A", "65");
    check("AA", "16705");
    check("AB", "16706");
    check("Hello world!", "22405534230753963835153736737");
    let b: BigInt = Zero::zero();
    assert_eq!(b.to_bytes_be(), (NoSign, vec![0]));

    // Test with leading/trailing zero bytes and a full BigDigit of value 0
    let b = BigInt::from_str_radix("00010000000000000200", 16).unwrap();
    assert_eq!(b.to_bytes_be(), (Plus, vec![1, 0, 0, 0, 0, 0, 0, 2, 0]));
}

#[test]
fn test_from_bytes_le() {
    fn check(s: &str, result: &str) {
        assert_eq!(BigInt::from_bytes_le(Plus, s.as_bytes()),
                   BigInt::parse_bytes(result.as_bytes(), 10).unwrap());
    }
    check("A", "65");
    check("AA", "16705");
    check("BA", "16706");
    check("!dlrow olleH", "22405534230753963835153736737");
    assert_eq!(BigInt::from_bytes_le(Plus, &[]), Zero::zero());
    assert_eq!(BigInt::from_bytes_le(Minus, &[]), Zero::zero());
}

#[test]
fn test_to_bytes_le() {
    fn check(s: &str, result: &str) {
        let b = BigInt::parse_bytes(result.as_bytes(), 10).unwrap();
        let (sign, v) = b.to_bytes_le();
        assert_eq!((Plus, s.as_bytes()), (sign, &*v));
    }
    check("A", "65");
    check("AA", "16705");
    check("BA", "16706");
    check("!dlrow olleH", "22405534230753963835153736737");
    let b: BigInt = Zero::zero();
    assert_eq!(b.to_bytes_le(), (NoSign, vec![0]));

    // Test with leading/trailing zero bytes and a full BigDigit of value 0
    let b = BigInt::from_str_radix("00010000000000000200", 16).unwrap();
    assert_eq!(b.to_bytes_le(), (Plus, vec![0, 2, 0, 0, 0, 0, 0, 0, 1]));
}

#[test]
fn test_to_signed_bytes_le() {
    fn check(s: &str, result: Vec<u8>) {
        assert_eq!(BigInt::parse_bytes(s.as_bytes(), 10).unwrap().to_signed_bytes_le(),
                   result);
    }

    check("0", vec![0]);
    check("32767", vec![0xff, 0x7f]);
    check("-1", vec![0xff]);
    check("16777216", vec![0, 0, 0, 1]);
    check("-100", vec![156]);
    check("-8388608", vec![0, 0, 0x80]);
    check("-192", vec![0x40, 0xff]);
}

#[test]
fn test_from_signed_bytes_le() {
    fn check(s: &[u8], result: &str) {
        assert_eq!(BigInt::from_signed_bytes_le(s),
                   BigInt::parse_bytes(result.as_bytes(), 10).unwrap());
    }

    check(&[], "0");
    check(&[0], "0");
    check(&[0; 10], "0");
    check(&[0xff, 0x7f], "32767");
    check(&[0xff], "-1");
    check(&[0, 0, 0, 1], "16777216");
    check(&[156], "-100");
    check(&[0, 0, 0x80], "-8388608");
    check(&[0xff; 10], "-1");
    check(&[0x40, 0xff], "-192");
}

#[test]
fn test_to_signed_bytes_be() {
    fn check(s: &str, result: Vec<u8>) {
        assert_eq!(BigInt::parse_bytes(s.as_bytes(), 10).unwrap().to_signed_bytes_be(),
                   result);
    }

    check("0", vec![0]);
    check("32767", vec![0x7f, 0xff]);
    check("-1", vec![255]);
    check("16777216", vec![1, 0, 0, 0]);
    check("-100", vec![156]);
    check("-8388608", vec![128, 0, 0]);
    check("-192", vec![0xff, 0x40]);
}

#[test]
fn test_from_signed_bytes_be() {
    fn check(s: &[u8], result: &str) {
        assert_eq!(BigInt::from_signed_bytes_be(s),
                   BigInt::parse_bytes(result.as_bytes(), 10).unwrap());
    }

    check(&[], "0");
    check(&[0], "0");
    check(&[0; 10], "0");
    check(&[127, 255], "32767");
    check(&[255], "-1");
    check(&[1, 0, 0, 0], "16777216");
    check(&[156], "-100");
    check(&[128, 0, 0], "-8388608");
    check(&[255; 10], "-1");
    check(&[0xff, 0x40], "-192");
}

#[test]
fn test_cmp() {
    let vs: [&[BigDigit]; 4] = [&[2 as BigDigit], &[1, 1], &[2, 1], &[1, 1, 1]];
    let mut nums = Vec::new();
    for s in vs.iter().rev() {
        nums.push(BigInt::from_slice(Minus, *s));
    }
    nums.push(Zero::zero());
    nums.extend(vs.iter().map(|s| BigInt::from_slice(Plus, *s)));

    for (i, ni) in nums.iter().enumerate() {
        for (j0, nj) in nums[i..].iter().enumerate() {
            let j = i + j0;
            if i == j {
                assert_eq!(ni.cmp(nj), Equal);
                assert_eq!(nj.cmp(ni), Equal);
                assert_eq!(ni, nj);
                assert!(!(ni != nj));
                assert!(ni <= nj);
                assert!(ni >= nj);
                assert!(!(ni < nj));
                assert!(!(ni > nj));
            } else {
                assert_eq!(ni.cmp(nj), Less);
                assert_eq!(nj.cmp(ni), Greater);

                assert!(!(ni == nj));
                assert!(ni != nj);

                assert!(ni <= nj);
                assert!(!(ni >= nj));
                assert!(ni < nj);
                assert!(!(ni > nj));

                assert!(!(nj <= ni));
                assert!(nj >= ni);
                assert!(!(nj < ni));
                assert!(nj > ni);
            }
        }
    }
}


#[test]
fn test_hash() {
    use hash;

    let a = BigInt::new(NoSign, vec![]);
    let b = BigInt::new(NoSign, vec![0]);
    let c = BigInt::new(Plus, vec![1]);
    let d = BigInt::new(Plus, vec![1, 0, 0, 0, 0, 0]);
    let e = BigInt::new(Plus, vec![0, 0, 0, 0, 0, 1]);
    let f = BigInt::new(Minus, vec![1]);
    assert!(hash(&a) == hash(&b));
    assert!(hash(&b) != hash(&c));
    assert!(hash(&c) == hash(&d));
    assert!(hash(&d) != hash(&e));
    assert!(hash(&c) != hash(&f));
}

#[test]
fn test_convert_i64() {
    fn check(b1: BigInt, i: i64) {
        let b2: BigInt = FromPrimitive::from_i64(i).unwrap();
        assert!(b1 == b2);
        assert!(b1.to_i64().unwrap() == i);
    }

    check(Zero::zero(), 0);
    check(One::one(), 1);
    check(i64::MIN.to_bigint().unwrap(), i64::MIN);
    check(i64::MAX.to_bigint().unwrap(), i64::MAX);

    assert_eq!((i64::MAX as u64 + 1).to_bigint().unwrap().to_i64(), None);

    assert_eq!(BigInt::from_biguint(Plus, BigUint::new(vec![1, 2, 3, 4, 5])).to_i64(),
               None);

    assert_eq!(BigInt::from_biguint(Minus,
                                    BigUint::new(vec![1, 0, 0, 1 << (big_digit::BITS - 1)]))
                   .to_i64(),
               None);

    assert_eq!(BigInt::from_biguint(Minus, BigUint::new(vec![1, 2, 3, 4, 5])).to_i64(),
               None);
}

#[test]
fn test_convert_u64() {
    fn check(b1: BigInt, u: u64) {
        let b2: BigInt = FromPrimitive::from_u64(u).unwrap();
        assert!(b1 == b2);
        assert!(b1.to_u64().unwrap() == u);
    }

    check(Zero::zero(), 0);
    check(One::one(), 1);
    check(u64::MIN.to_bigint().unwrap(), u64::MIN);
    check(u64::MAX.to_bigint().unwrap(), u64::MAX);

    assert_eq!(BigInt::from_biguint(Plus, BigUint::new(vec![1, 2, 3, 4, 5])).to_u64(),
               None);

    let max_value: BigUint = FromPrimitive::from_u64(u64::MAX).unwrap();
    assert_eq!(BigInt::from_biguint(Minus, max_value).to_u64(), None);
    assert_eq!(BigInt::from_biguint(Minus, BigUint::new(vec![1, 2, 3, 4, 5])).to_u64(),
               None);
}

#[test]
fn test_convert_f32() {
    fn check(b1: &BigInt, f: f32) {
        let b2 = BigInt::from_f32(f).unwrap();
        assert_eq!(b1, &b2);
        assert_eq!(b1.to_f32().unwrap(), f);
        let neg_b1 = -b1;
        let neg_b2 = BigInt::from_f32(-f).unwrap();
        assert_eq!(neg_b1, neg_b2);
        assert_eq!(neg_b1.to_f32().unwrap(), -f);
    }

    check(&BigInt::zero(), 0.0);
    check(&BigInt::one(), 1.0);
    check(&BigInt::from(u16::MAX), 2.0.powi(16) - 1.0);
    check(&BigInt::from(1u64 << 32), 2.0.powi(32));
    check(&BigInt::from_slice(Plus, &[0, 0, 1]), 2.0.powi(64));
    check(&((BigInt::one() << 100) + (BigInt::one() << 123)),
          2.0.powi(100) + 2.0.powi(123));
    check(&(BigInt::one() << 127), 2.0.powi(127));
    check(&(BigInt::from((1u64 << 24) - 1) << (128 - 24)), f32::MAX);

    // keeping all 24 digits with the bits at different offsets to the BigDigits
    let x: u32 = 0b00000000101111011111011011011101;
    let mut f = x as f32;
    let mut b = BigInt::from(x);
    for _ in 0..64 {
        check(&b, f);
        f *= 2.0;
        b = b << 1;
    }

    // this number when rounded to f64 then f32 isn't the same as when rounded straight to f32
    let mut n: i64 = 0b0000000000111111111111111111111111011111111111111111111111111111;
    assert!((n as f64) as f32 != n as f32);
    assert_eq!(BigInt::from(n).to_f32(), Some(n as f32));
    n = -n;
    assert!((n as f64) as f32 != n as f32);
    assert_eq!(BigInt::from(n).to_f32(), Some(n as f32));

    // test rounding up with the bits at different offsets to the BigDigits
    let mut f = ((1u64 << 25) - 1) as f32;
    let mut b = BigInt::from(1u64 << 25);
    for _ in 0..64 {
        assert_eq!(b.to_f32(), Some(f));
        f *= 2.0;
        b = b << 1;
    }

    // rounding
    assert_eq!(BigInt::from_f32(-f32::consts::PI),
               Some(BigInt::from(-3i32)));
    assert_eq!(BigInt::from_f32(-f32::consts::E), Some(BigInt::from(-2i32)));
    assert_eq!(BigInt::from_f32(-0.99999), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f32(-0.5), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f32(-0.0), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f32(f32::MIN_POSITIVE / 2.0),
               Some(BigInt::zero()));
    assert_eq!(BigInt::from_f32(f32::MIN_POSITIVE), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f32(0.5), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f32(0.99999), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f32(f32::consts::E), Some(BigInt::from(2u32)));
    assert_eq!(BigInt::from_f32(f32::consts::PI), Some(BigInt::from(3u32)));

    // special float values
    assert_eq!(BigInt::from_f32(f32::NAN), None);
    assert_eq!(BigInt::from_f32(f32::INFINITY), None);
    assert_eq!(BigInt::from_f32(f32::NEG_INFINITY), None);

    // largest BigInt that will round to a finite f32 value
    let big_num = (BigInt::one() << 128) - BigInt::one() - (BigInt::one() << (128 - 25));
    assert_eq!(big_num.to_f32(), Some(f32::MAX));
    assert_eq!((&big_num + BigInt::one()).to_f32(), None);
    assert_eq!((-&big_num).to_f32(), Some(f32::MIN));
    assert_eq!(((-&big_num) - BigInt::one()).to_f32(), None);

    assert_eq!(((BigInt::one() << 128) - BigInt::one()).to_f32(), None);
    assert_eq!((BigInt::one() << 128).to_f32(), None);
    assert_eq!((-((BigInt::one() << 128) - BigInt::one())).to_f32(), None);
    assert_eq!((-(BigInt::one() << 128)).to_f32(), None);
}

#[test]
fn test_convert_f64() {
    fn check(b1: &BigInt, f: f64) {
        let b2 = BigInt::from_f64(f).unwrap();
        assert_eq!(b1, &b2);
        assert_eq!(b1.to_f64().unwrap(), f);
        let neg_b1 = -b1;
        let neg_b2 = BigInt::from_f64(-f).unwrap();
        assert_eq!(neg_b1, neg_b2);
        assert_eq!(neg_b1.to_f64().unwrap(), -f);
    }

    check(&BigInt::zero(), 0.0);
    check(&BigInt::one(), 1.0);
    check(&BigInt::from(u32::MAX), 2.0.powi(32) - 1.0);
    check(&BigInt::from(1u64 << 32), 2.0.powi(32));
    check(&BigInt::from_slice(Plus, &[0, 0, 1]), 2.0.powi(64));
    check(&((BigInt::one() << 100) + (BigInt::one() << 152)),
          2.0.powi(100) + 2.0.powi(152));
    check(&(BigInt::one() << 1023), 2.0.powi(1023));
    check(&(BigInt::from((1u64 << 53) - 1) << (1024 - 53)), f64::MAX);

    // keeping all 53 digits with the bits at different offsets to the BigDigits
    let x: u64 = 0b0000000000011110111110110111111101110111101111011111011011011101;
    let mut f = x as f64;
    let mut b = BigInt::from(x);
    for _ in 0..128 {
        check(&b, f);
        f *= 2.0;
        b = b << 1;
    }

    // test rounding up with the bits at different offsets to the BigDigits
    let mut f = ((1u64 << 54) - 1) as f64;
    let mut b = BigInt::from(1u64 << 54);
    for _ in 0..128 {
        assert_eq!(b.to_f64(), Some(f));
        f *= 2.0;
        b = b << 1;
    }

    // rounding
    assert_eq!(BigInt::from_f64(-f64::consts::PI),
               Some(BigInt::from(-3i32)));
    assert_eq!(BigInt::from_f64(-f64::consts::E), Some(BigInt::from(-2i32)));
    assert_eq!(BigInt::from_f64(-0.99999), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f64(-0.5), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f64(-0.0), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f64(f64::MIN_POSITIVE / 2.0),
               Some(BigInt::zero()));
    assert_eq!(BigInt::from_f64(f64::MIN_POSITIVE), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f64(0.5), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f64(0.99999), Some(BigInt::zero()));
    assert_eq!(BigInt::from_f64(f64::consts::E), Some(BigInt::from(2u32)));
    assert_eq!(BigInt::from_f64(f64::consts::PI), Some(BigInt::from(3u32)));

    // special float values
    assert_eq!(BigInt::from_f64(f64::NAN), None);
    assert_eq!(BigInt::from_f64(f64::INFINITY), None);
    assert_eq!(BigInt::from_f64(f64::NEG_INFINITY), None);

    // largest BigInt that will round to a finite f64 value
    let big_num = (BigInt::one() << 1024) - BigInt::one() - (BigInt::one() << (1024 - 54));
    assert_eq!(big_num.to_f64(), Some(f64::MAX));
    assert_eq!((&big_num + BigInt::one()).to_f64(), None);
    assert_eq!((-&big_num).to_f64(), Some(f64::MIN));
    assert_eq!(((-&big_num) - BigInt::one()).to_f64(), None);

    assert_eq!(((BigInt::one() << 1024) - BigInt::one()).to_f64(), None);
    assert_eq!((BigInt::one() << 1024).to_f64(), None);
    assert_eq!((-((BigInt::one() << 1024) - BigInt::one())).to_f64(), None);
    assert_eq!((-(BigInt::one() << 1024)).to_f64(), None);
}

#[test]
fn test_convert_to_biguint() {
    fn check(n: BigInt, ans_1: BigUint) {
        assert_eq!(n.to_biguint().unwrap(), ans_1);
        assert_eq!(n.to_biguint().unwrap().to_bigint().unwrap(), n);
    }
    let zero: BigInt = Zero::zero();
    let unsigned_zero: BigUint = Zero::zero();
    let positive = BigInt::from_biguint(Plus, BigUint::new(vec![1, 2, 3]));
    let negative = -&positive;

    check(zero, unsigned_zero);
    check(positive, BigUint::new(vec![1, 2, 3]));

    assert_eq!(negative.to_biguint(), None);
}

#[test]
fn test_convert_from_uint() {
    macro_rules! check {
        ($ty:ident, $max:expr) => {
            assert_eq!(BigInt::from($ty::zero()), BigInt::zero());
            assert_eq!(BigInt::from($ty::one()), BigInt::one());
            assert_eq!(BigInt::from($ty::MAX - $ty::one()), $max - BigInt::one());
            assert_eq!(BigInt::from($ty::MAX), $max);
        }
    }

    check!(u8, BigInt::from_slice(Plus, &[u8::MAX as BigDigit]));
    check!(u16, BigInt::from_slice(Plus, &[u16::MAX as BigDigit]));
    check!(u32, BigInt::from_slice(Plus, &[u32::MAX as BigDigit]));
    check!(u64,
           BigInt::from_slice(Plus, &[u32::MAX as BigDigit, u32::MAX as BigDigit]));
    check!(usize, BigInt::from(usize::MAX as u64));
}

#[test]
fn test_convert_from_int() {
    macro_rules! check {
        ($ty:ident, $min:expr, $max:expr) => {
            assert_eq!(BigInt::from($ty::MIN), $min);
            assert_eq!(BigInt::from($ty::MIN + $ty::one()), $min + BigInt::one());
            assert_eq!(BigInt::from(-$ty::one()), -BigInt::one());
            assert_eq!(BigInt::from($ty::zero()), BigInt::zero());
            assert_eq!(BigInt::from($ty::one()), BigInt::one());
            assert_eq!(BigInt::from($ty::MAX - $ty::one()), $max - BigInt::one());
            assert_eq!(BigInt::from($ty::MAX), $max);
        }
    }

    check!(i8,
           BigInt::from_slice(Minus, &[1 << 7]),
           BigInt::from_slice(Plus, &[i8::MAX as BigDigit]));
    check!(i16,
           BigInt::from_slice(Minus, &[1 << 15]),
           BigInt::from_slice(Plus, &[i16::MAX as BigDigit]));
    check!(i32,
           BigInt::from_slice(Minus, &[1 << 31]),
           BigInt::from_slice(Plus, &[i32::MAX as BigDigit]));
    check!(i64,
           BigInt::from_slice(Minus, &[0, 1 << 31]),
           BigInt::from_slice(Plus, &[u32::MAX as BigDigit, i32::MAX as BigDigit]));
    check!(isize,
           BigInt::from(isize::MIN as i64),
           BigInt::from(isize::MAX as i64));
}

#[test]
fn test_convert_from_biguint() {
    assert_eq!(BigInt::from(BigUint::zero()), BigInt::zero());
    assert_eq!(BigInt::from(BigUint::one()), BigInt::one());
    assert_eq!(BigInt::from(BigUint::from_slice(&[1, 2, 3])),
               BigInt::from_slice(Plus, &[1, 2, 3]));
}

const N1: BigDigit = -1i32 as BigDigit;
const N2: BigDigit = -2i32 as BigDigit;

const SUM_TRIPLES: &'static [(&'static [BigDigit],
           &'static [BigDigit],
           &'static [BigDigit])] = &[(&[], &[], &[]),
                                     (&[], &[1], &[1]),
                                     (&[1], &[1], &[2]),
                                     (&[1], &[1, 1], &[2, 1]),
                                     (&[1], &[N1], &[0, 1]),
                                     (&[1], &[N1, N1], &[0, 0, 1]),
                                     (&[N1, N1], &[N1, N1], &[N2, N1, 1]),
                                     (&[1, 1, 1], &[N1, N1], &[0, 1, 2]),
                                     (&[2, 2, 1], &[N1, N2], &[1, 1, 2])];

#[test]
fn test_add() {
    for elm in SUM_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let (na, nb, nc) = (-&a, -&b, -&c);

        assert_op!(a + b == c);
        assert_op!(b + a == c);
        assert_op!(c + na == b);
        assert_op!(c + nb == a);
        assert_op!(a + nc == nb);
        assert_op!(b + nc == na);
        assert_op!(na + nb == nc);
        assert_op!(a + na == Zero::zero());
    }
}

#[test]
fn test_scalar_add() {
    for elm in SUM_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let (na, nb, nc) = (-&a, -&b, -&c);

        assert_scalar_op!(a + b == c);
        assert_scalar_op!(b + a == c);
        assert_scalar_op!(c + na == b);
        assert_scalar_op!(c + nb == a);
        assert_scalar_op!(a + nc == nb);
        assert_scalar_op!(b + nc == na);
        assert_scalar_op!(na + nb == nc);
        assert_scalar_op!(a + na == Zero::zero());
    }
}

#[test]
fn test_sub() {
    for elm in SUM_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let (na, nb, nc) = (-&a, -&b, -&c);

        assert_op!(c - a == b);
        assert_op!(c - b == a);
        assert_op!(nb - a == nc);
        assert_op!(na - b == nc);
        assert_op!(b - na == c);
        assert_op!(a - nb == c);
        assert_op!(nc - na == nb);
        assert_op!(a - a == Zero::zero());
    }
}

#[test]
fn test_scalar_sub() {
    for elm in SUM_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let (na, nb, nc) = (-&a, -&b, -&c);

        assert_scalar_op!(c - a == b);
        assert_scalar_op!(c - b == a);
        assert_scalar_op!(nb - a == nc);
        assert_scalar_op!(na - b == nc);
        assert_scalar_op!(b - na == c);
        assert_scalar_op!(a - nb == c);
        assert_scalar_op!(nc - na == nb);
        assert_scalar_op!(a - a == Zero::zero());
    }
}

const M: u32 = ::std::u32::MAX;
static MUL_TRIPLES: &'static [(&'static [BigDigit],
           &'static [BigDigit],
           &'static [BigDigit])] = &[(&[], &[], &[]),
                                     (&[], &[1], &[]),
                                     (&[2], &[], &[]),
                                     (&[1], &[1], &[1]),
                                     (&[2], &[3], &[6]),
                                     (&[1], &[1, 1, 1], &[1, 1, 1]),
                                     (&[1, 2, 3], &[3], &[3, 6, 9]),
                                     (&[1, 1, 1], &[N1], &[N1, N1, N1]),
                                     (&[1, 2, 3], &[N1], &[N1, N2, N2, 2]),
                                     (&[1, 2, 3, 4], &[N1], &[N1, N2, N2, N2, 3]),
                                     (&[N1], &[N1], &[1, N2]),
                                     (&[N1, N1], &[N1], &[1, N1, N2]),
                                     (&[N1, N1, N1], &[N1], &[1, N1, N1, N2]),
                                     (&[N1, N1, N1, N1], &[N1], &[1, N1, N1, N1, N2]),
                                     (&[M / 2 + 1], &[2], &[0, 1]),
                                     (&[0, M / 2 + 1], &[2], &[0, 0, 1]),
                                     (&[1, 2], &[1, 2, 3], &[1, 4, 7, 6]),
                                     (&[N1, N1], &[N1, N1, N1], &[1, 0, N1, N2, N1]),
                                     (&[N1, N1, N1],
                                      &[N1, N1, N1, N1],
                                      &[1, 0, 0, N1, N2, N1, N1]),
                                     (&[0, 0, 1], &[1, 2, 3], &[0, 0, 1, 2, 3]),
                                     (&[0, 0, 1], &[0, 0, 0, 1], &[0, 0, 0, 0, 0, 1])];

static DIV_REM_QUADRUPLES: &'static [(&'static [BigDigit],
           &'static [BigDigit],
           &'static [BigDigit],
           &'static [BigDigit])] = &[(&[1], &[2], &[], &[1]),
                                     (&[3], &[2], &[1], &[1]),
                                     (&[1, 1], &[2], &[M / 2 + 1], &[1]),
                                     (&[1, 1, 1], &[2], &[M / 2 + 1, M / 2 + 1], &[1]),
                                     (&[0, 1], &[N1], &[1], &[1]),
                                     (&[N1, N1], &[N2], &[2, 1], &[3])];

#[test]
fn test_mul() {
    for elm in MUL_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let (na, nb, nc) = (-&a, -&b, -&c);

        assert_op!(a * b == c);
        assert_op!(b * a == c);
        assert_op!(na * nb == c);

        assert_op!(na * b == nc);
        assert_op!(nb * a == nc);
    }

    for elm in DIV_REM_QUADRUPLES.iter() {
        let (a_vec, b_vec, c_vec, d_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let d = BigInt::from_slice(Plus, d_vec);

        assert!(a == &b * &c + &d);
        assert!(a == &c * &b + &d);
    }
}

#[test]
fn test_scalar_mul() {
    for elm in MUL_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let (na, nb, nc) = (-&a, -&b, -&c);

        assert_scalar_op!(a * b == c);
        assert_scalar_op!(b * a == c);
        assert_scalar_op!(na * nb == c);

        assert_scalar_op!(na * b == nc);
        assert_scalar_op!(nb * a == nc);
    }
}

#[test]
fn test_div_mod_floor() {
    fn check_sub(a: &BigInt, b: &BigInt, ans_d: &BigInt, ans_m: &BigInt) {
        let (d, m) = a.div_mod_floor(b);
        if !m.is_zero() {
            assert_eq!(m.sign, b.sign);
        }
        assert!(m.abs() <= b.abs());
        assert!(*a == b * &d + &m);
        assert!(d == *ans_d);
        assert!(m == *ans_m);
    }

    fn check(a: &BigInt, b: &BigInt, d: &BigInt, m: &BigInt) {
        if m.is_zero() {
            check_sub(a, b, d, m);
            check_sub(a, &b.neg(), &d.neg(), m);
            check_sub(&a.neg(), b, &d.neg(), m);
            check_sub(&a.neg(), &b.neg(), d, m);
        } else {
            let one: BigInt = One::one();
            check_sub(a, b, d, m);
            check_sub(a, &b.neg(), &(d.neg() - &one), &(m - b));
            check_sub(&a.neg(), b, &(d.neg() - &one), &(b - m));
            check_sub(&a.neg(), &b.neg(), d, &m.neg());
        }
    }

    for elm in MUL_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);

        if !a.is_zero() {
            check(&c, &a, &b, &Zero::zero());
        }
        if !b.is_zero() {
            check(&c, &b, &a, &Zero::zero());
        }
    }

    for elm in DIV_REM_QUADRUPLES.iter() {
        let (a_vec, b_vec, c_vec, d_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let d = BigInt::from_slice(Plus, d_vec);

        if !b.is_zero() {
            check(&a, &b, &c, &d);
        }
    }
}


#[test]
fn test_div_rem() {
    fn check_sub(a: &BigInt, b: &BigInt, ans_q: &BigInt, ans_r: &BigInt) {
        let (q, r) = a.div_rem(b);
        if !r.is_zero() {
            assert_eq!(r.sign, a.sign);
        }
        assert!(r.abs() <= b.abs());
        assert!(*a == b * &q + &r);
        assert!(q == *ans_q);
        assert!(r == *ans_r);

        let (a, b, ans_q, ans_r) = (a.clone(), b.clone(), ans_q.clone(), ans_r.clone());
        assert_op!(a / b == ans_q);
        assert_op!(a % b == ans_r);
    }

    fn check(a: &BigInt, b: &BigInt, q: &BigInt, r: &BigInt) {
        check_sub(a, b, q, r);
        check_sub(a, &b.neg(), &q.neg(), r);
        check_sub(&a.neg(), b, &q.neg(), &r.neg());
        check_sub(&a.neg(), &b.neg(), q, &r.neg());
    }
    for elm in MUL_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);

        if !a.is_zero() {
            check(&c, &a, &b, &Zero::zero());
        }
        if !b.is_zero() {
            check(&c, &b, &a, &Zero::zero());
        }
    }

    for elm in DIV_REM_QUADRUPLES.iter() {
        let (a_vec, b_vec, c_vec, d_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let d = BigInt::from_slice(Plus, d_vec);

        if !b.is_zero() {
            check(&a, &b, &c, &d);
        }
    }
}

#[test]
fn test_scalar_div_rem() {
    fn check_sub(a: &BigInt, b: BigDigit, ans_q: &BigInt, ans_r: &BigInt) {
        let (q, r) = (a / b, a % b);
        if !r.is_zero() {
            assert_eq!(r.sign, a.sign);
        }
        assert!(r.abs() <= From::from(b));
        assert!(*a == b * &q + &r);
        assert!(q == *ans_q);
        assert!(r == *ans_r);

        let (a, b, ans_q, ans_r) = (a.clone(), b.clone(), ans_q.clone(), ans_r.clone());
        assert_op!(a / b == ans_q);
        assert_op!(a % b == ans_r);

        if b <= i32::max_value() as u32 {
            let nb = -(b as i32);
            assert_op!(a / nb == -ans_q.clone());
            assert_op!(a % nb == ans_r);
        }
    }

    fn check(a: &BigInt, b: BigDigit, q: &BigInt, r: &BigInt) {
        check_sub(a, b, q, r);
        check_sub(&a.neg(), b, &q.neg(), &r.neg());
    }

    for elm in MUL_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);

        if a_vec.len() == 1 && a_vec[0] != 0 {
            let a = a_vec[0];
            check(&c, a, &b, &Zero::zero());
        }

        if b_vec.len() == 1 && b_vec[0] != 0 {
            let b = b_vec[0];
            check(&c, b, &a, &Zero::zero());
        }
    }

    for elm in DIV_REM_QUADRUPLES.iter() {
        let (a_vec, b_vec, c_vec, d_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let d = BigInt::from_slice(Plus, d_vec);

        if b_vec.len() == 1 && b_vec[0] != 0 {
            let b = b_vec[0];
            check(&a, b, &c, &d);
        }
    }

}

#[test]
fn test_checked_add() {
    for elm in SUM_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);

        assert!(a.checked_add(&b).unwrap() == c);
        assert!(b.checked_add(&a).unwrap() == c);
        assert!(c.checked_add(&(-&a)).unwrap() == b);
        assert!(c.checked_add(&(-&b)).unwrap() == a);
        assert!(a.checked_add(&(-&c)).unwrap() == (-&b));
        assert!(b.checked_add(&(-&c)).unwrap() == (-&a));
        assert!((-&a).checked_add(&(-&b)).unwrap() == (-&c));
        assert!(a.checked_add(&(-&a)).unwrap() == Zero::zero());
    }
}

#[test]
fn test_checked_sub() {
    for elm in SUM_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);

        assert!(c.checked_sub(&a).unwrap() == b);
        assert!(c.checked_sub(&b).unwrap() == a);
        assert!((-&b).checked_sub(&a).unwrap() == (-&c));
        assert!((-&a).checked_sub(&b).unwrap() == (-&c));
        assert!(b.checked_sub(&(-&a)).unwrap() == c);
        assert!(a.checked_sub(&(-&b)).unwrap() == c);
        assert!((-&c).checked_sub(&(-&a)).unwrap() == (-&b));
        assert!(a.checked_sub(&a).unwrap() == Zero::zero());
    }
}

#[test]
fn test_checked_mul() {
    for elm in MUL_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);

        assert!(a.checked_mul(&b).unwrap() == c);
        assert!(b.checked_mul(&a).unwrap() == c);

        assert!((-&a).checked_mul(&b).unwrap() == -&c);
        assert!((-&b).checked_mul(&a).unwrap() == -&c);
    }

    for elm in DIV_REM_QUADRUPLES.iter() {
        let (a_vec, b_vec, c_vec, d_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);
        let d = BigInt::from_slice(Plus, d_vec);

        assert!(a == b.checked_mul(&c).unwrap() + &d);
        assert!(a == c.checked_mul(&b).unwrap() + &d);
    }
}
#[test]
fn test_checked_div() {
    for elm in MUL_TRIPLES.iter() {
        let (a_vec, b_vec, c_vec) = *elm;
        let a = BigInt::from_slice(Plus, a_vec);
        let b = BigInt::from_slice(Plus, b_vec);
        let c = BigInt::from_slice(Plus, c_vec);

        if !a.is_zero() {
            assert!(c.checked_div(&a).unwrap() == b);
            assert!((-&c).checked_div(&(-&a)).unwrap() == b);
            assert!((-&c).checked_div(&a).unwrap() == -&b);
        }
        if !b.is_zero() {
            assert!(c.checked_div(&b).unwrap() == a);
            assert!((-&c).checked_div(&(-&b)).unwrap() == a);
            assert!((-&c).checked_div(&b).unwrap() == -&a);
        }

        assert!(c.checked_div(&Zero::zero()).is_none());
        assert!((-&c).checked_div(&Zero::zero()).is_none());
    }
}

#[test]
fn test_gcd() {
    fn check(a: isize, b: isize, c: isize) {
        let big_a: BigInt = FromPrimitive::from_isize(a).unwrap();
        let big_b: BigInt = FromPrimitive::from_isize(b).unwrap();
        let big_c: BigInt = FromPrimitive::from_isize(c).unwrap();

        assert_eq!(big_a.gcd(&big_b), big_c);
    }

    check(10, 2, 2);
    check(10, 3, 1);
    check(0, 3, 3);
    check(3, 3, 3);
    check(56, 42, 14);
    check(3, -3, 3);
    check(-6, 3, 3);
    check(-4, -2, 2);
}

#[test]
fn test_lcm() {
    fn check(a: isize, b: isize, c: isize) {
        let big_a: BigInt = FromPrimitive::from_isize(a).unwrap();
        let big_b: BigInt = FromPrimitive::from_isize(b).unwrap();
        let big_c: BigInt = FromPrimitive::from_isize(c).unwrap();

        assert_eq!(big_a.lcm(&big_b), big_c);
    }

    check(1, 0, 0);
    check(0, 1, 0);
    check(1, 1, 1);
    check(-1, 1, 1);
    check(1, -1, 1);
    check(-1, -1, 1);
    check(8, 9, 72);
    check(11, 5, 55);
}

#[test]
fn test_abs_sub() {
    let zero: BigInt = Zero::zero();
    let one: BigInt = One::one();
    assert_eq!((-&one).abs_sub(&one), zero);
    let one: BigInt = One::one();
    let zero: BigInt = Zero::zero();
    assert_eq!(one.abs_sub(&one), zero);
    let one: BigInt = One::one();
    let zero: BigInt = Zero::zero();
    assert_eq!(one.abs_sub(&zero), one);
    let one: BigInt = One::one();
    let two: BigInt = FromPrimitive::from_isize(2).unwrap();
    assert_eq!(one.abs_sub(&-&one), two);
}

#[test]
fn test_from_str_radix() {
    fn check(s: &str, ans: Option<isize>) {
        let ans = ans.map(|n| {
            let x: BigInt = FromPrimitive::from_isize(n).unwrap();
            x
        });
        assert_eq!(BigInt::from_str_radix(s, 10).ok(), ans);
    }
    check("10", Some(10));
    check("1", Some(1));
    check("0", Some(0));
    check("-1", Some(-1));
    check("-10", Some(-10));
    check("+10", Some(10));
    check("--7", None);
    check("++5", None);
    check("+-9", None);
    check("-+3", None);
    check("Z", None);
    check("_", None);

    // issue 10522, this hit an edge case that caused it to
    // attempt to allocate a vector of size (-1u) == huge.
    let x: BigInt = format!("1{}", repeat("0").take(36).collect::<String>()).parse().unwrap();
    let _y = x.to_string();
}

#[test]
fn test_lower_hex() {
    let a = BigInt::parse_bytes(b"A", 16).unwrap();
    let hello = BigInt::parse_bytes("-22405534230753963835153736737".as_bytes(), 10).unwrap();

    assert_eq!(format!("{:x}", a), "a");
    assert_eq!(format!("{:x}", hello), "-48656c6c6f20776f726c6421");
    assert_eq!(format!("{:♥>+#8x}", a), "♥♥♥♥+0xa");
}

#[test]
fn test_upper_hex() {
    let a = BigInt::parse_bytes(b"A", 16).unwrap();
    let hello = BigInt::parse_bytes("-22405534230753963835153736737".as_bytes(), 10).unwrap();

    assert_eq!(format!("{:X}", a), "A");
    assert_eq!(format!("{:X}", hello), "-48656C6C6F20776F726C6421");
    assert_eq!(format!("{:♥>+#8X}", a), "♥♥♥♥+0xA");
}

#[test]
fn test_binary() {
    let a = BigInt::parse_bytes(b"A", 16).unwrap();
    let hello = BigInt::parse_bytes("-224055342307539".as_bytes(), 10).unwrap();

    assert_eq!(format!("{:b}", a), "1010");
    assert_eq!(format!("{:b}", hello),
               "-110010111100011011110011000101101001100011010011");
    assert_eq!(format!("{:♥>+#8b}", a), "♥+0b1010");
}

#[test]
fn test_octal() {
    let a = BigInt::parse_bytes(b"A", 16).unwrap();
    let hello = BigInt::parse_bytes("-22405534230753963835153736737".as_bytes(), 10).unwrap();

    assert_eq!(format!("{:o}", a), "12");
    assert_eq!(format!("{:o}", hello), "-22062554330674403566756233062041");
    assert_eq!(format!("{:♥>+#8o}", a), "♥♥♥+0o12");
}

#[test]
fn test_display() {
    let a = BigInt::parse_bytes(b"A", 16).unwrap();
    let hello = BigInt::parse_bytes("-22405534230753963835153736737".as_bytes(), 10).unwrap();

    assert_eq!(format!("{}", a), "10");
    assert_eq!(format!("{}", hello), "-22405534230753963835153736737");
    assert_eq!(format!("{:♥>+#8}", a), "♥♥♥♥♥+10");
}

#[test]
fn test_neg() {
    assert!(-BigInt::new(Plus, vec![1, 1, 1]) == BigInt::new(Minus, vec![1, 1, 1]));
    assert!(-BigInt::new(Minus, vec![1, 1, 1]) == BigInt::new(Plus, vec![1, 1, 1]));
    let zero: BigInt = Zero::zero();
    assert_eq!(-&zero, zero);
}

#[test]
fn test_rand() {
    let mut rng = thread_rng();
    let _n: BigInt = rng.gen_bigint(137);
    assert!(rng.gen_bigint(0).is_zero());
}

#[test]
fn test_rand_range() {
    let mut rng = thread_rng();

    for _ in 0..10 {
        assert_eq!(rng.gen_bigint_range(&FromPrimitive::from_usize(236).unwrap(),
                                        &FromPrimitive::from_usize(237).unwrap()),
                   FromPrimitive::from_usize(236).unwrap());
    }

    fn check(l: BigInt, u: BigInt) {
        let mut rng = thread_rng();
        for _ in 0..1000 {
            let n: BigInt = rng.gen_bigint_range(&l, &u);
            assert!(n >= l);
            assert!(n < u);
        }
    }
    let l: BigInt = FromPrimitive::from_usize(403469000 + 2352).unwrap();
    let u: BigInt = FromPrimitive::from_usize(403469000 + 3513).unwrap();
    check(l.clone(), u.clone());
    check(-l.clone(), u.clone());
    check(-u.clone(), -l.clone());
}

#[test]
#[should_panic]
fn test_zero_rand_range() {
    thread_rng().gen_bigint_range(&FromPrimitive::from_isize(54).unwrap(),
                                  &FromPrimitive::from_isize(54).unwrap());
}

#[test]
#[should_panic]
fn test_negative_rand_range() {
    let mut rng = thread_rng();
    let l = FromPrimitive::from_usize(2352).unwrap();
    let u = FromPrimitive::from_usize(3513).unwrap();
    // Switching u and l should fail:
    let _n: BigInt = rng.gen_bigint_range(&u, &l);
}
