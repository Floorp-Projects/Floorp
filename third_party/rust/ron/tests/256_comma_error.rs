use ron::error::{Error, ErrorCode, Position};

#[derive(Debug, serde::Deserialize)]
struct Test {
    a: i32,
    b: i32,
}

#[test]
fn test_missing_comma_error() {
    let tuple_string = r#"(
        1 // <-- forgotten comma here
        2
    )"#;

    assert_eq!(
        ron::from_str::<(i32, i32)>(tuple_string).unwrap_err(),
        Error {
            code: ErrorCode::ExpectedComma,
            position: Position { line: 3, col: 9 }
        }
    );

    let list_string = r#"[
        0,
        1 // <-- forgotten comma here
        2
    ]"#;

    assert_eq!(
        ron::from_str::<Vec<i32>>(list_string).unwrap_err(),
        Error {
            code: ErrorCode::ExpectedComma,
            position: Position { line: 4, col: 9 }
        }
    );

    let struct_string = r#"Test(
        a: 1 // <-- forgotten comma here
        b: 2
    )"#;

    assert_eq!(
        ron::from_str::<Test>(struct_string).unwrap_err(),
        Error {
            code: ErrorCode::ExpectedComma,
            position: Position { line: 3, col: 9 }
        }
    );

    let map_string = r#"{
        "a": 1 // <-- forgotten comma here
        "b": 2
    }"#;

    assert_eq!(
        ron::from_str::<std::collections::HashMap<String, i32>>(map_string).unwrap_err(),
        Error {
            code: ErrorCode::ExpectedComma,
            position: Position { line: 3, col: 9 }
        }
    );
}

#[test]
fn test_comma_end() {
    assert_eq!(ron::from_str::<(i32, i32)>("(0, 1)").unwrap(), (0, 1));
    assert_eq!(ron::from_str::<(i32, i32)>("(0, 1,)").unwrap(), (0, 1));
    assert_eq!(ron::from_str::<()>("()"), Ok(()));
}
