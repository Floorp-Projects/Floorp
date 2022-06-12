#![allow(dead_code)]
#[macro_use]
extern crate derive_more;

use std::borrow::Cow;

#[derive(Into)]
#[into(owned, ref, ref_mut)]
struct EmptyTuple();

#[derive(Into)]
#[into(owned, ref, ref_mut)]
struct EmptyStruct {}

#[derive(Into)]
#[into(owned, ref, ref_mut)]
struct EmptyUnit;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[derive(Into)]
#[into(owned(types(i64, i128)), ref, ref_mut)]
struct MyInt(i32);

#[test]
fn explicit_types_struct_owned_only() {
    assert_eq!(i32::from(MyInt(42)), 42i32);
    assert_eq!(<&i32>::from(&MyInt(42)), &42i32);
    assert_eq!(<&mut i32>::from(&mut MyInt(42)), &mut 42i32);
    assert_eq!(i64::from(MyInt(42)), 42i64);
    assert_eq!(i128::from(MyInt(42)), 42i128);
}

#[derive(Into)]
#[into(owned, ref, ref_mut)]
struct MyInts(i32, i32);

#[derive(Into)]
#[into(owned, ref, ref_mut)]
struct Point1D {
    x: i32,
}

#[derive(Debug, Eq, PartialEq)]
#[derive(Into)]
#[into(owned, ref, ref_mut)]
struct Point2D {
    x: i32,
    y: i32,
}

#[derive(Into)]
#[into(owned, ref, ref_mut)]
struct Point2DWithIgnored {
    x: i32,
    y: i32,
    #[into(ignore)]
    useless: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[derive(Into)]
#[into(owned(types(i64, i128)), ref, ref_mut, types(i32))]
struct MyIntExplicit(MyInt);

#[test]
fn explicit_types_struct_all() {
    let mut input = MyIntExplicit(MyInt(42));
    assert_eq!(MyInt::from(input), MyInt(42));
    assert_eq!(<&MyInt>::from(&input), &MyInt(42));
    assert_eq!(<&mut MyInt>::from(&mut input), &mut MyInt(42));
    assert_eq!(i32::from(input), 42i32);
    assert_eq!(<&i32>::from(&input), &42i32);
    assert_eq!(<&mut i32>::from(&mut input), &mut 42i32);
    assert_eq!(i64::from(input), 42i64);
    assert_eq!(i128::from(input), 42i128);
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[derive(Into)]
#[into(owned(types(i32, i64, i128)), ref(types(i32)), ref_mut(types(i32)))]
struct MyIntsExplicit(i32, MyInt, MyIntExplicit);

#[test]
fn explicit_types_struct_tupled() {
    let mut input = MyIntsExplicit(42i32, MyInt(42), MyIntExplicit(MyInt(42)));
    assert_eq!(
        <(i32, MyInt, MyIntExplicit)>::from(input),
        (42i32, MyInt(42), MyIntExplicit(MyInt(42))),
    );
    assert_eq!(
        <(&i32, &MyInt, &MyIntExplicit)>::from(&input),
        (&42i32, &MyInt(42), &MyIntExplicit(MyInt(42))),
    );
    assert_eq!(
        <(&mut i32, &mut MyInt, &mut MyIntExplicit)>::from(&mut input),
        (&mut 42i32, &mut MyInt(42), &mut MyIntExplicit(MyInt(42))),
    );
    assert_eq!(<(i32, i32, i32)>::from(input), (42i32, 42i32, 42i32));
    assert_eq!(<(&i32, &i32, &i32)>::from(&input), (&42i32, &42i32, &42i32));
    assert_eq!(
        <(&mut i32, &mut i32, &mut i32)>::from(&mut input),
        (&mut 42i32, &mut 42i32, &mut 42i32),
    );
    assert_eq!(<(i64, i64, i64)>::from(input), (42i64, 42i64, 42i64));
    assert_eq!(<(i128, i128, i128)>::from(input), (42i128, 42i128, 42i128));
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[derive(Into)]
#[into(owned, ref, ref_mut, types(i32))]
struct Point2DExplicit {
    x: MyInt,
    y: MyInt,
}

#[test]
fn explicit_types_point_2d() {
    let mut input = Point2DExplicit {
        x: MyInt(42),
        y: MyInt(42),
    };
    assert_eq!(<(i32, i32)>::from(input), (42i32, 42i32));
    assert_eq!(<(&i32, &i32)>::from(&input), (&42i32, &42i32));
    assert_eq!(
        <(&mut i32, &mut i32)>::from(&mut input),
        (&mut 42i32, &mut 42i32)
    );
}

#[derive(Clone, Debug, Eq, PartialEq)]
#[derive(Into)]
#[into(owned(types("Cow<'_, str>")))]
struct Name(String);

#[test]
fn explicit_complex_types_name() {
    let name = "Ñolofinwë";
    let input = Name(name.to_owned());
    assert_eq!(String::from(input.clone()), name.to_owned());
    assert_eq!(Cow::from(input.clone()), Cow::Borrowed(name));
}
