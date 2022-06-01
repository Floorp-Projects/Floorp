use pin_project::pin_project;
use std::marker::PhantomPinned;

#[cfg_attr(any(), pin_project)]
struct Foo<T> {
    inner: T,
}

#[cfg_attr(not(any()), pin_project)]
struct Bar<T> {
    #[cfg_attr(not(any()), pin)]
    inner: T,
}

fn is_unpin<T: Unpin>() {}

fn main() {
    is_unpin::<Foo<PhantomPinned>>(); // ERROR E0277
    is_unpin::<Bar<()>>(); // Ok
    is_unpin::<Bar<PhantomPinned>>(); //~ ERROR E0277
}
