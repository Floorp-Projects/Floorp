use pin_project::{pin_project, pinned_drop};
use std::pin::Pin;

// In `Drop` impl, the implementor must specify the same requirement as type definition.

struct DropImpl<T> {
    field: T,
}

impl<T: Unpin> Drop for DropImpl<T> {
    //~^ ERROR E0367
    fn drop(&mut self) {}
}

#[pin_project(PinnedDrop)] //~ ERROR E0277
struct PinnedDropImpl<T> {
    #[pin]
    field: T,
}

#[pinned_drop]
impl<T: Unpin> PinnedDrop for PinnedDropImpl<T> {
    fn drop(self: Pin<&mut Self>) {}
}

fn main() {}
