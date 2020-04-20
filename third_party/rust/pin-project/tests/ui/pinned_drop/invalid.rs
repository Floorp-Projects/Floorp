use pin_project::{pin_project, pinned_drop};

#[pin_project(PinnedDrop)]
pub struct A {
    #[pin]
    field: u8,
}

#[pinned_drop(foo)] //~ ERROR unexpected token
impl PinnedDrop for A {
    fn drop(self: Pin<&mut Self>) {}
}

#[pin_project(PinnedDrop)]
pub struct B {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl Drop for B {
    //~^ ERROR #[pinned_drop] may only be used on implementation for the `PinnedDrop` trait
    fn drop(&mut self) {}
}

#[pin_project(PinnedDrop)]
pub struct C {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl C {
    //~^ ERROR #[pinned_drop] may only be used on implementation for the `PinnedDrop` trait
    fn drop(&mut self) {}
}

#[pin_project(PinnedDrop)]
pub struct D {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for D {
    fn drop(&mut self) {} //~ ERROR method `drop` must take an argument `self: Pin<&mut Self>`
}

#[pin_project(PinnedDrop)]
pub struct E {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for E {
    fn drop_baz(&mut self) {} //~ ERROR method `drop_baz` is not a member of trait `PinnedDrop
}

#[pin_project(PinnedDrop)]
pub struct F {
    #[pin]
    field: u8,
}

#[pinned_drop]
unsafe impl PinnedDrop for F {
    //~^ ERROR implementing the trait `PinnedDrop` is not unsafe
    fn drop(self: Pin<&mut Self>) {}
}

#[pin_project(PinnedDrop)]
pub struct G {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for G {
    unsafe fn drop(self: Pin<&mut Self>) {} //~ ERROR implementing the method `drop` is not unsafe
}

#[pin_project(PinnedDrop)]
pub struct H {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for H {
    const A: u8 = 0; //~ ERROR const `A` is not a member of trait `PinnedDrop`
    fn drop(self: Pin<&mut Self>) {}
}

#[pin_project(PinnedDrop)]
pub struct I {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for I {
    fn drop(self: Pin<&mut Self>) {}
    const A: u8 = 0; //~ ERROR const `A` is not a member of trait `PinnedDrop`
}

#[pin_project(PinnedDrop)]
pub struct J {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for J {
    type A = u8; //~ ERROR type `A` is not a member of trait `PinnedDrop`
    fn drop(self: Pin<&mut Self>) {}
}

#[pin_project(PinnedDrop)]
pub struct K {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for K {
    fn drop(self: Pin<&mut Self>) {}
    type A = u8; //~ ERROR type `A` is not a member of trait `PinnedDrop`
}

#[pin_project(PinnedDrop)]
pub struct L {
    #[pin]
    field: u8,
}

#[pinned_drop]
impl PinnedDrop for L {
    fn drop(self: Pin<&mut Self>) {}
    fn drop(self: Pin<&mut Self>) {} //~ ERROR duplicate definitions with name `drop`
}

#[pin_project(PinnedDrop)] //~ ERROR E0277
pub struct M {
    #[pin]
    field: u8,
}

#[pinned_drop]
fn drop(_this: Pin<&mut M>) {} //~ ERROR expected `impl`

fn main() {}
