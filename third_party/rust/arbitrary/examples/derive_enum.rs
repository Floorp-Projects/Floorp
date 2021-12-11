//! A simple example of deriving the `Arbitrary` trait for an `enum`.
//!
//! Note that this requires enabling the "derive" cargo feature.

use arbitrary::{Arbitrary, Unstructured};

#[derive(Arbitrary, Debug)]
enum MyEnum {
    UnitVariant,
    TupleVariant(bool, u32),
    StructVariant { x: i8, y: (u8, i32) },
}

fn main() {
    let raw = b"This is some raw, unstructured data!";

    let mut unstructured = Unstructured::new(raw);

    let instance = MyEnum::arbitrary(&mut unstructured)
        .expect("`unstructured` has enough underlying data to create all variants of `MyEnum`");

    println!("Here is an arbitrary enum: {:?}", instance);
}
