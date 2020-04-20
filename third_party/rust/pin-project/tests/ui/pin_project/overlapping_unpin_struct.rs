use pin_project::pin_project;
use std::marker::PhantomPinned;

#[pin_project]
struct Foo<T> {
    #[pin]
    inner: T,
}

struct __Foo {}

impl Unpin for __Foo {}

fn is_unpin<T: Unpin>() {}

fn main() {
    is_unpin::<Foo<PhantomPinned>>(); //~ ERROR E0277
}
