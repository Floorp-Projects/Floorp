use auxiliary_macros::remove_attr;
use pin_project::pin_project;
use std::{marker::PhantomPinned, pin::Pin};

fn is_unpin<T: Unpin>() {}

#[pin_project]
#[remove_attr(struct)]
struct Foo {
    #[pin] //~ ERROR cannot find attribute `pin` in this scope
    field: PhantomPinned,
}

#[remove_attr(struct)]
#[pin_project]
struct Bar {
    #[pin] //~ ERROR cannot find attribute `pin` in this scope
    field: PhantomPinned,
}

fn main() {
    is_unpin::<Foo>(); //~ ERROR E0277
    is_unpin::<Bar>(); //~ ERROR E0277

    let mut x = Foo { field: PhantomPinned };
    let _x = Pin::new(&mut x).project(); //~ ERROR E0277,E0599

    let mut x = Bar { field: PhantomPinned };
    let _x = Pin::new(&mut x).project(); //~ ERROR E0277,E0599
}
