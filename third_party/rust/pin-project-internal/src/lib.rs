//! An internal crate to support pin_project - **do not use directly**

#![recursion_limit = "256"]
#![doc(html_root_url = "https://docs.rs/pin-project-internal/0.4.9")]
#![doc(test(
    no_crate_inject,
    attr(deny(warnings, rust_2018_idioms, single_use_lifetimes), allow(dead_code))
))]
#![warn(unsafe_code)]
#![warn(rust_2018_idioms, single_use_lifetimes, unreachable_pub)]
#![warn(clippy::all)]
// mem::take requires Rust 1.40
#![allow(clippy::mem_replace_with_default)]
#![allow(clippy::needless_doctest_main)]
// While this crate supports stable Rust, it currently requires
// nightly Rust in order for rustdoc to correctly document auto-generated
// `Unpin` impls. This does not affect the runtime functionality of this crate,
// nor does it affect the safety of the api provided by this crate.
//
// This is disabled by default and can be enabled using
// `--cfg pin_project_show_unpin_struct` in RUSTFLAGS.
//
// Refs:
// * https://github.com/taiki-e/pin-project/pull/53#issuecomment-525906867
// * https://github.com/taiki-e/pin-project/pull/70
// * https://github.com/rust-lang/rust/issues/63281
#![cfg_attr(pin_project_show_unpin_struct, feature(proc_macro_def_site))]

// older compilers require explicit `extern crate`.
#[allow(unused_extern_crates)]
extern crate proc_macro;

#[macro_use]
mod utils;

mod pin_project;
mod pinned_drop;
mod project;

use proc_macro::TokenStream;

use utils::{Immutable, Mutable};

/// An attribute that creates a projection struct covering all the fields.
///
/// This attribute creates a projection struct according to the following rules:
///
/// - For the field that uses `#[pin]` attribute, makes the pinned reference to
/// the field.
/// - For the other fields, makes the unpinned reference to the field.
///
/// The following methods are implemented on the original `#[pin_project]` type:
///
/// ```
/// # #[rustversion::since(1.36)]
/// # fn dox() {
/// # use std::pin::Pin;
/// # type Projection<'a> = &'a ();
/// # type ProjectionRef<'a> = &'a ();
/// # trait Dox {
/// fn project(self: Pin<&mut Self>) -> Projection<'_>;
/// fn project_ref(self: Pin<&Self>) -> ProjectionRef<'_>;
/// # }
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
/// This attribute is completely safe. In the absence of other `unsafe` code *that you write*,
/// it is impossible to cause undefined behavior with this attribute.
///
/// This is accomplished by enforcing the four requirements for pin projection
/// stated in [the Rust documentation](https://doc.rust-lang.org/nightly/std/pin/index.html#projections-and-structural-pinning):
///
/// 1. The struct must only be Unpin if all the structural fields are Unpin.
///
///    To enforce this, this attribute will automatically generate an `Unpin` implementation
///    for you, which will require that all structurally pinned fields be `Unpin`
///    If you wish to provide an manual `Unpin` impl, you can do so via the
///    `UnsafeUnpin` argument.
///
/// 2. The destructor of the struct must not move structural fields out of its argument.
///
///    To enforce this, this attribute will generate code like this:
///
///    ```rust
///    struct MyStruct {}
///    trait MyStructMustNotImplDrop {}
///    impl<T: Drop> MyStructMustNotImplDrop for T {}
///    impl MyStructMustNotImplDrop for MyStruct {}
///    ```
///
///    If you attempt to provide an Drop impl, the blanket impl will
///    then apply to your type, causing a compile-time error due to
///    the conflict with the second impl.
///
///    If you wish to provide a custom `Drop` impl, you can annotate a function
///    with `#[pinned_drop]`. This function takes a pinned version of your struct -
///    that is, `Pin<&mut MyStruct>` where `MyStruct` is the type of your struct.
///
///    You can call `project()` on this type as usual, along with any other
///    methods you have defined. Because your code is never provided with
///    a `&mut MyStruct`, it is impossible to move out of pin-projectable
///    fields in safe code in your destructor.
///
/// 3. You must make sure that you uphold the Drop guarantee: once your struct is pinned,
///    the memory that contains the content is not overwritten or deallocated without calling the content's destructors.
///
///    Safe code doesn't need to worry about this - the only wait to violate this requirement
///    is to manually deallocate memory (which is `unsafe`), or to overwrite a field with something else.
///    Because your custom destructor takes `Pin<&mut MyStruct`, it's impossible to obtain
///    a mutable reference to a pin-projected field in safe code.
///
/// 4. You must not offer any other operations that could lead to data being moved out of the structural fields when your type is pinned.
///
///    As with requirement 3, it is impossible for safe code to violate this. This crate ensures that safe code can never
///    obtain a mutable reference to `#[pin]` fields, which prevents you from ever moving out of them in safe code.
///
/// Pin projections are also incompatible with `#[repr(packed)]` structs. Attempting to use this attribute
/// on a `#[repr(packed)]` struct results in a compile-time error.
///
///
/// ## Examples
///
/// Using `#[pin_project]` will automatically create the appropriate
/// conditional [`Unpin`] implementation:
///
/// ```rust
/// use pin_project::pin_project;
/// use std::pin::Pin;
///
/// #[pin_project]
/// struct Foo<T, U> {
///     #[pin]
///     future: T,
///     field: U,
/// }
///
/// impl<T, U> Foo<T, U> {
///     fn baz(self: Pin<&mut Self>) {
///         let this = self.project();
///         let _: Pin<&mut T> = this.future; // Pinned reference to the field
///         let _: &mut U = this.field; // Normal reference to the field
///     }
/// }
/// ```
///
/// Note that borrowing the field where `#[pin]` attribute is used multiple
/// times requires using [`.as_mut()`][`Pin::as_mut`] to avoid
/// consuming the `Pin`.
///
/// If you want to implement [`Unpin`] manually, you must use the `UnsafeUnpin`
/// argument to `#[pin_project]`.
///
/// ```rust
/// use pin_project::{pin_project, UnsafeUnpin};
/// use std::pin::Pin;
///
/// #[pin_project(UnsafeUnpin)]
/// struct Foo<T, U> {
///     #[pin]
///     future: T,
///     field: U,
/// }
///
/// impl<T, U> Foo<T, U> {
///     fn baz(self: Pin<&mut Self>) {
///         let this = self.project();
///         let _: Pin<&mut T> = this.future; // Pinned reference to the field
///         let _: &mut U = this.field; // Normal reference to the field
///     }
/// }
///
/// unsafe impl<T: Unpin, U> UnsafeUnpin for Foo<T, U> {} // Conditional Unpin impl
/// ```
///
/// Note the usage of the unsafe [`UnsafeUnpin`] trait, instead of the usual
/// [`Unpin`] trait. [`UnsafeUnpin`] behaves exactly like [`Unpin`], except that is
/// unsafe to implement. This unsafety comes from the fact that pin projections
/// are being used. If you implement [`UnsafeUnpin`], you must ensure that it is
/// only implemented when all pin-projected fields implement [`Unpin`].
///
/// See [`UnsafeUnpin`] trait for more details.
///
/// ### `#[pinned_drop]`
///
/// In order to correctly implement pin projections, a type's `Drop` impl must
/// not move out of any structurally pinned fields. Unfortunately, [`Drop::drop`]
/// takes `&mut Self`, not `Pin<&mut Self>`.
///
/// To ensure that this requirement is upheld, the `#[pin_project]` attribute will
/// provide a [`Drop`] impl for you. This `Drop` impl will delegate to an impl
/// block annotated with `#[pinned_drop]` if you use the `PinnedDrop` argument
/// to `#[pin_project]`. This impl block acts just like a normal [`Drop`] impl,
/// except for the following two:
///
/// * `drop` method takes `Pin<&mut Self>`
/// * Name of the trait is `PinnedDrop`.
///
/// `#[pin_project]` implements the actual [`Drop`] trait via `PinnedDrop` you
/// implemented. To drop a type that implements `PinnedDrop`, use the [`drop`]
/// function just like dropping a type that directly implements [`Drop`].
///
/// In particular, it will never be called more than once, just like [`Drop::drop`].
///
/// For example:
///
/// ```rust
/// use pin_project::{pin_project, pinned_drop};
/// use std::{fmt::Debug, pin::Pin};
///
/// #[pin_project(PinnedDrop)]
/// pub struct Foo<T: Debug, U: Debug> {
///     #[pin]
///     pinned_field: T,
///     unpin_field: U,
/// }
///
/// #[pinned_drop]
/// impl<T: Debug, U: Debug> PinnedDrop for Foo<T, U> {
///     fn drop(self: Pin<&mut Self>) {
///         println!("Dropping pinned field: {:?}", self.pinned_field);
///         println!("Dropping unpin field: {:?}", self.unpin_field);
///     }
/// }
///
/// fn main() {
///     let _x = Foo { pinned_field: true, unpin_field: 40 };
/// }
/// ```
///
/// See also [`pinned_drop`] attribute.
///
/// ## Supported Items
///
/// The current pin-project supports the following types of items.
///
/// ### Structs (structs with named fields):
///
/// ```rust
/// use pin_project::pin_project;
/// use std::pin::Pin;
///
/// #[pin_project]
/// struct Foo<T, U> {
///     #[pin]
///     future: T,
///     field: U,
/// }
///
/// impl<T, U> Foo<T, U> {
///     fn baz(self: Pin<&mut Self>) {
///         let this = self.project();
///         let _: Pin<&mut T> = this.future;
///         let _: &mut U = this.field;
///     }
/// }
/// ```
///
/// ### Tuple structs (structs with unnamed fields):
///
/// ```rust
/// use pin_project::pin_project;
/// use std::pin::Pin;
///
/// #[pin_project]
/// struct Foo<T, U>(#[pin] T, U);
///
/// impl<T, U> Foo<T, U> {
///     fn baz(self: Pin<&mut Self>) {
///         let this = self.project();
///         let _: Pin<&mut T> = this.0;
///         let _: &mut U = this.1;
///     }
/// }
/// ```
///
/// Structs without fields (unit-like struct and zero fields struct) are not
/// supported.
///
/// ### Enums
///
/// `pin_project` also supports enums, but to use it, you need to use with the
/// [`project`] attribute.
///
/// The attribute at the expression position is not stable, so you need to use
/// a dummy `#[project]` attribute for the function.
///
/// ```rust
/// use pin_project::{pin_project, project};
/// use std::pin::Pin;
///
/// #[pin_project]
/// enum Foo<A, B, C> {
///     Tuple(#[pin] A, B),
///     Struct { field: C },
///     Unit,
/// }
///
/// impl<A, B, C> Foo<A, B, C> {
///     #[project] // Nightly does not need a dummy attribute to the function.
///     fn baz(self: Pin<&mut Self>) {
///         #[project]
///         match self.project() {
///             Foo::Tuple(x, y) => {
///                 let _: Pin<&mut A> = x;
///                 let _: &mut B = y;
///             }
///             Foo::Struct { field } => {
///                 let _: &mut C = field;
///             }
///             Foo::Unit => {}
///         }
///     }
/// }
/// ```
///
/// Enums without variants (zero-variant enums) are not supported.
///
/// See also [`project`] and [`project_ref`] attributes.
///
/// [`Pin::as_mut`]: core::pin::Pin::as_mut
/// [`Pin::set`]: core::pin::Pin::set
/// [`drop`]: Drop::drop
/// [`UnsafeUnpin`]: https://docs.rs/pin-project/0.4/pin_project/trait.UnsafeUnpin.html
/// [`project`]: ./attr.project.html
/// [`project_ref`]: ./attr.project_ref.html
/// [`pinned_drop`]: ./attr.pinned_drop.html
#[proc_macro_attribute]
pub fn pin_project(args: TokenStream, input: TokenStream) -> TokenStream {
    pin_project::attribute(&args.into(), input.into()).into()
}

/// An attribute for annotating an impl block that implements [`Drop`].
///
/// This attribute is only needed when you wish to provide a [`Drop`]
/// impl for your type. The impl block annotated with `#[pinned_drop]` acts just
/// like a normal [`Drop`] impl, except for the fact that `drop` method takes
/// `Pin<&mut Self>`. In particular, it will never be called more than once,
/// just like [`Drop::drop`].
///
/// ## Example
///
/// ```rust
/// use pin_project::{pin_project, pinned_drop};
/// use std::pin::Pin;
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
/// See ["pinned-drop" section of `pin_project` attribute][pinned-drop] for more details.
///
/// [pinned-drop]: ./attr.pin_project.html#pinned_drop
#[proc_macro_attribute]
pub fn pinned_drop(args: TokenStream, input: TokenStream) -> TokenStream {
    let input = syn::parse_macro_input!(input);
    pinned_drop::attribute(&args.into(), input).into()
}

/// An attribute to provide way to refer to the projected type returned by
/// `project` method.
///
/// The following syntaxes are supported.
///
/// ## `impl` blocks
///
/// All methods (and associated functions) in `#[project] impl` block become
/// methods of the projected type. If you want to implement methods on the
/// original type, you need to create another (non-`#[project]`) `impl` block.
///
/// To call a method implemented in `#[project] impl` block, you need to first
/// get the projected-type with `let this = self.project();`.
///
/// ### Examples
///
/// ```rust
/// use pin_project::{pin_project, project};
/// use std::pin::Pin;
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
/// ## `let` bindings
///
/// *The attribute at the expression position is not stable, so you need to use
/// a dummy `#[project]` attribute for the function.*
///
/// ### Examples
///
/// ```rust
/// use pin_project::{pin_project, project};
/// use std::pin::Pin;
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
/// ## `match` expressions
///
/// *The attribute at the expression position is not stable, so you need to use
/// a dummy `#[project]` attribute for the function.*
///
/// ### Examples
///
/// ```rust
/// use pin_project::{pin_project, project};
/// use std::pin::Pin;
///
/// #[pin_project]
/// enum Foo<A, B, C> {
///     Tuple(#[pin] A, B),
///     Struct { field: C },
///     Unit,
/// }
///
/// impl<A, B, C> Foo<A, B, C> {
///     #[project] // Nightly does not need a dummy attribute to the function.
///     fn baz(self: Pin<&mut Self>) {
///         #[project]
///         match self.project() {
///             Foo::Tuple(x, y) => {
///                 let _: Pin<&mut A> = x;
///                 let _: &mut B = y;
///             }
///             Foo::Struct { field } => {
///                 let _: &mut C = field;
///             }
///             Foo::Unit => {}
///         }
///     }
/// }
/// ```
///
/// ## `use` statements
///
/// ### Examples
///
/// ```rust
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
///     use super::Foo;
///     use pin_project::project;
///     use std::pin::Pin;
///
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
#[proc_macro_attribute]
pub fn project(args: TokenStream, input: TokenStream) -> TokenStream {
    let input = syn::parse_macro_input!(input);
    project::attribute(&args.into(), input, Mutable).into()
}

/// An attribute to provide way to refer to the projected type returned by
/// `project_ref` method.
///
/// This is the same as [`project`] attribute except it refers to the projected
/// type returned by `project_ref` method.
///
/// See [`project`] attribute for more details.
///
/// [`project`]: ./attr.project.html
#[proc_macro_attribute]
pub fn project_ref(args: TokenStream, input: TokenStream) -> TokenStream {
    let input = syn::parse_macro_input!(input);
    project::attribute(&args.into(), input, Immutable).into()
}

/// An internal helper macro.
#[doc(hidden)]
#[proc_macro_derive(__PinProjectInternalDerive, attributes(pin))]
pub fn __pin_project_internal_derive(input: TokenStream) -> TokenStream {
    pin_project::derive(input.into()).into()
}
