#![allow(dead_code)]

#[macro_use]
extern crate derive_more;

#[derive(IsVariant)]
enum Either<TLeft, TRight> {
    Left(TLeft),
    Right(TRight),
}

#[derive(IsVariant)]
enum Maybe<T> {
    Nothing,
    Just(T),
}

#[derive(IsVariant)]
enum Color {
    RGB(u8, u8, u8),
    CMYK { c: u8, m: u8, y: u8, k: u8 },
}

#[derive(IsVariant)]
enum Nonsense<'a, T> {
    Ref(&'a T),
    NoRef,
    #[is_variant(ignore)]
    NoRefIgnored,
}

#[derive(IsVariant)]
enum WithConstraints<T>
where
    T: Copy,
{
    One(T),
    Two,
}
#[derive(IsVariant)]
enum KitchenSink<'a, 'b, T1: Copy, T2: Clone>
where
    T2: Into<T1> + 'b,
{
    Left(&'a T1),
    Right(&'b T2),
    OwnBoth { left: T1, right: T2 },
    Empty,
    NeverMind(),
    NothingToSeeHere {},
}

#[test]
pub fn test_is_variant() {
    assert!(Maybe::<()>::Nothing.is_nothing());
    assert!(!Maybe::<()>::Nothing.is_just());
}
