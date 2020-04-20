use pin_project::{pin_project, UnsafeUnpin};
use std::marker::PhantomPinned;

fn is_unpin<T: Unpin>() {}

#[pin_project(UnsafeUnpin)]
pub struct Blah<T, U> {
    field1: U,
    #[pin]
    field2: T,
}

#[allow(unsafe_code)]
unsafe impl<T: Unpin, U> UnsafeUnpin for Blah<T, U> {}

#[pin_project(UnsafeUnpin)]
pub struct TrivialBounds {
    #[pin]
    field1: PhantomPinned,
}

#[pin_project(UnsafeUnpin)]
pub struct OverlappingLifetimeNames<'pin, T, U> {
    #[pin]
    field1: U,
    #[pin]
    field2: Option<T>,
    field3: &'pin (),
}

#[allow(unsafe_code)]
unsafe impl<T: Unpin, U: Unpin> UnsafeUnpin for OverlappingLifetimeNames<'_, T, U> {}

fn main() {
    is_unpin::<Blah<PhantomPinned, ()>>(); //~ ERROR E0277
    is_unpin::<Blah<(), PhantomPinned>>(); // Ok
    is_unpin::<Blah<PhantomPinned, PhantomPinned>>(); //~ ERROR E0277

    is_unpin::<TrivialBounds>(); //~ ERROR E0277

    is_unpin::<OverlappingLifetimeNames<'_, PhantomPinned, ()>>(); //~ ERROR E0277
    is_unpin::<OverlappingLifetimeNames<'_, (), PhantomPinned>>(); //~ ERROR E0277
}
