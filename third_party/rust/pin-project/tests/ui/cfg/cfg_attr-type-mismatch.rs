use pin_project::pin_project;
use std::pin::Pin;

#[cfg_attr(not(any()), pin_project)]
struct Foo<T> {
    #[cfg_attr(any(), pin)]
    inner: T,
}

#[cfg_attr(not(any()), pin_project)]
struct Bar<T> {
    #[cfg_attr(not(any()), pin)]
    inner: T,
}

fn main() {
    let mut x = Foo { inner: 0_u8 };
    let x = Pin::new(&mut x).project();
    let _: Pin<&mut u8> = x.inner; //~ ERROR E0308

    let mut x = Bar { inner: 0_u8 };
    let x = Pin::new(&mut x).project();
    let _: &mut u8 = x.inner; //~ ERROR E0308
}
