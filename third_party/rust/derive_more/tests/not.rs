#![allow(dead_code)]
#[macro_use]
extern crate derive_more;

#[derive(Not)]
struct MyInts(i32, i32);

#[derive(Not)]
struct Point2D {
    x: i32,
    y: i32,
}

#[derive(Not)]
enum MixedInts {
    SmallInt(i32),
    BigInt(i64),
    TwoSmallInts(i32, i32),
    NamedSmallInts { x: i32, y: i32 },
    UnsignedOne(u32),
    UnsignedTwo(u32),
}

#[derive(Not)]
enum EnumWithUnit {
    SmallInt(i32),
    Unit,
}
