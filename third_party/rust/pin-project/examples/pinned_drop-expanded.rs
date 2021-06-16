// Original code (./pinned_drop.rs):
//
// ```rust
// #![allow(dead_code)]
//
// use pin_project::{pin_project, pinned_drop};
// use std::pin::Pin;
//
// #[pin_project(PinnedDrop)]
// pub struct Foo<'a, T> {
//     was_dropped: &'a mut bool,
//     #[pin]
//     field: T,
// }
//
// #[pinned_drop]
// fn drop_foo<T>(mut this: Pin<&mut Foo<'_, T>>) {
//     **this.project().was_dropped = true;
// }
//
// fn main() {}
// ```

#![allow(dead_code, unused_imports, unused_parens)]

use pin_project::{pin_project, pinned_drop};
use std::pin::Pin;

pub struct Foo<'a, T> {
    was_dropped: &'a mut bool,
    // #[pin]
    field: T,
}

#[allow(clippy::mut_mut)] // This lint warns `&mut &mut <ty>`.
#[allow(dead_code)] // This lint warns unused fields/variants.
pub(crate) struct __FooProjection<'pin, 'a, T> {
    was_dropped: &'pin mut (&'a mut bool),
    field: ::core::pin::Pin<&'pin mut (T)>,
}

#[allow(dead_code)] // This lint warns unused fields/variants.
pub(crate) struct __FooProjectionRef<'pin, 'a, T> {
    was_dropped: &'pin (&'a mut bool),
    field: ::core::pin::Pin<&'pin (T)>,
}

impl<'a, T> Foo<'a, T> {
    pub(crate) fn project<'pin>(
        self: ::core::pin::Pin<&'pin mut Self>,
    ) -> __FooProjection<'pin, 'a, T> {
        unsafe {
            let Foo { was_dropped, field } = self.get_unchecked_mut();
            __FooProjection { was_dropped, field: ::core::pin::Pin::new_unchecked(field) }
        }
    }
    pub(crate) fn project_ref<'pin>(
        self: ::core::pin::Pin<&'pin Self>,
    ) -> __FooProjectionRef<'pin, 'a, T> {
        unsafe {
            let Foo { was_dropped, field } = self.get_ref();
            __FooProjectionRef { was_dropped, field: ::core::pin::Pin::new_unchecked(field) }
        }
    }
}

#[allow(single_use_lifetimes)]
impl<'a, T> ::core::ops::Drop for Foo<'a, T> {
    fn drop(&mut self) {
        // Safety - we're in 'drop', so we know that 'self' will
        // never move again.
        let pinned_self = unsafe { ::core::pin::Pin::new_unchecked(self) };
        // We call `pinned_drop` only once. Since `PinnedDrop::drop`
        // is an unsafe function and a private API, it is never called again in safe
        // code *unless the user uses a maliciously crafted macro*.
        unsafe {
            ::pin_project::__private::PinnedDrop::drop(pinned_self);
        }
    }
}

// It is safe to implement PinnedDrop::drop, but it is not safe to call it.
// This is because destructors can be called multiple times (double dropping
// is unsound: rust-lang/rust#62360).
//
// Ideally, it would be desirable to be able to prohibit manual calls in the
// same way as Drop::drop, but the library cannot. So, by using macros and
// replacing them with private traits, we prevent users from calling
// PinnedDrop::drop.
//
// Users can implement `Drop` safely using `#[pinned_drop]`.
// **Do not call or implement this trait directly.**
impl<T> ::pin_project::__private::PinnedDrop for Foo<'_, T> {
    // Since calling it twice on the same object would be UB,
    // this method is unsafe.
    unsafe fn drop(self: Pin<&mut Self>) {
        fn __drop_inner<T>(__self: Pin<&mut Foo<'_, T>>) {
            **__self.project().was_dropped = true;
        }
        __drop_inner(self);
    }
}

// Automatically create the appropriate conditional `Unpin` implementation.
//
// See ./struct-default-expanded.rs and https://github.com/taiki-e/pin-project/pull/53.
// for details.
#[allow(non_snake_case)]
fn __unpin_scope_Foo() {
    pub struct __Foo<'pin, 'a, T> {
        __pin_project_use_generics: ::pin_project::__private::AlwaysUnpin<'pin, (T)>,
        __field0: T,
        __lifetime0: &'a (),
    }
    impl<'pin, 'a, T> ::core::marker::Unpin for Foo<'a, T> where
        __Foo<'pin, 'a, T>: ::core::marker::Unpin
    {
    }
}

// Ensure that it's impossible to use pin projections on a #[repr(packed)] struct.
//
// See ./struct-default-expanded.rs and https://github.com/taiki-e/pin-project/pull/34
// for details.
#[allow(single_use_lifetimes)]
#[allow(non_snake_case)]
#[deny(safe_packed_borrows)]
fn __pin_project_assert_not_repr_packed_Foo<'a, T>(val: &Foo<'a, T>) {
    &val.was_dropped;
    &val.field;
}

fn main() {}
