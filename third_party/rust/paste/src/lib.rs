//! [![github]](https://github.com/dtolnay/paste)&ensp;[![crates-io]](https://crates.io/crates/paste)&ensp;[![docs-rs]](https://docs.rs/paste)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
//!
//! <br>
//!
//! The nightly-only [`concat_idents!`] macro in the Rust standard library is
//! notoriously underpowered in that its concatenated identifiers can only refer to
//! existing items, they can never be used to define something new.
//!
//! [`concat_idents!`]: https://doc.rust-lang.org/std/macro.concat_idents.html
//!
//! This crate provides a flexible way to paste together identifiers in a macro,
//! including using pasted identifiers to define new items.
//!
//! This approach works with any stable or nightly Rust compiler 1.30+.
//!
//! <br>
//!
//! # Pasting identifiers
//!
//! There are two entry points, `paste::expr!` for macros in expression position and
//! `paste::item!` for macros in item position.
//!
//! Within either one, identifiers inside `[<`...`>]` are pasted together to form a
//! single identifier.
//!
//! ```
//! // Macro in item position: at module scope or inside of an impl block.
//! paste::item! {
//!     // Defines a const called `QRST`.
//!     const [<Q R S T>]: &str = "success!";
//! }
//!
//! fn main() {
//!     // Macro in expression position: inside a function body.
//!     assert_eq!(
//!         paste::expr! { [<Q R S T>].len() },
//!         8,
//!     );
//! }
//! ```
//!
//! <br><br>
//!
//! # More elaborate examples
//!
//! This program demonstrates how you may want to bundle a paste invocation inside
//! of a more convenient user-facing macro of your own. Here the `routes!(A, B)`
//! macro expands to a vector containing `ROUTE_A` and `ROUTE_B`.
//!
//! ```
//! const ROUTE_A: &str = "/a";
//! const ROUTE_B: &str = "/b";
//!
//! macro_rules! routes {
//!     ($($route:ident),*) => {{
//!         paste::expr! {
//!             vec![$( [<ROUTE_ $route>] ),*]
//!         }
//!     }}
//! }
//!
//! fn main() {
//!     let routes = routes!(A, B);
//!     assert_eq!(routes, vec!["/a", "/b"]);
//! }
//! ```
//!
//! The next example shows a macro that generates accessor methods for some struct
//! fields.
//!
//! ```
//! macro_rules! make_a_struct_and_getters {
//!     ($name:ident { $($field:ident),* }) => {
//!         // Define a struct. This expands to:
//!         //
//!         //     pub struct S {
//!         //         a: String,
//!         //         b: String,
//!         //         c: String,
//!         //     }
//!         pub struct $name {
//!             $(
//!                 $field: String,
//!             )*
//!         }
//!
//!         // Build an impl block with getters. This expands to:
//!         //
//!         //     impl S {
//!         //         pub fn get_a(&self) -> &str { &self.a }
//!         //         pub fn get_b(&self) -> &str { &self.b }
//!         //         pub fn get_c(&self) -> &str { &self.c }
//!         //     }
//!         paste::item! {
//!             impl $name {
//!                 $(
//!                     pub fn [<get_ $field>](&self) -> &str {
//!                         &self.$field
//!                     }
//!                 )*
//!             }
//!         }
//!     }
//! }
//!
//! make_a_struct_and_getters!(S { a, b, c });
//!
//! fn call_some_getters(s: &S) -> bool {
//!     s.get_a() == s.get_b() && s.get_c().is_empty()
//! }
//! #
//! # fn main() {}
//! ```
//!
//! <br><br>
//!
//! # Case conversion
//!
//! Use `$var:lower` or `$var:upper` in the segment list to convert an
//! interpolated segment to lower- or uppercase as part of the paste. For
//! example, `[<ld_ $reg:lower _expr>]` would paste to `ld_bc_expr` if invoked
//! with $reg=`Bc`.
//!
//! Use `$var:snake` to convert CamelCase input to snake\_case.
//! Use `$var:camel` to convert snake\_case to CamelCase.
//! These compose, so for example `$var:snake:upper` would give you SCREAMING\_CASE.
//!
//! The precise Unicode conversions are as defined by [`str::to_lowercase`] and
//! [`str::to_uppercase`].
//!
//! [`str::to_lowercase`]: https://doc.rust-lang.org/std/primitive.str.html#method.to_lowercase
//! [`str::to_uppercase`]: https://doc.rust-lang.org/std/primitive.str.html#method.to_uppercase

#![no_std]

use proc_macro_hack::proc_macro_hack;

/// Paste identifiers within a macro invocation that expands to an expression.
#[proc_macro_hack]
pub use paste_impl::expr;

/// Paste identifiers within a macro invocation that expands to one or more
/// items.
///
/// An item is like a struct definition, function, impl block, or anything else
/// that can appear at the top level of a module scope.
pub use paste_impl::item;

/// Paste identifiers within a macro invocation that expands to one or more
/// macro_rules macros or items containing macros.
pub use paste_impl::item_with_macros;

#[doc(hidden)]
pub use paste_impl::EnumHack;
