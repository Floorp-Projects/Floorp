use std::pin::Pin;

#[cfg_attr(any(), pin_project::pin_project)]
struct Foo<T> {
    inner: T,
}

fn main() {
    let mut x = Foo { inner: 0_u8 };
    let _x = Pin::new(&mut x).project(); //~ ERROR E0599
}
