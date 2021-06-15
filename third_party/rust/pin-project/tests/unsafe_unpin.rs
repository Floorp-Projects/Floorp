#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]

use std::{marker::PhantomPinned, pin::Pin};

use pin_project::{pin_project, UnsafeUnpin};

fn is_unpin<T: Unpin>() {}

#[pin_project(UnsafeUnpin)]
pub struct Blah<T, U> {
    field1: U,
    #[pin]
    field2: T,
}

unsafe impl<T: Unpin, U> UnsafeUnpin for Blah<T, U> {}

#[pin_project(UnsafeUnpin)]
pub struct OverlappingLifetimeNames<'pin, T, U> {
    #[pin]
    field1: T,
    field2: U,
    field3: &'pin (),
}

unsafe impl<T: Unpin, U> UnsafeUnpin for OverlappingLifetimeNames<'_, T, U> {}

#[test]
fn unsafe_unpin() {
    is_unpin::<Blah<(), PhantomPinned>>();
    is_unpin::<OverlappingLifetimeNames<'_, (), ()>>();
}

#[test]
fn trivial_bounds() {
    #[pin_project(UnsafeUnpin)]
    pub struct NotImplementUnsafUnpin {
        #[pin]
        field: PhantomPinned,
    }
}

#[test]
fn test() {
    let mut x = OverlappingLifetimeNames { field1: 0, field2: 1, field3: &() };
    let x = Pin::new(&mut x);
    let y = x.as_ref().project_ref();
    let _: Pin<&u8> = y.field1;
    let _: &u8 = y.field2;
    let y = x.project();
    let _: Pin<&mut u8> = y.field1;
    let _: &mut u8 = y.field2;
}
