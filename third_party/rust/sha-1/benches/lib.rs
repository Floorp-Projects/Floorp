#![no_std]
#![feature(test)]
#[macro_use]
extern crate digest;
extern crate sha1;

bench!(sha1::Sha1);
