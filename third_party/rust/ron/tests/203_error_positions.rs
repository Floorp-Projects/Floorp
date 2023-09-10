use std::num::NonZeroU32;

use ron::error::{Error, Position, SpannedError};
use serde::{
    de::{Deserialize, Error as DeError, Unexpected},
    Deserializer,
};

#[derive(Debug, serde::Deserialize, PartialEq)]
#[serde(deny_unknown_fields)]
enum Test {
    TupleVariant(i32, String),
    StructVariant { a: bool, b: NonZeroU32, c: i32 },
}

#[derive(Debug, PartialEq)] // GRCOV_EXCL_LINE
struct TypeError;

impl<'de> Deserialize<'de> for TypeError {
    fn deserialize<D: Deserializer<'de>>(_deserializer: D) -> Result<Self, D::Error> {
        Err(D::Error::invalid_type(Unexpected::Unit, &"impossible"))
    }
}

#[test]
fn test_error_positions() {
    assert_eq!(
        ron::from_str::<TypeError>("  ()"),
        Err(SpannedError {
            code: Error::InvalidValueForType {
                expected: String::from("impossible"),
                found: String::from("a unit value"),
            },
            position: Position { line: 1, col: 3 },
        })
    );

    assert_eq!(
        ron::from_str::<Test>("StructVariant(a: true, b: 0, c: -42)"),
        Err(SpannedError {
            code: Error::InvalidValueForType {
                expected: String::from("a nonzero u32"),
                found: String::from("the unsigned integer `0`"),
            },
            position: Position { line: 1, col: 28 },
        })
    );

    assert_eq!(
        ron::from_str::<Test>("TupleVariant(42)"),
        Err(SpannedError {
            code: Error::ExpectedDifferentLength {
                expected: String::from("tuple variant Test::TupleVariant with 2 elements"),
                found: 1,
            },
            position: Position { line: 1, col: 16 },
        })
    );

    assert_eq!(
        ron::from_str::<Test>("NotAVariant"),
        Err(SpannedError {
            code: Error::NoSuchEnumVariant {
                expected: &["TupleVariant", "StructVariant"],
                found: String::from("NotAVariant"),
                outer: Some(String::from("Test")),
            },
            position: Position { line: 1, col: 12 },
        })
    );

    assert_eq!(
        ron::from_str::<Test>("StructVariant(a: true, b: 1, c: -42, d: \"gotcha\")"),
        Err(SpannedError {
            code: Error::NoSuchStructField {
                expected: &["a", "b", "c"],
                found: String::from("d"),
                outer: Some(String::from("StructVariant")),
            },
            position: Position { line: 1, col: 39 },
        })
    );

    assert_eq!(
        ron::from_str::<Test>("StructVariant(a: true, c: -42)"),
        Err(SpannedError {
            code: Error::MissingStructField {
                field: "b",
                outer: Some(String::from("StructVariant")),
            },
            position: Position { line: 1, col: 30 },
        })
    );

    assert_eq!(
        ron::from_str::<Test>("StructVariant(a: true, b: 1, a: false, c: -42)"),
        Err(SpannedError {
            code: Error::DuplicateStructField {
                field: "a",
                outer: Some(String::from("StructVariant")),
            },
            position: Position { line: 1, col: 31 },
        })
    );
}
