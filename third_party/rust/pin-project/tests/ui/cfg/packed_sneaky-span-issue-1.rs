use auxiliary_macro::hidden_repr;
use pin_project::pin_project;

//~ ERROR may not be used on #[repr(packed)] types
// span is lost.
// Refs: https://github.com/rust-lang/rust/issues/43081
#[pin_project]
#[hidden_repr(packed)]
struct Foo {
    #[cfg(not(any()))]
    #[pin]
    field: u32,
    #[cfg(any())]
    #[pin]
    field: u8,
}

fn main() {}
