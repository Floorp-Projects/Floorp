// See ./pinned_drop-expanded.rs for generated code.

#![allow(dead_code)]

use pin_project::{pin_project, UnsafeUnpin};

#[pin_project(UnsafeUnpin)]
pub struct Foo<T, U> {
    #[pin]
    pinned: T,
    unpinned: U,
}

unsafe impl<T: Unpin, U> UnsafeUnpin for Foo<T, U> {}

fn main() {}
