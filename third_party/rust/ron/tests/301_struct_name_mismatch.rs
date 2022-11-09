use ron::error::{Error, Position, SpannedError};
use serde::Deserialize;

#[derive(Debug, Deserialize, PartialEq)]
struct MyUnitStruct;

#[derive(Debug, Deserialize, PartialEq)]
struct MyTupleStruct(bool, i32);

#[derive(Debug, Deserialize, PartialEq)]
struct MyNewtypeStruct(MyTupleStruct);

#[derive(Debug, Deserialize, PartialEq)]
struct MyStruct {
    a: bool,
    b: i32,
}

#[test]
fn test_unit_struct_name_mismatch() {
    assert_eq!(ron::from_str::<MyUnitStruct>("()"), Ok(MyUnitStruct),);
    assert_eq!(
        ron::from_str::<MyUnitStruct>("MyUnitStruct"),
        Ok(MyUnitStruct),
    );
    assert_eq!(
        ron::from_str::<MyUnitStruct>("MyUnit Struct"),
        Err(SpannedError {
            code: Error::ExpectedDifferentStructName {
                expected: "MyUnitStruct",
                found: String::from("MyUnit")
            },
            position: Position { line: 1, col: 7 }
        }),
    );
    assert_eq!(
        ron::from_str::<MyUnitStruct>("42"),
        Err(SpannedError {
            code: Error::ExpectedNamedStructLike("MyUnitStruct"),
            position: Position { line: 1, col: 1 }
        }),
    );
}

#[test]
fn test_tuple_struct_name_mismatch() {
    assert_eq!(
        ron::from_str::<MyTupleStruct>("(true, 42)"),
        Ok(MyTupleStruct(true, 42)),
    );
    assert_eq!(
        ron::from_str::<MyTupleStruct>("MyTupleStruct(true, 42)"),
        Ok(MyTupleStruct(true, 42)),
    );
    assert_eq!(
        ron::from_str::<MyTupleStruct>("MyTypleStruct(true, 42)"),
        Err(SpannedError {
            code: Error::ExpectedDifferentStructName {
                expected: "MyTupleStruct",
                found: String::from("MyTypleStruct")
            },
            position: Position { line: 1, col: 14 }
        }),
    );
    assert_eq!(
        ron::from_str::<MyTupleStruct>("42"),
        Err(SpannedError {
            code: Error::ExpectedNamedStructLike("MyTupleStruct"),
            position: Position { line: 1, col: 1 }
        }),
    );
}

#[test]
fn test_newtype_struct_name_mismatch() {
    assert_eq!(
        ron::from_str::<MyNewtypeStruct>("((true, 42))"),
        Ok(MyNewtypeStruct(MyTupleStruct(true, 42))),
    );
    assert_eq!(
        ron::from_str::<MyNewtypeStruct>("MyNewtypeStruct((true, 42))"),
        Ok(MyNewtypeStruct(MyTupleStruct(true, 42))),
    );
    assert_eq!(
        ron::from_str::<MyNewtypeStruct>("MyNewtypeStrucl((true, 42))"),
        Err(SpannedError {
            code: Error::ExpectedDifferentStructName {
                expected: "MyNewtypeStruct",
                found: String::from("MyNewtypeStrucl")
            },
            position: Position { line: 1, col: 16 }
        }),
    );
    assert_eq!(
        ron::from_str::<MyNewtypeStruct>("42"),
        Err(SpannedError {
            code: Error::ExpectedNamedStructLike("MyNewtypeStruct"),
            position: Position { line: 1, col: 1 }
        }),
    );
}

#[test]
fn test_struct_name_mismatch() {
    assert_eq!(
        ron::from_str::<MyStruct>("(a: true, b: 42)"),
        Ok(MyStruct { a: true, b: 42 }),
    );
    assert_eq!(
        ron::from_str::<MyStruct>("MyStruct(a: true, b: 42)"),
        Ok(MyStruct { a: true, b: 42 }),
    );
    assert_eq!(
        ron::from_str::<MyStruct>("MuStryct(a: true, b: 42)"),
        Err(SpannedError {
            code: Error::ExpectedDifferentStructName {
                expected: "MyStruct",
                found: String::from("MuStryct")
            },
            position: Position { line: 1, col: 9 }
        }),
    );
    assert_eq!(
        ron::from_str::<MyStruct>("42"),
        Err(SpannedError {
            code: Error::ExpectedNamedStructLike("MyStruct"),
            position: Position { line: 1, col: 1 }
        }),
    );
}
