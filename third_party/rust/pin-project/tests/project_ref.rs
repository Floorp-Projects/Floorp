#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]
#![allow(deprecated)]

use std::pin::Pin;

use pin_project::{pin_project, project_ref};

#[project_ref] // Nightly does not need a dummy attribute to the function.
#[test]
fn project_stmt_expr() {
    #[pin_project]
    struct Struct<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    let s = Struct { field1: 1, field2: 2 };

    #[project_ref]
    let Struct { field1, field2 } = Pin::new(&s).project_ref();

    let x: Pin<&i32> = field1;
    assert_eq!(*x, 1);

    let y: &i32 = field2;
    assert_eq!(*y, 2);

    // tuple struct

    #[pin_project]
    struct TupleStruct<T, U>(#[pin] T, U);

    let s = TupleStruct(1, 2);

    #[project_ref]
    let TupleStruct(x, y) = Pin::new(&s).project_ref();

    let x: Pin<&i32> = x;
    assert_eq!(*x, 1);

    let y: &i32 = y;
    assert_eq!(*y, 2);

    #[pin_project]
    enum Enum<A, B, C, D> {
        Variant1(#[pin] A, B),
        Variant2 {
            #[pin]
            field1: C,
            field2: D,
        },
        None,
    }

    let e = Enum::Variant1(1, 2);

    let e = Pin::new(&e).project_ref();

    #[project_ref]
    match &e {
        Enum::Variant1(x, y) => {
            let x: &Pin<&i32> = x;
            assert_eq!(**x, 1);

            let y: &&i32 = y;
            assert_eq!(**y, 2);
        }
        Enum::Variant2 { field1, field2 } => {
            let _x: &Pin<&i32> = field1;
            let _y: &&i32 = field2;
        }
        Enum::None => {}
    }

    #[project_ref]
    let val = match &e {
        Enum::Variant1(_, _) => true,
        Enum::Variant2 { .. } => false,
        Enum::None => false,
    };
    assert_eq!(val, true);
}

#[test]
fn project_impl() {
    #[pin_project]
    struct HasGenerics<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    #[project_ref]
    impl<T, U> HasGenerics<T, U> {
        fn a(self) {
            let Self { field1, field2 } = self;

            let _x: Pin<&T> = field1;
            let _y: &U = field2;
        }
    }

    #[pin_project]
    struct NoneGenerics {
        #[pin]
        field1: i32,
        field2: u32,
    }

    #[project_ref]
    impl NoneGenerics {}

    #[pin_project]
    struct HasLifetimes<'a, T, U> {
        #[pin]
        field1: &'a mut T,
        field2: U,
    }

    #[project_ref]
    impl<T, U> HasLifetimes<'_, T, U> {}

    #[pin_project]
    struct HasOverlappingLifetimes<'pin, T, U> {
        #[pin]
        field1: &'pin mut T,
        field2: U,
    }

    #[allow(single_use_lifetimes)]
    #[project_ref]
    impl<'pin, T, U> HasOverlappingLifetimes<'pin, T, U> {}

    #[pin_project]
    struct HasOverlappingLifetimes2<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    #[allow(single_use_lifetimes)]
    #[allow(clippy::needless_lifetimes)]
    #[project_ref]
    impl<T, U> HasOverlappingLifetimes2<T, U> {
        fn foo<'pin>(&'pin self) {}
    }
}

#[project_ref]
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
