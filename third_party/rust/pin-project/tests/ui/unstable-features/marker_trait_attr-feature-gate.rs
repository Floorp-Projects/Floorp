// NB: If you change this test, change 'marker_trait_attr.rs' at the same time.

use pin_project::pin_project;
use std::marker::PhantomPinned;

#[pin_project] //~ ERROR E0119
struct Foo<T> {
    #[pin]
    x: T,
}

// unsound Unpin impl
impl<T> Unpin for Foo<T> {}

fn is_unpin<T: Unpin>() {}

fn main() {
    is_unpin::<Foo<PhantomPinned>>()
}
