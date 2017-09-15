#![warn(missing_docs)]

//! See `BoxFnOnce` and `SendBoxFnOnce`.

/*
This crate requires:
a) A trait can dispatch through `self: Box<TraitName>` too: The "object-
   safety" RFC[^1] lists the following arguments as "dispatchable":
   `self`, `&self`, `&mut self`, or `self: Box<Self>`
b) If you have a v: Box<T> for a concrete (`Sized`) type T, you can
   extract the boxed value with *t (seems undocumented).

This basically duplicates what Box<FnBox> is doing, but without using
any unstable interface.

[1]: https://github.com/rust-lang/rfcs/blob/master/text/0255-object-safety.md#detailed-design
*/

#[macro_use]
mod macros;

mod traits;

mod no_send;
pub use self::no_send::BoxFnOnce;

mod send;
pub use self::send::SendBoxFnOnce;
