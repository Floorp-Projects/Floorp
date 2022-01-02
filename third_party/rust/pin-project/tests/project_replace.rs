#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]
#![allow(deprecated)]

use std::{marker::PhantomData, pin::Pin};

use pin_project::{pin_project, project_replace};

#[project_replace] // Nightly does not need a dummy attribute to the function.
#[test]
fn project_replace_stmt_expr() {
    #[pin_project(project_replace)]
    struct Struct<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    let mut s = Struct { field1: 1, field2: 2 };

    #[project_replace]
    let Struct { field1, field2 } =
        Pin::new(&mut s).project_replace(Struct { field1: 42, field2: 43 });

    let _x: PhantomData<i32> = field1;

    let y: i32 = field2;
    assert_eq!(y, 2);

    // tuple struct

    #[pin_project(project_replace)]
    struct TupleStruct<T, U>(#[pin] T, U);

    let mut s = TupleStruct(1, 2);

    #[project_replace]
    let TupleStruct(x, y) = Pin::new(&mut s).project_replace(TupleStruct(42, 43));

    let _x: PhantomData<i32> = x;
    let y: i32 = y;
    assert_eq!(y, 2);

    #[pin_project(project_replace)]
    enum Enum<A, B, C, D> {
        Variant1(#[pin] A, B),
        Variant2 {
            #[pin]
            field1: C,
            field2: D,
        },
        None,
    }

    let mut e = Enum::Variant1(1, 2);

    let e = Pin::new(&mut e).project_replace(Enum::None);

    #[project_replace]
    match e {
        Enum::Variant1(x, y) => {
            let _x: PhantomData<i32> = x;
            let y: i32 = y;
            assert_eq!(y, 2);
        }
        Enum::Variant2 { field1, field2 } => {
            let _x: PhantomData<i32> = field1;
            let _y: i32 = field2;
            panic!()
        }
        Enum::None => panic!(),
    }
}

#[project_replace]
#[test]
fn combine() {
    #[pin_project(project_replace)]
    enum Enum<A> {
        V1(#[pin] A),
        V2,
    }

    let mut x = Enum::V1(1);
    #[project]
    match Pin::new(&mut x).project() {
        Enum::V1(_) => {}
        Enum::V2 => unreachable!(),
    }
    #[project_ref]
    match Pin::new(&x).project_ref() {
        Enum::V1(_) => {}
        Enum::V2 => unreachable!(),
    }
    #[project_replace]
    match Pin::new(&mut x).project_replace(Enum::V2) {
        Enum::V1(_) => {}
        Enum::V2 => unreachable!(),
    }
}
