use ron::{de::from_str, error::ErrorCode};
use serde::Deserialize;

#[derive(Debug, Deserialize, Eq, PartialEq)]
enum TestEnum {
    Unit,
    PrimitiveNewtype(String),
    Tuple(u32, bool),
    Struct { a: u32, b: bool },
    TupleNewtypeUnit(Unit),
    TupleNewtypeNewtype(Newtype),
    TupleNewtypeTuple((u32, bool)),
    TupleNewtypeTupleStruct(TupleStruct),
    TupleNewtypeStruct(Struct),
    TupleNewtypeEnum(Enum),
}

#[derive(Debug, Deserialize, Eq, PartialEq)]
struct Unit;

#[derive(Debug, Deserialize, Eq, PartialEq)]
struct Newtype(i32);

#[derive(Debug, Deserialize, Eq, PartialEq)]
struct TupleStruct(u32, bool);

#[derive(Debug, Deserialize, Eq, PartialEq)]
struct Struct {
    a: u32,
    b: bool,
}

#[derive(Debug, Deserialize, Eq, PartialEq)]
enum Enum {
    A,
    B(Struct),
}

#[test]
fn test_deserialise_non_newtypes() {
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] Unit"#).unwrap(),
        TestEnum::Unit,
    );

    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] PrimitiveNewtype("hi")"#)
            .unwrap(),
        TestEnum::PrimitiveNewtype(String::from("hi")),
    );

    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] Tuple(4, false)"#).unwrap(),
        TestEnum::Tuple(4, false),
    );

    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] Struct(a: 4, b: false)"#)
            .unwrap(),
        TestEnum::Struct { a: 4, b: false },
    );
}

#[test]
fn test_deserialise_tuple_newtypes() {
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeUnit(Unit)"#)
            .unwrap_err()
            .code,
        ErrorCode::ExpectedStructEnd,
    );
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeUnit(())"#)
            .unwrap_err()
            .code,
        ErrorCode::ExpectedStructEnd,
    );
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeUnit()"#).unwrap(),
        TestEnum::TupleNewtypeUnit(Unit),
    );

    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeNewtype(Newtype(4))"#
        )
        .unwrap_err()
        .code,
        ErrorCode::ExpectedInteger,
    );
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeNewtype((4))"#)
            .unwrap_err()
            .code,
        ErrorCode::ExpectedInteger,
    );
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeNewtype(4)"#)
            .unwrap(),
        TestEnum::TupleNewtypeNewtype(Newtype(4)),
    );
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_newtypes)] TupleNewtypeNewtype(4)"#).unwrap(),
        TestEnum::TupleNewtypeNewtype(Newtype(4)),
    );
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_newtypes)] #![enable(unwrap_variant_newtypes)] TupleNewtypeNewtype(4)"#).unwrap(),
        TestEnum::TupleNewtypeNewtype(Newtype(4)),
    );

    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeTuple((4, false))"#
        )
        .unwrap_err()
        .code,
        ErrorCode::ExpectedInteger,
    );
    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeTuple(4, false)"#)
            .unwrap(),
        TestEnum::TupleNewtypeTuple((4, false)),
    );

    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeTupleStruct(TupleStruct(4, false))"#
        )
        .unwrap_err()
        .code,
        ErrorCode::ExpectedInteger,
    );
    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeTupleStruct((4, false))"#
        )
        .unwrap_err()
        .code,
        ErrorCode::ExpectedInteger,
    );
    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeTupleStruct(4, false)"#
        )
        .unwrap(),
        TestEnum::TupleNewtypeTupleStruct(TupleStruct(4, false)),
    );

    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeStruct(Struct(a: 4, b: false))"#
        )
        .unwrap_err()
        .code,
        ErrorCode::ExpectedMapColon,
    );
    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeStruct((a: 4, b: false))"#
        )
        .unwrap_err()
        .code,
        ErrorCode::ExpectedIdentifier,
    );
    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeStruct(a: 4, b: false)"#
        )
        .unwrap(),
        TestEnum::TupleNewtypeStruct(Struct { a: 4, b: false }),
    );

    assert_eq!(
        from_str::<TestEnum>(r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeEnum(A)"#).unwrap(),
        TestEnum::TupleNewtypeEnum(Enum::A),
    );
    assert_eq!(
        from_str::<TestEnum>(
            r#"#![enable(unwrap_variant_newtypes)] TupleNewtypeEnum(B(a: 4, b: false))"#
        )
        .unwrap(),
        TestEnum::TupleNewtypeEnum(Enum::B(Struct { a: 4, b: false })),
    );
}
