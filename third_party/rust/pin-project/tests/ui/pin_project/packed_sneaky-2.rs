use auxiliary_macros::hidden_repr_macro;
use pin_project::pin_project;

hidden_repr_macro! { //~ ERROR may not be used on #[repr(packed)] types
    #[pin_project]
    struct B {
        #[pin]
        field: u32,
    }
}

fn main() {}
