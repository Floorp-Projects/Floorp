use std::fmt::Display;
use thiserror::Error;

#[derive(Error, Debug)]
#[error("braced error: {msg}")]
struct BracedError {
    msg: String,
}

#[derive(Error, Debug)]
#[error("braced error")]
struct BracedUnused {
    extra: usize,
}

#[derive(Error, Debug)]
#[error("tuple error: {0}")]
struct TupleError(usize);

#[derive(Error, Debug)]
#[error("unit error")]
struct UnitError;

#[derive(Error, Debug)]
enum EnumError {
    #[error("braced error: {id}")]
    Braced { id: usize },
    #[error("tuple error: {0}")]
    Tuple(usize),
    #[error("unit error")]
    Unit,
}

#[derive(Error, Debug)]
#[error("{0}")]
enum Inherit {
    Unit(UnitError),
    #[error("other error")]
    Other(UnitError),
}

#[derive(Error, Debug)]
#[error("1 + 1 = {}", 1 + 1)]
struct Arithmetic;

#[derive(Error, Debug)]
#[error("!bool = {}", not(.0))]
struct NestedShorthand(bool);

#[derive(Error, Debug)]
#[error("...")]
pub enum Void {}

fn not(bool: &bool) -> bool {
    !*bool
}

fn assert<T: Display>(expected: &str, value: T) {
    assert_eq!(expected, value.to_string());
}

#[test]
fn test_display() {
    let msg = "T".to_owned();
    assert("braced error: T", BracedError { msg });
    assert("braced error", BracedUnused { extra: 0 });
    assert("tuple error: 0", TupleError(0));
    assert("unit error", UnitError);
    assert("braced error: 0", EnumError::Braced { id: 0 });
    assert("tuple error: 0", EnumError::Tuple(0));
    assert("unit error", EnumError::Unit);
    assert("unit error", Inherit::Unit(UnitError));
    assert("other error", Inherit::Other(UnitError));
    assert("1 + 1 = 2", Arithmetic);
    assert("!bool = false", NestedShorthand(true));
}
