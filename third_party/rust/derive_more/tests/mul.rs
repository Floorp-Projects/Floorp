#![allow(dead_code)]
#[macro_use]
extern crate derive_more;

#[derive(Mul)]
struct MyInt(i32);

#[derive(Mul)]
struct MyInts(i32, i32);

#[derive(Mul)]
struct Point1D {
    x: i32,
}

#[derive(Mul)]
struct Point2D {
    x: i32,
    y: i32,
}
