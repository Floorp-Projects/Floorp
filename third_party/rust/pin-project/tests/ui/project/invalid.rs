use pin_project::{pin_project, project};

#[pin_project]
struct A<T> {
    #[pin]
    future: T,
}

#[project]
fn foo() {
    let mut x = A { future: 0 };
    #[project(foo)] //~ ERROR unexpected token
    let A { future } = Pin::new(&mut x).project();
}

#[project]
fn bar() {
    let mut x = A { future: 0 };
    #[project]
    #[project] //~ ERROR duplicate #[project] attribute
    let A { future } = Pin::new(&mut x).project();
}

fn main() {}
