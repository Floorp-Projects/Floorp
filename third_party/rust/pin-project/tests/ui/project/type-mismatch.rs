#![allow(deprecated)]
#![feature(proc_macro_hygiene, stmt_expr_attributes)]

use pin_project::{pin_project, project};
use std::pin::Pin;

#[project]
fn type_mismatch() {
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

    let mut foo = Enum::Variant1(1, 2);
    let mut foo = Pin::new(&mut foo).project();

    #[project]
    match &mut foo {
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
        None => {} //~ ERROR mismatched types
    }
}

fn type_mismatch_span_issue() {
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

    let mut foo = Enum::Variant1(1, 2);
    let mut foo = Pin::new(&mut foo).project();

    #[project]
    match &mut foo {
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
        None => {} //~ ERROR mismatched types
    }
}

fn main() {}
