#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]
#![allow(deprecated)]

// Ceurrently, `#[attr] if true {}` doesn't even *parse* on MSRV,
// which means that it will error even behind a `#[rustversion::since(..)]`
//
// This trick makes sure that we don't even attempt to parse
// the `#[project] if let _` test on MSRV.
#[rustversion::since(1.43)]
include!("project_if_attr.rs.in");

use std::pin::Pin;

use pin_project::{pin_project, project, project_ref, project_replace};

#[project] // Nightly does not need a dummy attribute to the function.
#[test]
fn project_stmt_expr() {
    #[pin_project]
    struct Struct<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    let mut s = Struct { field1: 1, field2: 2 };

    #[project]
    let Struct { field1, field2 } = Pin::new(&mut s).project();

    let x: Pin<&mut i32> = field1;
    assert_eq!(*x, 1);

    let y: &mut i32 = field2;
    assert_eq!(*y, 2);

    #[pin_project]
    struct TupleStruct<T, U>(#[pin] T, U);

    let mut s = TupleStruct(1, 2);

    #[project]
    let TupleStruct(x, y) = Pin::new(&mut s).project();

    let x: Pin<&mut i32> = x;
    assert_eq!(*x, 1);

    let y: &mut i32 = y;
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

    let mut e = Enum::Variant1(1, 2);

    let mut e = Pin::new(&mut e).project();

    #[project]
    match &mut e {
        Enum::Variant1(x, y) => {
            let x: &mut Pin<&mut i32> = x;
            assert_eq!(**x, 1);

            let y: &mut &mut i32 = y;
            assert_eq!(**y, 2);
        }
        Enum::Variant2 { field1, field2 } => {
            let _x: &mut Pin<&mut i32> = field1;
            let _y: &mut &mut i32 = field2;
        }
        Enum::None => {}
    }

    #[project]
    let val = match &mut e {
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

    #[project]
    impl<T, U> HasGenerics<T, U> {
        fn a(self) {
            let Self { field1, field2 } = self;

            let _x: Pin<&mut T> = field1;
            let _y: &mut U = field2;
        }
    }

    #[pin_project]
    struct NoneGenerics {
        #[pin]
        field1: i32,
        field2: u32,
    }

    #[project]
    impl NoneGenerics {}

    #[pin_project]
    struct HasLifetimes<'a, T, U> {
        #[pin]
        field1: &'a mut T,
        field2: U,
    }

    #[project]
    impl<T, U> HasLifetimes<'_, T, U> {}

    #[pin_project]
    struct HasOverlappingLifetimes<'pin, T, U> {
        #[pin]
        field1: &'pin mut T,
        field2: U,
    }

    #[allow(single_use_lifetimes)]
    #[project]
    impl<'pin, T, U> HasOverlappingLifetimes<'pin, T, U> {}

    #[pin_project]
    struct HasOverlappingLifetimes2<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    #[allow(single_use_lifetimes)]
    #[allow(clippy::needless_lifetimes)]
    #[project]
    impl<T, U> HasOverlappingLifetimes2<T, U> {
        fn foo<'pin>(&'pin self) {}
    }
}

#[pin_project]
struct A {
    #[pin]
    field: u8,
}

mod project_use_1 {
    use std::pin::Pin;

    use pin_project::project;

    use crate::A;
    #[project]
    use crate::A;

    #[project]
    #[test]
    fn project_use() {
        let mut x = A { field: 0 };
        #[project]
        let A { field } = Pin::new(&mut x).project();
        let _: Pin<&mut u8> = field;
    }
}

mod project_use_2 {
    use pin_project::project;

    #[project]
    use crate::A;

    #[project]
    impl A {
        fn project_use(self) {}
    }
}

#[allow(clippy::unnecessary_operation, clippy::unit_arg)]
#[test]
#[project]
fn non_stmt_expr_match() {
    #[pin_project]
    enum Enum<A> {
        Variant(#[pin] A),
    }

    let mut x = Enum::Variant(1);
    let x = Pin::new(&mut x).project();

    Some(
        #[project]
        match x {
            Enum::Variant(_x) => {}
        },
    );
}

// https://github.com/taiki-e/pin-project/issues/206
#[allow(clippy::unnecessary_operation, clippy::unit_arg)]
#[test]
#[project]
fn issue_206() {
    #[pin_project]
    enum Enum<A> {
        Variant(#[pin] A),
    }

    let mut x = Enum::Variant(1);
    let x = Pin::new(&mut x).project();

    Some({
        #[project]
        match &x {
            Enum::Variant(_) => {}
        }
    });

    #[allow(clippy::never_loop)]
    loop {
        let _ = {
            #[project]
            match &x {
                Enum::Variant(_) => {}
            }
        };
        break;
    }
}

#[project]
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

// FIXME: This should be denied, but allowed for compatibility at this time.
#[project]
#[project_ref]
#[project_replace]
#[test]
fn combine_compat() {
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
