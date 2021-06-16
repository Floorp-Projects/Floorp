use auxiliary_macros::hidden_repr;
use pin_project::{pin_project, pinned_drop, UnsafeUnpin};
use std::pin::Pin;

#[pin_project] //~ ERROR may not be used on #[repr(packed)] types
#[hidden_repr(packed)]
struct A {
    #[pin]
    field: u32,
}

#[pin_project(UnsafeUnpin)] //~ ERROR may not be used on #[repr(packed)] types
#[hidden_repr(packed)]
struct C {
    #[pin]
    field: u32,
}

unsafe impl UnsafeUnpin for C {}

#[pin_project(PinnedDrop)] //~ ERROR may not be used on #[repr(packed)] types
#[hidden_repr(packed)]
struct D {
    #[pin]
    field: u32,
}

#[pinned_drop]
impl PinnedDrop for D {
    fn drop(self: Pin<&mut Self>) {}
}

fn main() {}
