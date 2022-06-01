#![allow(deprecated)]

use pin_project::pin_project;

#[pin_project]
struct A {
    field: u8,
}

pub mod b {
    use pin_project::project;

    #[project]
    pub use crate::A; //~ ERROR E0365
}

fn main() {}
