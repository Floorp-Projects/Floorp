#![allow(dead_code, unused_imports)]
#[macro_use]
extern crate derive_more;

#[derive(Deref)]
#[deref(forward)]
struct MyBoxedInt(Box<i32>);

#[derive(Deref)]
#[deref(forward)]
struct NumRef<'a> {
    num: &'a i32,
}

#[derive(Deref)]
struct NumRef2<'a> {
    #[deref(forward)]
    num: &'a i32,
    useless: bool,
}

#[derive(Deref)]
#[deref(forward)]
struct NumRef3<'a> {
    num: &'a i32,
    #[deref(ignore)]
    useless: bool,
}

#[derive(Deref)]
struct MyInt(i32);

#[derive(Deref)]
struct Point1D {
    x: i32,
}

#[derive(Deref)]
struct Point1D2 {
    x: i32,
    #[deref(ignore)]
    useless: bool,
}

#[derive(Deref)]
struct CoolVec {
    cool: bool,
    #[deref]
    vec: Vec<i32>,
}

#[derive(Deref)]
struct GenericVec<T>(Vec<T>);

#[test]
fn deref_generic() {
    let gv = GenericVec(Vec::<i32>::new());
    assert!(gv.is_empty())
}

#[derive(Deref)]
struct GenericBox<T>(#[deref(forward)] Box<T>);

#[test]
fn deref_generic_forward() {
    let boxed = GenericBox(Box::new(1i32));
    assert_eq!(*boxed, 1i32);
}
