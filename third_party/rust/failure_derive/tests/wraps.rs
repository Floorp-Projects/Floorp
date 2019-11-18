extern crate failure;
#[macro_use]
extern crate failure_derive;

use std::fmt;
use std::io;

use failure::{Backtrace, Fail};

#[derive(Fail, Debug)]
#[fail(display = "An error has occurred: {}", inner)]
struct WrapError {
    #[fail(cause)]
    inner: io::Error,
}

#[test]
fn wrap_error() {
    let inner = io::Error::from_raw_os_error(98);
    let err = WrapError { inner };
    assert!(err
        .cause()
        .and_then(|err| err.downcast_ref::<io::Error>())
        .is_some());
}

#[derive(Fail, Debug)]
#[fail(display = "An error has occurred: {}", _0)]
struct WrapTupleError(#[fail(cause)] io::Error);

#[test]
fn wrap_tuple_error() {
    let io_error = io::Error::from_raw_os_error(98);
    let err: WrapTupleError = WrapTupleError(io_error);
    assert!(err
        .cause()
        .and_then(|err| err.downcast_ref::<io::Error>())
        .is_some());
}

#[derive(Fail, Debug)]
#[fail(display = "An error has occurred: {}", inner)]
struct WrapBacktraceError {
    #[fail(cause)]
    inner: io::Error,
    backtrace: Backtrace,
}

#[test]
fn wrap_backtrace_error() {
    let inner = io::Error::from_raw_os_error(98);
    let err: WrapBacktraceError = WrapBacktraceError {
        inner,
        backtrace: Backtrace::new(),
    };
    assert!(err
        .cause()
        .and_then(|err| err.downcast_ref::<io::Error>())
        .is_some());
    assert!(err.backtrace().is_some());
    assert!(err.backtrace().unwrap().is_empty());
    assert!(err.backtrace().unwrap().to_string().trim().is_empty());
}

#[derive(Fail, Debug)]
enum WrapEnumError {
    #[fail(display = "An error has occurred: {}", _0)]
    Io(#[fail(cause)] io::Error),
    #[fail(display = "An error has occurred: {}", inner)]
    Fmt {
        #[fail(cause)]
        inner: fmt::Error,
        backtrace: Backtrace,
    },
}

#[test]
fn wrap_enum_error() {
    let io_error = io::Error::from_raw_os_error(98);
    let err: WrapEnumError = WrapEnumError::Io(io_error);
    assert!(err
        .cause()
        .and_then(|err| err.downcast_ref::<io::Error>())
        .is_some());
    assert!(err.backtrace().is_none());
    let fmt_error = fmt::Error::default();
    let err: WrapEnumError = WrapEnumError::Fmt {
        inner: fmt_error,
        backtrace: Backtrace::new(),
    };
    assert!(err
        .cause()
        .and_then(|err| err.downcast_ref::<fmt::Error>())
        .is_some());
    assert!(err.backtrace().is_some());
    assert!(err.backtrace().unwrap().is_empty());
    assert!(err.backtrace().unwrap().to_string().trim().is_empty());
}
