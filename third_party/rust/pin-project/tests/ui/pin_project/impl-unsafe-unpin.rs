use pin_project::{pin_project, UnsafeUnpin};

#[pin_project] //~ ERROR E0119
struct Foo<T, U> {
    #[pin]
    future: T,
    field: U,
}

unsafe impl<T, U> UnsafeUnpin for Foo<T, U> where T: Unpin {}

#[pin_project] //~ ERROR E0119
struct Bar<T, U> {
    #[pin]
    future: T,
    field: U,
}

unsafe impl<T, U> UnsafeUnpin for Bar<T, U> {}

#[pin_project] //~ ERROR E0119
struct Baz<T, U> {
    #[pin]
    future: T,
    field: U,
}

unsafe impl<T: Unpin, U: Unpin> UnsafeUnpin for Baz<T, U> {}

fn main() {}
