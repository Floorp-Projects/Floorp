//! Temporarily take ownership of a value at a mutable location, and replace it with a new value
//! based on the old one.
//!
//! <p style="font-family: 'Fira Sans',sans-serif;padding:0.3em 0"><strong>
//! <a href="https://crates.io/crates/replace_with">📦&nbsp;&nbsp;Crates.io</a>&nbsp;&nbsp;│&nbsp;&nbsp;<a href="https://github.com/alecmocatta/replace_with">📑&nbsp;&nbsp;GitHub</a>&nbsp;&nbsp;│&nbsp;&nbsp;<a href="https://constellation.zulipchat.com/#narrow/stream/213236-subprojects">💬&nbsp;&nbsp;Chat</a>
//! </strong></p>
//!
//! This crate provides the function [`replace_with()`], which is like [`std::mem::replace()`]
//! except it allows the replacement value to be mapped from the original value.
//!
//! See [RFC 1736](https://github.com/rust-lang/rfcs/pull/1736) for a lot of discussion as to its
//! merits. It was never merged, and the desired ability to temporarily move out of `&mut T` doesn't
//! exist yet, so this crate is my interim solution.
//!
//! It's very akin to [`take_mut`](https://github.com/Sgeo/take_mut), though uses `Drop` instead of
//! [`std::panic::catch_unwind()`] to react to unwinding, which avoids the optimisation barrier of
//! calling the `extern "C" __rust_maybe_catch_panic()`. As such it's up to ∞x faster. The API also
//! attempts to make slightly more explicit the behavior on panic – [`replace_with()`] accepts two
//! closures such that aborting in the "standard case" where the mapping closure (`FnOnce(T) -> T`)
//! panics (as [`take_mut::take()`](https://docs.rs/take_mut/0.2.2/take_mut/fn.take.html) does) is
//! avoided. If the second closure (`FnOnce() -> T`) panics, however, then it does indeed abort.
//! The "abort on first panic" behaviour is available with [`replace_with_or_abort()`].
//!
//! # Example
//!
//! Consider this motivating example:
//!
//! ```compile_fail
//! # use replace_with::*;
//! enum States {
//!     A(String),
//!     B(String),
//! }
//!
//! impl States {
//!     fn poll(&mut self) {
//!         // error[E0507]: cannot move out of borrowed content
//!         *self = match *self {
//!         //            ^^^^^ cannot move out of borrowed content
//!             States::A(a) => States::B(a),
//!             States::B(a) => States::A(a),
//!         };
//!     }
//! }
//! ```
//!
//! Depending on context this can be quite tricky to work around. With this crate, however:
//!
//! ```
//! # use replace_with::*;
//! enum States {
//!     A(String),
//!     B(String),
//! }
//!
//! # #[cfg(any(feature = "std", feature = "nightly"))]
//! impl States {
//!     fn poll(&mut self) {
//!         replace_with_or_abort(self, |self_| match self_ {
//!             States::A(a) => States::B(a),
//!             States::B(a) => States::A(a),
//!         });
//!     }
//! }
//! ```
//!
//! Huzzah!

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(
	all(not(feature = "std"), feature = "nightly"),
	feature(core_intrinsics)
)]
#![doc(html_root_url = "https://docs.rs/replace_with/0.1.7")]
#![warn(
	missing_copy_implementations,
	missing_debug_implementations,
	missing_docs,
	trivial_casts,
	trivial_numeric_casts,
	unused_import_braces,
	unused_qualifications,
	unused_results,
	// clippy::pedantic
)]
// #![allow(clippy::inline_always)]

#[cfg(not(feature = "std"))]
use core as std;

use std::{mem, ptr};

struct OnDrop<F: FnOnce()>(mem::ManuallyDrop<F>);
impl<F: FnOnce()> Drop for OnDrop<F> {
	#[inline(always)]
	fn drop(&mut self) {
		(unsafe { ptr::read(&*self.0) })();
	}
}

#[doc(hidden)]
#[inline(always)]
pub fn on_unwind<F: FnOnce() -> T, T, P: FnOnce()>(f: F, p: P) -> T {
	let x = OnDrop(mem::ManuallyDrop::new(p));
	let t = f();
	let mut x = mem::ManuallyDrop::new(x);
	unsafe { mem::ManuallyDrop::drop(&mut x.0) };
	t
}

#[doc(hidden)]
#[inline(always)]
pub fn on_return_or_unwind<F: FnOnce() -> T, T, P: FnOnce()>(f: F, p: P) -> T {
	let _x = OnDrop(mem::ManuallyDrop::new(p));
	f()
}

/// Temporarily takes ownership of a value at a mutable location, and replace it with a new value
/// based on the old one.
///
/// We move out of the reference temporarily, to apply a closure `f`, returning a new value, which
/// is then placed at the original value's location.
///
/// # An important note
///
/// On panic (or to be more precise, unwinding) of the closure `f`, `default` will be called to
/// provide a replacement value. `default` should not panic – doing so will constitute a double
/// panic and will most likely abort the process.
///
/// # Example
///
/// ```
/// # use replace_with::*;
/// enum States {
///     A(String),
///     B(String),
/// }
///
/// impl States {
///     fn poll(&mut self) {
///         replace_with(
///             self,
///             || States::A(String::new()),
///             |self_| match self_ {
///                 States::A(a) => States::B(a),
///                 States::B(a) => States::A(a),
///             },
///         );
///     }
/// }
/// ```
#[inline]
pub fn replace_with<T, D: FnOnce() -> T, F: FnOnce(T) -> T>(dest: &mut T, default: D, f: F) {
	unsafe {
		let old = ptr::read(dest);
		let new = on_unwind(move || f(old), || ptr::write(dest, default()));
		ptr::write(dest, new);
	}
}

/// Temporarily takes ownership of a value at a mutable location, and replace it with a new value
/// based on the old one. Replaces with [`Default::default()`] on panic.
///
/// We move out of the reference temporarily, to apply a closure `f`, returning a new value, which
/// is then placed at the original value's location.
///
/// # An important note
///
/// On panic (or to be more precise, unwinding) of the closure `f`, `T::default()` will be called to
/// provide a replacement value. `T::default()` should not panic – doing so will constitute a double
/// panic and will most likely abort the process.
///
/// Equivalent to `replace_with(dest, T::default, f)`.
///
/// Differs from `*dest = mem::replace(dest, Default::default())` in that `Default::default()` will
/// only be called on panic.
///
/// # Example
///
/// ```
/// # use replace_with::*;
/// enum States {
///     A(String),
///     B(String),
/// }
///
/// impl Default for States {
///     fn default() -> Self {
///         States::A(String::new())
///     }
/// }
///
/// impl States {
///     fn poll(&mut self) {
///         replace_with_or_default(self, |self_| match self_ {
///             States::A(a) => States::B(a),
///             States::B(a) => States::A(a),
///         });
///     }
/// }
/// ```
#[inline]
pub fn replace_with_or_default<T: Default, F: FnOnce(T) -> T>(dest: &mut T, f: F) {
	replace_with(dest, T::default, f);
}

/// Temporarily takes ownership of a value at a mutable location, and replace it with a new value
/// based on the old one. Aborts on panic.
///
/// We move out of the reference temporarily, to apply a closure `f`, returning a new value, which
/// is then placed at the original value's location.
///
/// # An important note
///
/// On panic (or to be more precise, unwinding) of the closure `f`, the process will **abort** to
/// avoid returning control while `dest` is in a potentially invalid state.
///
/// If this behaviour is undesirable, use [`replace_with`] or [`replace_with_or_default`].
///
/// Equivalent to `replace_with(dest, || process::abort(), f)`.
///
/// # Example
///
/// ```
/// # use replace_with::*;
/// enum States {
///     A(String),
///     B(String),
/// }
///
/// # #[cfg(any(feature = "std", feature = "nightly"))]
/// impl States {
///     fn poll(&mut self) {
///         replace_with_or_abort(self, |self_| match self_ {
///             States::A(a) => States::B(a),
///             States::B(a) => States::A(a),
///         });
///     }
/// }
/// ```
#[inline]
#[cfg(feature = "std")]
pub fn replace_with_or_abort<T, F: FnOnce(T) -> T>(dest: &mut T, f: F) {
	replace_with(dest, || std::process::abort(), f);
}

#[inline]
#[cfg(all(not(feature = "std"), feature = "nightly"))]
pub fn replace_with_or_abort<T, F: FnOnce(T) -> T>(dest: &mut T, f: F) {
	replace_with(dest, || unsafe { std::intrinsics::abort() }, f);
}

/// Temporarily takes ownership of a value at a mutable location, and replace it with a new value
/// based on the old one. Aborts on panic.
///
/// We move out of the reference temporarily, to apply a closure `f`, returning a new value, which
/// is then placed at the original value's location.
///
/// # An important note
///
/// On panic (or to be more precise, unwinding) of the closure `f`, the process will **abort** to
/// avoid returning control while `dest` is in a potentially invalid state.
///
/// Unlike for `replace_with_or_abort()` users of `replace_with_or_abort_unchecked()` are expected
/// to have `features = ["panic_abort", …]` defined in `Cargo.toml`
/// and `panic = "abort"` defined in their profile for it to behave semantically correct:
///
/// ```toml
/// # Cargo.toml
///
/// [profile.debug]
/// panic = "abort"
///
/// [profile.release]
/// panic = "abort"
/// ```
///
/// # Safety
///
/// It is crucial to only ever use this function having defined `panic = "abort"`, or else bad
/// things may happen. It's *up to you* to uphold this invariant!
///
/// If this behaviour is undesirable, use [`replace_with`] or [`replace_with_or_default`].
///
/// Equivalent to `replace_with(dest, || process::abort(), f)`.
///
/// # Example
///
/// ```
/// # use replace_with::*;
/// enum States {
///     A(String),
///     B(String),
/// }
///
/// impl States {
///     fn poll(&mut self) {
///         unsafe {
///             replace_with_or_abort_unchecked(self, |self_| match self_ {
///                 States::A(a) => States::B(a),
///                 States::B(a) => States::A(a),
///             });
///         }
///     }
/// }
/// ```
///
#[inline]
#[cfg(feature = "panic_abort")]
pub unsafe fn replace_with_or_abort_unchecked<T, F: FnOnce(T) -> T>(dest: &mut T, f: F) {
	ptr::write(dest, f(ptr::read(dest)));
}

/// Temporarily takes ownership of a value at a mutable location, and replace it with a new value
/// based on the old one. Lets the closure return a custom value as well.
///
/// We move out of the reference temporarily, to apply a closure `f`, returning a new value, which
/// is then placed at the original value's location.
///
/// This is effectively the same function as [`replace_with`], but it lets the closure return a
/// custom value as well.
///
/// # An important note
///
/// On panic (or to be more precise, unwinding) of the closure `f`, `default` will be called to
/// provide a replacement value. `default` should not panic – doing so will constitute a double
/// panic and will most likely abort the process.
///
/// # Example
///
/// ```
/// # use replace_with::*;
///
/// fn take<T>(option: &mut Option<T>) -> Option<T> {
///     replace_with_and_return(option, || None, |option| (option, None))
/// }
///
/// let mut opt = Some(3);
/// assert_eq!(take(&mut opt), Some(3));
/// assert_eq!(take(&mut opt), None);
/// ```
#[inline]
pub fn replace_with_and_return<T, U, D: FnOnce() -> T, F: FnOnce(T) -> (U, T)>(
	dest: &mut T, default: D, f: F,
) -> U {
	unsafe {
		let old = ptr::read(dest);
		let (res, new) = on_unwind(move || f(old), || ptr::write(dest, default()));
		ptr::write(dest, new);
		res
	}
}

/// Temporarily takes ownership of a value at a mutable location, and replace it with a new value
/// based on the old one. Replaces with [`Default::default()`] on panic.
/// Lets the closure return a custom value as well.
///
/// We move out of the reference temporarily, to apply a closure `f`, returning a new value, which
/// is then placed at the original value's location.
///
/// This is effectively the same function as [`replace_with_or_default`], but it lets the closure
/// return a custom value as well.
///
/// # An important note
///
/// On panic (or to be more precise, unwinding) of the closure `f`, `T::default()` will be called to
/// provide a replacement value. `T::default()` should not panic – doing so will constitute a double
/// panic and will most likely abort the process.
///
/// Equivalent to `replace_with_and_return(dest, T::default, f)`.
///
/// # Example
///
/// ```
/// # use replace_with::*;
///
/// fn take<T>(option: &mut Option<T>) -> Option<T> {
///     replace_with_or_default_and_return(option, |option| (option, None))
/// }
/// ```
#[inline]
pub fn replace_with_or_default_and_return<T: Default, U, F: FnOnce(T) -> (U, T)>(
	dest: &mut T, f: F,
) -> U {
	replace_with_and_return(dest, T::default, f)
}

/// Temporarily takes ownership of a value at a mutable location, and replace it with a new value
/// based on the old one. Aborts on panic. Lets the closure return a custom value as well.
///
/// We move out of the reference temporarily, to apply a closure `f`, returning a new value, which
/// is then placed at the original value's location.
///
/// This is effectively the same function as [`replace_with_or_abort`], but it lets the closure
/// return a custom value as well.
///
/// # An important note
///
/// On panic (or to be more precise, unwinding) of the closure `f`, the process will **abort** to
/// avoid returning control while `dest` is in a potentially invalid state.
///
/// If this behaviour is undesirable, use [`replace_with_and_return`] or
/// [`replace_with_or_default_and_return`] instead.
///
/// Equivalent to `replace_with_and_return(dest, || process::abort(), f)`.
///
/// # Example
///
/// ```
/// # use replace_with::*;
///
/// fn take<T>(option: &mut Option<T>) -> Option<T> {
///     replace_with_or_abort_and_return(option, |option| (option, None))
/// }
/// ```
#[inline]
#[cfg(feature = "std")]
pub fn replace_with_or_abort_and_return<T, U, F: FnOnce(T) -> (U, T)>(dest: &mut T, f: F) -> U {
	replace_with_and_return(dest, || std::process::abort(), f)
}

#[inline]
#[cfg(all(not(feature = "std"), feature = "nightly"))]
pub fn replace_with_or_abort_and_return<T, U, F: FnOnce(T) -> (U, T)>(dest: &mut T, f: F) -> U {
	replace_with_and_return(dest, || unsafe { std::intrinsics::abort() }, f)
}

/// Temporarily takes ownership of a value at a mutable location, and replace it with a new value
/// based on the old one. Aborts on panic. Lets the closure return a custom value as well.
///
/// We move out of the reference temporarily, to apply a closure `f`, returning a new value, which
/// is then placed at the original value's location.
///
/// This is effectively the same function as [`replace_with_or_abort_unchecked`], but it lets the
/// closure return a custom value as well.
///
/// # An important note
///
/// On panic (or to be more precise, unwinding) of the closure `f`, the process will **abort** to
/// avoid returning control while `dest` is in a potentially invalid state.
///
/// Unlike for `replace_with_or_abort()` users of `replace_with_or_abort_unchecked()` are expected
/// to have `features = ["panic_abort", …]` defined in `Cargo.toml`
/// and `panic = "abort"` defined in their profile for it to behave semantically correct:
///
/// ```toml
/// # Cargo.toml
///
/// [profile.debug]
/// panic = "abort"
///
/// [profile.release]
/// panic = "abort"
/// ```
///
/// # Safety
///
/// It is crucial to only ever use this function having defined `panic = "abort"`, or else bad
/// things may happen. It's *up to you* to uphold this invariant!
///
/// If this behaviour is undesirable, use [`replace_with_and_return`] or
/// [`replace_with_or_default_and_return`].
///
/// Equivalent to `replace_with_and_return(dest, || process::abort(), f)`.
///
/// # Example
///
/// ```
/// # use replace_with::*;
///
/// unsafe fn take<T>(option: &mut Option<T>) -> Option<T> {
///     replace_with_or_abort_and_return_unchecked(option, |option| (option, None))
/// }
/// ```
#[inline]
#[cfg(feature = "std")]
#[cfg(feature = "panic_abort")]
pub unsafe fn replace_with_or_abort_and_return_unchecked<T, U, F: FnOnce(T) -> (U, T)>(
	dest: &mut T, f: F,
) -> U {
	let (res, new) = f(ptr::read(dest));
	ptr::write(dest, new);
	res
}

#[cfg(test)]
mod test {
	// These functions copied from https://github.com/Sgeo/take_mut/blob/1bd70d842c6febcd16ec1fe3a954a84032b89f52/src/lib.rs#L102-L147

	// Copyright (c) 2016 Sgeo

	// Permission is hereby granted, free of charge, to any person obtaining a copy
	// of this software and associated documentation files (the "Software"), to deal
	// in the Software without restriction, including without limitation the rights
	// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	// copies of the Software, and to permit persons to whom the Software is
	// furnished to do so, subject to the following conditions:

	// The above copyright notice and this permission notice shall be included in all
	// copies or substantial portions of the Software.

	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	// SOFTWARE.

	use super::*;

	#[test]
	fn it_works_recover() {
		#[derive(PartialEq, Eq, Debug)]
		enum Foo {
			A,
			B,
		};
		impl Drop for Foo {
			#[cfg(feature = "std")]
			fn drop(&mut self) {
				match *self {
					Foo::A => println!("Foo::A dropped"),
					Foo::B => println!("Foo::B dropped"),
				}
			}

			#[cfg(not(feature = "std"))]
			fn drop(&mut self) {
				match *self {
					Foo::A | Foo::B => (),
				}
			}
		}
		let mut quax = Foo::A;
		replace_with(
			&mut quax,
			|| Foo::A,
			|f| {
				drop(f);
				Foo::B
			},
		);
		assert_eq!(&quax, &Foo::B);

		let n = replace_with_and_return(
			&mut quax,
			|| Foo::B,
			|f| {
				drop(f);
				(3, Foo::A)
			},
		);
		assert_eq!(n, 3);
		assert_eq!(&quax, &Foo::A);
	}

	#[cfg(all(feature = "std", not(miri)))] // https://github.com/rust-lang/miri/issues/658
	#[test]
	fn it_works_recover_panic() {
		use std::panic;

		#[derive(PartialEq, Eq, Debug)]
		enum Foo {
			A,
			B,
			C,
		};
		impl Drop for Foo {
			fn drop(&mut self) {
				match *self {
					Foo::A => println!("Foo::A dropped"),
					Foo::B => println!("Foo::B dropped"),
					Foo::C => println!("Foo::C dropped"),
				}
			}
		}
		let mut quax = Foo::A;

		let res = panic::catch_unwind(panic::AssertUnwindSafe(|| {
			replace_with(
				&mut quax,
				|| Foo::C,
				|f| {
					drop(f);
					panic!("panic");
					#[allow(unreachable_code)]
					Foo::B
				},
			);
		}));

		assert!(res.is_err());
		assert_eq!(&quax, &Foo::C);
	}
}
