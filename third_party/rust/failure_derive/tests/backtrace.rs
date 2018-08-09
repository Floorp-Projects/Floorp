extern crate failure;
#[macro_use]
extern crate failure_derive;

use failure::{Backtrace, Fail};

#[derive(Fail, Debug)]
#[fail(display = "Error code: {}", code)]
struct BacktraceError {
    backtrace: Backtrace,
    code: u32,
}

#[test]
fn backtrace_error() {
    let err = BacktraceError {
        backtrace: Backtrace::new(),
        code: 7,
    };
    let s = format!("{}", err);
    assert_eq!(&s[..], "Error code: 7");
    assert!(err.backtrace().is_some());
}

#[derive(Fail, Debug)]
#[fail(display = "An error has occurred.")]
struct BacktraceTupleError(Backtrace);

#[test]
fn backtrace_tuple_error() {
    let err = BacktraceTupleError(Backtrace::new());
    let s = format!("{}", err);
    assert_eq!(&s[..], "An error has occurred.");
    assert!(err.backtrace().is_some());
}

#[derive(Fail, Debug)]
enum BacktraceEnumError {
    #[fail(display = "Error code: {}", code)]
    StructVariant { code: i32, backtrace: Backtrace },
    #[fail(display = "Error: {}", _0)]
    TupleVariant(&'static str, Backtrace),
    #[fail(display = "An error has occurred.")]
    UnitVariant,
}

#[test]
fn backtrace_enum_error() {
    let err = BacktraceEnumError::StructVariant {
        code: 2,
        backtrace: Backtrace::new(),
    };
    let s = format!("{}", err);
    assert_eq!(&s[..], "Error code: 2");
    assert!(err.backtrace().is_some());
    let err = BacktraceEnumError::TupleVariant("foobar", Backtrace::new());
    let s = format!("{}", err);
    assert_eq!(&s[..], "Error: foobar");
    assert!(err.backtrace().is_some());
    let err = BacktraceEnumError::UnitVariant;
    let s = format!("{}", err);
    assert_eq!(&s[..], "An error has occurred.");
    assert!(err.backtrace().is_none());
}
