use auxiliary_macro::hidden_repr_cfg_not_any;
use pin_project::pin_project;

// `#[hidden_repr_cfg_not_any(packed)]` generates `#[cfg_attr(not(any()), repr(packed))]`.
#[pin_project]
#[hidden_repr_cfg_not_any(packed)] //~ ERROR may not be used on #[repr(packed)] types
struct Foo {
    #[pin]
    field: u32,
}

fn main() {}
