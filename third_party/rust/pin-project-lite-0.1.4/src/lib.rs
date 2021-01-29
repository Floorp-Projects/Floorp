//! A lightweight version of [pin-project] written with declarative macros.
//!
//! ## Examples
//!
//! ```rust
//! use pin_project_lite::pin_project;
//! use std::pin::Pin;
//!
//! pin_project! {
//!     struct Struct<T, U> {
//!         #[pin]
//!         pinned: T,
//!         unpinned: U,
//!     }
//! }
//!
//! impl<T, U> Struct<T, U> {
//!     fn foo(self: Pin<&mut Self>) {
//!         let this = self.project();
//!         let _: Pin<&mut T> = this.pinned; // Pinned reference to the field
//!         let _: &mut U = this.unpinned; // Normal reference to the field
//!     }
//! }
//! ```
//!
//! ## [pin-project] vs pin-project-lite
//!
//! Here are some similarities and differences compared to [pin-project].
//!
//! ### Similar: Safety
//!
//! pin-project-lite guarantees safety in much the same way as [pin-project]. Both are completely safe unless you write other unsafe code.
//!
//! ### Different: Minimal design
//!
//! This library does not tackle as expansive of a range of use cases as [pin-project] does. If your use case is not already covered, please use [pin-project].
//!
//! ### Different: No proc-macro related dependencies
//!
//! This is the **only** reason to use this crate. However, **if you already have proc-macro related dependencies in your crate's dependency graph, there is no benefit from using this crate.** (Note: There is almost no difference in the amount of code generated between [pin-project] and pin-project-lite.)
//!
//! ### Different: No useful error messages
//!
//! This macro does not handle any invalid input. So error messages are not to be useful in most cases. If you do need useful error messages, then upon error you can pass the same input to [pin-project] to receive a helpful description of the compile error.
//!
//! ### Different: Structs only
//!
//! pin-project-lite will refuse anything other than a braced struct with named fields. Enums and tuple structs are not supported.
//!
//! ### Different: No support for custom Drop implementation
//!
//! [pin-project supports this.][pinned-drop]
//!
//! ### Different: No support for custom Unpin implementation
//!
//! [pin-project supports this.][unsafe-unpin]
//!
//! ### Different: No support for pattern matching and destructing
//!
//! [pin-project supports this.][projection-helper]
//!
//! [pin-project]: https://github.com/taiki-e/pin-project
//! [pinned-drop]: https://docs.rs/pin-project/0.4/pin_project/attr.pin_project.html#pinned_drop
//! [unsafe-unpin]: https://docs.rs/pin-project/0.4/pin_project/trait.UnsafeUnpin.html
//! [projection-helper]: https://docs.rs/pin-project/0.4/pin_project/attr.project.html#let-bindings

#![no_std]
#![recursion_limit = "256"]
#![doc(html_root_url = "https://docs.rs/pin-project-lite/0.1.4")]
#![doc(test(
    no_crate_inject,
    attr(deny(warnings, rust_2018_idioms, single_use_lifetimes), allow(dead_code))
))]
#![warn(unsafe_code)]
#![warn(rust_2018_idioms, single_use_lifetimes, unreachable_pub)]
#![warn(clippy::all)]
// mem::take requires Rust 1.40
#![allow(clippy::mem_replace_with_default)]

/// A macro that creates a projection struct covering all the fields.
///
/// This macro creates a projection struct according to the following rules:
///
/// - For the field that uses `#[pin]` attribute, makes the pinned reference to
/// the field.
/// - For the other fields, makes the unpinned reference to the field.
///
/// The following methods are implemented on the original type:
///
/// ```
/// # use std::pin::Pin;
/// # type Projection<'a> = &'a ();
/// # type ProjectionRef<'a> = &'a ();
/// # trait Dox {
/// fn project(self: Pin<&mut Self>) -> Projection<'_>;
/// fn project_ref(self: Pin<&Self>) -> ProjectionRef<'_>;
/// # }
/// ```
///
/// The visibility of the projected type and projection method is based on the
/// original type. However, if the visibility of the original type is `pub`,
/// the visibility of the projected type and the projection method is `pub(crate)`.
///
/// If you want to call the `project` method multiple times or later use the
/// original Pin type, it needs to use [`.as_mut()`][`Pin::as_mut`] to avoid
/// consuming the `Pin`.
///
/// ## Safety
///
/// `pin_project!` macro guarantees safety in much the same way as [pin-project] crate.
/// Both are completely safe unless you write other unsafe code.
///
/// See [pin-project] crate for more details.
///
/// ## Examples
///
/// ```rust
/// use pin_project_lite::pin_project;
/// use std::pin::Pin;
///
/// pin_project! {
///     struct Struct<T, U> {
///         #[pin]
///         pinned: T,
///         unpinned: U,
///     }
/// }
///
/// impl<T, U> Struct<T, U> {
///     fn foo(self: Pin<&mut Self>) {
///         let this = self.project();
///         let _: Pin<&mut T> = this.pinned; // Pinned reference to the field
///         let _: &mut U = this.unpinned; // Normal reference to the field
///     }
/// }
/// ```
///
/// Note that borrowing the field where `#[pin]` attribute is used multiple
/// times requires using [`.as_mut()`][`Pin::as_mut`] to avoid
/// consuming the `Pin`.
///
/// [pin-project]: https://github.com/taiki-e/pin-project
/// [`Pin::as_mut`]: core::pin::Pin::as_mut
#[macro_export]
macro_rules! pin_project {
    // determine_visibility
    (
        $(#[$attrs:meta])*
        pub struct $ident:ident
            $(<
                $( $lifetime:lifetime $(: $lifetime_bound:lifetime)? ),* $(,)?
                $( $generics:ident $(: $generics_bound:path)? $(: ?$generics_unsized_bound:path)? $(: $generics_lifetime_bound:lifetime)? $(= $generics_default:ty)? ),* $(,)?
            >)?
            $(where
                $($where_clause_ty:ty : $where_clause_bound:path),* $(,)?
            )?
        {
            $(
                $(#[$pin:ident])?
                $field_vis:vis $field:ident: $field_ty:ty
            ),+ $(,)?
        }
    ) => {
        $crate::pin_project! {@internal (pub(crate))
            $(#[$attrs])*
            pub struct $ident
                $(<
                    $( $lifetime $(: $lifetime_bound)? ),*
                    $( $generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? $(= $generics_default)? ),*
                >)?
                $(where
                    $($where_clause_ty : $where_clause_bound),*
                )?
            {
                $(
                    $(#[$pin])?
                    $field_vis $field: $field_ty
                ),+
            }
        }
    };
    (
        $(#[$attrs:meta])*
        $vis:vis struct $ident:ident
            $(<
                $( $lifetime:lifetime $(: $lifetime_bound:lifetime)? ),* $(,)?
                $( $generics:ident $(: $generics_bound:path)? $(: ?$generics_unsized_bound:path)? $(: $generics_lifetime_bound:lifetime)? $(= $generics_default:ty)? ),* $(,)?
            >)?
            $(where
                $($where_clause_ty:ty : $where_clause_bound:path),* $(,)?
            )?
        {
            $(
                $(#[$pin:ident])?
                $field_vis:vis $field:ident: $field_ty:ty
            ),+ $(,)?
        }
    ) => {
        $crate::pin_project! {@internal ($vis)
            $(#[$attrs])*
            $vis struct $ident
                $(<
                    $( $lifetime $(: $lifetime_bound)? ),*
                    $( $generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? $(= $generics_default)? ),*
                >)?
                $(where
                    $($where_clause_ty : $where_clause_bound),*
                )?
            {
                $(
                    $(#[$pin])?
                    $field_vis $field: $field_ty
                ),+
            }
        }
    };

    (@internal ($proj_vis:vis)
        // limitation: does not support tuple structs and enums (wontfix)
        // limitation: no projection helper (wontfix)
        $(#[$attrs:meta])*
        $vis:vis struct $ident:ident
            $(<
                $( $lifetime:lifetime $(: $lifetime_bound:lifetime)? ),*
                // limitation: does not support multiple trait/lifetime bounds and ? trait bounds.
                $( $generics:ident $(: $generics_bound:path)? $(: ?$generics_unsized_bound:path)? $(: $generics_lifetime_bound:lifetime)? $(= $generics_default:ty)? ),*
            >)?
            $(where
                // limitation: does not support multiple trait/lifetime bounds and ? trait bounds.
                $($where_clause_ty:ty : $where_clause_bound:path),*
            )?
        {
            $(
                // limitation: cannot interoperate with other attributes.
                $(#[$pin:ident])?
                $field_vis:vis $field:ident: $field_ty:ty
            ),+
        }
    ) => {
        $(#[$attrs])*
        $vis struct $ident
            $(< $( $lifetime $(: $lifetime_bound)? ,)* $( $generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? $(= $generics_default)? ,)* >)?
            $(where
                $($where_clause_ty: $where_clause_bound),*
            )*
        {
            $(
                $field_vis $field: $field_ty
            ),+
        }

        // limitation: underscore_const_names requires rust 1.37+ (wontfix)
        const _: () = {
            #[allow(clippy::mut_mut)] // This lint warns `&mut &mut <ty>`.
            #[allow(dead_code)] // This lint warns unused fields/variants.
            $proj_vis struct Projection
                <'__pin $(, $( $lifetime $(: $lifetime_bound)? ,)* $( $generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? ),* )?>
                $(where
                    $($where_clause_ty: $where_clause_bound),*
                )*
            {
                $(
                    $field_vis $field: $crate::pin_project!(@make_proj_field $(#[$pin])? $field_ty; mut)
                ),+
            }
            #[allow(dead_code)] // This lint warns unused fields/variants.
            $proj_vis struct ProjectionRef
                <'__pin $(, $( $lifetime $(: $lifetime_bound)? ,)* $( $generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? ),* )?>
                $(where
                    $($where_clause_ty: $where_clause_bound),*
                )*
            {
                $(
                    $field_vis $field: $crate::pin_project!(@make_proj_field $(#[$pin])? $field_ty;)
                ),+
            }

            impl $(< $( $lifetime $(: $lifetime_bound)? ,)* $( $generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? ),* >)?
                $ident $(< $($lifetime,)* $($generics),* >)?
                $(where
                    $($where_clause_ty: $where_clause_bound),*
                )*
            {
                $proj_vis fn project<'__pin>(
                    self: ::core::pin::Pin<&'__pin mut Self>,
                ) -> Projection<'__pin $(, $($lifetime,)* $($generics),* )?> {
                    unsafe {
                        let this = self.get_unchecked_mut();
                        Projection {
                            $(
                                $field: $crate::pin_project!(@make_unsafe_field_proj this; $(#[$pin])? $field; mut)
                            ),+
                        }
                    }
                }
                $proj_vis fn project_ref<'__pin>(
                    self: ::core::pin::Pin<&'__pin Self>,
                ) -> ProjectionRef<'__pin $(, $($lifetime,)* $($generics),* )?> {
                    unsafe {
                        let this = self.get_ref();
                        ProjectionRef {
                            $(
                                $field: $crate::pin_project!(@make_unsafe_field_proj this; $(#[$pin])? $field;)
                            ),+
                        }
                    }
                }
            }

            // Automatically create the appropriate conditional `Unpin` implementation.
            //
            // Basically this is equivalent to the following code:
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
            // As a workaround for this, we generate a new struct, containing all of the pinned
            // fields from our #[pin_project] type. This struct is delcared within
            // a function, which makes it impossible to be named by user code.
            // This guarnatees that it will use the default auto-trait impl for Unpin -
            // that is, it will implement Unpin iff all of its fields implement Unpin.
            // This type can be safely declared as 'public', satisfiying the privacy
            // checker without actually allowing user code to access it.
            //
            // This allows users to apply the #[pin_project] attribute to types
            // regardless of the privacy of the types of their fields.
            //
            // See also https://github.com/taiki-e/pin-project/pull/53.
            $vis struct __Origin
                <'__pin $(, $( $lifetime $(: $lifetime_bound)? ,)* $( $generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? ),* )?>
                $(where
                    $($where_clause_ty: $where_clause_bound),*
                )*
            {
                __dummy_lifetime: ::core::marker::PhantomData<&'__pin ()>,
                $(
                    $field: $crate::pin_project!(@make_unpin_bound $(#[$pin])? $field_ty)
                ),+
            }
            impl <'__pin $(, $( $lifetime $(: $lifetime_bound)? ,)* $($generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? ),* )?> ::core::marker::Unpin
                for $ident $(< $($lifetime,)* $($generics),* >)?
            where
                __Origin <'__pin $(, $($lifetime,)* $($generics),* )?>: ::core::marker::Unpin
                $(,
                    $($where_clause_ty: $where_clause_bound),*
                )*
            {
            }

            // Ensure that struct does not implement `Drop`.
            //
            // There are two possible cases:
            // 1. The user type does not implement Drop. In this case,
            // the first blanked impl will not apply to it. This code
            // will compile, as there is only one impl of MustNotImplDrop for the user type
            // 2. The user type does impl Drop. This will make the blanket impl applicable,
            // which will then comflict with the explicit MustNotImplDrop impl below.
            // This will result in a compilation error, which is exactly what we want.
            trait MustNotImplDrop {}
            #[allow(clippy::drop_bounds)]
            impl<T: ::core::ops::Drop> MustNotImplDrop for T {}
            #[allow(single_use_lifetimes)]
            impl $(< $( $lifetime $(: $lifetime_bound)? ,)* $($generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? ),*>)? MustNotImplDrop
                for $ident $(< $($lifetime,)* $($generics),* >)?
                $(where
                    $($where_clause_ty: $where_clause_bound),*
                )*
            {}

            // Ensure that it's impossible to use pin projections on a #[repr(packed)] struct.
            //
            // Taking a reference to a packed field is unsafe, amd appplying
            // #[deny(safe_packed_borrows)] makes sure that doing this without
            // an 'unsafe' block (which we deliberately do not generate)
            // is a hard error.
            //
            // If the struct ends up having #[repr(packed)] applied somehow,
            // this will generate an (unfriendly) error message. Under all reasonable
            // circumstances, we'll detect the #[repr(packed)] attribute, and generate
            // a much nicer error above.
            //
            // See https://github.com/taiki-e/pin-project/pull/34 for more details.
            #[allow(single_use_lifetimes)]
            #[allow(non_snake_case)]
            #[deny(safe_packed_borrows)]
            fn __assert_not_repr_packed
                $(< $($lifetime $(: $lifetime_bound)? ,)* $( $generics $(: $generics_bound)? $(: ?$generics_unsized_bound)? $(: $generics_lifetime_bound)? ),* >)?
            (
                this: &$ident $(< $($lifetime,)* $($generics),* >)?
            )
                $(where
                    $($where_clause_ty: $where_clause_bound),*
                )*
            {
                $(
                    &this.$field;
                )+
            }
        };
    };

    // make_unpin_bound
    (@make_unpin_bound
        #[pin]
        $field_ty:ty
    ) => {
        $field_ty
    };
    (@make_unpin_bound
        $field_ty:ty
    ) => {
        $crate::__private::AlwaysUnpin<$field_ty>
    };

    // make_unsafe_field_proj
    (@make_unsafe_field_proj
        $this:ident;
        #[pin]
        $field:ident;
        $($mut:ident)?
    ) => {
        ::core::pin::Pin::new_unchecked(&$($mut)? $this.$field)
    };
    (@make_unsafe_field_proj
        $this:ident;
        $field:ident;
        $($mut:ident)?
    ) => {
        &$($mut)? $this.$field
    };

    // make_proj_field
    (@make_proj_field
        #[pin]
        $field_ty:ty;
        $($mut:ident)?
    ) => {
        ::core::pin::Pin<&'__pin $($mut)? ($field_ty)>
    };
    (@make_proj_field
        $field_ty:ty;
        $($mut:ident)?
    ) => {
        &'__pin $($mut)? ($field_ty)
    };

    // limitation: no useful error messages (wontfix)
}

// Not public API.
#[doc(hidden)]
pub mod __private {
    use core::marker::PhantomData;

    // This is an internal helper struct used by `pin_project!`.
    #[doc(hidden)]
    pub struct AlwaysUnpin<T: ?Sized>(PhantomData<T>);

    impl<T: ?Sized> Unpin for AlwaysUnpin<T> {}
}
