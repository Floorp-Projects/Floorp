use serde_derive::Serialize;

use super::to_string;

#[derive(Serialize)]
struct EmptyStruct1;

#[derive(Serialize)]
struct EmptyStruct2 {}

#[derive(Serialize)]
struct MyStruct {
    x: f32,
    y: f32,
}

#[derive(Serialize)]
enum MyEnum {
    A,
    B(bool),
    C(bool, f32),
    D { a: i32, b: i32 },
}

#[test]
fn test_empty_struct() {
    assert_eq!(to_string(&EmptyStruct1).unwrap(), "()");
    assert_eq!(to_string(&EmptyStruct2 {}).unwrap(), "()");
}

#[test]
fn test_struct() {
    let my_struct = MyStruct { x: 4.0, y: 7.0 };

    assert_eq!(to_string(&my_struct).unwrap(), "(x:4.0,y:7.0)");

    #[derive(Serialize)]
    struct NewType(i32);

    assert_eq!(to_string(&NewType(42)).unwrap(), "(42)");

    #[derive(Serialize)]
    struct TupleStruct(f32, f32);

    assert_eq!(to_string(&TupleStruct(2.0, 5.0)).unwrap(), "(2.0,5.0)");
}

#[test]
fn test_option() {
    assert_eq!(to_string(&Some(1u8)).unwrap(), "Some(1)");
    assert_eq!(to_string(&None::<u8>).unwrap(), "None");
}

#[test]
fn test_enum() {
    assert_eq!(to_string(&MyEnum::A).unwrap(), "A");
    assert_eq!(to_string(&MyEnum::B(true)).unwrap(), "B(true)");
    assert_eq!(to_string(&MyEnum::C(true, 3.5)).unwrap(), "C(true,3.5)");
    assert_eq!(to_string(&MyEnum::D { a: 2, b: 3 }).unwrap(), "D(a:2,b:3)");
}

#[test]
fn test_array() {
    let empty: [i32; 0] = [];
    assert_eq!(to_string(&empty).unwrap(), "()");
    let empty_ref: &[i32] = &empty;
    assert_eq!(to_string(&empty_ref).unwrap(), "[]");

    assert_eq!(to_string(&[2, 3, 4i32]).unwrap(), "(2,3,4)");
    assert_eq!(to_string(&(&[2, 3, 4i32] as &[i32])).unwrap(), "[2,3,4]");
}

#[test]
fn test_slice() {
    assert_eq!(to_string(&[0, 1, 2, 3, 4, 5][..]).unwrap(), "[0,1,2,3,4,5]");
    assert_eq!(to_string(&[0, 1, 2, 3, 4, 5][1..4]).unwrap(), "[1,2,3]");
}

#[test]
fn test_vec() {
    assert_eq!(to_string(&vec![0, 1, 2, 3, 4, 5]).unwrap(), "[0,1,2,3,4,5]");
}

#[test]
fn test_map() {
    use std::collections::HashMap;

    let mut map = HashMap::new();
    map.insert((true, false), 4);
    map.insert((false, false), 123);

    let s = to_string(&map).unwrap();
    s.starts_with('{');
    s.contains("(true,false):4");
    s.contains("(false,false):123");
    s.ends_with('}');
}

#[test]
fn test_string() {
    assert_eq!(to_string(&"Some string").unwrap(), "\"Some string\"");
}

#[test]
fn test_char() {
    assert_eq!(to_string(&'c').unwrap(), "'c'");
}

#[test]
fn test_escape() {
    assert_eq!(to_string(&r#""Quoted""#).unwrap(), r#""\"Quoted\"""#);
}

#[test]
fn test_byte_stream() {
    use serde_bytes;

    let small: [u8; 16] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
    assert_eq!(
        to_string(&small).unwrap(),
        "(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)"
    );

    let large = vec![255u8; 64];
    let large = serde_bytes::Bytes::new(&large);
    assert_eq!(
        to_string(&large).unwrap(),
        concat!(
            "\"/////////////////////////////////////////",
            "////////////////////////////////////////////w==\""
        )
    );
}

#[test]
fn rename() {
    #[derive(Serialize, Debug, PartialEq)]
    enum Foo {
        #[serde(rename = "2d")]
        D2,
        #[serde(rename = "triangle-list")]
        TriangleList,
    }

    assert_eq!(to_string(&Foo::D2).unwrap(), "r#2d");
    assert_eq!(to_string(&Foo::TriangleList).unwrap(), "r#triangle-list");
}
