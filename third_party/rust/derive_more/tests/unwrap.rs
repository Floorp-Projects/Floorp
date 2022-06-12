#![allow(dead_code)]

#[macro_use]
extern crate derive_more;

#[derive(Unwrap)]
enum Either<TLeft, TRight> {
    Left(TLeft),
    Right(TRight),
}

#[derive(Unwrap)]
enum Maybe<T> {
    Nothing,
    Just(T),
}

#[derive(Unwrap)]
enum Color {
    RGB(u8, u8, u8),
    CMYK(u8, u8, u8, u8),
}

#[derive(Unwrap)]
enum Nonsense<'a, T> {
    Ref(&'a T),
    NoRef,
    #[unwrap(ignore)]
    NoRefIgnored,
}

#[derive(Unwrap)]
enum WithConstraints<T>
where
    T: Copy,
{
    One(T),
    Two,
}
#[derive(Unwrap)]
enum KitchenSink<'a, 'b, T1: Copy, T2: Clone>
where
    T2: Into<T1> + 'b,
{
    Left(&'a T1),
    Right(&'b T2),
    OwnBoth(T1, T2),
    Empty,
    NeverMind(),
    NothingToSeeHere(),
}

#[test]
pub fn test_unwrap() {
    assert_eq!(Maybe::<()>::Nothing.unwrap_nothing(), ());
}

#[test]
#[should_panic]
pub fn test_unwrap_panic() {
    Maybe::<()>::Nothing.unwrap_just()
}
