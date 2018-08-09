#[macro_use]
extern crate failure;

use std::fmt::Debug;

use failure::Fail;

#[derive(Debug, Fail)]
#[fail(display = "An error has occurred.")]
pub struct UnboundedGenericTupleError<T: 'static + Debug + Send + Sync>(T);

#[test]
fn unbounded_generic_tuple_error() {
    let s = format!("{}", UnboundedGenericTupleError(()));
    assert_eq!(&s[..], "An error has occurred.");
}

#[derive(Debug, Fail)]
#[fail(display = "An error has occurred: {}", _0)]
pub struct FailBoundsGenericTupleError<T: Fail>(T);

#[test]
fn fail_bounds_generic_tuple_error() {
    let error = FailBoundsGenericTupleError(UnboundedGenericTupleError(()));
    let s = format!("{}", error);
    assert_eq!(&s[..], "An error has occurred: An error has occurred.");
}

pub trait NoDisplay: 'static + Debug + Send + Sync {}

impl NoDisplay for &'static str {}

#[derive(Debug, Fail)]
#[fail(display = "An error has occurred: {:?}", _0)]
pub struct CustomBoundsGenericTupleError<T: NoDisplay>(T);

#[test]
fn custom_bounds_generic_tuple_error() {
    let error = CustomBoundsGenericTupleError("more details unavailable.");
    let s = format!("{}", error);
    assert_eq!(
        &s[..],
        "An error has occurred: \"more details unavailable.\""
    );
}
