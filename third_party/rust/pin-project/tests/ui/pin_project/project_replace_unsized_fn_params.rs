#![feature(unsized_fn_params)]

use pin_project::pin_project;

#[pin_project(project_replace)] //~ ERROR E0277
struct Struct<T: ?Sized> {
    x: T,
}

#[pin_project(project_replace)] //~ ERROR E0277
struct TupleStruct<T: ?Sized>(T);

fn main() {}
