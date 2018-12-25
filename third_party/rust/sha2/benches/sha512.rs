#![no_std]
#![feature(test)]
#[macro_use]
extern crate digest;
extern crate sha2;

bench!(sha2::Sha512);
