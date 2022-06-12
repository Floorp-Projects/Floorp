#![allow(dead_code, non_camel_case_types)]
#[macro_use]
extern crate derive_more;

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
    MulAssign,
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

#[derive(From, Not, Add, Mul, AddAssign, MulAssign, Constructor, Sum)]
struct WrappedDouble<T: Clone, U: Clone>(T, U);

#[derive(From)]
#[from(forward)]
struct WrappedDouble2<T: Clone, U: Clone>(T, U);

#[cfg(feature = "nightly")]
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
    MulAssign,
    Deref,
    DerefMut,
    IntoIterator,
    Constructor
)]
struct WrappedWithConst<T, const C: u32>(T);

#[derive(
    From,
    FromStr,
    Display,
    Index,
    Not,
    Add,
    Mul,
    IndexMut,
    AddAssign,
    MulAssign,
    Deref,
    DerefMut,
    IntoIterator,
    Constructor,
    Sum
)]
#[deref(forward)]
#[deref_mut(forward)]
#[into_iterator(owned, ref, ref_mut)]
struct Struct<T: Clone> {
    t: T,
}

#[derive(Deref, DerefMut)]
struct Struct2<T: Clone> {
    t: T,
}

#[derive(From, Not, Add, Mul, AddAssign, MulAssign, Constructor, Sum)]
struct DoubleStruct<T: Clone, U: Clone> {
    t: T,
    u: U,
}

#[derive(From)]
#[from(forward)]
struct DoubleStruct2<T: Clone, U: Clone> {
    t: T,
    u: U,
}

#[derive(From, Not, Add)]
enum TupleEnum<T: Clone, U: Clone> {
    Tuple(T),
    DoubleTuple(T, U),
}

#[derive(From)]
#[from(forward)]
enum TupleEnum2<T: Clone, U: Clone, X: Clone> {
    DoubleTuple(T, U),
    TripleTuple(T, U, X),
}

#[derive(From, Not, Add)]
enum StructEnum<T: Clone, U: Clone> {
    Struct { t: T },
    DoubleStruct { t: T, u: U },
}

#[derive(From)]
#[from(forward)]
enum StructEnum2<T: Clone, U: Clone, X: Clone> {
    DoubleStruct { t: T, u: U },
    TripleStruct { t: T, u: U, x: X },
}
