use super::*;

#[derive(Debug, PartialEq, Deserialize)]
struct EmptyStruct1;

#[derive(Debug, PartialEq, Deserialize)]
struct EmptyStruct2 {}

#[derive(Clone, Copy, Debug, PartialEq, Deserialize)]
struct MyStruct { x: f32, y: f32 }

#[derive(Clone, Copy, Debug, PartialEq, Deserialize)]
enum MyEnum {
    A,
    B(bool),
    C(bool, f32),
    D { a: i32, b: i32 }
}

#[test]
fn test_empty_struct() {
    assert_eq!(Ok(EmptyStruct1), from_str("EmptyStruct1"));
    assert_eq!(Ok(EmptyStruct2 {}), from_str("EmptyStruct2()"));
}


#[test]
fn test_struct() {
    let my_struct = MyStruct { x: 4.0, y: 7.0 };

    assert_eq!(Ok(my_struct), from_str("MyStruct(x:4,y:7,)"));
    assert_eq!(Ok(my_struct), from_str("(x:4,y:7)"));

    #[derive(Debug, PartialEq, Deserialize)]
    struct NewType(i32);

    assert_eq!(Ok(NewType(42)), from_str("NewType(42)"));
    assert_eq!(Ok(NewType(33)), from_str("(33)"));

    #[derive(Debug, PartialEq, Deserialize)]
    struct TupleStruct(f32, f32);

    assert_eq!(Ok(TupleStruct(2.0, 5.0)), from_str("TupleStruct(2,5,)"));
    assert_eq!(Ok(TupleStruct(3.0, 4.0)), from_str("(3,4)"));
}


#[test]
fn test_option() {
    assert_eq!(Ok(Some(1u8)), from_str("Some(1)"));
    assert_eq!(Ok(None::<u8>), from_str("None"));
}

#[test]
fn test_enum() {
    assert_eq!(Ok(MyEnum::A), from_str("A"));
    assert_eq!(Ok(MyEnum::B(true)), from_str("B(true,)"));
    assert_eq!(Ok(MyEnum::C(true, 3.5)), from_str("C(true,3.5,)"));
    assert_eq!(Ok(MyEnum::D { a: 2, b: 3 }), from_str("D(a:2,b:3,)"));
}

#[test]
fn test_array() {
    let empty: [i32; 0] = [];
    assert_eq!(Ok(empty), from_str("()"));
    let empty_array = empty.to_vec();
    assert_eq!(Ok(empty_array), from_str("[]"));

    assert_eq!(Ok([2, 3, 4i32]), from_str("(2,3,4,)"));
    assert_eq!(Ok(([2, 3, 4i32].to_vec())), from_str("[2,3,4,]"));
}

#[test]
fn test_map() {
    use std::collections::HashMap;

    let mut map = HashMap::new();
    map.insert((true, false), 4);
    map.insert((false, false), 123);

    assert_eq!(Ok(map), from_str("{
        (true,false,):4,
        (false,false,):123,
    }"));
}

#[test]
fn test_string() {
    let s: String = from_str("\"String\"").unwrap();

    assert_eq!("String", s);
}

#[test]
fn test_char() {
    assert_eq!(Ok('c'), from_str("'c'"));
}

#[test]
fn test_escape_char() {
    assert_eq!('\'', from_str::<char>("'\\''").unwrap());
}

#[test]
fn test_escape() {
    assert_eq!("\"Quoted\"", from_str::<String>(r#""\"Quoted\"""#).unwrap());
}

#[test]
fn test_comment() {
    assert_eq!(MyStruct { x: 1.0, y: 2.0 }, from_str("(
x: 1.0, // x is just 1
// There is another comment in the very next line..
// And y is indeed
y: 2.0 // 2!
    )").unwrap());
}

fn err<T>(kind: ParseError, line: usize, col: usize) -> Result<T> {
    use parse::Position;

    Err(Error::Parser(kind, Position { line, col }))
}

#[test]
fn test_err_wrong_value() {
    use self::ParseError::*;
    use std::collections::HashMap;

    assert_eq!(from_str::<f32>("'c'"), err(ExpectedFloat, 1, 1));
    assert_eq!(from_str::<String>("'c'"), err(ExpectedString, 1, 1));
    assert_eq!(from_str::<HashMap<u32, u32>>("'c'"), err(ExpectedMap, 1, 1));
    assert_eq!(from_str::<[u8; 5]>("'c'"), err(ExpectedArray, 1, 1));
    assert_eq!(from_str::<Vec<u32>>("'c'"), err(ExpectedArray, 1, 1));
    assert_eq!(from_str::<MyEnum>("'c'"), err(ExpectedIdentifier, 1, 1));
    assert_eq!(from_str::<MyStruct>("'c'"), err(ExpectedStruct, 1, 1));
    assert_eq!(from_str::<(u8, bool)>("'c'"), err(ExpectedArray, 1, 1));
    assert_eq!(from_str::<bool>("notabool"), err(ExpectedBoolean, 1, 1));

    assert_eq!(from_str::<MyStruct>("MyStruct(\n    x: true)"), err(ExpectedFloat, 2, 8));
    assert_eq!(from_str::<MyStruct>("MyStruct(\n    x: 3.5, \n    y:)"),
               err(ExpectedFloat, 3, 7));
}

#[test]
fn test_perm_ws() {
    assert_eq!(from_str::<MyStruct>("\nMyStruct  \t ( \n x   : 3.5 , \t y\n: 4.5 \n ) \t\n"),
                Ok(MyStruct { x: 3.5, y: 4.5 }));
}

#[test]
fn untagged() {
    #[derive(Deserialize, Debug, PartialEq)]
    #[serde(untagged)]
    enum Untagged {
        U8(u8),
        Bool(bool),
    }

    assert_eq!(from_str::<Untagged>("true").unwrap(), Untagged::Bool(true));
}

#[test]
fn forgot_apostrophes() {
    let de: Result<(i32, String)> = from_str("(4, \"Hello)");

    assert!(match de {
        Err(Error::Parser(ParseError::ExpectedStringEnd, _)) => true,
        _ => false,
    });
}
