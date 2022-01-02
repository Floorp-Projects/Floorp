//! A try_format! macro replacing format!
use super::FallibleVec;
use crate::TryReserveError;
use alloc::fmt::{Arguments, Write};
use alloc::string::String;

/// Take a max capacity a try allocating a string with it.
///
/// # Warning:
///
/// the max capacity must be > to the formating of the
/// arguments. If writing the argument on the string exceed the
/// capacity, no error is return and an allocation can occurs which
/// can lead to a panic
pub fn try_format(max_capacity: usize, args: Arguments<'_>) -> Result<String, TryReserveError> {
    let v = FallibleVec::try_with_capacity(max_capacity)?;
    let mut s = String::from_utf8(v).expect("wtf an empty vec should be valid utf8");
    s.write_fmt(args)
        .expect("a formatting trait implementation returned an error");
    Ok(s)
}

#[macro_export]
/// Take a max capacity a try allocating a string with it.
///
/// # Warning:
///
/// the max capacity must be > to the formating of the
/// arguments. If writing the argument on the string exceed the
/// capacity, no error is return and an allocation can occurs which
/// can lead to a panic
macro_rules! tryformat {
    ($max_capacity:tt, $($arg:tt)*) => (
        $crate::format::try_format($max_capacity, format_args!($($arg)*))
    )
}

#[cfg(test)]
mod tests {
    #[test]
    fn format() {
        assert_eq!(tryformat!(1, "1").unwrap(), format!("1"));
        assert_eq!(tryformat!(1, "{}", 1).unwrap(), format!("{}", 1));
        assert_eq!(tryformat!(3, "{}", 123).unwrap(), format!("{}", 123));
    }
}
