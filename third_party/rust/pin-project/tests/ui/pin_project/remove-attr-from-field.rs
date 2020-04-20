use auxiliary_macros::remove_attr;
use pin_project::pin_project;
use std::{marker::PhantomPinned, pin::Pin};

fn is_unpin<T: Unpin>() {}

#[pin_project]
#[remove_attr(field)]
struct Foo {
    #[pin]
    field: PhantomPinned,
}

#[remove_attr(field)]
#[pin_project]
struct Bar {
    #[pin]
    field: PhantomPinned,
}

fn main() {
    is_unpin::<Foo>();
    is_unpin::<Bar>();

    let mut x = Foo { field: PhantomPinned };
    let x = Pin::new(&mut x).project();
    let _: Pin<&mut PhantomPinned> = x.field; //~ ERROR E0308

    let mut x = Bar { field: PhantomPinned };
    let x = Pin::new(&mut x).project();
    let _: Pin<&mut PhantomPinned> = x.field; //~ ERROR E0308
}
