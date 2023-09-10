use ron::error::{Error, Position, SpannedError};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
#[serde(rename = "Hello World")]
struct InvalidStruct;

#[derive(Serialize, Deserialize, Debug)]
#[serde(rename = "")]
struct EmptyStruct;

#[derive(Serialize, Deserialize, PartialEq, Eq, Debug)]
#[serde(rename = "Hello+World")]
#[serde(deny_unknown_fields)]
struct RawStruct {
    #[serde(rename = "ab.cd-ef")]
    field: bool,
}

#[derive(Serialize, Deserialize, PartialEq, Eq, Debug)]
enum RawEnum {
    #[serde(rename = "Hello-World")]
    RawVariant,
}

#[test]
fn test_invalid_identifiers() {
    let ser = ron::ser::to_string_pretty(
        &InvalidStruct,
        ron::ser::PrettyConfig::default().struct_names(true),
    );
    assert_eq!(
        ser,
        Err(Error::InvalidIdentifier(String::from("Hello World")))
    );

    let ser = ron::ser::to_string_pretty(
        &EmptyStruct,
        ron::ser::PrettyConfig::default().struct_names(true),
    );
    assert_eq!(ser, Err(Error::InvalidIdentifier(String::from(""))));

    let de = ron::from_str::<InvalidStruct>("Hello World").unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::ExpectedDifferentStructName {
                expected: "Hello World",
                found: String::from("Hello"),
            },
            position: Position { line: 1, col: 6 },
        }
    );

    let de = ron::from_str::<EmptyStruct>("").unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::ExpectedUnit,
            position: Position { line: 1, col: 1 },
        }
    );

    let de = ron::from_str::<EmptyStruct>("r#").unwrap_err();
    assert_eq!(
        format!("{}", de),
        "1:1: Expected only opening `(`, no name, for un-nameable struct"
    );

    let de = ron::from_str::<RawStruct>("").unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::ExpectedNamedStructLike("Hello+World"),
            position: Position { line: 1, col: 1 },
        },
    );

    let de = ron::from_str::<RawStruct>("r#").unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::ExpectedNamedStructLike("Hello+World"),
            position: Position { line: 1, col: 1 },
        },
    );

    let de = ron::from_str::<RawStruct>("Hello+World").unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::SuggestRawIdentifier(String::from("Hello+World")),
            position: Position { line: 1, col: 1 },
        }
    );

    let de = ron::from_str::<RawStruct>(
        "r#Hello+World(
        ab.cd-ef: true,
    )",
    )
    .unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::SuggestRawIdentifier(String::from("ab.cd-ef")),
            position: Position { line: 2, col: 9 },
        }
    );

    let de = ron::from_str::<RawStruct>(
        "r#Hello+World(
        r#ab.cd+ef: true,
    )",
    )
    .unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::NoSuchStructField {
                expected: &["ab.cd-ef"],
                found: String::from("ab.cd+ef"),
                outer: Some(String::from("Hello+World")),
            },
            position: Position { line: 2, col: 19 },
        }
    );

    let de = ron::from_str::<RawEnum>("Hello-World").unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::SuggestRawIdentifier(String::from("Hello-World")),
            position: Position { line: 1, col: 1 },
        }
    );

    let de = ron::from_str::<RawEnum>("r#Hello+World").unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::NoSuchEnumVariant {
                expected: &["Hello-World"],
                found: String::from("Hello+World"),
                outer: Some(String::from("RawEnum")),
            },
            position: Position { line: 1, col: 14 },
        }
    );

    let de = ron::from_str::<EmptyStruct>("r#+").unwrap_err();
    assert_eq!(
        format!("{}", de),
        r#"1:4: Expected struct ""_[invalid identifier] but found `r#+`"#,
    );
}

#[test]
fn test_raw_identifier_roundtrip() {
    let val = RawStruct { field: true };

    let ser =
        ron::ser::to_string_pretty(&val, ron::ser::PrettyConfig::default().struct_names(true))
            .unwrap();
    assert_eq!(ser, "r#Hello+World(\n    r#ab.cd-ef: true,\n)");

    let de: RawStruct = ron::from_str(&ser).unwrap();
    assert_eq!(de, val);

    let val = RawEnum::RawVariant;

    let ser = ron::ser::to_string(&val).unwrap();
    assert_eq!(ser, "r#Hello-World");

    let de: RawEnum = ron::from_str(&ser).unwrap();
    assert_eq!(de, val);
}
