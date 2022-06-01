use pin_project::{pin_project, pinned_drop};
use std::pin::Pin;

#[pin_project]
struct Foo {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for Foo { //~ ERROR E0119
    fn drop(self: Pin<&mut Self>) {}
}

fn main() {}
