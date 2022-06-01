use pin_project::pin_project;
use std::marker::PhantomPinned;

struct Inner<T> {
    val: T,
}

#[pin_project(!Unpin)]
struct Foo<T, U> {
    #[pin]
    inner: Inner<T>,
    other: U,
}

#[pin_project(!Unpin)]
struct TrivialBounds {
    #[pin]
    field1: PhantomPinned,
}

#[pin_project(!Unpin)]
struct Bar<'a, T, U> {
    #[pin]
    inner: &'a mut Inner<T>,
    other: U,
}

fn is_unpin<T: Unpin>() {}

fn main() {
    is_unpin::<Foo<(), ()>>(); //~ ERROR E0277
    is_unpin::<Foo<PhantomPinned, ()>>(); //~ ERROR E0277
    is_unpin::<Foo<(), PhantomPinned>>(); //~ ERROR E0277
    is_unpin::<Foo<PhantomPinned, PhantomPinned>>(); //~ ERROR E0277

    is_unpin::<TrivialBounds>(); //~ ERROR E0277

    is_unpin::<Bar<'_, (), ()>>(); //~ ERROR E0277
    is_unpin::<Bar<'_, PhantomPinned, PhantomPinned>>(); //~ ERROR E0277
}
