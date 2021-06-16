#![warn(unsafe_code)]
#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]

// This hack is needed until https://github.com/rust-lang/rust/pull/69201
// makes it way into stable.
// Ceurrently, `#[attr] if true {}` doesn't even *parse* on stable,
// which means that it will error even behind a `#[rustversion::nightly]`
//
// This trick makes sure that we don't even attempt to parse
// the `#[project] if let _` test on stable.
#[rustversion::nightly]
include!("project_if_attr.rs.in");

use pin_project::{pin_project, project};
use std::pin::Pin;

#[project] // Nightly does not need a dummy attribute to the function.
#[test]
fn project_stmt_expr() {
    // struct

    #[pin_project]
    struct Foo<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    let mut foo = Foo { field1: 1, field2: 2 };

    #[project]
    let Foo { field1, field2 } = Pin::new(&mut foo).project();

    let x: Pin<&mut i32> = field1;
    assert_eq!(*x, 1);

    let y: &mut i32 = field2;
    assert_eq!(*y, 2);

    // tuple struct

    #[pin_project]
    struct Bar<T, U>(#[pin] T, U);

    let mut bar = Bar(1, 2);

    #[project]
    let Bar(x, y) = Pin::new(&mut bar).project();

    let x: Pin<&mut i32> = x;
    assert_eq!(*x, 1);

    let y: &mut i32 = y;
    assert_eq!(*y, 2);

    // enum

    #[pin_project]
    enum Baz<A, B, C, D> {
        Variant1(#[pin] A, B),
        Variant2 {
            #[pin]
            field1: C,
            field2: D,
        },
        None,
    }

    let mut baz = Baz::Variant1(1, 2);

    let mut baz = Pin::new(&mut baz).project();

    #[project]
    match &mut baz {
        Baz::Variant1(x, y) => {
            let x: &mut Pin<&mut i32> = x;
            assert_eq!(**x, 1);

            let y: &mut &mut i32 = y;
            assert_eq!(**y, 2);
        }
        Baz::Variant2 { field1, field2 } => {
            let _x: &mut Pin<&mut i32> = field1;
            let _y: &mut &mut i32 = field2;
        }
        Baz::None => {}
    }

    #[project]
    let val = match &mut baz {
        Baz::Variant1(_, _) => true,
        Baz::Variant2 { .. } => false,
        Baz::None => false,
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
    use crate::A;
    use core::pin::Pin;
    use pin_project::project;

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
    #[project]
    use crate::A;
    use pin_project::project;

    #[project]
    impl A {
        fn project_use(self) {}
    }
}

#[pin_project]
struct StructWhereClause<T>
where
    T: Copy,
{
    field: T,
}

#[pin_project]
struct TupleStructWhereClause<T>(T)
where
    T: Copy;

#[pin_project]
enum EnumWhereClause<T>
where
    T: Copy,
{
    Variant(T),
}
