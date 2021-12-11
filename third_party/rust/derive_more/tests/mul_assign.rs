#![allow(dead_code)]
use std::marker::PhantomData;

#[macro_use]
extern crate derive_more;

#[derive(MulAssign)]
struct MyInt(i32);

#[derive(MulAssign)]
struct MyInts(i32, i32);

#[derive(MulAssign)]
#[mul_assign(forward)]
struct MyIntForward(i32);

#[derive(MulAssign)]
struct Point1D {
    x: i32,
}

#[derive(MulAssign)]
struct Point2D {
    x: i32,
    y: i32,
}

#[derive(MulAssign)]
struct MyInt2<T> {
    x: i32,
    ph: PhantomData<T>,
}
