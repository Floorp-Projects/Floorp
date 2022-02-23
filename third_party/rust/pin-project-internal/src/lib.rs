//! An internal crate to support pin_project - **do not use directly**

#![doc(test(
    no_crate_inject,
    attr(deny(warnings, rust_2018_idioms, single_use_lifetimes), allow(dead_code))
))]
#![warn(unsafe_code)]
#![warn(rust_2018_idioms, single_use_lifetimes, unreachable_pub)]
#![warn(clippy::default_trait_access, clippy::wildcard_imports)]
// mem::take and #[non_exhaustive] requires Rust 1.40
#![allow(clippy::mem_replace_with_default, clippy::manual_non_exhaustive)]
#![allow(clippy::needless_doctest_main)]

// older compilers require explicit `extern crate`.
#[allow(unused_extern_crates)]
extern crate proc_macro;

#[macro_use]
mod utils;

mod pin_project;
mod pinned_drop;
mod project;

use proc_macro::TokenStream;

use crate::utils::ProjKind;

/// An attribute that creates projection types covering all the fields of
/// struct or enum.
///
/// This attribute creates projection types according to the following rules:
///
/// * For the fields that use `#[pin]` attribute, create the pinned reference to
///   the field.
/// * For the other fields, create a normal reference to the field.
///
/// And the following methods are implemented on the original type:
///
/// ```rust
/// # use std::pin::Pin;
/// # type Projection<'a> = &'a ();
/// # type ProjectionRef<'a> = &'a ();
/// # trait Dox {
/// fn project(self: Pin<&mut Self>) -> Projection<'_>;
/// fn project_ref(self: Pin<&Self>) -> ProjectionRef<'_>;
/// # }
/// ```
///
/// By passing an argument with the same name as the method to the attribute,
/// you can name the projection type returned from the method:
///
/// ```rust
/// use std::pin::Pin;
///
/// use pin_project::pin_project;
///
/// #[pin_project(project = StructProj)]
/// struct Struct<T> {
///     #[pin]
///     field: T,
/// }
///
/// impl<T> Struct<T> {
///     fn method(self: Pin<&mut Self>) {
///         let this: StructProj<'_, T> = self.project();
///         let StructProj { field } = this;
///         let _: Pin<&mut T> = field;
///     }
/// }
/// ```
///
/// Note that the projection types returned by `project` and `project_ref` have
/// an additional lifetime at the beginning of generics.
///
/// The visibility of the projected type and projection method is based on the
/// original type. However, if the visibility of the original type is `pub`, the
/// visibility of the projected type and the projection method is downgraded to
/// `pub(crate)`.
///
/// # Safety
///
/// This attribute is completely safe. In the absence of other `unsafe` code
/// *that you write*, it is impossible to cause [undefined
/// behavior][undefined-behavior] with this attribute.
///
/// This is accomplished by enforcing the four requirements for pin projection
/// stated in [the Rust documentation][pin-projection]:
///
/// 1. The struct must only be [`Unpin`] if all the structural fields are
///    [`Unpin`].
///
///    To enforce this, this attribute will automatically generate an [`Unpin`]
///    implementation for you, which will require that all structurally pinned
///    fields be [`Unpin`].
///
///    If you attempt to provide an [`Unpin`] impl, the blanket impl will then
///    apply to your type, causing a compile-time error due to the conflict with
///    the second impl.
///
///    If you wish to provide a manual [`Unpin`] impl, you can do so via the
///    [`UnsafeUnpin`][unsafe-unpin] argument.
///
/// 2. The destructor of the struct must not move structural fields out of its
///    argument.
///
///    To enforce this, this attribute will generate code like this:
///
///    ```rust
///    struct MyStruct {}
///    trait MyStructMustNotImplDrop {}
///    # #[allow(unknown_lints, drop_bounds)]
///    impl<T: Drop> MyStructMustNotImplDrop for T {}
///    impl MyStructMustNotImplDrop for MyStruct {}
///    ```
///
///    If you attempt to provide an [`Drop`] impl, the blanket impl will then
///    apply to your type, causing a compile-time error due to the conflict with
///    the second impl.
///
///    If you wish to provide a custom [`Drop`] impl, you can annotate an impl
///    with [`#[pinned_drop]`][pinned-drop]. This impl takes a pinned version of
///    your struct - that is, [`Pin`]`<&mut MyStruct>` where `MyStruct` is the
///    type of your struct.
///
///    You can call `project()` on this type as usual, along with any other
///    methods you have defined. Because your code is never provided with
///    a `&mut MyStruct`, it is impossible to move out of pin-projectable
///    fields in safe code in your destructor.
///
/// 3. You must make sure that you uphold the [`Drop`
///    guarantee][drop-guarantee]: once your struct is pinned, the memory that
///    contains the content is not overwritten or deallocated without calling
///    the content's destructors.
///
///    Safe code doesn't need to worry about this - the only way to violate
///    this requirement is to manually deallocate memory (which is `unsafe`),
///    or to overwrite a field with something else.
///    Because your custom destructor takes [`Pin`]`<&mut MyStruct>`, it's
///    impossible to obtain a mutable reference to a pin-projected field in safe
///    code.
///
/// 4. You must not offer any other operations that could lead to data being
///    moved out of the structural fields when your type is pinned.
///
///    As with requirement 3, it is impossible for safe code to violate this.
///    This crate ensures that safe code can never obtain a mutable reference to
///    `#[pin]` fields, which prevents you from ever moving out of them in safe
///    code.
///
/// Pin projections are also incompatible with [`#[repr(packed)]`][repr-packed]
/// structs. Attempting to use this attribute on a
/// [`#[repr(packed)]`][repr-packed] struct results in a compile-time error.
///
/// # Examples
///
/// `#[pin_project]` can be used on structs and enums.
///
/// ```rust
/// use std::pin::Pin;
///
/// use pin_project::pin_project;
///
/// #[pin_project]
/// struct Struct<T, U> {
///     #[pin]
///     pinned: T,
///     unpinned: U,
/// }
///
/// impl<T, U> Struct<T, U> {
///     fn method(self: Pin<&mut Self>) {
///         let this = self.project();
///         let _: Pin<&mut T> = this.pinned;
///         let _: &mut U = this.unpinned;
///     }
/// }
/// ```
///
/// ```rust
/// use std::pin::Pin;
///
/// use pin_project::pin_project;
///
/// #[pin_project]
/// struct TupleStruct<T, U>(#[pin] T, U);
///
/// impl<T, U> TupleStruct<T, U> {
///     fn method(self: Pin<&mut Self>) {
///         let this = self.project();
///         let _: Pin<&mut T> = this.0;
///         let _: &mut U = this.1;
///     }
/// }
/// ```
///
/// To use `#[pin_project]` on enums, you need to name the projection type
/// returned from the method.
///
/// ```rust
/// use std::pin::Pin;
///
/// use pin_project::pin_project;
///
/// #[pin_project(project = EnumProj)]
/// enum Enum<T, U> {
///     Tuple(#[pin] T),
///     Struct { field: U },
///     Unit,
/// }
///
/// impl<T, U> Enum<T, U> {
///     fn method(self: Pin<&mut Self>) {
///         match self.project() {
///             EnumProj::Tuple(x) => {
///                 let _: Pin<&mut T> = x;
///             }
///             EnumProj::Struct { field } => {
///                 let _: &mut U = field;
///             }
///             EnumProj::Unit => {}
///         }
///     }
/// }
/// ```
///
/// If you want to call the `project()` method multiple times or later use the
/// original [`Pin`] type, it needs to use [`.as_mut()`][`Pin::as_mut`] to avoid
/// consuming the [`Pin`].
///
/// ```rust
/// use std::pin::Pin;
///
/// use pin_project::pin_project;
///
/// #[pin_project]
/// struct Struct<T> {
///     #[pin]
///     field: T,
/// }
///
/// impl<T> Struct<T> {
///     fn call_project_twice(mut self: Pin<&mut Self>) {
///         // `project` consumes `self`, so reborrow the `Pin<&mut Self>` via `as_mut`.
///         self.as_mut().project();
///         self.as_mut().project();
///     }
/// }
/// ```
///
/// # `!Unpin`
///
/// If you want to ensure that [`Unpin`] is not implemented, use the `!Unpin`
/// argument to `#[pin_project]`.
///
/// ```rust
/// use pin_project::pin_project;
///
/// #[pin_project(!Unpin)]
/// struct Struct<T> {
///     field: T,
/// }
/// ```
///
/// This is equivalent to using `#[pin]` attribute for the [`PhantomPinned`]
/// field.
///
/// ```rust
/// use std::marker::PhantomPinned;
///
/// use pin_project::pin_project;
///
/// #[pin_project]
/// struct Struct<T> {
///     field: T,
///     #[pin]
///     _pin: PhantomPinned,
/// }
/// ```
///
/// Note that using [`PhantomPinned`] without `#[pin]` attribute has no effect.
///
/// # `UnsafeUnpin`
///
/// If you want to implement [`Unpin`] manually, you must use the `UnsafeUnpin`
/// argument to `#[pin_project]`.
///
/// ```rust
/// use pin_project::{pin_project, UnsafeUnpin};
///
/// #[pin_project(UnsafeUnpin)]
/// struct Struct<T, U> {
///     #[pin]
///     pinned: T,
///     unpinned: U,
/// }
///
/// unsafe impl<T: Unpin, U> UnsafeUnpin for Struct<T, U> {}
/// ```
///
/// Note the usage of the unsafe [`UnsafeUnpin`] trait, instead of the usual
/// [`Unpin`] trait. [`UnsafeUnpin`] behaves exactly like [`Unpin`], except that
/// is unsafe to implement. This unsafety comes from the fact that pin
/// projections are being used. If you implement [`UnsafeUnpin`], you must
/// ensure that it is only implemented when all pin-projected fields implement
/// [`Unpin`].
///
/// See [`UnsafeUnpin`] trait for more details.
///
/// # `#[pinned_drop]`
///
/// In order to correctly implement pin projections, a type's [`Drop`] impl must
/// not move out of any structurally pinned fields. Unfortunately,
/// [`Drop::drop`] takes `&mut Self`, not [`Pin`]`<&mut Self>`.
///
/// To ensure that this requirement is upheld, the `#[pin_project]` attribute
/// will provide a [`Drop`] impl for you. This [`Drop`] impl will delegate to
/// an impl block annotated with `#[pinned_drop]` if you use the `PinnedDrop`
/// argument to `#[pin_project]`.
///
/// This impl block acts just like a normal [`Drop`] impl,
/// except for the following two:
///
/// * `drop` method takes [`Pin`]`<&mut Self>`
/// * Name of the trait is `PinnedDrop`.
///
/// ```rust
/// # use std::pin::Pin;
/// pub trait PinnedDrop {
///     fn drop(self: Pin<&mut Self>);
/// }
/// ```
///
/// `#[pin_project]` implements the actual [`Drop`] trait via `PinnedDrop` you
/// implemented. To drop a type that implements `PinnedDrop`, use the [`drop`]
/// function just like dropping a type that directly implements [`Drop`].
///
/// In particular, it will never be called more than once, just like
/// [`Drop::drop`].
///
/// For example:
///
/// ```rust
/// use std::{fmt::Debug, pin::Pin};
///
/// use pin_project::{pin_project, pinned_drop};
///
/// #[pin_project(PinnedDrop)]
/// struct Struct<T: Debug, U: Debug> {
///     #[pin]
///     pinned_field: T,
///     unpin_field: U,
/// }
///
/// #[pinned_drop]
/// impl<T: Debug, U: Debug> PinnedDrop for Struct<T, U> {
///     fn drop(self: Pin<&mut Self>) {
///         println!("Dropping pinned field: {:?}", self.pinned_field);
///         println!("Dropping unpin field: {:?}", self.unpin_field);
///     }
/// }
///
/// fn main() {
///     let _x = Struct { pinned_field: true, unpin_field: 40 };
/// }
/// ```
///
/// See also [`#[pinned_drop]`][`pinned_drop`] attribute.
///
/// # `project_replace()`
///
/// In addition to the `project()` and `project_ref()` methods which are always
/// provided when you use the `#[pin_project]` attribute, there is a third
/// method, `project_replace()` which can be useful in some situations. It is
/// equivalent to [`Pin::set`], except that the unpinned fields are moved and
/// returned, instead of being dropped in-place.
///
/// ```rust
/// # use std::pin::Pin;
/// # type ProjectionOwned = ();
/// # trait Dox {
/// fn project_replace(self: Pin<&mut Self>, other: Self) -> ProjectionOwned;
/// # }
/// ```
///
/// The `ProjectionOwned` type is identical to the `Self` type, except that
/// all pinned fields have been replaced by equivalent [`PhantomData`] types.
///
/// This method is opt-in, because it is only supported for [`Sized`] types, and
/// because it is incompatible with the [`#[pinned_drop]`][pinned-drop]
/// attribute described above. It can be enabled by using
/// `#[pin_project(project_replace)]`.
///
/// For example:
///
/// ```rust
/// use std::{marker::PhantomData, pin::Pin};
///
/// use pin_project::pin_project;
///
/// #[pin_project(project_replace)]
/// struct Struct<T, U> {
///     #[pin]
///     pinned_field: T,
///     unpinned_field: U,
/// }
///
/// impl<T, U> Struct<T, U> {
///     fn method(self: Pin<&mut Self>, other: Self) {
///         let this = self.project_replace(other);
///         let _: U = this.unpinned_field;
///         let _: PhantomData<T> = this.pinned_field;
///     }
/// }
/// ```
///
/// By passing the value to the `project_replace` argument, you can name the
/// returned type of `project_replace()`. This is necessary whenever
/// destructuring the return type of `project_replace()`, and work in exactly
/// the same way as the `project` and `project_ref` arguments.
///
/// ```rust
/// use pin_project::pin_project;
///
/// #[pin_project(project_replace = EnumProjOwn)]
/// enum Enum<T, U> {
///     A {
///         #[pin]
///         pinned_field: T,
///         unpinned_field: U,
///     },
///     B,
/// }
///
/// let mut x = Box::pin(Enum::A { pinned_field: 42, unpinned_field: "hello" });
///
/// match x.as_mut().project_replace(Enum::B) {
///     EnumProjOwn::A { unpinned_field, .. } => assert_eq!(unpinned_field, "hello"),
///     EnumProjOwn::B => unreachable!(),
/// }
/// ```
///
/// [`PhantomData`]: core::marker::PhantomData
/// [`PhantomPinned`]: core::marker::PhantomPinned
/// [`Pin::as_mut`]: core::pin::Pin::as_mut
/// [`Pin::set`]: core::pin::Pin::set
/// [`Pin`]: core::pin::Pin
/// [`UnsafeUnpin`]: https://docs.rs/pin-project/0.4/pin_project/trait.UnsafeUnpin.html
/// [`pinned_drop`]: ./attr.pinned_drop.html
/// [drop-guarantee]: https://doc.rust-lang.org/nightly/std/pin/index.html#drop-guarantee
/// [pin-projection]: https://doc.rust-lang.org/nightly/std/pin/index.html#projections-and-structural-pinning
/// [pinned-drop]: ./attr.pin_project.html#pinned_drop
/// [repr-packed]: https://doc.rust-lang.org/nomicon/other-reprs.html#reprpacked
/// [undefined-behavior]: https://doc.rust-lang.org/reference/behavior-considered-undefined.html
/// [unsafe-unpin]: ./attr.pin_project.html#unsafeunpin
#[proc_macro_attribute]
pub fn pin_project(args: TokenStream, input: TokenStream) -> TokenStream {
    pin_project::attribute(&args.into(), input.into()).into()
}

/// An attribute for annotating an impl block that implements [`Drop`].
///
/// This attribute is only needed when you wish to provide a [`Drop`]
/// impl for your type.
///
/// This impl block acts just like a normal [`Drop`] impl,
/// except for the following two:
///
/// * `drop` method takes [`Pin`]`<&mut Self>`
/// * Name of the trait is `PinnedDrop`.
///
/// ```rust
/// # use std::pin::Pin;
/// pub trait PinnedDrop {
///     fn drop(self: Pin<&mut Self>);
/// }
/// ```
///
/// `#[pin_project]` implements the actual [`Drop`] trait via `PinnedDrop` you
/// implemented. To drop a type that implements `PinnedDrop`, use the [`drop`]
/// function just like dropping a type that directly implements [`Drop`].
///
/// In particular, it will never be called more than once, just like
/// [`Drop::drop`].
///
/// # Example
///
/// ```rust
/// use std::pin::Pin;
///
/// use pin_project::{pin_project, pinned_drop};
///
/// #[pin_project(PinnedDrop)]
/// struct Foo {
///     #[pin]
///     field: u8,
/// }
///
/// #[pinned_drop]
/// impl PinnedDrop for Foo {
///     fn drop(self: Pin<&mut Self>) {
///         println!("Dropping: {}", self.field);
///     }
/// }
///
/// fn main() {
///     let _x = Foo { field: 50 };
/// }
/// ```
///
/// See also ["pinned-drop" section of `#[pin_project]` attribute][pinned-drop].
///
/// # Why `#[pinned_drop]` attribute is needed?
///
/// Implementing `PinnedDrop::drop` is safe, but calling it is not safe.
// This is because destructors can be called multiple times in safe code and
/// [double dropping is unsound](https://github.com/rust-lang/rust/pull/62360).
///
/// Ideally, it would be desirable to be able to forbid manual calls in
/// the same way as [`Drop::drop`], but the library cannot do it. So, by using
/// macros and replacing them with private traits like the following, we prevent
/// users from calling `PinnedDrop::drop` in safe code.
///
/// ```rust
/// # use std::pin::Pin;
/// pub trait PinnedDrop {
///     unsafe fn drop(self: Pin<&mut Self>);
/// }
/// ```
///
/// This allows implementing [`Drop`] safely using `#[pinned_drop]`.
/// Also by using the [`drop`] function just like dropping a type that directly
/// implements [`Drop`], can drop safely a type that implements `PinnedDrop`.
///
/// [`Pin`]: core::pin::Pin
/// [pinned-drop]: ./attr.pin_project.html#pinned_drop
#[proc_macro_attribute]
pub fn pinned_drop(args: TokenStream, input: TokenStream) -> TokenStream {
    let input = syn::parse_macro_input!(input);
    pinned_drop::attribute(&args.into(), input).into()
}

/// (deprecated) An attribute to provide way to refer to the projected type returned by
/// `project` method.
///
/// **This attribute is deprecated. Consider naming projected type by passing
/// `project` argument to `#[pin_project]` attribute instead, see [release note]
/// for details**
///
/// The following syntaxes are supported.
///
/// # `let` bindings
///
/// *The attribute at the expression position is not stable, so you need to use
/// a dummy `#[project]` attribute for the function.*
///
/// ## Examples
///
/// ```rust
/// # #![allow(deprecated)]
/// use std::pin::Pin;
///
/// use pin_project::{pin_project, project};
///
/// #[pin_project]
/// struct Foo<T, U> {
///     #[pin]
///     future: T,
///     field: U,
/// }
///
/// impl<T, U> Foo<T, U> {
///     #[project] // Nightly does not need a dummy attribute to the function.
///     fn baz(self: Pin<&mut Self>) {
///         #[project]
///         let Foo { future, field } = self.project();
///
///         let _: Pin<&mut T> = future;
///         let _: &mut U = field;
///     }
/// }
/// ```
///
/// # `match` expressions
///
/// *The attribute at the expression position is not stable, so you need to use
/// a dummy `#[project]` attribute for the function.*
///
/// ## Examples
///
/// ```rust
/// # #![allow(deprecated)]
/// use std::pin::Pin;
///
/// use pin_project::{pin_project, project};
///
/// #[pin_project]
/// enum Enum<A, B, C> {
///     Tuple(#[pin] A, B),
///     Struct { field: C },
///     Unit,
/// }
///
/// impl<A, B, C> Enum<A, B, C> {
///     #[project] // Nightly does not need a dummy attribute to the function.
///     fn baz(self: Pin<&mut Self>) {
///         #[project]
///         match self.project() {
///             Enum::Tuple(x, y) => {
///                 let _: Pin<&mut A> = x;
///                 let _: &mut B = y;
///             }
///             Enum::Struct { field } => {
///                 let _: &mut C = field;
///             }
///             Enum::Unit => {}
///         }
///     }
/// }
/// ```
///
/// # `impl` blocks
///
/// All methods and associated functions in `#[project] impl` block become
/// methods of the projected type. If you want to implement methods on the
/// original type, you need to create another (non-`#[project]`) `impl` block.
///
/// To call a method implemented in `#[project] impl` block, you need to first
/// get the projected-type with `let this = self.project();`.
///
/// ## Examples
///
/// ```rust
/// # #![allow(deprecated)]
/// use std::pin::Pin;
///
/// use pin_project::{pin_project, project};
///
/// #[pin_project]
/// struct Foo<T, U> {
///     #[pin]
///     future: T,
///     field: U,
/// }
///
/// // impl for the original type
/// impl<T, U> Foo<T, U> {
///     fn bar(self: Pin<&mut Self>) {
///         self.project().baz()
///     }
/// }
///
/// // impl for the projected type
/// #[project]
/// impl<T, U> Foo<T, U> {
///     fn baz(self) {
///         let Self { future, field } = self;
///
///         let _: Pin<&mut T> = future;
///         let _: &mut U = field;
///     }
/// }
/// ```
///
/// # `use` statements
///
/// ## Examples
///
/// ```rust
/// # #![allow(deprecated)]
/// # mod dox {
/// use pin_project::pin_project;
///
/// #[pin_project]
/// struct Foo<A> {
///     #[pin]
///     field: A,
/// }
///
/// mod bar {
///     use std::pin::Pin;
///
///     use pin_project::project;
///
///     use super::Foo;
///     #[project]
///     use super::Foo;
///
///     #[project]
///     fn baz<A>(foo: Pin<&mut Foo<A>>) {
///         #[project]
///         let Foo { field } = foo.project();
///         let _: Pin<&mut A> = field;
///     }
/// }
/// # }
/// ```
///
/// [release note]: https://github.com/taiki-e/pin-project/releases/tag/v0.4.21
#[cfg_attr(
    deprecated_proc_macro,
    deprecated(
        since = "0.4.21",
        note = "consider naming projected type by passing `project` \
                argument to #[pin_project] attribute instead, see release note \
                <https://github.com/taiki-e/pin-project/releases/tag/v0.4.21> \
                for details"
    )
)]
#[proc_macro_attribute]
pub fn project(args: TokenStream, input: TokenStream) -> TokenStream {
    let input = syn::parse_macro_input!(input);
    project::attribute(&args.into(), input, ProjKind::Mutable).into()
}

/// (deprecated) An attribute to provide way to refer to the projected type returned by
/// `project_ref` method.
///
/// **This attribute is deprecated. Consider naming projected type by passing
/// `project_ref` argument to `#[pin_project]` attribute instead, see [release note]
/// for details**
///
/// This is the same as [`#[project]`][`project`] attribute except it refers to
/// the projected type returned by the `project_ref` method.
///
/// See [`#[project]`][`project`] attribute for more details.
///
/// [release note]: https://github.com/taiki-e/pin-project/releases/tag/v0.4.21
/// [`project`]: ./attr.project.html
#[cfg_attr(
    deprecated_proc_macro,
    deprecated(
        since = "0.4.21",
        note = "consider naming projected type by passing `project_ref` \
                argument to #[pin_project] attribute instead, see release note \
                <https://github.com/taiki-e/pin-project/releases/tag/v0.4.21> \
                for details"
    )
)]
#[proc_macro_attribute]
pub fn project_ref(args: TokenStream, input: TokenStream) -> TokenStream {
    let input = syn::parse_macro_input!(input);
    project::attribute(&args.into(), input, ProjKind::Immutable).into()
}

/// (deprecated) An attribute to provide way to refer to the projected type returned by
/// `project_replace` method.
///
/// **This attribute is deprecated. Consider naming projected type by passing
/// `project_replace` argument to `#[pin_project]` attribute instead, see [release note]
/// for details**
///
/// This is the same as [`#[project]`][`project`] attribute except it refers to
/// the projected type returned by the `project_replace` method.
///
/// See [`#[project]`][`project`] attribute for more details.
///
/// [release note]: https://github.com/taiki-e/pin-project/releases/tag/v0.4.21
/// [`project`]: ./attr.project.html
#[cfg_attr(
    deprecated_proc_macro,
    deprecated(
        since = "0.4.21",
        note = "consider naming projected type by passing `project_replace` \
                argument to #[pin_project] attribute instead, see release note \
                <https://github.com/taiki-e/pin-project/releases/tag/v0.4.21> \
                for details"
    )
)]
#[proc_macro_attribute]
pub fn project_replace(args: TokenStream, input: TokenStream) -> TokenStream {
    let input = syn::parse_macro_input!(input);
    project::attribute(&args.into(), input, ProjKind::Owned).into()
}

// An internal helper macro. Not public API.
#[doc(hidden)]
#[proc_macro_derive(__PinProjectInternalDerive, attributes(pin))]
pub fn __pin_project_internal_derive(input: TokenStream) -> TokenStream {
    pin_project::derive(input.into()).into()
}
