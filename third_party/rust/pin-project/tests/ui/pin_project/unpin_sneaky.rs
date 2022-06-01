use pin_project::pin_project;

#[pin_project]
struct Foo {
    #[pin]
    inner: u8,
}

impl Unpin for __Foo {} //~ ERROR E0412,E0321

fn main() {}
