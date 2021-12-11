use auxiliary_macro::remove_attr;
use pin_project::pin_project;
use std::{marker::PhantomPinned, pin::Pin};

fn is_unpin<T: Unpin>() {}

#[pin_project]
#[remove_attr(field_all)]
struct A {
    #[pin]
    field: PhantomPinned,
}

#[remove_attr(field_all)]
#[pin_project]
struct B {
    #[pin]
    field: PhantomPinned,
}

fn main() {
    is_unpin::<A>();
    is_unpin::<B>();

    let mut x = A { field: PhantomPinned };
    let x = Pin::new(&mut x).project();
    let _: Pin<&mut PhantomPinned> = x.field; //~ ERROR E0308

    let mut x = B { field: PhantomPinned };
    let x = Pin::new(&mut x).project();
    let _: Pin<&mut PhantomPinned> = x.field; //~ ERROR E0308
}
