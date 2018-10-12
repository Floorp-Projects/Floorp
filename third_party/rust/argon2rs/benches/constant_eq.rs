// demonstrates (to some degree of certainty modulo process scheduling) that the
// run time of `verifier::constant_eq` is independent of its inputs.
#![feature(test)]

extern crate test;
extern crate argon2rs;
use argon2rs::verifier::constant_eq;

#[bench]
fn param_are_equal(b: &mut test::Bencher) {
    let lhs = (0..255).cycle().take(9001).collect::<Vec<u8>>();
    b.iter(|| constant_eq(&lhs[..], &lhs[..]));
}

#[bench]
fn params_are_unequal(b: &mut test::Bencher) {
    let lhs = (0..255).cycle().take(9001).collect::<Vec<u8>>();
    let mut rhs = lhs.clone();
    rhs[0] += 24;
    b.iter(|| constant_eq(&lhs[..], &rhs[..]));
}
