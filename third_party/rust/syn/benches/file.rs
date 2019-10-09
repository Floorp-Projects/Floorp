// $ cargo bench --features full --bench file

#![feature(rustc_private, test)]

extern crate test;

#[path = "../tests/repo/mod.rs"]
pub mod repo;

use proc_macro2::TokenStream;
use std::fs;
use std::str::FromStr;
use test::Bencher;

const FILE: &str = "tests/rust/src/libcore/str/mod.rs";

#[bench]
fn parse_file(b: &mut Bencher) {
    repo::clone_rust();
    let content = fs::read_to_string(FILE).unwrap();
    let tokens = TokenStream::from_str(&content).unwrap();
    b.iter(|| syn::parse2::<syn::File>(tokens.clone()));
}
