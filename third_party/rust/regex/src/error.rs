use std::fmt;
use std::iter::repeat;

/// An error that occurred during parsing or compiling a regular expression.
#[derive(Clone, PartialEq)]
pub enum Error {
    /// A syntax error.
    Syntax(String),
    /// The compiled program exceeded the set size limit.
    /// The argument is the size limit imposed.
    CompiledTooBig(usize),
    /// Hints that destructuring should not be exhaustive.
    ///
    /// This enum may grow additional variants, so this makes sure clients
    /// don't count on exhaustive matching. (Otherwise, adding a new variant
    /// could break existing code.)
    #[doc(hidden)]
    __Nonexhaustive,
}

impl ::std::error::Error for Error {
    // TODO: Remove this method entirely on the next breaking semver release.
    #[allow(deprecated)]
    fn description(&self) -> &str {
        match *self {
            Error::Syntax(ref err) => err,
            Error::CompiledTooBig(_) => "compiled program too big",
            Error::__Nonexhaustive => unreachable!(),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Error::Syntax(ref err) => err.fmt(f),
            Error::CompiledTooBig(limit) => write!(
                f,
                "Compiled regex exceeds size limit of {} bytes.",
                limit
            ),
            Error::__Nonexhaustive => unreachable!(),
        }
    }
}

// We implement our own Debug implementation so that we show nicer syntax
// errors when people use `Regex::new(...).unwrap()`. It's a little weird,
// but the `Syntax` variant is already storing a `String` anyway, so we might
// as well format it nicely.
impl fmt::Debug for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Error::Syntax(ref err) => {
                let hr: String = repeat('~').take(79).collect();
                writeln!(f, "Syntax(")?;
                writeln!(f, "{}", hr)?;
                writeln!(f, "{}", err)?;
                writeln!(f, "{}", hr)?;
                write!(f, ")")?;
                Ok(())
            }
            Error::CompiledTooBig(limit) => {
                f.debug_tuple("CompiledTooBig").field(&limit).finish()
            }
            Error::__Nonexhaustive => {
                f.debug_tuple("__Nonexhaustive").finish()
            }
        }
    }
}
