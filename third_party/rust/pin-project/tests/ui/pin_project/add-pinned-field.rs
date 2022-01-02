use auxiliary_macro::add_pinned_field;
use pin_project::pin_project;

fn is_unpin<T: Unpin>() {}

#[pin_project]
#[add_pinned_field]
struct Foo {
    #[pin]
    field: u32,
}

#[add_pinned_field]
#[pin_project]
struct Bar {
    #[pin]
    field: u32,
}

fn main() {
    is_unpin::<Foo>(); //~ ERROR E0277
    is_unpin::<Bar>(); //~ ERROR E0277
}
