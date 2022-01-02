// Original code (./struct-default.rs):
//
// ```rust
// #![allow(dead_code)]
//
// use pin_project::pin_project;
//
// #[pin_project]
// struct Struct<T, U> {
//     #[pin]
//     pinned: T,
//     unpinned: U,
// }
//
// fn main() {}
// ```

#![allow(dead_code, unused_imports, unused_parens, unknown_lints, renamed_and_removed_lints)]
#![allow(clippy::no_effect, clippy::needless_lifetimes)]

use pin_project::pin_project;

struct Struct<T, U> {
    // #[pin]
    pinned: T,
    unpinned: U,
}

#[doc(hidden)]
#[allow(dead_code)]
#[allow(single_use_lifetimes)]
#[allow(clippy::mut_mut)]
#[allow(clippy::type_repetition_in_bounds)]
struct __StructProjection<'pin, T, U>
where
    Struct<T, U>: 'pin,
{
    pinned: ::pin_project::__private::Pin<&'pin mut (T)>,
    unpinned: &'pin mut (U),
}
#[doc(hidden)]
#[allow(dead_code)]
#[allow(single_use_lifetimes)]
#[allow(clippy::type_repetition_in_bounds)]
struct __StructProjectionRef<'pin, T, U>
where
    Struct<T, U>: 'pin,
{
    pinned: ::pin_project::__private::Pin<&'pin (T)>,
    unpinned: &'pin (U),
}

#[doc(hidden)]
#[allow(non_upper_case_globals)]
#[allow(single_use_lifetimes)]
#[allow(clippy::used_underscore_binding)]
const _: () = {
    impl<T, U> Struct<T, U> {
        fn project<'pin>(
            self: ::pin_project::__private::Pin<&'pin mut Self>,
        ) -> __StructProjection<'pin, T, U> {
            unsafe {
                let Self { pinned, unpinned } = self.get_unchecked_mut();
                __StructProjection {
                    pinned: ::pin_project::__private::Pin::new_unchecked(pinned),
                    unpinned,
                }
            }
        }
        fn project_ref<'pin>(
            self: ::pin_project::__private::Pin<&'pin Self>,
        ) -> __StructProjectionRef<'pin, T, U> {
            unsafe {
                let Self { pinned, unpinned } = self.get_ref();
                __StructProjectionRef {
                    pinned: ::pin_project::__private::Pin::new_unchecked(pinned),
                    unpinned,
                }
            }
        }
    }

    // Automatically create the appropriate conditional `Unpin` implementation.
    //
    // Basically this is equivalent to the following code:
    //
    // ```rust
    // impl<T, U> Unpin for Struct<T, U> where T: Unpin {}
    // ```
    //
    // However, if struct is public and there is a private type field,
    // this would cause an E0446 (private type in public interface).
    //
    // When RFC 2145 is implemented (rust-lang/rust#48054),
    // this will become a lint, rather then a hard error.
    //
    // As a workaround for this, we generate a new struct, containing all of
    // the pinned fields from our #[pin_project] type. This struct is declared
    // within a function, which makes it impossible to be named by user code.
    // This guarantees that it will use the default auto-trait impl for Unpin -
    // that is, it will implement Unpin iff all of its fields implement Unpin.
    // This type can be safely declared as 'public', satisfying the privacy
    // checker without actually allowing user code to access it.
    //
    // This allows users to apply the #[pin_project] attribute to types
    // regardless of the privacy of the types of their fields.
    //
    // See also https://github.com/taiki-e/pin-project/pull/53.
    struct __Struct<'pin, T, U> {
        __pin_project_use_generics: ::pin_project::__private::AlwaysUnpin<
            'pin,
            (::pin_project::__private::PhantomData<T>, ::pin_project::__private::PhantomData<U>),
        >,
        __field0: T,
    }
    impl<'pin, T, U> ::pin_project::__private::Unpin for Struct<T, U> where
        __Struct<'pin, T, U>: ::pin_project::__private::Unpin
    {
    }
    // A dummy impl of `UnsafeUnpin`, to ensure that the user cannot implement it.
    //
    // To ensure that users don't accidentally write a non-functional `UnsafeUnpin`
    // impls, we emit one ourselves. If the user ends up writing an `UnsafeUnpin`
    // impl, they'll get a "conflicting implementations of trait" error when
    // coherence checks are run.
    unsafe impl<T, U> ::pin_project::UnsafeUnpin for Struct<T, U> {}

    // Ensure that struct does not implement `Drop`.
    //
    // If you attempt to provide an Drop impl, the blanket impl will
    // then apply to your type, causing a compile-time error due to
    // the conflict with the second impl.
    trait StructMustNotImplDrop {}
    #[allow(clippy::drop_bounds, drop_bounds)]
    impl<T: ::pin_project::__private::Drop> StructMustNotImplDrop for T {}
    impl<T, U> StructMustNotImplDrop for Struct<T, U> {}
    // A dummy impl of `PinnedDrop`, to ensure that users don't accidentally
    // write a non-functional `PinnedDrop` impls.
    impl<T, U> ::pin_project::__private::PinnedDrop for Struct<T, U> {
        unsafe fn drop(self: ::pin_project::__private::Pin<&mut Self>) {}
    }

    // Ensure that it's impossible to use pin projections on a #[repr(packed)]
    // struct.
    //
    // Taking a reference to a packed field is UB, and applying
    // `#[forbid(unaligned_references)]` makes sure that doing this is a hard error.
    //
    // If the struct ends up having #[repr(packed)] applied somehow,
    // this will generate an (unfriendly) error message. Under all reasonable
    // circumstances, we'll detect the #[repr(packed)] attribute, and generate
    // a much nicer error above.
    //
    // See https://github.com/taiki-e/pin-project/pull/34 for more details.
    #[forbid(unaligned_references, safe_packed_borrows)]
    fn __assert_not_repr_packed<T, U>(this: &Struct<T, U>) {
        let _ = &this.pinned;
        let _ = &this.unpinned;
    }
};

fn main() {}
