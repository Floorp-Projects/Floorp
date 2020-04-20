// Original code (./unsafe_unpin.rs):
//
// ```rust
// #![allow(dead_code)]
//
// use pin_project::{pin_project, UnsafeUnpin};
//
// #[pin_project(UnsafeUnpin)]
// pub struct Foo<T, U> {
//     #[pin]
//     pinned: T,
//     unpinned: U,
// }
//
// unsafe impl<T: Unpin, U> UnsafeUnpin for Foo<T, U> {}
//
// fn main() {}
// ```

#![allow(dead_code, unused_imports, unused_parens)]

use pin_project::{pin_project, UnsafeUnpin};

pub struct Foo<T, U> {
    // #[pin]
    pinned: T,
    unpinned: U,
}

#[allow(clippy::mut_mut)] // This lint warns `&mut &mut <ty>`.
#[allow(dead_code)] // This lint warns unused fields/variants.
pub(crate) struct __FooProjection<'pin, T, U> {
    pinned: ::core::pin::Pin<&'pin mut (T)>,
    unpinned: &'pin mut (U),
}
#[allow(dead_code)] // This lint warns unused fields/variants.
pub(crate) struct __FooProjectionRef<'pin, T, U> {
    pinned: ::core::pin::Pin<&'pin (T)>,
    unpinned: &'pin (U),
}

impl<T, U> Foo<T, U> {
    pub(crate) fn project<'pin>(
        self: ::core::pin::Pin<&'pin mut Self>,
    ) -> __FooProjection<'pin, T, U> {
        unsafe {
            let Foo { pinned, unpinned } = self.get_unchecked_mut();
            __FooProjection { pinned: ::core::pin::Pin::new_unchecked(pinned), unpinned }
        }
    }
    pub(crate) fn project_ref<'pin>(
        self: ::core::pin::Pin<&'pin Self>,
    ) -> __FooProjectionRef<'pin, T, U> {
        unsafe {
            let Foo { pinned, unpinned } = self.get_ref();
            __FooProjectionRef { pinned: ::core::pin::Pin::new_unchecked(pinned), unpinned }
        }
    }
}

unsafe impl<T: Unpin, U> UnsafeUnpin for Foo<T, U> {}

#[allow(single_use_lifetimes)]
impl<'pin, T, U> ::core::marker::Unpin for Foo<T, U> where
    ::pin_project::__private::Wrapper<'pin, Self>: ::pin_project::UnsafeUnpin
{
}

// Ensure that struct does not implement `Drop`.
//
// See ./struct-default-expanded.rs for details.
trait FooMustNotImplDrop {}
#[allow(clippy::drop_bounds)]
impl<T: ::core::ops::Drop> FooMustNotImplDrop for T {}
#[allow(single_use_lifetimes)]
impl<T, U> FooMustNotImplDrop for Foo<T, U> {}

// Ensure that it's impossible to use pin projections on a #[repr(packed)] struct.
//
// See ./struct-default-expanded.rs and https://github.com/taiki-e/pin-project/pull/34
// for details.
#[allow(single_use_lifetimes)]
#[allow(non_snake_case)]
#[deny(safe_packed_borrows)]
fn __pin_project_assert_not_repr_packed_Foo<T, U>(val: &Foo<T, U>) {
    &val.pinned;
    &val.unpinned;
}

fn main() {}
