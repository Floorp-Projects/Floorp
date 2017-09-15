use traits::FnBox;

/// `BoxFnOnce` boxes any `FnOnce` function up to a certain number of
/// arguments (10 as of now).
///
/// As `Box<FnOnce()>` doesn't work yet, and `Box<FnBox()>` will not be
/// available in stable rust, `BoxFnOnce` tries to provide a safe
/// implementation.
///
/// Instead of `Box<FnOnce(Args...) -> Result>` (or `Box<FnBox(Args...)
/// -> Result>`) the box type is `BoxFnOnce<(Args...,), Result>`  (the
/// arguments are always given as tuple type).  If the function doesn't
/// return a value (i.e. the empty tuple) `Result` can be ommitted:
/// `BoxFnOnce<(Args...,)>`.
///
/// Internally it is implemented similar to `Box<FnBox()>`, but there is
/// no `FnOnce` implementation for `BoxFnOnce`.
///
/// You can build boxes for diverging functions too, but specifying the
/// type (like `BoxFnOnce<(), !>`) is not possible as the `!` type is
/// experimental.
///
/// If you need to send the FnOnce use `SendBoxFnOnce` instead.
///
/// # Examples
///
/// Move value into closure and box it:
///
/// ```
/// use boxfnonce::BoxFnOnce;
/// let s = String::from("foo");
/// let f : BoxFnOnce<()> = BoxFnOnce::from(|| {
///     println!("Got called: {}", s);
///     drop(s);
/// });
/// f.call();
/// ```
///
/// Move value into closure to return it, and box the closure:
///
/// ```
/// use boxfnonce::BoxFnOnce;
/// let s = String::from("foo");
/// let f : BoxFnOnce<(), String> = BoxFnOnce::from(|| {
///     println!("Got called: {}", s);
///     s
/// });
/// assert_eq!(f.call(), "foo".to_string());
/// ```
pub struct BoxFnOnce<Arguments, Result = ()> (Box<FnBox<Arguments, Result>>);

impl<Args, Result> BoxFnOnce<Args, Result> {
	/// call inner function, consumes the box.
	///
	/// `call_tuple` can be used if the arguments are available as
	/// tuple. Each usable instance of BoxFnOnce<(...), Result> has a
	/// separate `call` method for passing arguments "untupled".
	#[inline]
	pub fn call_tuple(self, args: Args) -> Result {
		self.0.call(args)
	}

	/// `BoxFnOnce::new` is an alias for `BoxFnOnce::from`.
	#[inline]
	pub fn new<F>(func: F) -> Self
	where Self: From<F>
	{
		Self::from(func)
	}
}

build_n_args!(BoxFnOnce[]: );
build_n_args!(BoxFnOnce[]: a1: A1);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2, a3: A3);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2, a3: A3, a4: A4);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8, a9: A9);
build_n_args!(BoxFnOnce[]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8, a9: A9, a10: A10);

#[cfg(test)]
mod test {
	use super::BoxFnOnce;

	use std::rc::Rc;

	struct NoSendString(String, Rc<()>);
	impl NoSendString {
		fn into(self) -> String {
			self.0
		}
	}

	fn closure_string() -> NoSendString {
		NoSendString(String::from("abc"), Rc::new(()))
	}

	#[test]
	fn test_arg0() {
		let f = BoxFnOnce::from({
			let s = closure_string();
			move || -> String {
				s.into()
			}
		});
		assert_eq!(f.call(), "abc");
	}

	#[test]
	fn test_arg1() {
		let f = BoxFnOnce::from({
			let s = closure_string();
			move |_| -> String {
				s.into()
			}
		});
		assert_eq!(f.call(0), "abc");
	}

	#[test]
	fn test_arg1_fixed_argument_type() {
		let f : BoxFnOnce<(i32,), String> = BoxFnOnce::from({
			let s = closure_string();
			move |_| -> String {
				s.into()
			}
		});
		assert_eq!(f.call(0), "abc");
	}

	#[test]
	fn test_arg2() {
		let f = BoxFnOnce::from({
			let s = closure_string();
			move |_, _| -> String {
				s.into()
			}
		});
		assert_eq!(f.call(0, 0), "abc");
	}

	#[test]
	fn test_arg3() {
		let f = BoxFnOnce::from({
			let s = closure_string();
			move |_, _, _| -> String {
				s.into()
			}
		});
		assert_eq!(f.call(0, 0, 0), "abc");
	}

	#[test]
	fn test_arg4_void() {
		let f = BoxFnOnce::from({
			let s = closure_string();
			move |_, _, _, _| {
				drop(s);
			}
		});
		f.call(0, 0, 0, 0);
	}

	#[test]
	#[should_panic(expected = "inner diverging")]
	fn test_arg4_diverging() {
		let f = BoxFnOnce::from({
			let s = closure_string();
			move |_, _, _, _| -> ! {
				drop(s);
				panic!("inner diverging");
			}
		});
		f.call(0, 0, 0, 0);
	}
}
