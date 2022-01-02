// NB: If you change this test, change 'stmt_expr_attributes-feature-gate.rs' at the same time.

#![allow(deprecated)]
// proc_macro_hygiene
// Tracking issue: https://github.com/rust-lang/rust/issues/54727
#![feature(proc_macro_hygiene)]
// stmt_expr_attributes
// Tracking issue: https://github.com/rust-lang/rust/issues/15701
#![feature(stmt_expr_attributes)]

use pin_project::{pin_project, project};
use std::pin::Pin;

fn project_stmt_expr_nightly() {
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

    let () = #[project]
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
    };
}

fn main() {}
