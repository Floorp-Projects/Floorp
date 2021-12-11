#![cfg(feature = "rand")]

extern crate num_bigint;
extern crate num_traits;
extern crate rand;

use num_bigint::RandBigInt;
use num_traits::Zero;
use rand::prelude::*;

fn test_mul_divide_torture_count(count: usize) {
    let bits_max = 1 << 12;
    let seed = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
    let mut rng = SmallRng::from_seed(seed);

    for _ in 0..count {
        // Test with numbers of random sizes:
        let xbits = rng.gen_range(0, bits_max);
        let ybits = rng.gen_range(0, bits_max);

        let x = rng.gen_biguint(xbits);
        let y = rng.gen_biguint(ybits);

        if x.is_zero() || y.is_zero() {
            continue;
        }

        let prod = &x * &y;
        assert_eq!(&prod / &x, y);
        assert_eq!(&prod / &y, x);
    }
}

#[test]
fn test_mul_divide_torture() {
    test_mul_divide_torture_count(1000);
}

#[test]
#[ignore]
fn test_mul_divide_torture_long() {
    test_mul_divide_torture_count(1000000);
}
