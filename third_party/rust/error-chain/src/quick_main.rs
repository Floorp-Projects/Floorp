/// Convenient wrapper to be able to use `try!` and such in the main. You can
/// use it with a separated function:
///
/// ```ignore
/// # #[macro_use] extern crate error_chain;
/// # error_chain! {}
/// quick_main!(run);
///
/// fn run() -> Result<()> {
///     Err("error".into())
/// }
/// ```
///
/// or with a closure:
///
/// ```ignore
/// # #[macro_use] extern crate error_chain;
/// # error_chain! {}
/// quick_main!(|| -> Result<()> {
///     Err("error".into())
/// });
/// ```
///
/// You can also set the exit value of the process by returning a type that implements [`ExitCode`](trait.ExitCode.html):
///
/// ```ignore
/// # #[macro_use] extern crate error_chain;
/// # error_chain! {}
/// quick_main!(run);
///
/// fn run() -> Result<u32> {
///     Err("error".into())
/// }
/// ```
#[macro_export]
macro_rules! quick_main {
    ($main:expr) => {
        fn main() {
            use ::std::io::Write;
            let stderr = &mut ::std::io::stderr();
            let errmsg = "Error writing to stderr";

            ::std::process::exit(match $main() {
                Ok(ret) => $crate::ExitCode::code(ret),
                Err(ref e) => {
                    let e: &$crate::ChainedError<ErrorKind=_> = e;
                    writeln!(stderr, "Error: {}", e).expect(errmsg);

                    for e in e.iter().skip(1) {
                        writeln!(stderr, "Caused by: {}", e).expect(errmsg);
                    }

                    if let Some(backtrace) = e.backtrace() {
                        writeln!(stderr, "{:?}", backtrace).expect(errmsg);
                    }

                    1
                }
            });
        }
    };
}

/// Represents a value that can be used as the exit status of the process.
/// See [`quick_main!`](macro.quick_main.html).
pub trait ExitCode {
    /// Returns the value to use as the exit status.
    fn code(self) -> i32;
}

impl ExitCode for i32 {
    fn code(self) -> i32 { self }
}

impl ExitCode for () {
    fn code(self) -> i32 { 0 }
}
