#![no_std]
#[macro_use]
extern crate digest;
extern crate sha1;

use digest::dev::{one_million_a, digest_test};

new_test!(sha1_main, "sha1", sha1::Sha1, digest_test);

#[test]
fn sha1_1million_a() {
    let output = include_bytes!("data/one_million_a.bin");
    one_million_a::<sha1::Sha1>(output);
}
