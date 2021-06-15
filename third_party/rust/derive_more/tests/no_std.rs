#![no_std]
#![allow(dead_code)]

#[macro_use]
extern crate derive_more;

#[derive(
    AddAssign,
    MulAssign,
    Add,
    Mul,
    Not,
    Index,
    Display,
    FromStr,
    Into,
    From,
    IndexMut,
    Sum,
    Deref,
    DerefMut,
    Constructor
)]
#[into(owned, ref, ref_mut)]
struct MyInts(u64);

#[derive(Deref, DerefMut)]
#[deref(forward)]
#[deref_mut(forward)]
struct MyBoxedInt<'a>(&'a mut u64);

#[derive(
    From,
    FromStr,
    Display,
    Index,
    Not,
    Add,
    Mul,
    Sum,
    IndexMut,
    AddAssign,
    Deref,
    DerefMut,
    IntoIterator,
    Constructor
)]
#[deref(forward)]
#[deref_mut(forward)]
#[into_iterator(owned, ref, ref_mut)]
struct Wrapped<T: Clone>(T);

#[derive(Deref, DerefMut)]
struct Wrapped2<T: Clone>(T);

#[derive(From, Not, Add, Mul, AddAssign, Constructor, Sum)]
struct WrappedDouble<T: Clone, U: Clone>(T, U);

#[derive(Add, Not, TryInto)]
#[try_into(owned, ref, ref_mut)]
enum MixedInts {
    SmallInt(i32),
    BigInt(i64),
    TwoSmallInts(i32, i32),
    NamedSmallInts { x: i32, y: i32 },
    UnsignedOne(u32),
    UnsignedTwo(u32),
}

#[derive(Not, Add)]
enum EnumWithUnit {
    SmallInt(i32),
    Unit,
}
