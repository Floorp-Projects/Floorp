// NB: If you change this test, change 'stmt_expr_attributes.rs' at the same time.

#![allow(deprecated)]

use pin_project::{pin_project, project};
use std::pin::Pin;

fn project_stmt_expr_nightly() {
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

    let mut baz = Enum::Variant1(1, 2);

    let mut baz = Pin::new(&mut baz).project();

    #[project] //~ ERROR E0658
    match &mut baz {
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

    let () = #[project] //~ ERROR E0658
    match &mut baz {
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
    };
}

fn main() {}
