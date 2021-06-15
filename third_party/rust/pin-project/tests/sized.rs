#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]

use pin_project::pin_project;

#[pin_project]
struct Foo<'a, I: ?Sized, Item>
where
    I: Iterator,
{
    iter: &'a mut I,
    item: Option<Item>,
}
