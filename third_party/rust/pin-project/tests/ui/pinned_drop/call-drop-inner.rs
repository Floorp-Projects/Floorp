use pin_project::{pin_project, pinned_drop};
use std::pin::Pin;

#[pin_project(PinnedDrop)]
struct Struct {
    dropped: bool,
}

#[pinned_drop]
impl PinnedDrop for Struct {
    fn drop(mut self: Pin<&mut Self>) {
        __drop_inner(__self);
    }
}

fn main() {}
