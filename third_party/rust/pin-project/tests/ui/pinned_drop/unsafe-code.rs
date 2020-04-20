use pin_project::{pin_project, pinned_drop};
use std::pin::Pin;

#[pin_project(PinnedDrop)]
pub struct Struct {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for Struct {
    fn drop(self: Pin<&mut Self>) {
        self.project().field.get_unchecked_mut(); //~ ERROR call to unsafe function is unsafe and requires unsafe function or block [E0133]
    }
}

fn main() {}
