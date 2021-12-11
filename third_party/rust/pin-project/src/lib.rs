//! A crate for safe and ergonomic [pin-projection].
//!
//! # Examples
//!
//! [`#[pin_project]`][`pin_project`] attribute creates projection types
//! covering all the fields of struct or enum.
//!
//! ```rust
//! use std::pin::Pin;
//!
//! use pin_project::pin_project;
//!
//! #[pin_project]
//! struct Struct<T, U> {
//!     #[pin]
//!     pinned: T,
//!     unpinned: U,
//! }
//!
//! impl<T, U> Struct<T, U> {
//!     fn method(self: Pin<&mut Self>) {
//!         let this = self.project();
//!         let _: Pin<&mut T> = this.pinned; // Pinned reference to the field
//!         let _: &mut U = this.unpinned; // Normal reference to the field
//!     }
//! }
//! ```
//!
//! [*code like this will be generated*][struct-default-expanded]
//!
//! See [`#[pin_project]`][`pin_project`] attribute for more details, and
//! see [examples] directory for more examples and generated code.
//!
//! [`pin_project`]: attr.pin_project.html
//! [examples]: https://github.com/taiki-e/pin-project/blob/master/examples/README.md
//! [pin-projection]: https://doc.rust-lang.org/nightly/std/pin/index.html#projections-and-structural-pinning
//! [struct-default-expanded]: https://github.com/taiki-e/pin-project/blob/master/examples/struct-default-expanded.rs

#![no_std]
#![doc(test(
    no_crate_inject,
    attr(deny(warnings, rust_2018_idioms, single_use_lifetimes), allow(dead_code))
))]
#![warn(missing_docs, rust_2018_idioms, single_use_lifetimes, unreachable_pub)]
#![warn(clippy::all, clippy::default_trait_access)]
// mem::take and #[non_exhaustive] requires Rust 1.40
#![allow(clippy::mem_replace_with_default, clippy::manual_non_exhaustive)]
#![allow(clippy::needless_doctest_main)]

#[doc(inline)]
pub use pin_project_internal::pin_project;
#[doc(inline)]
pub use pin_project_internal::pinned_drop;
#[allow(deprecated)]
#[doc(inline)]
pub use pin_project_internal::project;
#[allow(deprecated)]
#[doc(inline)]
pub use pin_project_internal::project_ref;
#[allow(deprecated)]
#[doc(inline)]
pub use pin_project_internal::project_replace;

/// A trait used for custom implementations of [`Unpin`].
/// This trait is used in conjunction with the `UnsafeUnpin`
/// argument to [`#[pin_project]`][`pin_project`]
///
/// The Rust [`Unpin`] trait is safe to implement - by itself,
/// implementing it cannot lead to undefined behavior. Undefined
/// behavior can only occur when other unsafe code is used.
///
/// It turns out that using pin projections, which requires unsafe code,
/// imposes additional requirements on an [`Unpin`] impl. Normally, all of this
/// unsafety is contained within this crate, ensuring that it's impossible for
/// you to violate any of the guarantees required by pin projection.
///
/// However, things change if you want to provide a custom [`Unpin`] impl
/// for your `#[pin_project]` type. As stated in [the Rust
/// documentation][pin-projection], you must be sure to only implement [`Unpin`]
/// when all of your `#[pin]` fields (i.e. structurally pinned fields) are also
/// [`Unpin`].
///
/// To help highlight this unsafety, the `UnsafeUnpin` trait is provided.
/// Implementing this trait is logically equivalent to implementing [`Unpin`] -
/// this crate will generate an [`Unpin`] impl for your type that 'forwards' to
/// your `UnsafeUnpin` impl. However, this trait is `unsafe` - since your type
/// uses structural pinning (otherwise, you wouldn't be using this crate!),
/// you must be sure that your `UnsafeUnpin` impls follows all of
/// the requirements for an [`Unpin`] impl of a structurally-pinned type.
///
/// Note that if you specify `#[pin_project(UnsafeUnpin)]`, but do *not*
/// provide an impl of `UnsafeUnpin`, your type will never implement [`Unpin`].
/// This is effectively the same thing as adding a [`PhantomPinned`] to your
/// type.
///
/// Since this trait is `unsafe`, impls of it will be detected by the
/// `unsafe_code` lint, and by tools like [`cargo geiger`][cargo-geiger].
///
/// # Examples
///
/// An `UnsafeUnpin` impl which, in addition to requiring that structurally
/// pinned fields be [`Unpin`], imposes an additional requirement:
///
/// ```rust
/// use pin_project::{pin_project, UnsafeUnpin};
///
/// #[pin_project(UnsafeUnpin)]
/// struct Foo<K, V> {
///     #[pin]
///     field_1: K,
///     field_2: V,
/// }
///
/// unsafe impl<K, V> UnsafeUnpin for Foo<K, V> where K: Unpin + Clone {}
/// ```
///
/// [`PhantomPinned`]: core::marker::PhantomPinned
/// [`pin_project`]: attr.pin_project.html
/// [pin-projection]: https://doc.rust-lang.org/nightly/std/pin/index.html#projections-and-structural-pinning
/// [cargo-geiger]: https://github.com/rust-secure-code/cargo-geiger
pub unsafe trait UnsafeUnpin {}

// Not public API.
#[doc(hidden)]
pub mod __private {
    #[doc(hidden)]
    pub use core::{
        marker::{PhantomData, PhantomPinned, Unpin},
        mem::ManuallyDrop,
        ops::Drop,
        pin::Pin,
        ptr,
    };

    #[doc(hidden)]
    pub use pin_project_internal::__PinProjectInternalDerive;

    use super::UnsafeUnpin;

    // Implementing `PinnedDrop::drop` is safe, but calling it is not safe.
    // This is because destructors can be called multiple times in safe code and
    // [double dropping is unsound](https://github.com/rust-lang/rust/pull/62360).
    //
    // Ideally, it would be desirable to be able to forbid manual calls in
    // the same way as [`Drop::drop`], but the library cannot do it. So, by using
    // macros and replacing them with private traits, we prevent users from
    // calling `PinnedDrop::drop`.
    //
    // Users can implement [`Drop`] safely using `#[pinned_drop]` and can drop a
    // type that implements `PinnedDrop` using the [`drop`] function safely.
    // **Do not call or implement this trait directly.**
    #[doc(hidden)]
    pub trait PinnedDrop {
        #[doc(hidden)]
        unsafe fn drop(self: Pin<&mut Self>);
    }

    // This is an internal helper struct used by `pin-project-internal`.
    // This allows us to force an error if the user tries to provide
    // a regular `Unpin` impl when they specify the `UnsafeUnpin` argument.
    // This is why we need Wrapper:
    //
    // Supposed we have the following code:
    //
    // ```rust
    // #[pin_project(UnsafeUnpin)]
    // struct MyStruct<T> {
    //     #[pin] field: T
    // }
    //
    // impl<T> Unpin for MyStruct<T> where MyStruct<T>: UnsafeUnpin {} // generated by pin-project-internal
    // impl<T> Unpin for MyStruct<T> where T: Copy // written by the user
    // ```
    //
    // We want this code to be rejected - the user is completely bypassing
    // `UnsafeUnpin`, and providing an unsound Unpin impl in safe code!
    //
    // Unfortunately, the Rust compiler will accept the above code.
    // Because MyStruct is declared in the same crate as the user-provided impl,
    // the compiler will notice that `MyStruct<T>: UnsafeUnpin` never holds.
    //
    // The solution is to introduce the `Wrapper` struct, which is defined
    // in the `pin-project` crate.
    //
    // We now have code that looks like this:
    //
    // ```rust
    // impl<T> Unpin for MyStruct<T> where Wrapper<MyStruct<T>>: UnsafeUnpin {} // generated by pin-project-internal
    // impl<T> Unpin for MyStruct<T> where T: Copy // written by the user
    // ```
    //
    // We also have `unsafe impl<T> UnsafeUnpin for Wrapper<T> where T: UnsafeUnpin {}`
    // in the `pin-project` crate.
    //
    // Now, our generated impl has a bound involving a type defined in another
    // crate - Wrapper. This will cause rust to conservatively assume that
    // `Wrapper<MyStruct<T>>: UnsafeUnpin` holds, in the interest of preserving
    // forwards compatibility (in case such an impl is added for Wrapper<T> in
    // a new version of the crate).
    //
    // This will cause rust to reject any other `Unpin` impls for MyStruct<T>,
    // since it will assume that our generated impl could potentially apply in
    // any situation.
    //
    // This achieves the desired effect - when the user writes
    // `#[pin_project(UnsafeUnpin)]`, the user must either provide no impl of
    // `UnsafeUnpin` (which is equivalent to making the type never implement
    // Unpin), or provide an impl of `UnsafeUnpin`. It is impossible for them to
    // provide an impl of `Unpin`
    #[doc(hidden)]
    pub struct Wrapper<'a, T: ?Sized>(PhantomData<&'a ()>, T);

    unsafe impl<T: ?Sized> UnsafeUnpin for Wrapper<'_, T> where T: UnsafeUnpin {}

    // This is an internal helper struct used by `pin-project-internal`.
    //
    // See https://github.com/taiki-e/pin-project/pull/53 for more details.
    #[doc(hidden)]
    pub struct AlwaysUnpin<'a, T>(PhantomData<&'a ()>, PhantomData<T>);

    impl<T> Unpin for AlwaysUnpin<'_, T> {}

    // This is an internal helper used to ensure a value is dropped.
    #[doc(hidden)]
    pub struct UnsafeDropInPlaceGuard<T: ?Sized>(pub *mut T);

    impl<T: ?Sized> Drop for UnsafeDropInPlaceGuard<T> {
        fn drop(&mut self) {
            unsafe {
                ptr::drop_in_place(self.0);
            }
        }
    }

    // This is an internal helper used to ensure a value is overwritten without
    // its destructor being called.
    #[doc(hidden)]
    pub struct UnsafeOverwriteGuard<T> {
        pub value: ManuallyDrop<T>,
        pub target: *mut T,
    }

    impl<T> Drop for UnsafeOverwriteGuard<T> {
        fn drop(&mut self) {
            unsafe {
                ptr::write(self.target, ptr::read(&*self.value));
            }
        }
    }
}
