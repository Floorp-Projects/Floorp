use pin_project::pin_project;

#[pin_project]
struct A {
    field: u8,
}

mod b {
    use pin_project::project;

    #[project]
    use crate::A as B; //~ ERROR #[project] attribute may not be used on renamed imports
    #[project]
    use crate::*; //~ ERROR #[project] attribute may not be used on glob imports
}

fn main() {}
