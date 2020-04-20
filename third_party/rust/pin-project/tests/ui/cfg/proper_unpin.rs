use pin_project::pin_project;
use std::marker::PhantomPinned;

#[pin_project]
struct Foo<T> {
    #[cfg(any())]
    #[pin]
    inner: T,
    #[cfg(not(any()))]
    inner: T,
}

#[pin_project]
struct Bar<T> {
    #[cfg(any())]
    inner: T,
    #[cfg(not(any()))]
    #[pin]
    inner: T,
}

fn is_unpin<T: Unpin>() {}

fn main() {
    is_unpin::<Foo<PhantomPinned>>(); // Ok
    is_unpin::<Bar<()>>(); // Ok
    is_unpin::<Bar<PhantomPinned>>(); //~ ERROR E0277
}
