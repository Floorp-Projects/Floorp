use traits::FnBox;

/// `SendBoxFnOnce` boxes any `FnOnce + Send` function up to a certain
/// number of arguments (10 as of now).
///
/// As `Box<FnOnce()>` doesn't work yet, and `Box<FnBox()>` will not be
/// available in stable rust, `SendBoxFnOnce` tries to provide a safe
/// implementation.
///
/// Instead of `Box<FnOnce(Args...) -> Result>` (or `Box<FnBox(Args...)
/// -> Result>`) the box type is `SendBoxFnOnce<(Args...,), Result>`  (the
/// arguments are always given as tuple type).  If the function doesn't
/// return a value (i.e. the empty tuple) `Result` can be ommitted:
/// `SendBoxFnOnce<(Args...,)>`.
///
/// Internally it is implemented similar to `Box<FnBox()>`, but there is
/// no `FnOnce` implementation for `SendBoxFnOnce`.
///
/// You can build boxes for diverging functions too, but specifying the
/// type (like `SendBoxFnOnce<(), !>`) is not possible as the `!` type is
/// experimental.
///
/// # Examples
///
/// Move value into closure to return it, box the closure and send it:
///
/// ```
/// use boxfnonce::SendBoxFnOnce;
/// use std::thread;
///
/// let s = String::from("foo");
/// let f : SendBoxFnOnce<(), String> = SendBoxFnOnce::from(|| {
///     println!("Got called: {}", s);
///     s
/// });
/// let result = thread::Builder::new().spawn(move || {
///     f.call()
/// }).unwrap().join().unwrap();
/// assert_eq!(result, "foo".to_string());
/// ```
pub struct SendBoxFnOnce<Arguments, Result = ()> (Box<FnBox<Arguments, Result> + Send>);

impl<Args, Result> SendBoxFnOnce<Args, Result> {
	/// call inner function, consumes the box.
	///
	/// `call_tuple` can be used if the arguments are available as
	/// tuple. Each usable instance of SendBoxFnOnce<(...), Result> has
	/// a separate `call` method for passing arguments "untupled".
	#[inline]
	pub fn call_tuple(self, args: Args) -> Result {
		self.0.call(args)
	}

	/// `SendBoxFnOnce::new` is an alias for `SendBoxFnOnce::from`.
	#[inline]
	pub fn new<F>(func: F) -> Self
	where Self: From<F>
	{
		Self::from(func)
	}
}

build_n_args!(SendBoxFnOnce[+Send]: );
build_n_args!(SendBoxFnOnce[+Send]: a1: A1);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2, a3: A3);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2, a3: A3, a4: A4);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8, a9: A9);
build_n_args!(SendBoxFnOnce[+Send]: a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8, a9: A9, a10: A10);

#[cfg(test)]
mod test {
	use super::SendBoxFnOnce;

	use std::thread;
	use std::sync::Arc;

	struct SendString(String, Arc<()>);
	impl SendString {
		fn into(self) -> String {
			self.0
		}
	}

	fn closure_string() -> SendString {
		SendString(String::from("abc"), Arc::new(()))
	}

	fn try_send<Result, F>(name: &str, func: F) -> thread::Result<Result>
	where
		Result: 'static + Send,
		F: 'static + Send + FnOnce() -> Result
	{
		thread::Builder::new().name(name.to_string()).spawn(func).unwrap().join()
	}

	fn send<Result, F>(func: F) -> Result
	where
		Result: 'static + Send,
		F: 'static + Send + FnOnce() -> Result
	{
		try_send("test thread", func).unwrap()
	}

	#[test]
	fn test_arg0() {
		let f = SendBoxFnOnce::from({
			let s = closure_string();
			|| -> String {
				s.into()
			}
		});
		let result = send(|| {
			f.call()
		});
		assert_eq!(result, "abc");
	}

	#[test]
	fn test_arg1() {
		let f = SendBoxFnOnce::from({
			let s = closure_string();
			|_| -> String {
				s.into()
			}
		});
		let result = send(|| {
			f.call(0)
		});
		assert_eq!(result, "abc");
	}

	#[test]
	fn test_arg1_fixed_argument_type() {
		let f : SendBoxFnOnce<(i32,), String> = SendBoxFnOnce::from({
			let s = closure_string();
			|_| -> String {
				s.into()
			}
		});
		let result = send(|| {
			f.call(0)
		});
		assert_eq!(result, "abc");
	}

	#[test]
	fn test_arg2() {
		let f = SendBoxFnOnce::from({
			let s = closure_string();
			|_, _| -> String {
				s.into()
			}
		});
		let result = send(|| {
			f.call(0, 0)
		});
		assert_eq!(result, "abc");
	}

	#[test]
	fn test_arg3() {
		let f = SendBoxFnOnce::from({
			let s = closure_string();
			|_, _, _| -> String {
				s.into()
			}
		});
		let result = send(|| {
			f.call(0, 0, 0)
		});
		assert_eq!(result, "abc");
	}

	#[test]
	fn test_arg4_void() {
		let f = SendBoxFnOnce::from({
			let s = closure_string();
			|_, _, _, _| {
				drop(s);
			}
		});
		send(|| {
			f.call(0, 0, 0, 0);
		});
	}

	#[test]
	fn test_arg4_diverging() {
		use std::panic;

		let f = SendBoxFnOnce::from({
			let s = closure_string();
			|_, _, _, _| -> ! {
				drop(s);
				// a panic! but without the default hook printing stuff
				panic::resume_unwind(Box::new("inner diverging"));
			}
		});
		let result = try_send("diverging test thread", || {
			f.call(0, 0, 0, 0);
		}).map_err(|e| e.downcast::<&str>().unwrap());
		assert_eq!(result, Err(Box::new("inner diverging")));
	}
}
