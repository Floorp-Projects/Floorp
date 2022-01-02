#![allow(dead_code)]
#[macro_use]
extern crate derive_more;

#[derive(FromStr)]
struct MyInt(i32);

#[derive(FromStr)]
struct Point1D {
    x: i32,
}
