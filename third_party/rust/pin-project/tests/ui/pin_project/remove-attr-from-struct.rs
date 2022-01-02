use auxiliary_macro::remove_attr;
use pin_project::pin_project;
use std::{marker::PhantomPinned, pin::Pin};

fn is_unpin<T: Unpin>() {}

#[pin_project]
#[remove_attr(struct_all)]
struct A {
    #[pin] //~ ERROR cannot find attribute `pin` in this scope
    field: PhantomPinned,
}

#[remove_attr(struct_all)]
#[pin_project]
struct B {
    #[pin] //~ ERROR cannot find attribute `pin` in this scope
    field: PhantomPinned,
}

#[pin_project] //~ ERROR has been removed
#[remove_attr(struct_pin)]
struct C {
    field: PhantomPinned,
}

#[remove_attr(struct_pin)]
#[pin_project] // Ok
struct D {
    field: PhantomPinned,
}

fn main() {
    is_unpin::<A>(); //~ ERROR E0277
    is_unpin::<B>(); //~ ERROR E0277
    is_unpin::<D>(); // Ok

    let mut x = A { field: PhantomPinned };
    let _ = Pin::new(&mut x).project(); //~ ERROR E0277,E0599

    let mut x = B { field: PhantomPinned };
    let _ = Pin::new(&mut x).project(); //~ ERROR E0277,E0599

    let mut x = D { field: PhantomPinned };
    let _ = Pin::new(&mut x).project(); //~ Ok
}
