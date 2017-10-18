#![allow(dead_code)]
#![no_std]

#[macro_use]
extern crate bitflags;

#[allow(unused_imports)]
use core::fmt::Display;

bitflags! {
    /// baz
    struct Flags: u32 {
        const A       = 0b00000001;
    }
}

#[test]
fn main() {

}
